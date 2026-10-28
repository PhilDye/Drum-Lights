import XCTest
@testable import DrumLightsKit

final class ProtocolTests: XCTestCase {
    func testBLECommandIsLittleEndianInt32() {
        XCTAssertEqual([UInt8](BLEProtocol.encode(Command(mode: 13))), [13, 0, 0, 0])
        XCTAssertEqual([UInt8](BLEProtocol.encode(Command(mode: 199))), [199, 0, 0, 0])
        XCTAssertEqual([UInt8](BLEProtocol.encode(Command(mode: -1))), [0xFF, 0xFF, 0xFF, 0xFF])
    }

    func testBLECommandCarriesParameters() {
        XCTAssertEqual([UInt8](BLEProtocol.encode(Command(mode: 1, parameters: [7, 8]))), [1, 0, 0, 0, 7, 8])
    }

    func testBLEStateRoundTrip() throws {
        for state in [TXState(mode: 13, auto: false), TXState(mode: 84, auto: true), TXState(mode: 0, auto: false)] {
            XCTAssertEqual(try BLEProtocol.decodeState(BLEProtocol.encodeState(state)), state)
        }
        XCTAssertEqual(try BLEProtocol.decodeState(Data([0x0D, 0, 0, 0, 0x01])), TXState(mode: 13, auto: true))
    }

    func testBLEStateIgnoresExtraBytesAndOtherFlags() throws {
        XCTAssertEqual(try BLEProtocol.decodeState(Data([5, 0, 0, 0, 0b110, 9, 9])), TXState(mode: 5, auto: false))
    }

    func testBLEStateTooShort() {
        XCTAssertThrowsError(try BLEProtocol.decodeState(Data([1, 0, 0, 0]))) {
            XCTAssertEqual($0 as? ProtocolError, .tooShort(4))
        }
    }

    func testBLEInfo() throws {
        let info = try BLEProtocol.decodeInfo(Data(#"{"proto":1,"fw":"1a7a0a1","catalogue":1}"#.utf8))
        XCTAssertEqual(info, TXInfo(proto: 1, firmware: "1a7a0a1", catalogue: 1))
        XCTAssertThrowsError(try BLEProtocol.decodeInfo(Data("not json".utf8)))
    }

    func testWebSocketCommand() {
        XCTAssertEqual(WebSocketProtocol.encode(Command(mode: 13)), #"{"mode":13}"#)
        XCTAssertEqual(WebSocketProtocol.encode(Command(mode: -1)), #"{"mode":-1}"#)
    }

    func testWebSocketStateV1() throws {
        let decoded = try WebSocketProtocol.decode(#"{"mode":13,"auto":true,"proto":1,"fw":"v2.0-3-gabc","catalogue":1}"#)
        XCTAssertEqual(decoded.state, TXState(mode: 13, auto: true))
        XCTAssertEqual(decoded.info, TXInfo(proto: 1, firmware: "v2.0-3-gabc", catalogue: 1))
    }

    func testWebSocketStateFromOldFirmware() throws {
        let decoded = try WebSocketProtocol.decode(#"{"mode":4}"#)
        XCTAssertEqual(decoded.state, TXState(mode: 4, auto: false))
        XCTAssertNil(decoded.info)
    }

    func testWebSocketIgnoresUnknownFields() throws {
        XCTAssertEqual(try WebSocketProtocol.decode(#"{"mode":4,"brightness":9}"#).state.mode, 4)
        XCTAssertThrowsError(try WebSocketProtocol.decode(#"{"auto":true}"#))
    }

    func testCompatibility() throws {
        let catalogue = try Catalogue.bundled()
        let v = catalogue.version
        XCTAssertEqual(Compatibility.check(nil, against: catalogue), .unknown)
        XCTAssertEqual(Compatibility.check(TXInfo(proto: 1, firmware: nil, catalogue: v), against: catalogue), .matches)
        XCTAssertEqual(Compatibility.check(TXInfo(proto: 1, firmware: nil, catalogue: v + 1), against: catalogue), .transmitterNewer)
        XCTAssertEqual(Compatibility.check(TXInfo(proto: 1, firmware: nil, catalogue: v - 1), against: catalogue), .transmitterOlder)
        XCTAssertEqual(Compatibility.check(TXInfo(proto: 2, firmware: nil, catalogue: v), against: catalogue), .unsupportedProtocol(2))
        XCTAssertEqual(Compatibility.transmitterNewer.warning, "Transmitter has newer effects; update the app")
        XCTAssertNil(Compatibility.matches.warning)
    }
}
