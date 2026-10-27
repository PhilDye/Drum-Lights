import Foundation
#if canImport(FoundationNetworking)
import FoundationNetworking
#endif

/// The TX's existing WebSocket (`ws://192.168.4.1/ws`). Works with firmware that has no
/// Bluetooth, but the phone has to be on the TX hotspot and the socket is suspended soon
/// after the screen locks.
///
/// The TX echoes its state after every command, so the next state message after a send
/// is taken as the acceptance. That is approximate: the messages carry no request id, so a
/// state sent for another reason at the same moment (an Auto change, another controller,
/// the state on connect) also counts. Bluetooth write responses don't have this problem.
@MainActor
public final class WebSocketTransport: Transport {
    public let kind = TransportKind.wifi
    public let events: AsyncStream<TransportEvent>
    private let continuation: AsyncStream<TransportEvent>.Continuation

    private let url: URL
    private let session: URLSession
    private let retryDelay: Duration
    private let prepare: (@Sendable () async throws -> Void)?
    private let teardown: (@Sendable () async -> Void)?

    private var socket: URLSessionWebSocketTask?
    private var runTask: Task<Void, Never>?
    private var connected = false
    private var waiters: [CheckedContinuation<Void, Error>] = []

    /// - Parameters:
    ///   - prepare: runs once each time the transport starts, before connecting;
    ///     the app uses it to join the TX hotspot
    ///   - teardown: runs when the transport stops, e.g. to leave the hotspot
    public init(url: URL = WebSocketProtocol.defaultURL,
                session: URLSession = .shared,
                retryDelay: Duration = .seconds(2),
                prepare: (@Sendable () async throws -> Void)? = nil,
                teardown: (@Sendable () async -> Void)? = nil) {
        self.url = url
        self.session = session
        self.retryDelay = retryDelay
        self.prepare = prepare
        self.teardown = teardown
        (events, continuation) = AsyncStream.makeStream(of: TransportEvent.self)
    }

    public func start() {
        guard runTask == nil else { return }
        continuation.yield(.link(.connecting))
        runTask = Task { [weak self] in
            await self?.run()
        }
    }

    public func stop() {
        runTask?.cancel()
        runTask = nil
        socket?.cancel(with: .goingAway, reason: nil)
        socket = nil
        setDisconnected()
        continuation.yield(.link(.idle))
        if let teardown {
            Task { await teardown() }
        }
    }

    public func send(_ command: Command) async throws {
        guard connected, let socket else { throw TransportError.notConnected }
        try await withCheckedThrowingContinuation { (waiter: CheckedContinuation<Void, Error>) in
            waiters.append(waiter)
            let message = WebSocketProtocol.encode(command)
            Task { [weak self] in
                do {
                    try await socket.send(.string(message))
                } catch {
                    self?.failWaiters(error)
                }
            }
        }
    }

    private func run() async {
        if let prepare {
            do {
                try await prepare()
            } catch {
                // carry on: the phone may already be on the hotspot
            }
        }
        while !Task.isCancelled {
            let socket = session.webSocketTask(with: url)
            self.socket = socket
            socket.resume()
            await receiveLoop(socket)
            setDisconnected()
            guard !Task.isCancelled else { return }
            continuation.yield(.link(.connecting))
            try? await Task.sleep(for: retryDelay)
        }
    }

    private func receiveLoop(_ socket: URLSessionWebSocketTask) async {
        // firmware before protocol v1 says nothing on connect, so a pong also counts
        socket.sendPing { [weak self] error in
            guard error == nil else { return }
            Task { @MainActor in self?.markConnected(socket) }
        }
        while !Task.isCancelled {
            let message: URLSessionWebSocketTask.Message
            do {
                message = try await socket.receive()
            } catch {
                return
            }
            markConnected(socket)
            let text: String?
            switch message {
            case .string(let string): text = string
            case .data(let data): text = String(data: data, encoding: .utf8)
            @unknown default: text = nil
            }
            guard let text, let decoded = try? WebSocketProtocol.decode(text) else { continue }
            if let info = decoded.info { continuation.yield(.info(info)) }
            continuation.yield(.state(decoded.state))
            let accepted = waiters
            waiters.removeAll()
            accepted.forEach { $0.resume() }
        }
    }

    private func markConnected(_ socket: URLSessionWebSocketTask) {
        guard socket === self.socket, !connected else { return }
        connected = true
        continuation.yield(.link(.connected))
    }

    private func setDisconnected() {
        connected = false
        failWaiters(TransportError.notConnected)
    }

    private func failWaiters(_ error: Error) {
        let failed = waiters
        waiters.removeAll()
        failed.forEach { $0.resume(throwing: error) }
    }
}
