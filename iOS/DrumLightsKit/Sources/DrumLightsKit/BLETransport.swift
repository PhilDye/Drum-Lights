#if canImport(CoreBluetooth)
import CoreBluetooth
import Foundation

/// The TX's GATT service (`DESIGN.md` section 3.2).
///
/// - Remembers the TX it first connects to, and from then on asks iOS to connect to that one.
///   A pending `connect` never times out, so iOS reconnects as soon as the TX is back in range,
///   even with the app in the background (`bluetooth-central` background mode).
/// - Supports CoreBluetooth state restoration, so iOS can relaunch the app for BLE events.
/// - Pairing is iOS's job: the TX asks for it on connect, iOS shows the PIN prompt, then
///   keeps the bond. Apps can't remove a bond; "Forget" here only forgets which TX to use.
@MainActor
public final class BLETransport: NSObject, Transport {
    public let kind = TransportKind.bluetooth
    public let events: AsyncStream<TransportEvent>
    private let continuation: AsyncStream<TransportEvent>.Continuation

    private let restoreIdentifier: String?
    private let defaults: UserDefaults
    private static let rememberedKey = "BLETransport.transmitter"
    private static let pairedKey = "BLETransport.paired"

    private var central: CBCentralManager?
    private var peripheral: CBPeripheral?
    private var stateCharacteristic: CBCharacteristic?
    private var commandCharacteristic: CBCharacteristic?
    private var running = false
    private var connected = false
    private var pendingWrites: [CheckedContinuation<Void, Error>] = []

    private let serviceUUID = CBUUID(string: BLEProtocol.serviceUUID)
    private let stateUUID = CBUUID(string: BLEProtocol.stateUUID)
    private let commandUUID = CBUUID(string: BLEProtocol.commandUUID)
    private let infoUUID = CBUUID(string: BLEProtocol.infoUUID)

    /// - Parameter restoreIdentifier: enables CoreBluetooth state restoration (iOS only);
    ///   must stay the same across launches
    public init(restoreIdentifier: String? = "DrumLightsCentral", defaults: UserDefaults = .standard) {
        self.restoreIdentifier = restoreIdentifier
        self.defaults = defaults
        (events, continuation) = AsyncStream.makeStream(of: TransportEvent.self)
        super.init()
    }

    /// The TX this phone uses, once one has been found
    public var rememberedTransmitter: UUID? {
        defaults.string(forKey: Self.rememberedKey).flatMap(UUID.init(uuidString:))
    }

    /// True once a command has been accepted over an encrypted link, so iOS holds a bond
    public var isPaired: Bool {
        defaults.bool(forKey: Self.pairedKey)
    }

    public func start() {
        guard !running else { return }
        running = true
        if let central {
            connectOrScan(central)
        } else {
            // creating the manager is what shows the Bluetooth permission prompt
            var options: [String: Any] = [CBCentralManagerOptionShowPowerAlertKey: true]
            #if os(iOS)
            if let restoreIdentifier {
                options[CBCentralManagerOptionRestoreIdentifierKey] = restoreIdentifier
            }
            #endif
            central = CBCentralManager(delegate: self, queue: .main, options: options)
        }
        continuation.yield(.link(.connecting))
    }

    public func stop() {
        running = false
        central?.stopScan()
        if let peripheral { central?.cancelPeripheralConnection(peripheral) }
        setDisconnected()
        continuation.yield(.link(.idle))
    }

    /// Drop the remembered TX and look for any TX again
    public func forgetTransmitter() {
        defaults.removeObject(forKey: Self.rememberedKey)
        defaults.removeObject(forKey: Self.pairedKey)
        if let peripheral { central?.cancelPeripheralConnection(peripheral) }
        peripheral = nil
        setDisconnected()
        if running, let central {
            continuation.yield(.link(.connecting))
            connectOrScan(central)
        }
    }

    public func send(_ command: Command) async throws {
        guard connected, let peripheral, let commandCharacteristic else {
            throw TransportError.notConnected
        }
        try await withCheckedThrowingContinuation { (waiter: CheckedContinuation<Void, Error>) in
            // writes with response complete in order, so a FIFO matches them up
            pendingWrites.append(waiter)
            peripheral.writeValue(BLEProtocol.encode(command), for: commandCharacteristic, type: .withResponse)
        }
    }

    // MARK: Connection

    private func connectOrScan(_ central: CBCentralManager) {
        guard running, central.state == .poweredOn else { return }
        if let peripheral {
            central.connect(peripheral)
            return
        }
        if let id = rememberedTransmitter,
           let known = central.retrievePeripherals(withIdentifiers: [id]).first {
            use(known, central: central)
            return
        }
        // connected to iOS already, e.g. by another app or the watch
        if let linked = central.retrieveConnectedPeripherals(withServices: [serviceUUID]).first,
           rememberedTransmitter == nil || linked.identifier == rememberedTransmitter {
            use(linked, central: central)
            return
        }
        central.scanForPeripherals(withServices: [serviceUUID])
    }

    private func use(_ found: CBPeripheral, central: CBCentralManager) {
        central.stopScan()
        peripheral = found
        found.delegate = self
        defaults.set(found.identifier.uuidString, forKey: Self.rememberedKey)
        if let name = found.name { continuation.yield(.transmitter(name: name)) }
        central.connect(found)
    }

    private func setDisconnected() {
        let wasConnected = connected
        connected = false
        stateCharacteristic = nil
        commandCharacteristic = nil
        let failed = pendingWrites
        pendingWrites.removeAll()
        failed.forEach { $0.resume(throwing: TransportError.notConnected) }
        if wasConnected && running { continuation.yield(.link(.connecting)) }
    }

