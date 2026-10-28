import Foundation

/// A pretend TX for SwiftUI previews, the Simulator (which has no Bluetooth) and tests.
/// It follows the firmware's `applyMode()` rules: Auto keeps running through a Strobe and
/// stops for anything else, and every change is echoed as a state message.
@MainActor
public final class MockTransport: Transport {
    public let kind: TransportKind
    public let events: AsyncStream<TransportEvent>
    private let continuation: AsyncStream<TransportEvent>.Continuation

    public let catalogue: Catalogue
    /// How long the pretend TX takes to accept a command
    public var latency: Duration
    /// Set false to simulate a TX that never answers
    public var acknowledges = true
    /// How often Auto picks a new mode (the real TX uses 30 s)
    public var autoInterval: Duration
    public var info: TXInfo

    /// Every command the pretend TX accepted, oldest first
    public private(set) var received: [Command] = []
    public private(set) var state: TXState
    public private(set) var link: LinkState = .idle

    private var autoTask: Task<Void, Never>?
    private var connectTask: Task<Void, Never>?
    private let connectDelay: Duration

    public init(kind: TransportKind = .demo,
                catalogue: Catalogue,
                initialMode: Int = 13,
                latency: Duration = .milliseconds(20),
                connectDelay: Duration = .milliseconds(300),
                autoInterval: Duration = .seconds(5),
                info: TXInfo? = nil) {
        self.kind = kind
        self.catalogue = catalogue
        self.latency = latency
        self.connectDelay = connectDelay
        self.autoInterval = autoInterval
        self.state = TXState(mode: initialMode, auto: false)
        self.info = info ?? TXInfo(proto: supportedProtocolVersion, firmware: "demo", catalogue: catalogue.version)
        (events, continuation) = AsyncStream.makeStream(of: TransportEvent.self)
    }

    public func start() {
        guard link == .idle else { return }
        setLink(.connecting)
        connectTask = Task { [weak self, connectDelay] in
            try? await Task.sleep(for: connectDelay)
            guard !Task.isCancelled else { return }
            self?.simulateReconnect()
        }
    }

    public func stop() {
        connectTask?.cancel()
        autoTask?.cancel()
        setLink(.idle)
    }

    public func send(_ command: Command) async throws {
        guard link == .connected else { throw TransportError.notConnected }
        try await Task.sleep(for: latency)
        guard link == .connected else { throw TransportError.notConnected }
        guard acknowledges else {
            try await Task.sleep(for: .seconds(3600))
            throw TransportError.timedOut
        }
        received.append(command)
        apply(command.mode)
    }

    /// The link drops, as when the TX goes out of range
    public func simulateDrop() {
        connectTask?.cancel()
        setLink(.connecting)
    }

    /// The link comes back; the TX sends its state on connect
    public func simulateReconnect() {
        setLink(.connected)
        continuation.yield(.transmitter(name: "Drum Lights TX (demo)"))
        continuation.yield(.info(info))
        continuation.yield(.state(state))
    }

    /// Another controller (the web UI, another phone) changes the mode
    public func simulateRemote(mode: Int) {
        apply(mode)
    }

    private func setLink(_ new: LinkState) {
        link = new
        continuation.yield(.link(new))
    }

    private func apply(_ mode: Int) {
        if mode == catalogue.actions.auto {
            state = TXState(mode: catalogue.auto.randomElement() ?? 0, auto: true)
            startAuto()
        } else if mode == catalogue.actions.strobe {
            // one-shot: the receivers revert by themselves, mode and Auto are unchanged
        } else {
            autoTask?.cancel()
            state = TXState(mode: mode, auto: false)
        }
        continuation.yield(.state(state))
    }

    private func startAuto() {
        autoTask?.cancel()
        autoTask = Task { [weak self] in
            while !Task.isCancelled {
                guard let interval = self?.autoInterval else { return }
                try? await Task.sleep(for: interval)
                guard !Task.isCancelled, let self else { return }
                self.state = TXState(mode: self.catalogue.auto.randomElement() ?? 0, auto: true)
                self.continuation.yield(.state(self.state))
            }
        }
    }
}
