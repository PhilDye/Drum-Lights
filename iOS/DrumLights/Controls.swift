import DrumLightsKit
import SwiftUI

/// "● Connected · Bluetooth", or amber "Reconnecting…"; never a blocking modal
struct StatusPill: View {
    let status: ConnectionStatus

    var body: some View {
        HStack(spacing: 8) {
            Circle()
                .fill(status.isConnected ? Theme.connected : Theme.reconnecting)
                .frame(width: 9, height: 9)
            Text(text)
                .font(.system(size: 14, weight: .semibold))
                .lineLimit(1)
        }
        .padding(.horizontal, 14)
        .frame(height: 34)
        .background(Theme.surface, in: Capsule())
        .accessibilityElement(children: .combine)
    }

    private var text: String {
        switch status {
        case .connected(let kind): return "Connected · \(kind.rawValue)"
        case .reconnecting: return "Reconnecting…"
        case .searching: return "Searching…"
        case .unavailable(let reason): return reason
        }
    }
}

/// Fires `onPress` when a finger lands and `onRelease` when it lifts or slides off
private struct PressGesture: ViewModifier {
    let onPress: () -> Void
    let onRelease: () -> Void
    @State private var pressed = false

    func body(content: Content) -> some View {
        content.gesture(
            DragGesture(minimumDistance: 0)
                .onChanged { _ in
                    guard !pressed else { return }
                    pressed = true
                    onPress()
                }
                .onEnded { _ in
                    pressed = false
                    onRelease()
                }
        )
    }
}

extension View {
    func onPress(_ press: @escaping () -> Void, release: @escaping () -> Void) -> some View {
        modifier(PressGesture(onPress: press, onRelease: release))
    }
}

/// A tile that needs a press-and-hold (999): a bar fills behind the label, and letting go early cancels
struct HoldTile: View {
    let title: String
    let selected: Bool
    var duration: Duration = .milliseconds(600)
    let onComplete: () -> Void

    @State private var progress: CGFloat = 0
    @State private var holdTask: Task<Void, Never>?

    var body: some View {
        ZStack(alignment: .leading) {
            GeometryReader { geometry in
                Theme.accent
                    .opacity(0.45)
                    .frame(width: geometry.size.width * progress)
            }
            VStack(alignment: .leading, spacing: 2) {
                Text(title).font(.system(size: 14, weight: .bold))
                Text("Hold to activate").font(.system(size: 11)).foregroundStyle(Theme.secondaryText)
            }
            .padding(.horizontal, 12)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .leading)
        .background(Theme.surface)
        .clipShape(RoundedRectangle(cornerRadius: 12))
        .overlay(RoundedRectangle(cornerRadius: 12).strokeBorder(selected ? Theme.primaryText : .clear, lineWidth: 2))
        .contentShape(Rectangle())
        .onPress(begin, release: cancel)
        .accessibilityElement(children: .ignore)
        .accessibilityLabel("\(title), press and hold to activate")
        .accessibilityAddTraits(.isButton)
        .accessibilityAction { onComplete() }
    }

    private func begin() {
        let seconds = Double(duration.components.seconds) + Double(duration.components.attoseconds) / 1e18
        withAnimation(.linear(duration: seconds)) { progress = 1 }
        holdTask = Task {
            try? await Task.sleep(for: duration)
            guard !Task.isCancelled else { return }
            holdTask = nil
            onComplete()
            withAnimation(.easeOut(duration: 0.15)) { progress = 0 }
        }
    }

    private func cancel() {
        holdTask?.cancel()
        holdTask = nil
        withAnimation(.easeOut(duration: 0.15)) { progress = 0 }
    }
}

/// Off, Strobe (widest, tap or hold to repeat) and Auto, in thumb reach
struct ActionRow: View {
    let offSelected: Bool
    let autoRunning: Bool
    let onOff: () -> Void
    let onStrobeStart: () -> Void
    let onStrobeEnd: () -> Void
    let onAuto: () -> Void

    var body: some View {
        HStack(spacing: 10) {
            Button(action: onOff) {
                Text("Off")
                    .font(.system(size: 15, weight: .semibold))
                    .frame(width: 76, height: 68)
                    .background(Theme.surface, in: RoundedRectangle(cornerRadius: 16))
                    .overlay(RoundedRectangle(cornerRadius: 16)
                        .strokeBorder(offSelected ? Theme.primaryText : .clear, lineWidth: 2))
            }

            VStack(spacing: 1) {
                Text("STROBE").font(.system(size: 19, weight: .heavy)).tracking(2)
                Text("Tap, or hold to repeat").font(.system(size: 11, weight: .semibold))
                    .foregroundStyle(Theme.strobeHint)
            }
            .foregroundStyle(.black)
            .frame(maxWidth: .infinity)
            .frame(height: 68)
            .background(Theme.strobe, in: RoundedRectangle(cornerRadius: 16))
            .contentShape(Rectangle())
            .onPress(onStrobeStart, release: onStrobeEnd)
            .accessibilityElement(children: .ignore)
            .accessibilityLabel("Strobe")
            .accessibilityHint("Tap for one flash, hold to repeat")
            .accessibilityAddTraits(.isButton)
            .accessibilityAction {
                onStrobeStart()
                onStrobeEnd()
            }

            Button(action: onAuto) {
                Text("Auto")
                    .font(.system(size: 15, weight: .semibold))
                    .frame(width: 76, height: 68)
                    .background(autoRunning ? Theme.autoBackground : Theme.surface,
                                in: RoundedRectangle(cornerRadius: 16))
                    .overlay(RoundedRectangle(cornerRadius: 16)
                        .strokeBorder(autoRunning ? Theme.accent : .clear, lineWidth: 2))
            }
            .accessibilityValue(autoRunning ? "Running" : "")
        }
        .buttonStyle(.plain)
        .foregroundStyle(Theme.primaryText)
    }
}
