import Foundation

/// What the TX reports after every change, from any controller
public struct TXState: Sendable, Equatable {
    public var mode: Int
    /// Auto is running and picked `mode`
    public var auto: Bool

    public init(mode: Int, auto: Bool) {
        self.mode = mode
        self.auto = auto
    }
}

/// Which firmware the TX runs. Every field is optional: firmware from before
/// protocol v1 sends only `{"mode": n}`.
public struct TXInfo: Sendable, Equatable {
    public var proto: Int?
    public var firmware: String?
    public var catalogue: Int?

    public init(proto: Int?, firmware: String?, catalogue: Int?) {
        self.proto = proto
        self.firmware = firmware
        self.catalogue = catalogue
    }
}

/// A request to the TX.
///
/// `parameters` is the optional block from `DESIGN.md` section 3.6: bytes appended after the mode
/// that today's TX and receivers ignore, so tempo, brightness or drum groups can be added later
/// without changing what a command is.
public struct Command: Sendable, Equatable {
    public var mode: Int
    public var parameters: [UInt8]

    public init(mode: Int, parameters: [UInt8] = []) {
        self.mode = mode
        self.parameters = parameters
    }
}

public enum ProtocolError: Error, Equatable {
    case tooShort(Int)
    case invalidMessage
}

/// The protocol version this app speaks (`PROTOCOL_VERSION` in `TX/src/main.cpp`)
public let supportedProtocolVersion = 1

/// The GATT service from `DESIGN.md` section 3.2
public enum BLEProtocol {
    public static let serviceUUID = "9A9476F2-AE0D-406A-847A-D0FECAF34CE5"
    /// read, notify: int32 mode (LE) + uint8 flags
    public static let stateUUID = "E1EBAF93-75C1-4681-8F5B-D07D93D86FDE"
    /// encrypted write with response: int32 mode (LE), -1 = Auto, then optional parameters
    public static let commandUUID = "53F118A7-863E-429A-BF92-AC19864C8219"
    /// read: small UTF-8 JSON
    public static let infoUUID = "B1AB44F5-A9A1-4152-852B-50A7D7EC8427"

    static let autoFlag: UInt8 = 0x01

    public static func encode(_ command: Command) -> Data {
        var data = littleEndian(Int32(clamping: command.mode))
        data.append(contentsOf: command.parameters)
        return data
    }

    /// Bytes past the flags byte are ignored, so the TX can add fields later
    public static func decodeState(_ data: Data) throws -> TXState {
        let bytes = [UInt8](data)
        guard bytes.count >= 5 else { throw ProtocolError.tooShort(bytes.count) }
        let raw = UInt32(bytes[0]) | UInt32(bytes[1]) << 8 | UInt32(bytes[2]) << 16 | UInt32(bytes[3]) << 24
        return TXState(mode: Int(Int32(bitPattern: raw)), auto: bytes[4] & autoFlag != 0)
    }

    public static func encodeState(_ state: TXState) -> Data {
        var data = littleEndian(Int32(clamping: state.mode))
        data.append(state.auto ? autoFlag : 0)
        return data
    }

    public static func decodeInfo(_ data: Data) throws -> TXInfo {
        do {
            let raw = try JSONDecoder().decode(RawInfo.self, from: data)
            return TXInfo(proto: raw.proto, firmware: raw.fw, catalogue: raw.catalogue)
        } catch {
            throw ProtocolError.invalidMessage
        }
    }

    private static func littleEndian(_ value: Int32) -> Data {
        withUnsafeBytes(of: value.littleEndian) { Data($0) }
    }
}

/// The JSON messages on `ws://192.168.4.1/ws` (`DESIGN.md` section 3.4)
public enum WebSocketProtocol {
    public static let defaultURL = URL(string: "ws://192.168.4.1/ws")!

    public static func encode(_ command: Command) -> String {
        "{\"mode\":\(command.mode)}"
    }

    /// A state message; `info` is nil when the firmware predates protocol v1
    public static func decode(_ text: String) throws -> (state: TXState, info: TXInfo?) {
        guard let raw = try? JSONDecoder().decode(RawWebSocketState.self, from: Data(text.utf8)) else {
            throw ProtocolError.invalidMessage
        }
        let state = TXState(mode: raw.mode, auto: raw.auto ?? false)
        let hasInfo = raw.proto != nil || raw.fw != nil || raw.catalogue != nil
        return (state, hasInfo ? TXInfo(proto: raw.proto, firmware: raw.fw, catalogue: raw.catalogue) : nil)
    }
}

private struct RawInfo: Decodable {
    let proto: Int?
    let fw: String?
    let catalogue: Int?
}

private struct RawWebSocketState: Decodable {
    let mode: Int
    let auto: Bool?
    let proto: Int?
    let fw: String?
    let catalogue: Int?
}

/// How the bundled catalogue and protocol compare with what the TX reports
public enum Compatibility: Sendable, Equatable {
    /// Not connected yet, or the firmware doesn't say
    case unknown
    case matches
    /// The TX knows effects this app doesn't; known modes still work
    case transmitterNewer
    /// The app lists effects the TX may not have
    case transmitterOlder
    /// The TX speaks a protocol this app doesn't understand
    case unsupportedProtocol(Int)

    public static func check(_ info: TXInfo?, against catalogue: Catalogue) -> Compatibility {
        guard let info else { return .unknown }
        if let proto = info.proto, proto > supportedProtocolVersion { return .unsupportedProtocol(proto) }
        guard let txCatalogue = info.catalogue else { return .unknown }
        if txCatalogue > catalogue.version { return .transmitterNewer }
        if txCatalogue < catalogue.version { return .transmitterOlder }
        return .matches
    }

    /// For Settings and the Live tab, nil when there is nothing to warn about
    public var warning: String? {
        switch self {
        case .unknown, .matches:
            return nil
        case .transmitterNewer:
            return "Transmitter has newer effects; update the app"
        case .transmitterOlder:
            return "Transmitter firmware is older than the app; some effects may do nothing"
        case .unsupportedProtocol:
            return "Transmitter firmware is too new for this app; update the app"
        }
    }
}
