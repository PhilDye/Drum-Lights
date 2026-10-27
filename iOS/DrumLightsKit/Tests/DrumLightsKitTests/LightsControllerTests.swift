import XCTest
@testable import DrumLightsKit

@MainActor
final class RecordingFeedback: FeedbackPlayer {
    var events: [FeedbackEvent] = []
    func play(_ event: FeedbackEvent) { events.append(event) }
}

@MainActor
final class LightsControllerTests: XCTestCase {
    var catalogue: Catalogue!
    var feedback: RecordingFeedback!

    override func setUp() async throws {
        catalogue = try Catalogue.bundled()
        feedback = RecordingFeedback()
    }

    private func fastTiming() -> LightsController.Timing {
        var timing = LightsController.Timing()
        timing.ackWarning = .milliseconds(100)
        timing.ackGiveUp = .milliseconds(400)
        timing.sentDisplay = .milliseconds(150)
        timing.fallbackDelay = .milliseconds(150)
        return timing
    }

    private func mock(_ kind: TransportKind = .bluetooth, connectDelay: Duration = .milliseconds(10)) -> MockTransport {
        MockTransport(kind: kind, catalogue: catalogue, latency: .milliseconds(5), connectDelay: connectDelay,
                      autoInterval: .milliseconds(50))
    }

    private func controller(primary: Transport, fallback: Transport? = nil) -> LightsController {
        let controller = LightsController(catalogue: catalogue, primary: primary, fallback: fallback, timing: fastTiming())
        controller.feedback = feedback
        return controller
    }

    /// Polls until the condition holds, or fails after the timeout
    private func eventually(_ message: String = "", timeout: TimeInterval = 2,
                            _ condition: () -> Bool, file: StaticString = #filePath, line: UInt = #line) async {
        let deadline = Date().addingTimeInterval(timeout)
        while !condition() {
            if Date() > deadline {
                XCTFail("timed out: \(message)", file: file, line: line)
                return
            }
            try? await Task.sleep(for: .milliseconds(5))
        }
    }

    private func pause(_ ms: Int) async {
        try? await Task.sleep(for: .milliseconds(ms))
    }

    func testConnectsAndShowsTheTransmitterState() async {
        let tx = mock()
        let lights = controller(primary: tx)
        XCTAssertEqual(lights.status, .searching)
        lights.start()
        await eventually { lights.status == .connected(.bluetooth) }
        await eventually { lights.state == TXState(mode: 13, auto: false) }
        XCTAssertEqual(lights.compatibility, .matches)
        XCTAssertEqual(lights.transmitterName, "Drum Lights TX (demo)")
    }

    func testSelectIsAcknowledgedWithSentAndAHaptic() async {
        let tx = mock()
        let lights = controller(primary: tx)
        lights.start()
        await eventually { lights.isConnected }
        lights.select(22)
        await eventually { lights.state?.mode == 22 }
        await eventually { lights.showSent }
        XCTAssertEqual(feedback.events, [.acknowledged])
        XCTAssertEqual(tx.received, [Command(mode: 22)])
        // "Sent to TX" goes away again
        await eventually { !lights.showSent }
    }

    func testNoAcknowledgementGivesAWarningAndNoSent() async {
        let tx = mock()
        let lights = controller(primary: tx)
        lights.start()
        await eventually { lights.isConnected }
        tx.acknowledges = false
        lights.select(22)
        await eventually { self.feedback.events == [.notAcknowledged] }
        await pause(400)
        XCTAssertFalse(lights.showSent)
        XCTAssertEqual(lights.state?.mode, 13)
    }

    func testCommandsAreNotQueuedWhileDisconnected() async {
        let tx = mock()
        let lights = controller(primary: tx)
        lights.start()
        await eventually { lights.isConnected }
        tx.simulateDrop()
        await eventually { lights.status == .reconnecting }
        lights.select(22)
        lights.strobe()
        tx.simulateReconnect()
        await eventually { lights.isConnected }
        await pause(50)
        XCTAssertEqual(tx.received, [], "nothing sent while down may arrive later")
        XCTAssertEqual(lights.state?.mode, 13)
        XCTAssertEqual(feedback.events, [.reconnected])
    }

    func testFollowsOtherControllers() async {
        let tx = mock()
        let lights = controller(primary: tx)
        lights.start()
        await eventually { lights.isConnected }
        tx.simulateRemote(mode: 84)
        await eventually { lights.state?.mode == 84 }
        XCTAssertEqual(tx.received, [])
    }

    func testAutoIsShownAndKeepsRunningThroughStrobe() async {
        let tx = mock()
        let lights = controller(primary: tx)
        lights.start()
        await eventually { lights.isConnected }
        lights.startAuto()
        await eventually { lights.state?.auto == true }
        XCTAssertTrue(catalogue.auto.contains(lights.state!.mode))

        lights.strobe()
        await eventually { tx.received.last == Command(mode: 98) }
        XCTAssertEqual(lights.state?.auto, true)

        lights.off()
        await eventually { lights.state == TXState(mode: 0, auto: false) }
    }

