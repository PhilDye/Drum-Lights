import Foundation
import Observation

/// Touch feedback the controller asks for; the app maps these to haptics
public enum FeedbackEvent: Sendable, Equatable {
    /// The TX accepted a command in time
    case acknowledged
    /// No acceptance within `Timing.ackWarning`
    case notAcknowledged
    /// Strobe pressed (played straight away, not on the ack)
    case strobe
    /// A press-and-hold mode such as 999 completed
    case holdCompleted
    /// The link came back after a drop
    case reconnected
}

@MainActor
public protocol FeedbackPlayer: AnyObject {
    func play(_ event: FeedbackEvent)
}

/// What the status pill shows
public enum ConnectionStatus: Sendable, Equatable {
    /// Never connected yet in this session
    case searching
    case connected(TransportKind)
    /// Was connected, now trying again
    case reconnecting
    /// Nothing can connect until the user acts, e.g. Bluetooth is off and there is no WiFi fallback
    case unavailable(String)

    public var isConnected: Bool {
        if case .connected = self { return true }
        return false
    }
}

/// Owns the connection to the TX and the state every screen shows.
///
/// - Bluetooth is the primary transport. The optional fallback (the WebSocket) only starts
///   after the primary has been down for `Timing.fallbackDelay`, and stops as soon as the
///   primary connects again.
/// - Commands are never queued. While disconnected, sends are dropped.
/// - The UI shows what the TX reports, not what was tapped, so it stays in step with
///   other controllers.
@MainActor
@Observable
public final class LightsController {
    public struct Timing: Sendable {
        /// Warn if the TX hasn't accepted a command by then
        public var ackWarning: Duration = .milliseconds(300)
        /// Give up waiting for acceptance altogether
        public var ackGiveUp: Duration = .seconds(2)
        /// How long "Sent to TX" stays up
        public var sentDisplay: Duration = .milliseconds(1200)
        /// How long the primary may be down before the fallback starts
        public var fallbackDelay: Duration = .seconds(6)

        public init() {}
    }

    public let catalogue: Catalogue

    public private(set) var status: ConnectionStatus = .searching
    /// Last state the TX reported; nil until it has said anything
    public private(set) var state: TXState?
    public private(set) var info: TXInfo?
    public private(set) var transmitterName: String?
    /// True for `Timing.sentDisplay` after the TX accepts a command
    public private(set) var showSent = false
    /// Goes up by one for every strobe sent, so screens can flash in time with it
    public private(set) var strobeCount = 0
    /// How often Strobe repeats while held
    public var strobeInterval: Duration = .milliseconds(300)

    public var compatibility: Compatibility { .check(info, against: catalogue) }
    public var isConnected: Bool { status.isConnected }
    public var activeTransportKind: TransportKind? { active?.kind }

    @ObservationIgnored public weak var feedback: FeedbackPlayer?
    @ObservationIgnored public var timing: Timing

    @ObservationIgnored private let primary: Transport
    @ObservationIgnored private var fallback: Transport?
    @ObservationIgnored private var links: [ObjectIdentifier: LinkState] = [:]
    @ObservationIgnored private var listeners: [ObjectIdentifier: Task<Void, Never>] = [:]
    @ObservationIgnored private var fallbackRunning = false
    @ObservationIgnored private var fallbackTimer: Task<Void, Never>?
    @ObservationIgnored private var everConnected = false
    @ObservationIgnored private var started = false
    @ObservationIgnored private var sentTimer: Task<Void, Never>?
    @ObservationIgnored private var strobeTask: Task<Void, Never>?
    @ObservationIgnored private var strobeInFlight = false

    public init(catalogue: Catalogue,
                primary: Transport,
                fallback: Transport? = nil,
                timing: Timing = Timing()) {
        self.catalogue = catalogue
        self.primary = primary
        self.fallback = fallback
        self.timing = timing
    }

    // MARK: Connection

    public func start() {
        guard !started else { return }
        started = true
        listen(to: primary)
        primary.start()
        armFallback()
    }

    public func stop() {
        started = false
        fallbackTimer?.cancel()
        endStrobe()
        primary.stop()
        stopFallback()
        listeners.values.forEach { $0.cancel() }
        listeners.removeAll()
        links.removeAll()
    }

    /// Swap the WiFi fallback, e.g. after the SSID changes in Settings; nil turns it off
    public func setFallback(_ transport: Transport?) {
        stopFallback()
        if let old = fallback {
            listeners.removeValue(forKey: ObjectIdentifier(old))?.cancel()
            links[ObjectIdentifier(old)] = nil
        }
        fallback = transport
        updateStatus()
        if started { armFallback() }
    }

    private var active: Transport? {
        [primary, fallback].compactMap { $0 }.first { links[ObjectIdentifier($0)] == .connected }
    }

    private func listen(to transport: Transport) {
        let id = ObjectIdentifier(transport)
        guard listeners[id] == nil else { return }
        let stream = transport.events
        listeners[id] = Task { [weak self] in
            for await event in stream {
                self?.handle(event, from: transport)
            }
        }
    }

