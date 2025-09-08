# ESP32 DevKit v1 Wiring Guide for E-Bike Controller

## Overview
This document describes the complete wiring of the ESP32 DevKit v1 for the E-Bike Controller System with PAS sensors, torque sensor, and VESC motor controller.

⚠️ **IMPORTANT**: ESP32 operates with 3.3V logic! 5V sensors require Logic Level Converter!

## ESP32 DevKit v1 Pin Assignment

### PAS (Pedal Assist) Sensors - Hall Sensors
```
PAS Sensor A (Phase A) → GPIO18 (via Logic Level Converter)
PAS Sensor B (Phase B) → GPIO19 (via Logic Level Converter)
```

**PAS Sensors Wiring:**
- **Red (5V)**: To external 5V power supply (not ESP32 5V!)
- **Black (GND)**: Common Ground (ESP32, 5V supply, sensors)
- **Green (Signal A)**: Via Logic Level Converter to GPIO18
- **White/Yellow (Signal B)**: Via Logic Level Converter to GPIO19

### Torque Sensor - Analog
```
Torque Sensor Signal → GPIO36 (ADC1_CH0, SVP)
```

**Torque Sensor Wiring:**
- **Red (5V)**: To external 5V power supply
- **Black (GND)**: Common Ground
- **Signal**: Directly to GPIO36 (already 3.3V compatible)

### VESC Motor Controller - UART
```
VESC RX ← GPIO17 (TX2) - ESP32 sends commands
VESC TX → GPIO16 (RX2) - ESP32 receives data
```

**VESC Wiring:**
- **VESC UART_RX**: GPIO17 (TX2) of ESP32
- **VESC UART_TX**: GPIO16 (RX2) of ESP32
- **GND**: Common Ground between ESP32 and VESC

### Wheel Speed Sensor (Future)
```
Wheel Speed Pulse Cable → GPIO5
```

**Wheel Speed Sensor Wiring:**
- **Signal**: GPIO5 (interrupt-capable for pulse detection)
- **GND**: Common Ground
- **3.3V/5V**: Depending on sensor (5V sensors need Logic Level Converter)
- **Function**: Counts wheel pulses for precise speed measurement independent of VESC

### Status LEDs and Headlight
```
Onboard LED → GPIO2 (for Light Mode Display)
Battery Status LED → GPIO4 (for Low Battery Warning)
Headlight (Front Light) → GPIO25 (via MOSFET)
```

**Battery LED Wiring:**
- **LED Anode (+)**: GPIO4
- **LED Cathode (-)**: ESP32 GND via 220Ω resistor
- **Behavior**: 
  - Battery OK (>20%): LED off
  - Battery Low (≤20%): LED blinks every 500ms
  - Battery Critical (≤10%): LED blinks fast every 200ms

**Headlight (Front Light) Wiring:**
- **ESP32 GPIO25**: MOSFET Gate (via 1kΩ resistor)
- **MOSFET Source**: Common Ground
- **MOSFET Drain**: LED Headlight Cathode (-)
- **LED Headlight Anode (+)**: VESC 5V Port
- **Specification**: 5V LED Headlight, max. 1A
- **MOSFET**: N-Channel (e.g. 2N7002, IRLZ44N)
- **Control**: PWM possible for dimming, On/Off via software

## Logic Level Converter Wiring - Extended

**For all 5V ↔ 3.3V signals (4-Channel Converter required):**

```
4-Channel Logic Level Converter:

5V Side (HV - High Voltage):
├── HV ──── VESC 5V Port
├── GND ─── Common Ground  
├── HV1 ─── PAS Sensor A Signal (Green)
├── HV2 ─── PAS Sensor B Signal (White/Yellow)
├── HV3 ─── VESC UART TX (5V level)
└── HV4 ─── VESC UART RX (5V level)

3.3V Side (LV - Low Voltage):
├── LV ──── ESP32 3.3V
├── GND ─── Common Ground
├── LV1 ─── ESP32 GPIO18 (PAS_PIN_A)
├── LV2 ─── ESP32 GPIO19 (PAS_PIN_B)  
├── LV3 ─── ESP32 GPIO16 (RX2) ◄── VESC TX
└── LV4 ─── ESP32 GPIO17 (TX2) ──► VESC RX
```

