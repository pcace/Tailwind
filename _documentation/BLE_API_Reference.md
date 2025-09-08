# BLE Characteristics Reference

This file documents all BLE (Bluetooth Low Energy) characteristics of the Tailwind Controller for mobile app development.

## Device Information Service (180A)

Standard Bluetooth service for device information.

| Characteristic | UUID | Type | Description |
|----------------|------|------|-------------|
| Manufacturer Name | 2A29 | Read | "Tailwind Project" |
| Model Number | 2A24 | Read | "ESP32-Controller-v1.0" |
| Firmware Revision | 2A26 | Read | "1.0.0" |

## Telemetry Service (12345678-1234-1234-1234-123456789abc)

Main service for all Tailwind telemetry data.

| Characteristic | UUID | Type | Format | Description |
|----------------|------|------|--------|-------------|
| Speed | ...a001 | Read/Notify | UInt16 (2 bytes) | Speed in 0.1 km/h (e.g. 254 = 25.4 km/h) |
| Cadence | ...a002 | Read/Notify | UInt8 (1 byte) | Cadence in RPM (0-255) |
| Torque | ...a003 | Read/Notify | UInt16 (2 bytes) | Torque in 0.01 Nm (e.g. 350 = 3.50 Nm) |
| Battery | ...a004 | Read/Notify | UInt8 (1 byte) | Battery level in % (0-100) |
| Motor Current | ...a005 | Read/Notify | UInt16 (2 bytes) | Motor current in 0.01 A (e.g. 1250 = 12.50 A) |
| VESC Data | ...a006 | Read/Notify | JSON String | Extended VESC data |
| System Status | ...a007 | Read/Notify | JSON String | System status and mode |

### VESC Data JSON Format
```json
{
  "motor_rpm": 1250.5,
  "duty_cycle": 45.2,
  "temp_mosfet": 38.5,
  "temp_motor": 42.1,
  "battery_voltage": 48.2,
  "amp_hours": 2.45,
  "watt_hours": 118.5
}
```

### System Status JSON Format
```json
{
  "mode": 2,
  "mode_name": "Urban",
  "motor_enabled": true,
  "timestamp": 123456789
}
```

## Control Service (12345678-1234-1234-1234-123456789def)

Service für Steuerung und Kontrolle des E-Bikes.

| Characteristic | UUID | Type | Format | Beschreibung |
|----------------|------|------|--------|-------------|
| Mode Control | ...b001 | Write | UInt8 (1 byte) | Mode-Nummer zum Wechseln |
| Mode List | ...b002 | Read/Notify | JSON String | Verfügbare Modi |
| Command | ...b003 | Write | String | Text-Kommandos |

### Mode List JSON Format
```json
{
  "modes": [
    {
      "index": 0,
      "name": "Linear",
      "description": "linear profile",
      "hasLight": true
    },
    {
      "index": 1,
      "name": "Touring",
      "description": "Fast start-up, gentle slope to 30km/h",
      "hasLight": false
    }
  ]
}
```

### Verfügbare Kommandos
- `GET_STATUS` - Aktuelle Status-Updates anfordern
- `GET_MODES` - Mode-Liste anfordern
- `EMERGENCY_STOP` - Notfall-Stop (wechselt zu "No Assist" Mode)

## Datenformat-Details

### Integer-Formate (Little Endian)
Alle Integer-Werte werden im Little Endian Format übertragen:
- **UInt8**: Direkter Byte-Wert (0-255)
- **UInt16**: 2 Bytes, niedriges Byte zuerst
  - Beispiel: Wert 1234 → Bytes [0xD2, 0x04]
  - Dekodierung: `value = byte[0] | (byte[1] << 8)`

### Skalierungsfaktoren
- **Speed**: Faktor 10 (0.1 km/h Auflösung, max 6553.5 km/h)
- **Torque**: Faktor 100 (0.01 Nm Auflösung, max 655.35 Nm)  
- **Current**: Faktor 100 (0.01 A Auflösung, max 655.35 A)
- **Cadence**: Direkter Wert (1 RPM Auflösung, max 255 RPM)
- **Battery**: Direkter Wert (1% Auflösung, 0-100%)

### Warum Integer statt Float?
Integer-Formate sind:
- **Kompatibel** mit den meisten BLE-Apps
- **Effizienter** bei der Übertragung
- **Einfacher** zu dekodieren in mobilen Apps
- **Präzise** genug für E-Bike-Anwendungen

## Verbindungsbeispiel (Android/Kotlin)

