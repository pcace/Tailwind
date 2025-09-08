# Tailwind

An e-bike controller built on ESP32 with dual-core architecture for responsive pedal assist control. This project implements speed-dependent assist profiles with real-time sensor processing and VESC motor controller integration.

## Table of Contents

- [What the ESP32 Does](#what-the-esp32-does)
  - [Core Functionality](#core-functionality)
  - [How It Works](#how-it-works)
    - [Multi-Core Architecture](#multi-core-architecture)
    - [Speed-Dependent Assist Algorithm](#speed-dependent-assist-algorithm)
    - [Sensor Integration](#sensor-integration)
    - [VESC Communication Protocol](#vesc-communication-protocol)
- [System Components](#system-components)
- [Safety Features](#safety-features)
- [Getting Started](#getting-started)
  - [Hardware Setup](#hardware-setup)
  - [Software Configuration](#software-configuration)
- [Code Structure](#code-structure)
- [Key Features](#key-features)
- [Debug Mode](#debug-mode)
  - [Debug Features](#debug-features)
  - [Debug Modes](#debug-modes)
  - [Simulation Modes](#simulation-modes)
  - [Debug Configuration](#debug-configuration)
  - [Serial Monitor Output](#serial-monitor-output)
- [WiFi Web Interface](#wifi-web-interface)
  - [Web Interface Features](#web-interface-features)
  - [WiFi Configuration](#wifi-configuration)
  - [How to Access](#how-to-access)
  - [Technical Implementation](#technical-implementation)
  - [Mobile Compatibility](#mobile-compatibility)
- [BLE (Bluetooth Low Energy) Interface](#ble-bluetooth-low-energy-interface)
  - [BLE Features](#ble-features)
  - [BLE Configuration](#ble-configuration)
  - [How to Connect](#how-to-connect)
  - [iPhone/iOS Connection](#iphoneios-connection)
  - [Android Connection](#android-connection)
  - [Mobile App Development](#mobile-app-development)
  - [Technical Implementation](#technical-implementation-1)
- [Legal Compliance](#legal-compliance)
- [Configuration](#configuration)
- [Contributing](#contributing)
- [License](#license)

<img src="/_documentation/concept.png" alt="Tailwind System Concept" width="500">

## What the ESP32 Does

The ESP32 acts as the intelligent brain of the tailwind system, processing multiple sensor inputs and controlling motor assistance in real-time. Here's how it works:

### Core Functionality
- **Pedal Assist Control**: Monitors pedal activity through PAS (Pedal Assist Sensor) and provides appropriate motor assistance
- **Speed-Dependent Assistance**: Dynamically adjusts assist levels based on current speed using interpolated curves
- **Torque Sensing**: Reads rider torque input for more natural and responsive assistance
- **VESC Integration**: Communicates with VESC motor controller via UART for motor control and telemetry
- **Safety Monitoring**: Continuously monitors battery voltage, temperature, and system health

### How It Works

#### Multi-Core Architektur
The ESP32's dual-core processor is utilized for optimal performance:

- **Core 0 (Sensor Core)**: Handles time-critical sensor processing
  - PAS sensor interrupts and debouncing
  - Torque sensor ADC readings
  - Assist level calculations
  - Mode switching and button inputs

- **Core 1 (Communication Core)**: Manages external communication
  - VESC UART communication
  - Speed data retrieval
  - Motor control commands
  - Debug output and monitoring

#### Speed-Dependent Assist Algorithm

The system implements sophisticated assist profiles with speed-dependent curves based on 6 speed points:

**Speed Points**: 0, 5, 10, 15, 20, 30 km/h

**Currently Active Profile**: Linear (1.0 across all speeds)

**Available Profiles** (commented out in `config.cpp`):
```
Touring:      2.9 → 2.15 → 1.75 → 1.4 → 1.2 → 0.8  (0-30 km/h)
Mountain Bike: 2.0 → 1.6 → 0.5 → 0.8 → 1.2 → 1.0   (0-30 km/h)
Urban:        2.9 → 1.5 → 0.75 → 1.0 → 1.2 → 0.9   (0-30 km/h)
Speed:        1.0 → 1.5 → 2.5 → 2.6 → 2.7 → 3.0    (0-30 km/h)
No Assist:    0.0 → 0.0 → 0.0 → 0.0 → 0.0 → 0.0    (disabled)
```

**How it works**: Linear interpolation between speed points ensures smooth transitions without sudden power changes. Each profile has a different character - Touring starts strong then tapers off, Speed builds progressively, Mountain Bike has variable power for terrain changes.

#### Sensor Integration

**PAS (Pedal Assist Sensor)**
- 8 hall sensors per crank revolution
- Interrupt-driven with hardware debouncing
- Calculates pedaling cadence and direction
- Provides immediate assist activation/deactivation

**Torque Sensor**
- Analog strain gauge measurement
- 12V supply via step-up converter
- Real-time torque-based assist scaling
- Natural riding feel with proportional assistance

**Speed Sensor**
- Derived from VESC eRPM data
- Motor pole pairs and gear ratio calculation
- Accurate speed measurement for assist curves
- Used for legal speed limit compliance

#### VESC Communication Protocol

The system communicates with the VESC motor controller using:
- UART protocol at 115200 baud
- Real-time telemetry data retrieval
- Current-based motor control commands
- Battery monitoring and protection

## System Components

- **Microcontroller**: ESP32 DevKit v1 (dual-core, 3.3V logic)
- **Motor Controller**: VESC (Flipsky FESC 6.7 pro mini)
- **Motor**: Q100C CST 36V/350W Hub Motor with 14.2:1 gear ratio
- **Battery**: 48V 13S2P Li-ion with integrated BMS
- **Sensors**: PAS hall sensors, analog torque sensor, optional wheel speed sensor
- **Additional**: Logic level converters, step-up converter, MOSFET headlight control

## Safety Features

- **Battery Protection**: Voltage monitoring with automatic cutoff
- **Thermal Protection**: Temperature monitoring and throttling
- **Speed Limiting**: Configurable maximum assist speed
- **Fault Detection**: System health monitoring with error codes
- **Fail-Safe**: Graceful degradation when sensors fail

## Getting Started

### Hardware Setup
For complete wiring instructions and component connections, see [Wiring Guide](/_documentation/Wiring.md).

### Software Configuration
1. Install PlatformIO IDE
2. Clone this repository
3. Configure your specific motor and battery parameters in `include/tailwind_controller.h`
4. Configure telemetry interfaces in `src/config.cpp`:
   - Set `enable_wifi_telemetry = true;` for minimalist WiFi web interface
   - Set `enable_ble_telemetry = true;` for BLE mobile interface
   - Both can be enabled simultaneously (requires huge_app partition)
5. Upload firmware to ESP32

**Memory Requirements**: The minimalist WiFi interface is optimized for memory efficiency. WiFi + BLE together requires ~1.2MB flash memory (reduced from previous 1.6MB). The project uses `huge_app.csv` partition (3.1MB app space) to accommodate both interfaces comfortably. The simplified WiFi implementation significantly reduces memory pressure and improves overall system stability.

## Code Structure

```
src/
├── main.cpp              # Multi-core initialization and main loops
├── config.cpp            # Assist profiles and global variables
├── assist_calculation.cpp # Speed-dependent assist algorithms
├── motor_control.cpp     # VESC control and safety limits
├── pas_sensor.cpp        # PAS interrupt handling and debouncing
├── torque_sensor.cpp     # Analog torque measurement
├── vesc_communication.cpp # UART protocol with VESC
├── mode_management.cpp   # User interface and mode switching
├── debug_output.cpp      # Serial monitoring and diagnostics
└── initialization.cpp    # Hardware setup and calibration
```

## Key Features

- ✅ **Real-time Performance**: Sub-millisecond sensor response times
- ✅ **Smooth Assistance**: Linear interpolation prevents power jerks
- ✅ **Energy Efficient**: Smart algorithms maximize range
- ✅ **User Friendly**: Simple mode switching with LED feedback
- ✅ **Diagnostic Tools**: Comprehensive debug output and monitoring
- ✅ **Safety First**: Multiple protection layers and fail-safes
- ✅ **Memory Optimized**: Minimalist WiFi interface for maximum stability
- ✅ **Dual Interface**: WiFi web monitoring + BLE mobile connectivity
- ✅ **Open Source**: Fully documented and customizable

**Note**: The WiFi web interface has been optimized for stability and memory efficiency. It now provides essential monitoring data only (speed, battery, torque, mode) through a simplified interface designed for reliable operation on ESP32 hardware.

## Debug Mode

The system includes comprehensive debugging and testing capabilities for development and troubleshooting:

### Debug Features
- **Real-time Monitoring**: Serial output shows sensor states, speed, torque, and system status
- **Sensor Simulation**: Simulate PAS and torque sensors without physical hardware
- **Performance Analysis**: Monitor loop times, sensor response, and algorithm performance
- **Built-in LED Status**: GPIO2 LED indicates system status and debug information

### Debug Modes
1. **Normal Debug**: Real sensors with detailed serial output
2. **PAS Simulation**: Simulated pedal sensor for testing without pedaling
3. **Torque Simulation**: Simulated torque values for algorithm testing
4. **Systematic Testing**: Automated testing of all cadence/torque combinations

### Simulation Modes
- **Smooth Cycle**: Realistic cycling simulation with varying cadence and torque
- **Systematic Test**: Tests predefined combinations of cadence (20-120 RPM) and torque (5-40 Nm)

### Debug Configuration

Enable debug features in `include/tailwind_controller.h`:
```cpp
// debug_mode will skip reading real values from pas and torque and instead
// cycle through torque and cadence values automatically. 
// ATTENTION: it WILL SPIN the motor, so be aware that it actually can spin freely!!
bool debug_mode = true;              // Enable debug mode
bool debug_simulate_pas = false;     // Simulate PAS sensor / or use the physical sensor
bool debug_simulate_torque = false;  // Simulate torque sensor / or use the physical sensor
```

### Serial Monitor Output
The debug output shows:
- Sensor states (A/B hall sensors)
- Pedal direction and position
- Cadence in RPM
- Speed in km/h
- Raw and filtered torque values
- Current assist factor and motor current
- System warnings and error states

## WiFi Web Interface

The tailwind controller includes a minimalist WiFi web interface optimized for memory efficiency and stability. When enabled, the ESP32 creates a WiFi access point allowing you to monitor your e-bike via any web browser with essential information only.

### Web Interface Features

**Minimalist Real-time Dashboard**
- **Essential Metrics Only**: Speed (km/h), Battery level (%), Torque (Nm), Active assist mode
- **Simplified Layout**: Clean, mobile-optimized interface with large, readable values
- **Low Memory Footprint**: Optimized for ESP32 stability with minimal resource usage
- **Manual Refresh**: Single refresh button to update all values on demand
- **Auto-refresh**: Automatic updates every 5 seconds to reduce system load

**Key Design Principles**
- **Stability First**: Extremely simple implementation to prevent memory issues and crashes
- **Essential Data Only**: Shows only the most important riding information
- **Mobile Optimized**: Large text and buttons for easy reading while riding
- **Low Bandwidth**: Minimal data transfer for reliable connection
- **No Complex Features**: Removed logging, detailed VESC data, and mode switching to ensure stability

### WiFi Configuration

**Access Point Settings**
- **SSID**: `Tailwind`
- **Password**: `tailwind123`
- **IP Address**: `192.168.4.1`
- **Port**: 80 (HTTP)
- **Max Connections**: 4 devices simultaneously
- **Channel**: 6 (optimized for better connection stability)

### How to Access

1. **Enable WiFi** in `src/config.cpp`: Set `enable_wifi_telemetry = true;`
2. **Upload firmware** to ESP32
3. **Connect device** to WiFi network "Tailwind" (password: tailwind123)
4. **Open browser** and navigate to `http://192.168.4.1`
5. **Monitor** essential e-bike data with a simple, reliable interface

### Technical Implementation

The minimalist web interface is designed for maximum stability:

- **Single API Endpoint**: Only `/status` endpoint providing essential data (speed, battery, torque, mode)
- **No Logging System**: Removed all logging functionality to save memory
- **Simplified HTML**: Minimal CSS and JavaScript for fast loading and low memory usage
- **Reduced Stack Size**: WiFi task uses only 8KB stack (reduced from 16KB)
- **Thread-safe Data Access**: Semaphore-protected access to shared sensor data
- **Error Recovery**: Automatic WiFi Access Point restart if connection fails
- **Low Update Rate**: 2-second intervals to reduce system load

### Memory Optimization

**What was removed for stability:**
- System logging and log display
- Detailed VESC telemetry (temperatures, voltages, etc.)
- Interactive mode switching via web interface
- Complex JSON processing (replaced with simple string concatenation)
- Debug output and verbose error messages
- Real-time auto-refresh (now manual refresh + 5-second intervals)

**Result:** Significantly reduced memory usage and improved stability, especially important when running alongside BLE interface.

### Mobile Compatibility

The minimal interface is optimized for:
- Quick status checks during rides
- Handlebar-mounted devices with limited screen space
- Reliable connection even with low signal strength
- Fast loading on slower mobile connections

## BLE (Bluetooth Low Energy) Interface

In addition to the WiFi web interface, the tailwind controller also provides a comprehensive BLE interface for mobile app integration and energy-efficient wireless monitoring.

### BLE Features

**Device Information Service**
- **Device Name**: `Tailwind`
- **Manufacturer**: Tailwind Project
- **Model**: ESP32-Controller-v1.0
- **Firmware Version**: 1.0.0

**Telemetry Service** (Notifications enabled)
- **Speed**: Real-time speed data (float, 4 bytes)
- **Cadence**: Pedaling cadence in RPM (float, 4 bytes)
- **Torque**: Applied torque in Nm (float, 4 bytes)
- **Battery**: Battery level percentage (uint8, 1 byte)
- **Motor Current**: Current motor current in A (float, 4 bytes)
- **VESC Data**: Extended motor controller data (JSON string)
- **System Status**: Mode, status flags, and timestamps (JSON string)

**Control Service**
- **Mode Control**: Write characteristic to change assist modes (uint8, 1 byte)
- **Mode List**: Available assist profiles with descriptions (JSON string)
- **Command Interface**: Text commands (GET_STATUS, GET_MODES, EMERGENCY_STOP)

### BLE Configuration

**Connection Settings**
- **Update Rate**: 2 seconds (0.5Hz) for battery efficiency
- **Service UUIDs**: Custom UUIDs for tailwind specific data
- **Auto-reconnect**: Automatic advertising restart after disconnection
- **Low Power**: Optimized for mobile device battery life

### How to Connect

1. **Enable BLE** in `src/config.cpp`: Set `enable_ble_telemetry = true;`
2. **Upload firmware** to ESP32
3. **Scan for devices** named "Tailwind" in your BLE app
4. **Connect and subscribe** to notification characteristics for live data
5. **Control modes** by writing mode numbers to the Mode Control characteristic

### iPhone/iOS Connection

**Important**: The Tailwind controller will **NOT** appear in iPhone's standard Bluetooth settings! This is normal for BLE devices.

**Required**: You need a BLE GATT-compatible app. Recommended apps:
- **LightBlue® Explorer** (free, excellent for testing)
- **nRF Connect for Mobile** (professional BLE tool)
- **BLE Scanner** (simple and user-friendly)

**Step-by-step for iPhone**:
1. Install "LightBlue Explorer" from App Store
2. Open app (automatically starts BLE scan)
3. Look for "Tailwind" in device list
4. Tap device → "Connect"
5. Explore services and characteristics
6. Enable notifications on telemetry characteristics for live data
7. Use "Mode Control" characteristic to change assist modes (write hex values: 00, 01, 02, etc.)

### Android Connection

Android apps can use the same BLE tools or custom apps using the Android BLE API with the UUIDs provided in the [BLE API Reference](_documentation/BLE_API_Reference.md).

### Mobile App Development

The BLE interface is designed for easy mobile app integration:

- **Standard BLE GATT**: Compatible with iOS, Android, and other BLE platforms
- **JSON Data Format**: Easy parsing of complex data structures
- **Notification-based**: Efficient real-time updates without polling
- **Command Interface**: Send text commands for specific actions
- **Error Handling**: Robust connection management and reconnection

### Technical Implementation

The BLE interface runs on the same FreeRTOS task architecture:

- **Core 1 Task**: Low priority task alongside WiFi and VESC communication
- **Thread-safe**: Semaphore-protected access to sensor data
- **GATT Services**: Standard Bluetooth services with custom characteristics
- **Memory Efficient**: Optimized data structures for embedded systems
- **Auto-advertising**: Automatic restart after disconnection

## Legal Compliance

**⚠️ Important Notice: This controller is NOT compliant with European e-bike regulations (EN 15194).**

This is an open-source project designed for:
- Research and educational purposes
- Private property use only
- Countries/regions without strict e-bike speed limitations
- Custom applications where regulations permit

**Key differences from EU regulations:**
- No 25 km/h speed limit enforcement
- Configurable assist profiles up to 30+ km/h
- No automatic power limitation at legal speeds
- Full user control over assist parameters

**User Responsibility:** It is the user's responsibility to ensure compliance with local laws and regulations when using this controller on public roads.

## Configuration

To customize the controller for your specific setup:

- **Hardware Settings**: Motor specifications, battery parameters, and pin assignments are configured in `include/tailwind_controller.h`
- **Assist Profiles**: Speed curves, assist modes, and behavior profiles are defined in `src/config.cpp`
- **Speed Points**: Modify the `SPEED_POINTS_KMH` array to change speed ranges for assist curves
- **Motor Parameters**: Adjust pole count, gear ratios, and power limits for your specific motor

## Contributing

This is an open-source project. Contributions, improvements, and adaptations are welcome! Please see the wiring documentation and code comments for technical details.

## License

This project is released under the MIT License. The hardware designs, software code, and documentation are freely available for:
- Personal use and modification
- Educational and research purposes
- Commercial and non-commercial applications
- Further development and improvement
- Distribution and selling of modified versions

The MIT License allows you to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of this software, with the only requirement being to include the original copyright notice and license text.

Feel free to fork, modify, and share your improvements with the community!