**Signal Directions:**
- **PAS Sensors**: 5V Sensors → ESP32 (INPUT)
- **VESC UART TX**: VESC → ESP32 (INPUT)  
- **VESC UART RX**: ESP32 → VESC (OUTPUT)
- **Bidirectional**: The converter supports both directions automatically

**Why Logic Level Converter?**
- ESP32 GPIO pins are **ONLY 3.3V tolerant**
- 5V signals would damage ESP32
- VESC UART operates with 5V TTL levels
- PAS sensors require 5V supply and output 5V signals

## Power Supply and Voltage Distribution - Extended

### Main Power Supply
- **48V Battery (13S2P)**: Main energy source
- **BMS**: Protection and monitoring of battery
- **Antispark Switch**: Safe on/off switching before VESC

### VESC Power Supply
- **Main Voltage**: 48V directly from battery via antispark switch
- **5V Port**: Internal 5V supply (max. 1A) for all peripheral devices
- **GND**: Common ground for all components

### 5V Distribution from VESC Port
```
VESC 5V Port (1A max)
├── ESP32 VIN (500mA) ──► ESP32 DevKit v1
├── Step-Up Converter (100mA) ──► 12V for Torque Sensor  
├── Logic Level Converter HV (50mA) ──► 5V Side for PAS Sensors
└── MOSFET Circuit (1A max) ──► LED Headlight 5V
```

### Voltage Converter Details

#### Step-Up Converter (5V → 12V)
- **Input**: 5V from VESC Port
- **Output**: 12V/50mA for Torque Sensor
- **Type**: DC-DC Boost Converter (e.g. MT3608)
- **Efficiency**: ~85%

#### Logic Level Converter (3.3V ↔ 5V)
- **5V Side**: PAS Sensors, VESC UART (5V logic)
- **3.3V Side**: ESP32 GPIO Pins (3.3V logic)  
- **Type**: Bidirectional 4-Channel (BSS138 based)
- **Channels**: PAS_A, PAS_B, UART_TX, UART_RX

### Ground Connections (CRITICAL!)
**All components must have common ground:**
- ESP32 GND
- VESC GND (from 5V Port)
- Battery GND (via VESC)
- PAS Sensors GND
- Torque Sensor GND (via Step-Up)
- Logic Level Converter GND
- MOSFET Source GND
- Headlight GND

⚠️ **IMPORTANT**: Missing common ground leads to sensor errors and UART communication problems!

## Complete Pin Table - Extended

| Component | ESP32 Pin | Voltage | Remarks | Power Consumption |
|-----------|-----------|---------|---------|-------------------|
| PAS Sensor A | GPIO18 | 3.3V | Via Logic Level Converter | Signal Only |
| PAS Sensor B | GPIO19 | 3.3V | Via Logic Level Converter | Signal Only |
| Torque Sensor Signal | GPIO36 | 3.3V | ADC Input, SVP Pin | Signal Only |
| VESC UART RX | GPIO17 | 3.3V | Hardware UART2 TX, via Logic Level | Signal Only |
| VESC UART TX | GPIO16 | 3.3V | Hardware UART2 RX, via Logic Level | Signal Only |
| Status LED (Onboard) | GPIO2 | 3.3V | Built-in LED (Debug/Status) | 20mA |
| Battery LED | GPIO4 | 3.3V | External LED with 220Ω resistor | 15mA |
| **Headlight MOSFET** | **GPIO25** | **3.3V** | **N-Channel MOSFET Gate (PWM capable)** | **Signal Only** |
| Wheel Speed Sensor | GPIO5 | 3.3V | Wheel pulse cable (future, interrupt-capable) | Signal Only |

### Additional Hardware Components

