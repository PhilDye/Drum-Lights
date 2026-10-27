import Foundation

/// A colour as plain numbers, so it can be shared with the watch and widgets and tested
public struct RGB: Sendable, Hashable {
    public let red: Double
    public let green: Double
    public let blue: Double

    public init(red: Double, green: Double, blue: Double) {
        self.red = red
        self.green = green
        self.blue = blue
    }

    /// "#RRGGBB"; anything else becomes grey
    public init(hex: String) {
        let digits = hex.hasPrefix("#") ? String(hex.dropFirst()) : hex
        guard digits.count == 6, let value = UInt32(digits, radix: 16) else {
            self.init(red: 0.5, green: 0.5, blue: 0.5)
            return
        }
        self.init(red: Double((value >> 16) & 0xFF) / 255,
                  green: Double((value >> 8) & 0xFF) / 255,
                  blue: Double(value & 0xFF) / 255)
    }

    /// HSL with hue in degrees, saturation and lightness 0...1
    public init(hue: Double, saturation: Double, lightness: Double) {
        let c = (1 - abs(2 * lightness - 1)) * saturation
        let h = (hue.truncatingRemainder(dividingBy: 360) + 360).truncatingRemainder(dividingBy: 360) / 60
        let x = c * (1 - abs(h.truncatingRemainder(dividingBy: 2) - 1))
        let (r, g, b): (Double, Double, Double)
        switch h {
        case 0..<1: (r, g, b) = (c, x, 0)
        case 1..<2: (r, g, b) = (x, c, 0)
        case 2..<3: (r, g, b) = (0, c, x)
        case 3..<4: (r, g, b) = (0, x, c)
        case 4..<5: (r, g, b) = (x, 0, c)
        default: (r, g, b) = (c, 0, x)
        }
        let m = lightness - c / 2
        self.init(red: r + m, green: g + m, blue: b + m)
    }
}

/// How a mode is previewed on the ring
public enum ModeKind: String, Sendable, CaseIterable {
    case solid, chase, twinkle, fire
    case rioSpin, rioDisco, rioFlag, rainbow, hazards, nineNineNine
    case off, unknown
}

/// On-screen colours and effect kinds, lifted from the mockup (`iOS/mockup/Main.dc.html`, `cat()`).
///
/// The LED hex values in `modes.json` read badly on a phone (pure #0000FF, and "Pink" is Indigo
/// on the LEDs), so swatches use the iOS system colours the mockup chose. Colours or modes the app
/// doesn't know yet fall back to the catalogue's hex, so a newer catalogue still draws.
public enum Appearance {
    static let blue = RGB(hex: "#0A84FF")
    static let green = RGB(hex: "#30D158")
    static let yellow = RGB(hex: "#FFD60A")
    static let offGrey = RGB(hex: "#3A3A3C")
    static let unlit = RGB(hex: "#1C1C1E")

    static let screenColours: [String: RGB] = [
        "green": green,
        "yellow": yellow,
        "blue": blue,
        "cyan": RGB(hex: "#64D2FF"),
        "red": RGB(hex: "#FF453A"),
        "pink": RGB(hex: "#FF6EC7"),
        "orange": RGB(hex: "#FF9F0A"),
        "white": RGB(hex: "#F2F2F7")
    ]

    static let fireColours: [String: [RGB]] = [
        "orange": ["#FF3B30", "#FF9F0A", "#FFD60A"].map(RGB.init(hex:)),
        "green": ["#0B6E2E", "#30D158", "#B4F5C4"].map(RGB.init(hex:)),
        "blue": ["#0040DD", "#0A84FF", "#9FD0FF"].map(RGB.init(hex:)),
        "red": ["#8B0000", "#FF453A", "#FF9F0A"].map(RGB.init(hex:))
    ]

    static let specialKinds: [Int: ModeKind] = [
        91: .rioSpin, 92: .rioDisco, 93: .rioFlag, 99: .rainbow, 97: .hazards, 199: .nineNineNine
    ]

    public static func kind(of mode: Int, in catalogue: Catalogue) -> ModeKind {
        if mode == catalogue.actions.off { return .off }
        if let special = specialKinds[mode] { return special }
        guard let entry = catalogue.entry(for: mode) else { return .unknown }
        return ModeKind(rawValue: entry.effectID) ?? .unknown
    }

    public static func screenColour(_ colourID: String, in catalogue: Catalogue) -> RGB {
        screenColours[colourID] ?? RGB(hex: catalogue.colours[colourID]?.hex ?? "")
    }

