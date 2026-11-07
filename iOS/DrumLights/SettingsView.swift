import DrumLightsKit
import SwiftUI

/// Sheet from the gear: transmitter and pairing, WiFi fallback, feedback, versions
struct SettingsView: View {
    @Environment(AppModel.self) private var model
    @Environment(\.dismiss) private var dismiss
    @State private var confirmForget = false

    private var lights: LightsController { model.controller }

    var body: some View {
        @Bindable var settings = model.settings
        NavigationStack {
            Form {
                Section("Transmitter") {
                    LabeledContent(lights.transmitterName ?? "Drum Lights TX", value: linkDescription)
                    NavigationLink {
                        WiFiFallbackView()
                    } label: {
                        LabeledContent("WiFi fallback", value: wifiDescription)
                    }
                    Button("Forget transmitter", role: .destructive) {
                        confirmForget = true
                    }
                    .confirmationDialog("Forget this transmitter?", isPresented: $confirmForget, titleVisibility: .visible) {
                        Button("Forget", role: .destructive) { model.forgetTransmitter() }
                    } message: {
                        Text("The app will look for any Drum Lights transmitter again. To remove the Bluetooth pairing as well, forget it in Settings › Bluetooth.")
                    }
                }

                Section("Feedback") {
                    Toggle("Haptics", isOn: $settings.hapticsEnabled)
                    Picker("Strobe repeat rate", selection: $settings.strobeIntervalMilliseconds) {
                        ForEach(AppSettings.strobeIntervals, id: \.self) { ms in
                            Text("Every \(ms) ms").tag(ms)
                        }
                    }
                    .onChange(of: settings.strobeIntervalMilliseconds) { model.strobeIntervalChanged() }
                }

                Section {
                    LabeledContent("Effect catalogue", value: catalogueDescription)
                    LabeledContent("TX firmware", value: lights.info?.firmware ?? "Unknown")
                    LabeledContent("App", value: appVersion)
                } header: {
                    Text("About")
                } footer: {
                    if let warning = lights.compatibility.warning {
                        Label(warning, systemImage: "exclamationmark.triangle.fill")
                            .foregroundStyle(Theme.reconnecting)
                    }
                }
            }
            .navigationTitle("Settings")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("Done") { dismiss() }
                }
            }
        }
    }

    private var linkDescription: String {
        switch lights.status {
        case .connected(.bluetooth):
            return model.isPaired ? "Paired · Bluetooth" : "Connected · Bluetooth"
        case .connected(let kind):
            return "Connected · \(kind.rawValue)"
        case .reconnecting:
            return "Reconnecting…"
        case .searching:
            return "Searching…"
        case .unavailable(let reason):
            return reason
        }
    }

    private var wifiDescription: String {
        guard model.settings.wifiFallbackEnabled else { return "Off" }
        return model.settings.wifiSSID.isEmpty ? "On" : model.settings.wifiSSID
    }

    private var catalogueDescription: String {
        let version = "v\(lights.catalogue.version)"
        switch lights.compatibility {
        case .matches: return "\(version) · matches TX"
        case .unknown: return version
        case .transmitterNewer: return "\(version) · TX has v\(lights.info?.catalogue ?? 0)"
        case .transmitterOlder: return "\(version) · TX has v\(lights.info?.catalogue ?? 0)"
        case .unsupportedProtocol(let proto): return "\(version) · TX protocol \(proto)"
        }
    }

    private var appVersion: String {
        let info = Bundle.main.infoDictionary
        let version = info?["CFBundleShortVersionString"] as? String ?? "?"
        let build = info?["CFBundleVersion"] as? String ?? "?"
        return "\(version) (\(build))"
    }
}

/// The TX hotspot, used only when Bluetooth can't reach the transmitter
struct WiFiFallbackView: View {
    @Environment(AppModel.self) private var model

    var body: some View {
        @Bindable var settings = model.settings
        Form {
            Section {
                Toggle("Use WiFi when Bluetooth can't connect", isOn: $settings.wifiFallbackEnabled)
            } footer: {
                Text("After a few seconds without Bluetooth, the app joins the transmitter's hotspot and uses its WebSocket. While on the hotspot your phone has no internet, and control stops soon after the screen locks. The app leaves the hotspot once Bluetooth is back.")
            }

            if settings.wifiFallbackEnabled {
                Section {
                    TextField("Network name (SSID)", text: $settings.wifiSSID)
                        .textInputAutocapitalization(.never)
                        .autocorrectionDisabled()
                    SecureField("Password", text: $settings.wifiPassword)
                } footer: {
                    Text("WIFI_SSID and WIFI_PASS from the transmitter's secrets.h. Leave the name empty if you join the hotspot yourself.")
                }
            }
        }
        .navigationTitle("WiFi fallback")
        .onDisappear { model.wifiFallbackChanged() }
    }
}

#Preview {
    SettingsView()
        .environment(AppModel.preview())
        .preferredColorScheme(.dark)
}