| Component | Input Voltage | Output Voltage | Remarks |
|-----------|---------------|----------------|---------|
| **Step-Up Converter** | **5V (VESC Port)** | **12V/50mA** | **For Torque Sensor** |
| **Logic Level Converter** | **5V ↔ 3.3V** | **Bidirectional** | **4-Channel for PAS + UART** |
| **MOSFET Circuit** | **5V (VESC Port)** | **5V/1A** | **For Headlight** |
| VESC 5V Port | 48V Battery | 5V/1A max | Supplies all 5V components |
| BMS | 48V Battery | 48V | With antispark switch |

## Complete System Wiring Diagram

### Overview Graphic - Complete E-Bike System

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                           E-BIKE SYSTEM OVERVIEW                                    │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  48V Battery ──► BMS ──► Antispark ──► VESC ──► Q100C Motor                         │
│       │           │         │          │                                            │
│       │           │         │          └─── 5V Port ──► LogicLevelConv              |
|       |           |         |                             |                         |
|       |           |         |                         ESP32                         |
│       │           │         │                                                       │
│       │           └── Charging Port                                                 │
│       │                                                                             │
│       └── Direct to Charging Adapter                                                │
│                                                                                     │
│  ESP32 ◄────► VESC (UART)                                                           │
│    │                                                                                │
│    ├── PAS Sensors (via Logic Level Converter)                                      │
│    ├── Torque Sensor (via 12V Step-Up)                                              │
│    ├── Battery Status LED                                                           │
│    ├── Headlight (via MOSFET)                                                       │
│    └── Wheel Speed (currently not used)                                             │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

### Detailed Connection Diagrams

#### 1. ESP32 DevKit v1 - Main Controller
```
                    ESP32 DevKit v1
                   ┌─────────────────┐
                   │                 │
         PAS_A ────┤ GPIO18          │  ◄── Logic Level Converter
         PAS_B ────┤ GPIO19          │  ◄── Logic Level Converter
                   │                 │
    Torque_ADC ────┤ GPIO36 (SVP)    │  ◄── Step-Up Converter (12V→3.3V)
                   │                 │
      VESC_RX  ────┤ GPIO17 (TX2)    │  ◄── Logic Level Converter (3.3V↔5V)
      VESC_TX  ────┤ GPIO16 (RX2)    │  ◄── Logic Level Converter (3.3V↔5V)
                   │                 │
    Status_LED ────┤ GPIO2           │  ◄── Onboard LED (Debug/Status)
   Battery_LED ────┤ GPIO4           │  ◄── External LED + 220Ω (Debug/Status)
 Headlight_PWM ────┤ GPIO25          │  ◄── MOSFET Gate (Headlight)
  Wheel_Speed  ────┤ GPIO5           │  ◄── Speed Sensor (not used currently)
                   │                 │
           5V  ────┤ VIN             │  ◄── VESC 5V Port
           GND ────┤ GND             │  ◄── Common Ground
                   └─────────────────┘
```

#### 2. VESC Motor Controller - Central Control
```
                      VESC Controller
                   ┌─────────────────────┐
                   │                     │
    Motor_A ───────┤ Phase A (Green)     │
    Motor_B ───────┤ Phase B (Yellow)    │
    Motor_C ───────┤ Phase C (Blue)      │
                   │                     │
   Hall_H1 ───────┤ Hall 1 (Orange)      │
   Hall_H2 ───────┤ Hall 2 (Yellow)      │
   Hall_H3 ───────┤ Hall 3 (Green)       │
                   │                     │
ESP32_UART_TX ────┤ UART RX              │
ESP32_UART_RX ────┤ UART TX              │
                   │                     │
    5V_Out ───────┤ 5V Port              │ ──► ESP32 + Step-Up + Logic Level
   GND_Out ───────┤ GND                  │ ──► Common Ground
                   │                     │
  Battery+ ───────┤ Power Input +        │ ◄── Antispark Switch
  Battery- ───────┤ Power Input -        │ ◄── Battery GND / Antispark Switch
                   └─────────────────────┘
```

