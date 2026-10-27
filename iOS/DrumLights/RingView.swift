import DrumLightsKit
import SwiftUI

/// 24 dots that animate the current mode roughly as the drums show it.
/// Every dot flashes white for each strobe.
struct RingView: View {
    let kind: ModeKind
    let colours: [RGB]
    let strobeCount: Int

    @State private var flashing = false
    @Environment(\.accessibilityReduceMotion) private var reduceMotion

    var body: some View {
        TimelineView(.animation(paused: reduceMotion)) { context in
            let time = context.date.timeIntervalSinceReferenceDate
            Canvas { canvas, size in
                draw(in: &canvas, size: size, time: reduceMotion ? 0 : time)
            }
        }
        .aspectRatio(1, contentMode: .fit)
        .onChange(of: strobeCount) {
            flashing = true
            Task {
                try? await Task.sleep(for: .milliseconds(120))
                flashing = false
            }
        }
        .accessibilityHidden(true)
    }

    private func draw(in canvas: inout GraphicsContext, size: CGSize, time: TimeInterval) {
        let side = min(size.width, size.height)
        // proportions from the mockup: a 170 pt ring with 12 pt dots on a 72 pt radius
        let dotSize = side * 12 / 170
        let radius = side * 72 / 170
        let centre = CGPoint(x: size.width / 2, y: size.height / 2)
        let turn = RingPattern.rotation(kind: kind, at: time)
        let dots = RingPattern.dots(kind: kind, colours: colours, at: time)

        for (index, dot) in dots.enumerated() {
            let angle: Double = (Double(index) / Double(dots.count) + turn) * 2 * .pi - .pi / 2
            let point = CGPoint(x: centre.x + radius * CGFloat(Foundation.cos(angle)),
                                y: centre.y + radius * CGFloat(Foundation.sin(angle)))
            let rect = CGRect(x: point.x - dotSize / 2, y: point.y - dotSize / 2, width: dotSize, height: dotSize)
            let colour = flashing ? Color.white : Color(dot.colour)
            var layer = canvas
            layer.opacity = flashing ? 1 : dot.opacity
            layer.addFilter(.shadow(color: colour, radius: dotSize * 0.6))
            layer.fill(Path(ellipseIn: rect), with: .color(colour))
        }
    }
}

#Preview {
    let catalogue = try! Catalogue.bundled()
    return LazyVGrid(columns: Array(repeating: GridItem(), count: 3)) {
        ForEach([13, 61, 50, 91, 92, 93, 99, 97, 199, 0], id: \.self) { mode in
            RingView(kind: Appearance.kind(of: mode, in: catalogue),
                     colours: Appearance.colours(for: mode, in: catalogue),
                     strobeCount: 0)
        }
    }
    .padding()
    .background(.black)
}
