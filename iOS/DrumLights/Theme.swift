import DrumLightsKit
import SwiftUI

/// The iOS dark-mode palette the mockup uses (`iOS/mockup/README.md`)
enum Theme {
    static let background = Color.black
    static let surface = Color(hex: 0x1C1C1E)
    static let surfaceRaised = Color(hex: 0x2C2C2E)
    static let selectedChip = Color(hex: 0x48484A)
    static let primaryText = Color(hex: 0xF2F2F7)
    static let secondaryText = Color(hex: 0xAEAEB2)
    static let accent = Color(hex: 0x0A84FF)
    static let autoBackground = Color(hex: 0x0A2A4D)
    static let strobe = Color(hex: 0xFF9F0A)
    static let strobeHint = Color(hex: 0x3A2400)
    static let connected = Color(hex: 0x30D158)
    static let reconnecting = Color(hex: 0xFF9F0A)

    /// Controls fade to this while disconnected
    static let disconnectedOpacity = 0.35
}

extension Color {
    init(hex: UInt32) {
        self.init(red: Double((hex >> 16) & 0xFF) / 255,
                  green: Double((hex >> 8) & 0xFF) / 255,
                  blue: Double(hex & 0xFF) / 255)
    }

    init(_ rgb: RGB) {
        self.init(red: rgb.red, green: rgb.green, blue: rgb.blue)
    }
}

/// Hard-edged bands of colour, like the mockup's swatches and tile strips
struct ColourBands: ShapeStyle {
    let colours: [RGB]
    var horizontal = true

    func resolve(in environment: EnvironmentValues) -> LinearGradient {
        let n = max(colours.count, 1)
        let stops = colours.enumerated().flatMap { index, rgb in
            [Gradient.Stop(color: Color(rgb), location: Double(index) / Double(n)),
             Gradient.Stop(color: Color(rgb), location: Double(index + 1) / Double(n))]
        }
        return LinearGradient(stops: stops,
                              startPoint: horizontal ? .leading : .top,
                              endPoint: horizontal ? .trailing : .bottom)
    }
}
