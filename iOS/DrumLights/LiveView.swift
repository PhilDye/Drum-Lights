import DrumLightsKit
import SwiftUI

/// The Live tab (`DESIGN.md` section 5.2): the current mode, then effect and colour as two quick choices
struct LiveView: View {
    @Environment(AppModel.self) private var model
    @State private var effectID: String?
    @State private var showSettings = false

    private var lights: LightsController { model.controller }
    private var catalogue: Catalogue { lights.catalogue }
    private var currentMode: Int? { lights.state?.mode }

    var body: some View {
        VStack(spacing: 12) {
            HStack {
                StatusPill(status: lights.status)
                Spacer()
                Button {
                    showSettings = true
                } label: {
                    Image(systemName: "gearshape")
                        .font(.system(size: 20))
                        .frame(width: 44, height: 44)
                        .background(Theme.surface, in: Circle())
                }
                .foregroundStyle(Theme.primaryText)
                .accessibilityLabel("Settings")
            }

            VStack(spacing: 12) {
                header
                if let warning = lights.compatibility.warning {
                    Label(warning, systemImage: "exclamationmark.triangle.fill")
                        .font(.system(size: 13))
                        .foregroundStyle(Theme.reconnecting)
                        .frame(maxWidth: .infinity, alignment: .leading)
                }
                EffectPicker(effects: catalogue.effects, selection: selectedEffect) { effectID = $0 }
                picker
                    .frame(maxHeight: .infinity)
                ActionRow(offSelected: currentMode == catalogue.actions.off,
                          autoRunning: lights.state?.auto == true,
                          onOff: { lights.off() },
                          onStrobeStart: { lights.beginStrobe() },
                          onStrobeEnd: { lights.endStrobe() },
                          onAuto: { lights.startAuto() })
            }
            .opacity(lights.isConnected ? 1 : Theme.disconnectedOpacity)
            .allowsHitTesting(lights.isConnected)
            .animation(.easeInOut(duration: 0.2), value: lights.isConnected)
        }
        .padding(.horizontal, 16)
        .padding(.top, 6)
        .padding(.bottom, 12)
        .background(Theme.background)
        .foregroundStyle(Theme.primaryText)
        .sheet(isPresented: $showSettings) {
            SettingsView()
        }
        .onChange(of: lights.state?.mode, initial: true) {
            // open on the effect that is showing; after that, browsing is the user's
            if effectID == nil, let mode = currentMode, let entry = catalogue.entry(for: mode) {
                effectID = entry.effectID
            }
        }
    }

    private var selectedEffect: Catalogue.Effect {
        catalogue.effects.first { $0.id == effectID } ?? catalogue.effects[0]
    }

    // MARK: Current mode