    func testStrobeHapticIsImmediateAndRepeatsWhileHeld() async {
        let tx = mock()
        let lights = controller(primary: tx)
        lights.strobeInterval = .milliseconds(30)
        lights.start()
        await eventually { lights.isConnected }
        lights.beginStrobe()
        XCTAssertEqual(feedback.events, [.strobe], "heavy haptic on press, before the TX answers")
        await pause(200)
        lights.endStrobe()
        let strobes = tx.received.filter { $0.mode == 98 }.count
        XCTAssertGreaterThanOrEqual(strobes, 3)
        XCTAssertEqual(lights.strobeCount, strobes)
        await pause(100)
        XCTAssertEqual(tx.received.filter { $0.mode == 98 }.count, strobes, "stops on release")
        XCTAssertFalse(feedback.events.contains(.acknowledged), "no tick on top of the strobe thud")
    }

    func testStrobeRepeatsDoNotPileUpBehindASlowTransmitter() async {
        let tx = mock()
        tx.latency = .milliseconds(200)
        let lights = controller(primary: tx)
        lights.strobeInterval = .milliseconds(20)
        lights.start()
        await eventually { lights.isConnected }
        lights.beginStrobe()
        await pause(300)
        lights.endStrobe()
        await pause(300)
        XCTAssertLessThanOrEqual(tx.received.count, 2)
    }

    func testStrobeStopsWhenTheLinkDrops() async {
        let tx = mock()
        let lights = controller(primary: tx)
        lights.strobeInterval = .milliseconds(20)
        lights.start()
        await eventually { lights.isConnected }
        lights.beginStrobe()
        tx.simulateDrop()
        await eventually { !lights.isConnected }
        tx.simulateReconnect()
        await eventually { lights.isConnected }
        let count = tx.received.count
        await pause(100)
        XCTAssertEqual(tx.received.count, count)
    }

    func testHoldToConfirm() async {
        let tx = mock()
        let lights = controller(primary: tx)
        lights.start()
        await eventually { lights.isConnected }
        lights.selectAfterHold(199)
        await eventually { lights.state?.mode == 199 }
        XCTAssertEqual(feedback.events, [.holdCompleted])
    }

    func testFallsBackToWiFiOnlyAfterADelayAndHandsBackToBluetooth() async {
        let ble = mock(.bluetooth, connectDelay: .seconds(3600))
        let wifi = mock(.wifi)
        let lights = controller(primary: ble, fallback: wifi)
        lights.start()
        await pause(50)
        XCTAssertEqual(wifi.link, .idle, "WiFi waits for the fallback delay")
        await eventually { lights.status == .connected(.wifi) }

        lights.select(5)
        await eventually { wifi.received == [Command(mode: 5)] }

        ble.simulateReconnect()
        await eventually { lights.status == .connected(.bluetooth) }
        XCTAssertEqual(wifi.link, .idle, "WiFi stops once Bluetooth is back")
        lights.select(6)
        await eventually { ble.received == [Command(mode: 6)] }
        XCTAssertEqual(wifi.received, [Command(mode: 5)])
    }

    func testIgnoresStateFromTheIdleTransport() async {
        let ble = mock(.bluetooth)
        let wifi = mock(.wifi)
        let lights = controller(primary: ble, fallback: wifi)
        lights.start()
        await eventually { lights.status == .connected(.bluetooth) }
        // a WiFi transport that isn't in use says nothing about the TX
        wifi.simulateRemote(mode: 50)
        await pause(50)
        XCTAssertEqual(lights.state?.mode, 13)
    }

    func testUnavailableWithoutFallback() async {
        let tx = UnavailableTransport()
        let lights = controller(primary: tx)
        lights.start()
        await eventually { lights.status == .unavailable("Bluetooth is off") }
        lights.select(1)
        XCTAssertEqual(feedback.events, [])
    }

    func testCatalogueMismatchWarning() async {
        let tx = MockTransport(kind: .bluetooth, catalogue: catalogue, connectDelay: .milliseconds(10),
                               info: TXInfo(proto: 1, firmware: "new", catalogue: catalogue.version + 1))
        let lights = controller(primary: tx)
        lights.start()
        await eventually { lights.info != nil }
        XCTAssertEqual(lights.compatibility, .transmitterNewer)
        XCTAssertEqual(lights.compatibility.warning, "Transmitter has newer effects; update the app")
    }
}

@MainActor
private final class UnavailableTransport: Transport {
    let kind = TransportKind.bluetooth
    let events: AsyncStream<TransportEvent>
    private let continuation: AsyncStream<TransportEvent>.Continuation

    init() { (events, continuation) = AsyncStream.makeStream(of: TransportEvent.self) }
    func start() { continuation.yield(.link(.unavailable("Bluetooth is off"))) }
    func stop() {}
    func send(_ command: Command) async throws { throw TransportError.notConnected }
}
