import Foundation
import Observation
import Security

/// User preferences. The hotspot password goes in the Keychain, everything else in UserDefaults.
@MainActor
@Observable
final class AppSettings {
    static let strobeIntervals = [150, 200, 300, 500]

    @ObservationIgnored private let defaults: UserDefaults

    var hapticsEnabled: Bool {
        didSet { defaults.set(hapticsEnabled, forKey: Keys.haptics) }
    }

    var strobeIntervalMilliseconds: Int {
        didSet { defaults.set(strobeIntervalMilliseconds, forKey: Keys.strobeInterval) }
    }

    var wifiFallbackEnabled: Bool {
        didSet { defaults.set(wifiFallbackEnabled, forKey: Keys.wifiEnabled) }
    }

    var wifiSSID: String {
        didSet { defaults.set(wifiSSID, forKey: Keys.wifiSSID) }
    }

    var wifiPassword: String {
        didSet { Keychain.set(wifiPassword, for: Keys.wifiPassword) }
    }

    init(defaults: UserDefaults = .standard) {
        self.defaults = defaults
        defaults.register(defaults: [
            Keys.haptics: true,
            Keys.strobeInterval: 300,
            Keys.wifiEnabled: false
        ])
        hapticsEnabled = defaults.bool(forKey: Keys.haptics)
        strobeIntervalMilliseconds = defaults.integer(forKey: Keys.strobeInterval)
        wifiFallbackEnabled = defaults.bool(forKey: Keys.wifiEnabled)
        wifiSSID = defaults.string(forKey: Keys.wifiSSID) ?? ""
        wifiPassword = Keychain.get(Keys.wifiPassword) ?? ""
    }

    private enum Keys {
        static let haptics = "haptics"
        static let strobeInterval = "strobeInterval"
        static let wifiEnabled = "wifiFallback.enabled"
        static let wifiSSID = "wifiFallback.ssid"
        static let wifiPassword = "wifiFallback.password"
    }
}

/// Just enough Keychain for one password
enum Keychain {
    private static let service = "org.phildye.drumlights"

    static func get(_ account: String) -> String? {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: account,
            kSecReturnData as String: true
        ]
        var result: AnyObject?
        guard SecItemCopyMatching(query as CFDictionary, &result) == errSecSuccess,
              let data = result as? Data else { return nil }
        return String(data: data, encoding: .utf8)
    }

    static func set(_ value: String, for account: String) {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: account
        ]
        SecItemDelete(query as CFDictionary)
        guard !value.isEmpty else { return }
        var item = query
        item[kSecValueData as String] = Data(value.utf8)
        item[kSecAttrAccessible as String] = kSecAttrAccessibleAfterFirstUnlock
        SecItemAdd(item as CFDictionary, nil)
    }
}
