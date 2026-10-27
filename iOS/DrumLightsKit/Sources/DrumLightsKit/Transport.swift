import Foundation

public enum TransportKind: String, Sendable {
    case bluetooth = "Bluetooth"
    case wifi = "WiFi"
    case demo = "Demo"
}

public enum LinkState: Sendable, Equatable {
    /// Not started, or stopped
    case idle
    /// Looking for the TX, or waiting to reconnect to it
    case connecting
    /// Ready to send
    case connected
    /// Can't work until the user does something (Bluetooth off, permission denied)
    case unavailable(String)
}

public enum TransportEvent: Sendable, Equatable {
    case link(LinkState)
    case state(TXState)
    case info(TXInfo)
    /// The transmitter's advertised name, once found
    case transmitter(name: String)
}

public enum TransportError: Error, Equatable {
    case notConnected
    case timedOut
    /// The TX refused the write, e.g. the pairing PIN was cancelled or wrong
    case rejected(String)
}

/// A way of reaching the TX.
///
/// Implementations reconnect by themselves once started. They never queue commands:
/// a stale "Blue" sent 20 s late is worse than nothing, so `send` fails straight away
/// when the link is down.
@MainActor
public protocol Transport: AnyObject {
    var kind: TransportKind { get }

    /// Everything the transport reports. Has a single consumer, the `LightsController`.
    var events: AsyncStream<TransportEvent> { get }

    func start()
    func stop()

    /// Returns once the TX has accepted the command (BLE write response, or the
    /// WebSocket state echo). That is all it can confirm: receivers never answer.
    func send(_ command: Command) async throws
}
