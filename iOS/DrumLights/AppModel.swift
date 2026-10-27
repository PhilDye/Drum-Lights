import DrumLightsKit
import Observation
import SwiftUI

/// Wires the kit's controller to the transports, settings and haptics for this device
@MainActor
@Observable
final class AppModel {
    let settings: AppSettings
    let controller: LightsController
    @ObservationIgnored private let haptics: Haptics
    @ObservationIgnored private let bluetooth: BLETransport?

    init(settings: AppSettings = AppSettings()) {
        // the bundled catalogue is part of the build; if it doesn't load, the build is broken
        let catalogue = try! Catalogue.bundled()
        self.settings = settings

        #if targetEnvironment(simulator)
        // the Simulator has no Bluetooth: drive a pretend TX instead
        let primary: Transport = MockTransport(kind: .demo, catalogue: catalogue)
        bluetooth = nil
        #else
        let ble = BLETransport()
        let primary: Transport = ble
        bluetooth = ble
        #endif

        controller = LightsController(catalogue: catalogue, primary: primary,
                                      fallback: Self.makeFallback(settings))
        haptics = Haptics(settings: settings)
        controller.feedback = haptics
        controller.strobeInterval = .milliseconds(settings.strobeIntervalMilliseconds)
    }

    /// For SwiftUI previews
    init(preview transport: MockTransport) {
        settings = AppSettings(defaults: UserDefaults(suiteName: "preview") ?? .standard)
        controller = LightsController(catalogue: transport.catalogue, primary: transport)
        haptics = Haptics(settings: settings)
        bluetooth = nil
    }

    func start() {
        controller.start()
    }

    var isPaired: Bool {
        bluetooth?.isPaired ?? false
    }

    func forgetTransmitter() {
        bluetooth?.forgetTransmitter()
    }

    func strobeIntervalChanged() {
        controller.strobeInterval = .milliseconds(settings.strobeIntervalMilliseconds)
    }

    func wifiFallbackChanged() {
        controller.setFallback(Self.makeFallback(settings))
    }

    private static func makeFallback(_ settings: AppSettings) -> Transport? {
        guard settings.wifiFallbackEnabled else { return nil }
        let ssid = settings.wifiSSID.trimmingCharacters(in: .whitespaces)
        guard !ssid.isEmpty else {
            // already on the TX hotspot, joined by hand
            return WebSocketTransport()
        }
        let password = settings.wifiPassword
        return WebSocketTransport(
            prepare: { try await HotspotJoiner.join(ssid: ssid, passphrase: password) },
            teardown: { HotspotJoiner.leave(ssid: ssid) }
        )
    }
}

extension AppModel {
    static func preview(connected: Bool = true, mode: Int = 13) -> AppModel {
        let catalogue = try! Catalogue.bundled()
        let transport = MockTransport(catalogue: catalogue, initialMode: mode,
                                      connectDelay: connected ? .zero : .seconds(3600))
        let model = AppModel(preview: transport)
        model.start()
        return model
    }
}
