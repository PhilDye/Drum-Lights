#if os(iOS)
import Foundation
import NetworkExtension

/// Joins the TX's WiFi hotspot for the WebSocket fallback, so nobody has to visit the Settings app.
/// Needs the Hotspot Configuration entitlement.
public enum HotspotJoiner {
    public static func join(ssid: String, passphrase: String) async throws {
        let configuration = passphrase.isEmpty
            ? NEHotspotConfiguration(ssid: ssid)
            : NEHotspotConfiguration(ssid: ssid, passphrase: passphrase, isWEP: false)
        configuration.joinOnce = false
        do {
            try await NEHotspotConfigurationManager.shared.apply(configuration)
        } catch let error as NSError
            where error.domain == NEHotspotConfigurationErrorDomain
            && error.code == NEHotspotConfigurationError.alreadyAssociated.rawValue {
            // already on it
        }
    }

    /// Gives the phone its normal WiFi back once Bluetooth has taken over
    public static func leave(ssid: String) {
        NEHotspotConfigurationManager.shared.removeConfiguration(forSSID: ssid)
    }
}
#endif
