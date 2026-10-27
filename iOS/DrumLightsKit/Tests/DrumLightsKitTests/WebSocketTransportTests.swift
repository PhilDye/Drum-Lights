import XCTest
@testable import DrumLightsKit

/// Runs against a real TX, or anything that speaks its WebSocket protocol, when
/// `DRUMLIGHTS_WS_URL` is set, e.g. `DRUMLIGHTS_WS_URL=ws://192.168.4.1/ws swift test`.
/// Changes the lights: it selects Off.
@MainActor
final class WebSocketTransportTests: XCTestCase {
    func testAgainstTransmitter() async throws {
        guard let address = ProcessInfo.processInfo.environment["DRUMLIGHTS_WS_URL"],
              let url = URL(string: address) else {
            throw XCTSkip("set DRUMLIGHTS_WS_URL to run against a transmitter")
        }
        let transport = WebSocketTransport(url: url, retryDelay: .milliseconds(200))
        var iterator = transport.events.makeAsyncIterator()
        transport.start()
        // stopping sends .idle, which ends any wait below
        let watchdog = Task {
            try? await Task.sleep(for: .seconds(5))
            transport.stop()
        }
        defer {
            watchdog.cancel()
            transport.stop()
        }

        func next(_ match: (TransportEvent) -> Bool) async throws -> TransportEvent {
            while let event = await iterator.next() {
                if match(event) { return event }
                if event == .link(.idle) { break }
            }
            XCTFail("no answer from \(address) within 5 s")
            throw TransportError.timedOut
        }

        _ = try await next { $0 == .link(.connected) }
        try await transport.send(Command(mode: 0))
        let event = try await next {
            if case .state = $0 { return true }
            return false
        }
        XCTAssertEqual(event, .state(TXState(mode: 0, auto: false)))
    }
}