    private func becameReady() {
        guard !connected, commandCharacteristic != nil else { return }
        connected = true
        continuation.yield(.link(.connected))
    }

    fileprivate func centralStateChanged(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            continuation.yield(.link(.connecting))
            connectOrScan(central)
        case .poweredOff:
            setDisconnected()
            continuation.yield(.link(.unavailable("Bluetooth is off")))
        case .unauthorized:
            continuation.yield(.link(.unavailable("Bluetooth permission is off")))
        case .unsupported:
            continuation.yield(.link(.unavailable("No Bluetooth LE on this device")))
        default:
            break
        }
    }

    fileprivate func discovered(_ found: CBPeripheral, central: CBCentralManager) {
        guard peripheral == nil else { return }
        if let id = rememberedTransmitter, found.identifier != id { return }
        use(found, central: central)
    }

    fileprivate func didConnect(_ connectedPeripheral: CBPeripheral) {
        guard connectedPeripheral === peripheral else { return }
        connectedPeripheral.discoverServices([serviceUUID])
    }

    fileprivate func didDisconnect(_ lost: CBPeripheral, central: CBCentralManager) {
        guard lost === peripheral else { return }
        setDisconnected()
        // a pending connect never times out: iOS completes it when the TX is back
        if running { central.connect(lost) }
    }

    fileprivate func restore(_ restored: [CBPeripheral], central: CBCentralManager) {
        guard let found = restored.first else { return }
        peripheral = found
        found.delegate = self
        if found.state == .connected {
            found.discoverServices([serviceUUID])
        }
    }

    // MARK: GATT

    fileprivate func discoveredServices(_ p: CBPeripheral) {
        guard let service = p.services?.first(where: { $0.uuid == serviceUUID }) else {
            // not a Drum Lights TX after all
            if let central { central.cancelPeripheralConnection(p) }
            return
        }
        p.discoverCharacteristics([stateUUID, commandUUID, infoUUID], for: service)
    }

    fileprivate func discoveredCharacteristics(_ p: CBPeripheral, service: CBService) {
        for characteristic in service.characteristics ?? [] {
            switch characteristic.uuid {
            case stateUUID:
                stateCharacteristic = characteristic
                p.setNotifyValue(true, for: characteristic)
                p.readValue(for: characteristic)
            case commandUUID:
                commandCharacteristic = characteristic
            case infoUUID:
                p.readValue(for: characteristic)
            default:
                break
            }
        }
        becameReady()
    }

    fileprivate func updatedValue(_ characteristic: CBCharacteristic) {
        guard let value = characteristic.value else { return }
        switch characteristic.uuid {
        case stateUUID:
            if let state = try? BLEProtocol.decodeState(value) { continuation.yield(.state(state)) }
        case infoUUID:
            if let info = try? BLEProtocol.decodeInfo(value) { continuation.yield(.info(info)) }
        default:
            break
        }
    }

    fileprivate func wrote(_ characteristic: CBCharacteristic, error: Error?) {
        guard characteristic.uuid == commandUUID, !pendingWrites.isEmpty else { return }
        let waiter = pendingWrites.removeFirst()
        if let error {
            waiter.resume(throwing: TransportError.rejected(error.localizedDescription))
        } else {
            defaults.set(true, forKey: Self.pairedKey)
            waiter.resume()
        }
    }
}

// The manager is created with the main queue, so every callback arrives on the main thread
extension BLETransport: CBCentralManagerDelegate {
    nonisolated public func centralManagerDidUpdateState(_ central: CBCentralManager) {
        MainActor.assumeIsolated { centralStateChanged(central) }
    }

    nonisolated public func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                                           advertisementData: [String: Any], rssi RSSI: NSNumber) {
        MainActor.assumeIsolated { discovered(peripheral, central: central) }
    }

    nonisolated public func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        MainActor.assumeIsolated { didConnect(peripheral) }
    }

    nonisolated public func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral,
                                           error: Error?) {
        MainActor.assumeIsolated { didDisconnect(peripheral, central: central) }
    }

    nonisolated public func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral,
                                           error: Error?) {
        MainActor.assumeIsolated { didDisconnect(peripheral, central: central) }
    }

    #if os(iOS)
    nonisolated public func centralManager(_ central: CBCentralManager, willRestoreState dict: [String: Any]) {
        let restored = dict[CBCentralManagerRestoredStatePeripheralsKey] as? [CBPeripheral] ?? []
        MainActor.assumeIsolated { restore(restored, central: central) }
    }
    #endif
}

extension BLETransport: CBPeripheralDelegate {
    nonisolated public func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        MainActor.assumeIsolated { discoveredServices(peripheral) }
    }

    nonisolated public func peripheral(_ peripheral: CBPeripheral,
                                       didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        MainActor.assumeIsolated { discoveredCharacteristics(peripheral, service: service) }
    }

    nonisolated public func peripheral(_ peripheral: CBPeripheral,
                                       didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        MainActor.assumeIsolated { updatedValue(characteristic) }
    }

    nonisolated public func peripheral(_ peripheral: CBPeripheral,
                                       didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        MainActor.assumeIsolated { wrote(characteristic, error: error) }
    }

    nonisolated public func peripheralDidUpdateName(_ peripheral: CBPeripheral) {
        let name = peripheral.name
        MainActor.assumeIsolated {
            if let name { continuation.yield(.transmitter(name: name)) }
        }
    }
}
#endif