    /// The colours that represent a mode in swatches, tile strips and the ring
    public static func colours(for mode: Int, in catalogue: Catalogue) -> [RGB] {
        switch kind(of: mode, in: catalogue) {
        case .off:
            return [offGrey]
        case .rioSpin, .rioDisco:
            return [blue, green, yellow]
        case .rioFlag:
            return [green, yellow, blue]
        case .rainbow:
            return ["#FF453A", "#FF9F0A", "#FFD60A", "#30D158", "#0A84FF", "#BF5AF2"].map(RGB.init(hex:))
        case .hazards:
            return [RGB(hex: "#FF9F0A")]
        case .nineNineNine:
            return [blue]
        case .fire:
            let colour = catalogue.entry(for: mode)?.colours.first ?? ""
            return fireColours[colour] ?? [screenColour(colour, in: catalogue)]
        default:
            let ids = catalogue.entry(for: mode)?.colours ?? []
            return ids.isEmpty ? [offGrey] : ids.map { screenColour($0, in: catalogue) }
        }
    }
}

/// The 24-dot ring on the Live tab, computed for a point in time so SwiftUI can draw it
/// from a `TimelineView` and tests can check it. Mirrors `dots()` and `ringClass()` in the mockup.
public enum RingPattern {
    public static let dotCount = 24

    public struct Dot: Sendable, Equatable {
        public let colour: RGB
        public let opacity: Double
    }

    /// Whole-ring rotation in turns (0..<1); chase, Rio Spin/Flag and Rainbow go round every 6 s
    public static func rotation(kind: ModeKind, at time: TimeInterval) -> Double {
        switch kind {
        case .chase, .rioSpin, .rioFlag, .rainbow:
            return (time / 6).truncatingRemainder(dividingBy: 1)
        default:
            return 0
        }
    }

    public static func dots(kind: ModeKind, colours: [RGB], at time: TimeInterval) -> [Dot] {
        let cols = colours.isEmpty ? [Appearance.offGrey] : colours
        let tail: [Double] = [1, 0.5, 0.25, 0.1, 0.04, 0.04]
        return (0..<dotCount).map { i in
            var colour = cols[0]
            var opacity = 1.0
            switch kind {
            case .chase:
                colour = cols[(i / 6) % cols.count]
                opacity = tail[i % 6]
            case .twinkle, .rioDisco:
                colour = cols[i % cols.count]
                opacity = twinkle(time, delay: Double((i * 373) % 1600) / 1000)
            case .fire:
                colour = cols[i % min(3, cols.count)]
                opacity = flicker(time, delay: Double((i * 97) % 450) / 1000)
            case .rioSpin:
                colour = cols[min(i / 8, cols.count - 1)]
            case .rioFlag:
                colour = cols[(i / 2) % cols.count]
            case .rainbow:
                colour = RGB(hue: Double(i * 15), saturation: 1, lightness: 0.55)
            case .hazards:
                let lit = (3...8).contains(i) || (15...20).contains(i)
                colour = lit ? cols[0] : Appearance.unlit
                opacity = lit ? blink(time, delay: 0) : 1
            case .nineNineNine:
                opacity = blink(time, delay: i < 12 ? 0 : 0.25)
            case .off:
                opacity = 0.5
            case .solid, .unknown:
                break
            }
            return Dot(colour: colour, opacity: opacity)
        }
    }

    // 1.6 s ease in and out between 0.08 and 1
    static func twinkle(_ t: TimeInterval, delay: Double) -> Double {
        let phase = ((t - delay) / 1.6).truncatingRemainder(dividingBy: 1)
        let wave = (1 - cos(2 * .pi * phase)) / 2
        return 0.08 + 0.92 * wave
    }

    // 0.45 s each way between 0.35 and 1
    static func flicker(_ t: TimeInterval, delay: Double) -> Double {
        let phase = ((t - delay) / 0.9).truncatingRemainder(dividingBy: 1)
        let wave = (1 - cos(2 * .pi * phase)) / 2
        return 0.35 + 0.65 * wave
    }

    // 0.5 s period, on for the first half
    static func blink(_ t: TimeInterval, delay: Double) -> Double {
        var phase = ((t - delay) / 0.5).truncatingRemainder(dividingBy: 1)
        if phase < 0 { phase += 1 }
        return phase < 0.5 ? 1 : 0.06
    }
}