    private var header: some View {
        let mode = currentMode ?? catalogue.actions.off
        return HStack(spacing: 16) {
            RingView(kind: Appearance.kind(of: mode, in: catalogue),
                     colours: Appearance.colours(for: mode, in: catalogue),
                     strobeCount: lights.strobeCount)
                .frame(width: 110, height: 110)
                .overlay(alignment: .topLeading) {
                    if lights.state?.auto == true {
                        Text("AUTO")
                            .font(.system(size: 12, weight: .bold))
                            .tracking(0.5)
                            .padding(.horizontal, 10)
                            .padding(.vertical, 4)
                            .background(Theme.accent, in: RoundedRectangle(cornerRadius: 10))
                            .foregroundStyle(.white)
                    }
                }

            VStack(alignment: .leading, spacing: 4) {
                Text(currentMode.map(catalogue.displayName(for:)) ?? "Waiting for the transmitter")
                    .font(.system(size: 22, weight: .bold))
                    .lineLimit(2)
                    .minimumScaleFactor(0.7)
                if let mode = currentMode {
                    Text(lights.state?.auto == true
                         ? "Mode \(mode) · picked by Auto, changes every 30 s"
                         : "Mode \(mode)")
                        .font(.system(size: 13))
                        .foregroundStyle(Theme.secondaryText)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .accessibilityElement(children: .combine)
        }
        .overlay(alignment: .topTrailing) {
            if lights.showSent {
                // the TX accepted it; nothing can confirm the drums did, so never "Delivered"
                Text("Sent to TX")
                    .font(.system(size: 12, weight: .semibold))
                    .foregroundStyle(Theme.connected)
                    .padding(.horizontal, 10)
                    .padding(.vertical, 5)
                    .background(Theme.surface, in: RoundedRectangle(cornerRadius: 12))
                    .transition(.opacity)
            }
        }
        .animation(.easeOut(duration: 0.15), value: lights.showSent)
    }

    // MARK: Swatches and tiles

    @ViewBuilder
    private var picker: some View {
        let effect = selectedEffect
        if effect.modes.allSatisfy({ !$0.colours.isEmpty }) {
            VStack(alignment: .leading, spacing: 10) {
                SwatchGrid(entries: effect.modes, catalogue: catalogue, current: currentMode, onSelect: { lights.select($0) })
                if !effect.twoColour.isEmpty {
                    Text("TWO-COLOUR")
                        .font(.system(size: 11, weight: .semibold))
                        .tracking(0.6)
                        .foregroundStyle(Theme.secondaryText)
                    SwatchGrid(entries: effect.twoColour, catalogue: catalogue, current: currentMode, onSelect: { lights.select($0) })
                }
            }
        } else {
            SpecialsGrid(entries: effect.modes, catalogue: catalogue, current: currentMode,
                         onSelect: { lights.select($0) }, onHoldComplete: { lights.selectAfterHold($0) })
        }
    }
}

/// Segmented control of effects, 44 pt tall
struct EffectPicker: View {
    let effects: [Catalogue.Effect]
    let selection: Catalogue.Effect
    let onSelect: (String) -> Void

    var body: some View {
        HStack(spacing: 6) {
            ForEach(effects) { effect in
                let selected = effect.id == selection.id
                Button {
                    onSelect(effect.id)
                } label: {
                    Text(effect.name)
                        .font(.system(size: 13, weight: .semibold))
                        .lineLimit(1)
                        .minimumScaleFactor(0.8)
                        .frame(maxWidth: .infinity)
                        .frame(height: 44)
                        .background(selected ? Theme.selectedChip : .clear, in: RoundedRectangle(cornerRadius: 8))
                        .foregroundStyle(selected ? Theme.primaryText : Theme.secondaryText)
                }
                .buttonStyle(.plain)
                .accessibilityAddTraits(selected ? .isSelected : [])
            }
        }
        .padding(3)
        .background(Theme.surface, in: RoundedRectangle(cornerRadius: 10))
    }
}

/// Colour swatches, 4 to a row, up to 60 pt and shrinking to fit small phones
struct SwatchGrid: View {
    let entries: [Catalogue.ModeEntry]
    let catalogue: Catalogue
    let current: Int?
    let onSelect: (Int) -> Void

    private let columns = 4

    var body: some View {
        let rows = stride(from: 0, to: entries.count, by: columns).map {
            Array(entries[$0..<min($0 + columns, entries.count)])
        }
        VStack(spacing: 8) {
            ForEach(rows.indices, id: \.self) { index in
                HStack(spacing: 4) {
                    ForEach(rows[index]) { entry in
                        swatch(entry)
                    }
                    ForEach(0..<(columns - rows[index].count), id: \.self) { _ in
                        Color.clear.frame(maxWidth: .infinity)
                    }
                }
            }
        }
    }

    private func swatch(_ entry: Catalogue.ModeEntry) -> some View {
        let selected = entry.mode == current
        return Button {
            onSelect(entry.mode)
        } label: {
            VStack(spacing: 6) {
                Circle()
                    .fill(ColourBands(colours: Appearance.colours(for: entry.mode, in: catalogue)))
                    .padding(5)
                    .overlay(Circle().strokeBorder(Theme.primaryText, lineWidth: 2).opacity(selected ? 1 : 0))
                    .frame(maxWidth: 70, maxHeight: 70)
                    .aspectRatio(1, contentMode: .fit)
                Text(catalogue.label(for: entry.mode)
                        .replacingOccurrences(of: "/", with: " / "))
                    .font(.system(size: 12))
                    .foregroundStyle(Theme.secondaryText)
                    .lineLimit(1)
                    .minimumScaleFactor(0.7)
            }
            .frame(maxWidth: .infinity)
        }
        .buttonStyle(.plain)
        .accessibilityLabel(catalogue.displayName(for: entry.mode))
        .accessibilityAddTraits(selected ? .isSelected : [])
    }
}

/// Named effects as tiles, 2 to a row; modes that need confirming are press-and-hold tiles
struct SpecialsGrid: View {
    let entries: [Catalogue.ModeEntry]
    let catalogue: Catalogue
    let current: Int?
    let onSelect: (Int) -> Void
    let onHoldComplete: (Int) -> Void

    var body: some View {
        let rows = stride(from: 0, to: entries.count, by: 2).map {
            Array(entries[$0..<min($0 + 2, entries.count)])
        }
        VStack(spacing: 8) {
            ForEach(rows.indices, id: \.self) { index in
                HStack(spacing: 8) {
                    ForEach(rows[index]) { entry in
                        tile(entry)
                    }
                    if rows[index].count == 1 {
                        Color.clear.frame(maxWidth: .infinity)
                    }
                }
                .frame(minHeight: 52, maxHeight: 76)
            }
            Spacer(minLength: 0)
        }
    }

    @ViewBuilder
    private func tile(_ entry: Catalogue.ModeEntry) -> some View {
        let selected = entry.mode == current
        let title = catalogue.label(for: entry.mode)
        if entry.needsConfirmation {
            HoldTile(title: title, selected: selected) { onHoldComplete(entry.mode) }
        } else {
            Button {
                onSelect(entry.mode)
            } label: {
                VStack(alignment: .leading, spacing: 6) {
                    Text(title).font(.system(size: 14, weight: .semibold))
                    Capsule()
                        .fill(ColourBands(colours: Appearance.colours(for: entry.mode, in: catalogue)))
                        .frame(height: 5)
                }
                .padding(.horizontal, 12)
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .leading)
                .background(Theme.surface, in: RoundedRectangle(cornerRadius: 12))
                .overlay(RoundedRectangle(cornerRadius: 12)
                    .strokeBorder(selected ? Theme.primaryText : .clear, lineWidth: 2))
            }
            .buttonStyle(.plain)
            .accessibilityAddTraits(selected ? .isSelected : [])
        }
    }
}

#Preview("Connected") {
    LiveView()
        .environment(AppModel.preview())
        .preferredColorScheme(.dark)
}

#Preview("Reconnecting") {
    LiveView()
        .environment(AppModel.preview(connected: false))
        .preferredColorScheme(.dark)
}
