import XCTest
@testable import DrumLightsKit

final class CatalogueTests: XCTestCase {
    func testBundledCatalogueLoads() throws {
        let catalogue = try Catalogue.bundled()
        XCTAssertEqual(catalogue.effects.map(\.id), ["solid", "twinkle", "chase", "fire", "fx"])
        XCTAssertEqual(catalogue.actions, Catalogue.Actions(off: 0, auto: -1, strobe: 98))
        XCTAssertEqual(catalogue.auto.count, 24)
    }

    /// The app bundles a copy of the web UI's catalogue; this fails when the two drift apart
    func testBundledCopyMatchesWebCatalogue() throws {
        let repoCopy = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent() // DrumLightsKitTests
            .deletingLastPathComponent() // Tests
            .deletingLastPathComponent() // DrumLightsKit
            .deletingLastPathComponent() // iOS
            .deletingLastPathComponent() // repo root
            .appendingPathComponent("TX/app/src/modes.json")
        guard FileManager.default.fileExists(atPath: repoCopy.path) else {
            throw XCTSkip("not running inside the Drum-Lights repo")
        }
        let bundled = try XCTUnwrap(Bundle.module.url(forResource: "modes", withExtension: "json"))
        XCTAssertEqual(try Data(contentsOf: bundled), try Data(contentsOf: repoCopy),
                       "copy TX/app/src/modes.json to iOS/DrumLightsKit/Sources/DrumLightsKit/Resources/")
    }

    func testLookupAndLabels() throws {
        let catalogue = try Catalogue.bundled()
        XCTAssertEqual(catalogue.entry(for: 13)?.effectID, "chase")
        XCTAssertEqual(catalogue.label(for: 13), "Blue")
        XCTAssertEqual(catalogue.label(for: 22), "Yellow/Blue")
        XCTAssertEqual(catalogue.label(for: 91), "Rio Spin")
        XCTAssertEqual(catalogue.displayName(for: 13), "Blue Chase")
        XCTAssertEqual(catalogue.displayName(for: 61), "Green / Red Twinkle")
        XCTAssertEqual(catalogue.displayName(for: 6), "Solid Cyan")
        XCTAssertEqual(catalogue.displayName(for: 52), "Blue Fire")
        XCTAssertEqual(catalogue.displayName(for: 0), "Off")
        XCTAssertEqual(catalogue.displayName(for: 4242), "Mode 4242")
    }

    func testConfirmationModes() throws {
        let catalogue = try Catalogue.bundled()
        XCTAssertEqual(catalogue.allModes.filter(\.needsConfirmation).map(\.mode), [199])
    }

    func testAutoModesAreAllInTheCatalogue() throws {
        let catalogue = try Catalogue.bundled()
        for mode in catalogue.auto {
            XCTAssertNotNil(catalogue.entry(for: mode), "auto mode \(mode)")
        }
    }

    func testRejectsDuplicatesAndUnknownColours() {
        let duplicate = """
        {"catalogue":1,"colours":{"red":{"name":"Red","fastled":"Red","hex":"#FF0000"}},
         "effects":[{"id":"solid","name":"Solid","modes":[{"mode":1,"colours":["red"]},{"mode":1,"colours":["red"]}]}],
         "actions":{"off":0,"auto":-1,"strobe":98},"auto":[]}
        """
        XCTAssertThrowsError(try Catalogue(json: Data(duplicate.utf8))) {
            XCTAssertEqual($0 as? Catalogue.LoadError, .duplicateMode(1))
        }
        let unknown = duplicate.replacingOccurrences(of: "{\"mode\":1,\"colours\":[\"red\"]}]",
                                                     with: "{\"mode\":2,\"colours\":[\"teal\"]}]")
        XCTAssertThrowsError(try Catalogue(json: Data(unknown.utf8))) {
            XCTAssertEqual($0 as? Catalogue.LoadError, .unknownColour(mode: 2, colour: "teal"))
        }
    }

    func testEveryModeHasAKindAndColours() throws {
        let catalogue = try Catalogue.bundled()
        for entry in catalogue.allModes {
            XCTAssertNotEqual(Appearance.kind(of: entry.mode, in: catalogue), .unknown, "mode \(entry.mode)")
            XCTAssertFalse(Appearance.colours(for: entry.mode, in: catalogue).isEmpty)
        }
        XCTAssertEqual(Appearance.kind(of: 0, in: catalogue), .off)
        XCTAssertEqual(Appearance.kind(of: 199, in: catalogue), .nineNineNine)
        XCTAssertEqual(Appearance.colours(for: 22, in: catalogue).count, 2)
    }

    func testRingPattern() {
        let red = RGB(hex: "#FF0000")
        let blue = RGB(hex: "#0000FF")
        let chase = RingPattern.dots(kind: .chase, colours: [red, blue], at: 0)
        XCTAssertEqual(chase.count, RingPattern.dotCount)
        XCTAssertEqual(chase[0], .init(colour: red, opacity: 1))
        XCTAssertEqual(chase[6].colour, blue)
        XCTAssertEqual(RingPattern.rotation(kind: .chase, at: 3), 0.5, accuracy: 1e-9)
        XCTAssertEqual(RingPattern.rotation(kind: .solid, at: 3), 0)

        // 999: the two halves alternate
        let early = RingPattern.dots(kind: .nineNineNine, colours: [blue], at: 0.1)
        XCTAssertEqual(early[0].opacity, 1)
        XCTAssertLessThan(early[12].opacity, 0.1)

        // hazards: only the two side segments light
        let hazards = RingPattern.dots(kind: .hazards, colours: [red], at: 0.1)
        XCTAssertEqual(hazards[5].colour, red)
        XCTAssertNotEqual(hazards[0].colour, red)

        for kind in ModeKind.allCases {
            for dot in RingPattern.dots(kind: kind, colours: [red], at: 1.234) {
                XCTAssert((0...1).contains(dot.opacity), "\(kind)")
            }
        }
    }

    func testHexParsing() {
        XCTAssertEqual(RGB(hex: "#FF8000"), RGB(red: 1, green: 128.0 / 255, blue: 0))
        XCTAssertEqual(RGB(hex: "nonsense"), RGB(red: 0.5, green: 0.5, blue: 0.5))
    }
}