#### 3. Power Supply and Converters
```
48V Battery (13S2P)
       │
       ├── BMS ──── Charging Port (HV Pins)
       │
       └── Antispark Switch ──── VESC Power Input
                   │
                   └── VESC 5V Port (1A max)
                           │
                           ├── ESP32 VIN (5V/500mA)
                           │
                           ├── Step-Up Converter (5V→12V)
                           │   └── Torque Sensor (12V/50mA)
                           │
                           ├── Logic Level Converter (5V Side)
                           │   └── PAS Sensors (5V/100mA)
                           │
                           └── MOSFET for Headlight (5V/1A max)
                               └── LED Headlight (5V/1A)
```

#### 4. Logic Level Converter Connections
```
    Logic Level Converter (4-Channel)
    ┌─────────────┬─────────────┐
    │  5V Side    │  3.3V Side  │
    │             │             │
    │ HV ── 5V    │ LV ── 3.3V  │ ◄── ESP32 3.3V
    │ GND ─────────── GND       │ ◄── Common Ground
    │             │             │
    │ HV1 ─────── LV1 ── GPIO18 │ ◄── PAS Sensor A
    │ HV2 ─────── LV2 ── GPIO19 │ ◄── PAS Sensor B
    │ HV3 ─────── LV3 ── GPIO16 │ ◄── VESC UART RX
    │ HV4 ─────── LV4 ── GPIO17 │ ◄── VESC UART TX
    └─────────────┴─────────────┘
           │             │
    PAS Sensors     ESP32 DevKit v1
    (5V Signals)    (3.3V Logic)
```

#### 5. Torque Sensor with Step-Up Converter
```
                    5V→12V Step-Up Converter
                   ┌─────────────────────┐
    VESC 5V ───────┤ VIN (5V)            │
    Common GND ────┤ GND                 │
                   │                     │
                   │ VOUT (12V) ─────────┤──── Torque Sensor VCC (White, Pin13)
                   └─────────────────────┘
                                         │
    Torque Sensor Connections:           │
    ├── Orange (Pin14) "Throttle Sig" ───┼──── ESP32 GPIO36 (ADC) 3KOhm resistor!
    ├── White (Pin13) "+12V" ────────────┘
    ├── Brown (Pin9) "PFS" ─────────────────── Not used
    ├── Green (Pin12) "CRUISE" ─────────────── Not used  
    └── Black (Pin16) "GND" ────────────────── Common Ground
```

#### 6. Headlight with MOSFET Control
```
                    MOSFET Circuit Headlight
                                     ┌─── 5V (VESC Port)
                                     │
                                   [LED Headlight 5V/1A]
                                     │
                              Drain ─┴─ Source
                                   │ │
    ESP32 GPIO25 ──[1kΩ]──── Gate ─┘ │
                                     │
                                   GND (Common Ground)
    
    MOSFET: N-Channel (e.g. 2N7002, IRLZ44N)
    - Gate Threshold: < 3V (for 3.3V ESP32 logic)
    - Drain-Source: 5V/1A min.
    - Gate Resistor: 1kΩ (protection)
```

#### 7. PAS Sensors with Hall Effect
```
                PAS Sensor A & B (Hall Effect)
                ┌─────────────────────────────┐
                │         PAS Sensor          │
                │                             │
    5V ─────────┤ VCC (Red)                   │
    GND ────────┤ GND (Black)                 │
                │                             │
                │ Signal A (Green) ───────────┤──── Logic Level Converter HV1
                │ Signal B (White/Yellow) ────┤──── Logic Level Converter HV2
                └─────────────────────────────┘
                
    Operating Principle:
    - 8 pulses per pedal revolution per pin
    - Quadrature encoder for direction detection
    - 90° phase shift between A and B
```

#### 8. Motor Q100C with Hall Sensors
```
                      Q100C Hub Motor
                   ┌─────────────────────┐
                   │                     │
                   │ Phase A (Green) ────┤──── VESC Phase A
                   │ Phase B (Yellow) ───┤──── VESC Phase B  
                   │ Phase C (Blue) ─────┤──── VESC Phase C
                   │                     │
                   │ Hall H1 (Orange) ───┤──── VESC Hall 1
                   │ Hall H2 (Yellow) ───┤──── VESC Hall 2
                   │ Hall H3 (Green) ────┤──── VESC Hall 3
                   │                     │
                   │ Encoder (White) ────┤──── Optional: VESC Encoder
                   │ GND ────────────────┤──── Motor Ground
                   │ 5V+ ────────────────┤──── Hall Sensor VCC
                   └─────────────────────┘
                   
    Specifications:
    - 36V/350W nominal, 48V compatible
    - Gear Ratio: 14.2:1
    - 16 Poles (8 pole pairs)
    - 135mm dropout, 36 holes
```

