import Foundation

/// The mode catalogue: a bundled copy of `TX/app/src/modes.json`, the single source
/// of mode numbers shared with the web UI and checked against the firmware.
public struct Catalogue: Sendable, Equatable {
    /// `catalogue` in the JSON; compared with the version the TX reports
    public let version: Int
    public let colours: [String: Colour]
    /// In display order
    public let effects: [Effect]
    public let actions: Actions
    /// The modes the TX picks from while Auto runs
    public let auto: [Int]

    private let entriesByMode: [Int: ModeEntry]

    public struct Colour: Sendable, Equatable {
        public let id: String
        public let name: String
        /// The FastLED colour the receivers use, e.g. "Indigo" for Pink
        public let fastled: String
        /// The LED colour; see `Appearance` for the colour drawn on screen
        public let hex: String
    }

    public struct Effect: Sendable, Equatable, Identifiable {
        public let id: String
        public let name: String
        public let modes: [ModeEntry]
        public let twoColour: [ModeEntry]
    }

    public struct ModeEntry: Sendable, Equatable, Identifiable {
        public let mode: Int
        public let effectID: String
        public let colours: [String]
        public let name: String?
        /// Present for modes that must not fire on a stray tap (999)
        public let confirm: Confirm?

        public var id: Int { mode }
        public var needsConfirmation: Bool { confirm != nil }
    }

    public struct Confirm: Sendable, Equatable, Decodable {
        public let title: String
        public let message: String
    }

    public struct Actions: Sendable, Equatable, Decodable {
        public let off: Int
        public let auto: Int
        public let strobe: Int
    }

    public enum LoadError: Error, Equatable {
        case missingResource
        case duplicateMode(Int)
        case unknownColour(mode: Int, colour: String)
    }

    /// The copy bundled with the app
    public static func bundled() throws -> Catalogue {
        guard let url = Bundle.module.url(forResource: "modes", withExtension: "json") else {
            throw LoadError.missingResource
        }
        return try Catalogue(json: Data(contentsOf: url))
    }

    public init(json: Data) throws {
        let raw = try JSONDecoder().decode(RawCatalogue.self, from: json)

        var colours: [String: Colour] = [:]
        for (id, c) in raw.colours {
            colours[id] = Colour(id: id, name: c.name, fastled: c.fastled, hex: c.hex)
        }

        var byMode: [Int: ModeEntry] = [:]
        var effects: [Effect] = []
        for effect in raw.effects {
            func entries(_ list: [RawMode]?) throws -> [ModeEntry] {
                try (list ?? []).map { m in
                    for colour in m.colours ?? [] where colours[colour] == nil {
                        throw LoadError.unknownColour(mode: m.mode, colour: colour)
                    }
                    let entry = ModeEntry(mode: m.mode, effectID: effect.id, colours: m.colours ?? [],
                                          name: m.name, confirm: m.confirm)
                    if byMode[m.mode] != nil { throw LoadError.duplicateMode(m.mode) }
                    byMode[m.mode] = entry
                    return entry
                }
            }
            effects.append(Effect(id: effect.id, name: effect.name,
                                  modes: try entries(effect.modes), twoColour: try entries(effect.twoColour)))
        }

        self.version = raw.catalogue
        self.colours = colours
        self.effects = effects
        self.actions = raw.actions
        self.auto = raw.auto
        self.entriesByMode = byMode
    }

    public func entry(for mode: Int) -> ModeEntry? {
        entriesByMode[mode]
    }

    public var allModes: [ModeEntry] {
        effects.flatMap { $0.modes + $0.twoColour }
    }

    /// Short label, as on the web UI buttons: the mode's name, or its colours joined with "/"
    public func label(for mode: Int) -> String {
        if mode == actions.off { return "Off" }
        if mode == actions.strobe { return "Strobe" }
        guard let entry = entry(for: mode) else { return "Mode \(mode)" }
        if let name = entry.name { return name }
        return entry.colours.map(colourName).joined(separator: "/")
    }

    /// Full name for the current-mode line, e.g. "Blue Chase", "Green / Red Twinkle", "Solid Cyan"
    public func displayName(for mode: Int) -> String {
        if mode == actions.off { return "Off" }
        if mode == actions.strobe { return "Strobe" }
        guard let entry = entry(for: mode) else { return "Mode \(mode)" }
        if let name = entry.name { return name }
        let colourNames = entry.colours.map(colourName).joined(separator: " / ")
        let effectName = effects.first { $0.id == entry.effectID }?.name ?? ""
        if entry.effectID == "solid" { return "\(effectName) \(colourNames)" }
        return "\(colourNames) \(effectName)"
    }

    private func colourName(_ id: String) -> String {
        colours[id]?.name ?? id
    }
}

private struct RawCatalogue: Decodable {
    let catalogue: Int
    let colours: [String: RawColour]
    let effects: [RawEffect]
    let actions: Catalogue.Actions
    let auto: [Int]
}

private struct RawColour: Decodable {
    let name: String
    let fastled: String
    let hex: String
}

private struct RawEffect: Decodable {
    let id: String
    let name: String
    let modes: [RawMode]?
    let twoColour: [RawMode]?
}

private struct RawMode: Decodable {
    let mode: Int
    let colours: [String]?
    let name: String?
    let confirm: Catalogue.Confirm?
}