```kotlin
// Service UUIDs
val TELEMETRY_SERVICE_UUID = UUID.fromString("12345678-1234-1234-1234-123456789abc")
val CONTROL_SERVICE_UUID = UUID.fromString("12345678-1234-1234-1234-123456789def")

// Charakteristik UUIDs  
val SPEED_CHAR_UUID = UUID.fromString("12345678-1234-1234-1234-12345678a001")
val MODE_CONTROL_CHAR_UUID = UUID.fromString("12345678-1234-1234-1234-12345678b001")

// Speed Notification abonnieren
val speedCharacteristic = gatt.getService(TELEMETRY_SERVICE_UUID)
    .getCharacteristic(SPEED_CHAR_UUID)
gatt.setCharacteristicNotification(speedCharacteristic, true)

// Speed-Wert auslesen (UInt16 Little Endian)
override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
    when (characteristic.uuid) {
        SPEED_CHAR_UUID -> {
            val speedRaw = characteristic.value
            val speedValue = ((speedRaw[1].toInt() and 0xFF) shl 8) or (speedRaw[0].toInt() and 0xFF)
            val speedKmh = speedValue / 10.0f  // Convert to km/h
            Log.d("BLE", "Speed: $speedKmh km/h")
        }
    }
}

// Mode wechseln (Beispiel: Mode 2)
val modeCharacteristic = gatt.getService(CONTROL_SERVICE_UUID)
    .getCharacteristic(MODE_CONTROL_CHAR_UUID)
modeCharacteristic.value = byteArrayOf(2)
gatt.writeCharacteristic(modeCharacteristic)
```

## Verbindungsbeispiel (iOS/Swift)

```swift
// Service UUIDs
let telemetryServiceUUID = CBUUID(string: "12345678-1234-1234-1234-123456789abc")
let controlServiceUUID = CBUUID(string: "12345678-1234-1234-1234-123456789def")

// Charakteristik UUIDs
let speedCharUUID = CBUUID(string: "12345678-1234-1234-1234-12345678a001")
let modeControlCharUUID = CBUUID(string: "12345678-1234-1234-1234-12345678b001")

// Speed Notification abonnieren
if let speedChar = peripheral.services?.first(where: { $0.uuid == telemetryServiceUUID })?
    .characteristics?.first(where: { $0.uuid == speedCharUUID }) {
    peripheral.setNotifyValue(true, for: speedChar)
}

// Speed-Wert auslesen (UInt16 Little Endian)
func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
    guard let data = characteristic.value else { return }
    
    switch characteristic.uuid {
    case speedCharUUID:
        let speedRaw = UInt16(data[0]) | (UInt16(data[1]) << 8)  // Little Endian
        let speedKmh = Float(speedRaw) / 10.0  // Convert to km/h
        print("Speed: \(speedKmh) km/h")
        
    case cadenceCharUUID:
        let cadenceRpm = UInt8(data[0])
        print("Cadence: \(cadenceRpm) RPM")
        
    case torqueCharUUID:
        let torqueRaw = UInt16(data[0]) | (UInt16(data[1]) << 8)
        let torqueNm = Float(torqueRaw) / 100.0  // Convert to Nm
        print("Torque: \(torqueNm) Nm")
    }
}

// Mode wechseln (Beispiel: Mode 2)
if let modeChar = peripheral.services?.first(where: { $0.uuid == controlServiceUUID })?
    .characteristics?.first(where: { $0.uuid == modeControlCharUUID }) {
    let modeData = Data([2])
    peripheral.writeValue(modeData, for: modeChar, type: .withResponse)
}
```

## Update-Frequenzen

- **Telemetrie-Daten**: 2 Sekunden (0.5 Hz)
- **VESC-Daten**: 2 Sekunden (0.5 Hz)
- **System Status**: Bei Änderungen oder auf Anfrage
- **Mode List**: Bei Änderungen oder auf Anfrage

## Fehlerbehandlung

- Bei Verbindungsabbruch startet der ESP32 automatisch wieder Advertising
- Ungültige Mode-Nummern werden ignoriert
- Unbekannte Kommandos werden in den Logs vermerkt
- Timeout bei Semaphore-Zugriff führt zu Fehlermeldung statt Systemabsturz

## Entwicklungshinweise

1. **Notifications abonnieren**: Für Live-Daten immer Notifications aktivieren
2. **Thread-Safety**: Alle BLE-Operationen sind thread-safe implementiert
3. **Energieeffizienz**: 2s Update-Rate optimiert für Akkulaufzeit
4. **Reconnection**: Apps sollten automatisches Reconnection implementieren
5. **JSON Parsing**: Robuste JSON-Parser für VESC/Status-Daten verwenden