#### 9. Charging System
```
                    Charging Port (6-Pin)
                   ┌─────────────────────┐
                   │                     │
   Charger_HV+  ───┤ HV+ ────────────────┤──── Direct to BMS Input
   Charger_HV-  ───┤ HV- ────────────────┤──── Direct to BMS Input  
                   │                     │
                   └─────────────────────┘
```

#### 10. Power Flow Diagram
```
48V Battery ──► BMS ──► Antispark Switch ──► VESC (12-60V Input)
     │                                          │
     │                                          ├─► Motor (36-48V)
     │                                          │
     └─── Charging Port ◄─── Charger           └─► 5V Port (max 1A)
                                                       │
                                                       ├─► ESP32 (500mA)
                                                       ├─► Step-Up (12V, 50mA)
                                                       ├─► Logic Level (100mA)
                                                       └─► Headlight (1A)
```

## Calibration and Testing

### 1. Torque Sensor Calibration
```cpp
// Defined in code:
#define TORQUE_STANDSTILL   2820   // Neutral Position
#define TORQUE_MAX_FORWARD  4095   // Max Forward
#define TORQUE_MAX_BACKWARD 0      // Max Backward
```

**Test**: Move torque sensor and check ADC values in Serial Monitor.

### 2. PAS Sensor Test
```cpp
// Defined in code:
#define PAS_PULSES_PER_REV  8      // 8 pulses per revolution per pin
```

**Test**: Turn pedals and follow interrupt events in Serial Monitor.

### 3. VESC Communication Test
- UART Baud rate: Usually 115200 (configure in VESC Tool)
- Test with simple VESC commands

## Troubleshooting - Extended

### PAS Sensors not responding:
- Logic Level Converter correctly wired? (4-Channel required)
- 5V supply from VESC Port available? (Check with multimeter)
- Common ground between all components? (Continuity test)
- GPIO18/19 interrupt-capable and correctly configured?
- PAS sensor wire colors correct: Red=5V, Black=GND, Green=A, White=B

### Torque Sensor fluctuating/not working:
- Step-Up Converter output voltage set to 12V?
- Torque sensor receiving 12V at Pin13 (White)? (Check with multimeter)
- ADC Pin GPIO36 correctly connected to Pin14 (Orange)?
- Sensor GND (Pin16, Black) connected to common ground?
- Interference from other components? (Try noise filter capacitor)

### VESC Communication missing:
- Logic Level Converter for UART correctly wired?
- TX/RX correctly crossed? (ESP32 TX → VESC RX, ESP32 RX ← VESC TX)
- Baud rate in VESC Tool and ESP32 code identical? (115200)
- UART activated in VESC Tool? (App Settings → UART)
- 5V TTL level at VESC UART? (Check Logic Level Converter)

### Headlight not working:
- MOSFET Gate Threshold < 3V? (For ESP32 3.3V logic)
- MOSFET correctly wired? (Gate=GPIO25, Source=GND, Drain=LED-)
- LED Headlight voltage/current? (Max. 5V/1A from VESC Port)
- GPIO25 configured as OUTPUT and signal present?

### VESC 5V Port overloaded:
- Total current of all components under 1A?
  - ESP32: 100mA
  - Step-Up: 100mA  
  - Logic Level: 50mA
  - Headlight: max 1A
- If overloaded: Use external 5V supply or buck converter

### General Debugging Tips:
- Use Serial Monitor for debug output
- Multimeter for voltage measurements at all supply points
- Oscilloscope for signal analysis with UART/PAS problems
- Test components individually (ESP32 alone, then gradually expand)