    private func handle(_ event: TransportEvent, from transport: Transport) {
        // a transport that was swapped out may still have events in flight
        guard transport === primary || transport === fallback else { return }
        switch event {
        case .link(let link):
            links[ObjectIdentifier(transport)] = link
            if transport === primary {
                if link == .connected {
                    fallbackTimer?.cancel()
                    stopFallback()
                } else {
                    armFallback()
                }
            }
            updateStatus()
        case .state(let newState):
            // only the transport in use speaks for the TX
            guard transport === active else { return }
            state = newState
        case .info(let newInfo):
            guard transport === active else { return }
            info = newInfo
        case .transmitter(let name):
            transmitterName = name
        }
    }

    private func armFallback() {
        guard started, fallback != nil, !fallbackRunning, fallbackTimer == nil,
              links[ObjectIdentifier(primary)] != .connected else { return }
        fallbackTimer = Task { [weak self, timing] in
            try? await Task.sleep(for: timing.fallbackDelay)
            guard !Task.isCancelled, let self else { return }
            self.fallbackTimer = nil
            self.startFallback()
        }
    }

    private func startFallback() {
        guard let fallback, !fallbackRunning,
              links[ObjectIdentifier(primary)] != .connected else { return }
        fallbackRunning = true
        listen(to: fallback)
        fallback.start()
    }

    private func stopFallback() {
        fallbackTimer?.cancel()
        fallbackTimer = nil
        guard fallbackRunning, let fallback else { return }
        fallbackRunning = false
        fallback.stop()
        links[ObjectIdentifier(fallback)] = .idle
    }

    private func updateStatus() {
        let wasConnected = status.isConnected
        if let active {
            status = .connected(active.kind)
            if !wasConnected && everConnected { feedback?.play(.reconnected) }
            everConnected = true
        } else if case .unavailable(let reason) = links[ObjectIdentifier(primary)], fallback == nil {
            status = .unavailable(reason)
        } else {
            status = everConnected ? .reconnecting : .searching
        }
        if !status.isConnected { endStrobe() }
    }

    // MARK: Commands

    /// Select a mode (or an action such as Off). Ignored while disconnected.
    public func select(_ mode: Int) {
        send(Command(mode: mode), feedbackOnAck: true)
    }

    public func off() { select(catalogue.actions.off) }
    public func startAuto() { select(catalogue.actions.auto) }

    /// For press-and-hold modes such as 999, once the hold completes
    public func selectAfterHold(_ mode: Int) {
        guard isConnected else { return }
        feedback?.play(.holdCompleted)
        send(Command(mode: mode), feedbackOnAck: false)
    }

    /// One strobe, with a heavy haptic straight away
    public func strobe() {
        guard isConnected, !strobeInFlight else { return }
        feedback?.play(.strobe)
        strobeCount += 1
        strobeInFlight = true
        send(Command(mode: catalogue.actions.strobe), feedbackOnAck: false) { [weak self] in
            self?.strobeInFlight = false
        }
    }

    /// Strobe now, then again every `strobeInterval` until `endStrobe()`.
    /// A repeat is skipped while the previous strobe is still waiting for the TX.
    public func beginStrobe() {
        guard isConnected else { return }
        strobeTask?.cancel()
        strobe()
        strobeTask = Task { [weak self] in
            while !Task.isCancelled {
                guard let interval = self?.strobeInterval else { return }
                try? await Task.sleep(for: interval)
                guard !Task.isCancelled else { return }
                self?.strobe()
            }
        }
    }

    public func endStrobe() {
        strobeTask?.cancel()
        strobeTask = nil
    }

    /// Returns false when the command was dropped because nothing is connected
    @discardableResult
    private func send(_ command: Command, feedbackOnAck: Bool, completion: (() -> Void)? = nil) -> Bool {
        guard let transport = active else {
            completion?()
            return false
        }
        let timing = self.timing
        let sendTask = Task { try await transport.send(command) }
        let giveUp = Task {
            try? await Task.sleep(for: timing.ackGiveUp)
            sendTask.cancel()
        }
        Task { [weak self] in
            let warning = Task { [weak self] in
                try? await Task.sleep(for: timing.ackWarning)
                guard !Task.isCancelled else { return false }
                self?.feedback?.play(.notAcknowledged)
                return true
            }
            let result = await sendTask.result
            giveUp.cancel()
            warning.cancel()
            let warned = await warning.value
            if case .success = result {
                if feedbackOnAck && !warned { self?.feedback?.play(.acknowledged) }
                self?.flashSent()
            }
            completion?()
        }
        return true
    }

    private func flashSent() {
        showSent = true
        sentTimer?.cancel()
        sentTimer = Task { [weak self, timing] in
            try? await Task.sleep(for: timing.sentDisplay)
            guard !Task.isCancelled else { return }
            self?.showSent = false
        }
    }
}
