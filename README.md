# Open Modbus OM-16LED

![Modbus RTU](https://img.shields.io/badge/Modbus-RTU-blue?style=for-the-badge&logo=modbus)
![MCU STM32G474](https://img.shields.io/badge/MCU-STM32G474-orange?style=for-the-badge&logo=stmicroelectronics)
![16 Channel Outputs](https://img.shields.io/badge/Outputs-16_CH-black?style=for-the-badge)
![Input Voltage 12V or 15-27V DC](https://img.shields.io/badge/Input_Voltage-12V_or_15--27V_DC-yellow?style=for-the-badge)
![Load Voltage Up to 60V DC](https://img.shields.io/badge/Load_Voltage-Up_to_60V_DC-red?style=for-the-badge)
![DIN-Rail UM72S Profile](https://img.shields.io/badge/DIN--Rail-UM72S_Profile-lightgrey?style=for-the-badge)
![License GNU GPL v3](https://img.shields.io/badge/License-GNU_GPL_v3-blue?style=for-the-badge&logo=gnu)

A compact industrial-grade **16-channel Modbus RTU LED driver module** featuring high-current MOSFET outputs, per-bank terminal grouping, wide load voltage range, DIN-rail support, and full configuration via Modbus holding registers. Designed for lighting automation, distributed I/O, and custom control applications.

## 1. Product Overview

The **Open Modbus OM-16LED** is a 16-channel low-side LED driver module intended for industrial automation, lighting control, and distributed I/O networks using **Modbus RTU** over RS-485.
It switches DC LED loads such as LED strips, architectural lighting, and indicator arrays up to **60 VDC** at up to **8 A per channel**.

The module combines an **STM32G474** microcontroller, eight **2EDN7534B** dual-channel low-side gate drivers, and sixteen **NTMFS4D0N08XT1G** N-channel power MOSFETs.
This architecture enables reliable, high-current, and scalable LED output control with built-in protection and per-channel MOSFET switching.

### 📦 Hardware Files

The full schematic and BOM are available at:

- Schematic → [https://github.com/OpenModbus/OM-16LED/blob/main/assets/schematics.pdf](https://github.com/OpenModbus/OM-16LED/blob/main/assets/schematics.pdf)
- BOM → [https://github.com/OpenModbus/OM-16LED/blob/main/assets/bom.csv](https://github.com/OpenModbus/OM-16LED/blob/main/assets/bom.csv)

Check the latest release for more!

## 2. Key Features

- 16 low-side MOSFET output channels
- Up to **8 A per channel**, **60 VDC** maximum load voltage
- **12 VDC** direct input or **15–27 VDC** via onboard converter (jumper selectable)
- RS-485 Modbus RTU communication (THVD1450 transceiver)
- Fully configurable serial parameters
- DEGSON 2EDGVC pluggable screw terminals
- DIN-rail mount using **UM72S profile**
- Compact PCB (100 × 72 mm)

## 3. Applications

Ideal for:

- LED strip lighting control
- Architectural and mood lighting
- Building automation
- PLC expansion
- Industrial signaling and indicators
- Stage and display lighting
- Custom distributed lighting networks

## 4. Hardware Architecture

### Microcontroller

The STM32G474RETx MCU handles Modbus RTU processing, configuration storage, and output control logic.
It is an ARM Cortex-M4 running at up to 170 MHz with 512 KB flash and 128 KB RAM, providing ample headroom for advanced output control features.

### Gate Drivers

Eight **2EDN7534B** (Infineon) dual-channel low-side gate drivers control the power MOSFETs.
Their key characteristics include:

- 2 channels per IC (8 ICs → 16 channels total)
- 5 A peak gate drive current
- Precise timing and low output resistance
- Designed for reliable switching of high-current loads

### Output MOSFETs

Sixteen **NTMFS4D0N08XT1G** (onsemi) N-channel MOSFETs perform the actual load switching.
Their key characteristics include:

- 80 V drain-source breakdown voltage
- High continuous drain current
- SO8FL package

### RS-485 Communication

A **THVD1450** (Texas Instruments) transceiver provides differential RS-485 communication with:

- ±18 kV IEC ESD protection
- Fail-safe features
- Suitable for long cable runs

### Power

The board supports two input modes selected by an onboard jumper:

- **15–27 VDC input** — passes through the **R-78E12-1.0** step-down regulator (RECOM), which generates the 12 V gate driver supply
- **12 VDC direct input** — bypasses the 12 V converter; 12 V is fed directly to the gate drivers

In both modes the **R-78E3.3-0.5** step-down regulator (RECOM) generates 3.3 V for the MCU and logic from the main input rail.

## 5. Electrical Specifications

### Supply

- **12 VDC** direct input (jumper: bypass mode) or **15–27 VDC** input (jumper: converter mode)
- Internal regulated **12 V gate drive** supply (converter mode only)
- Internal regulated **3.3 V logic** supply

### Outputs

- 16 low-side MOSFET channels
- Up to **60 VDC** load voltage
- 80 VDC absolute maximum
- **8 A max per channel**
- Channels grouped into **4 banks of 4**

### Communication

- RS-485, 2-wire
- Modbus RTU protocol
- Default settings: **9600 baud, 8N1, address 1**
- All communication parameters can be reconfigured via holding registers

## 6. Mechanical Information

- PCB: **100 × 72 mm**
- DIN-rail enclosure compatibility: **UM72S profile**
- DEGSON 2EDGVC pluggable screw terminals
- Mounting suitable for industrial control cabinets

## 7. Functional Overview

Each output channel is controlled by a per-channel duty cycle value stored in a holding register.
The MCU generates a PWM signal accordingly, driving the gate driver and MOSFET to regulate LED brightness.
A duty cycle of **0** turns the channel fully off; the maximum value drives it fully on.

## 8. Modbus Interface

### Supported Function Codes

> **To be announced soon.**

### Holding Registers

Configuration registers start at **0x00** and are stored in non-volatile memory.

| Register Address (Hex) | Name           | Description                                                                                      | Allowed Values                                                                                                          | Default |
|------------------------|----------------|--------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------|---------|
| 0 (**0x00**)           | Slave Address  | Modbus device ID                                                                                 | 1–247                                                                                                                   | 1       |
| 1 (**0x01**)           | Baud Rate Code | Serial speed selector: **0=1200**, **1=2400**, **2=4800**, **3=9600**, **4=19200**, **5=38400**, **6=57600**, **7=115200** | 0–7                                                                                                                     | 3       |
| 2 (**0x02**)           | Parity         | Communication parity                                                                             | 0=None, 1=Even, 2=Odd                                                                                                   | 0       |
| 3 (**0x03**)           | Data Bits      | Serial data width                                                                                | 5–9 (typ. 8)                                                                                                            | 8       |
| 4 (**0x04**)           | Stop Bits      | Serial stop bits                                                                                 | 1–2                                                                                                                     | 1       |

### Example Modbus Commands

> **To be announced soon.**

## 9. Wiring

The **OM-16LED** uses low-side (MOSFET) switching.
The module connects the LED load to ground when ON; the load must be powered from your external power supply.

The 16 outputs are grouped into **four banks of 4 channels** (Bank A–D).
This allows one board to drive LED loads powered from different voltages simultaneously.

**Examples**:

- Bank A → 12 V LED strips
- Bank B → 24 V LED strips
- Bank C → 48 V LED channels
- Bank D → 5 V LED indicators

### Low-Side Output Principle

Each output contains a power MOSFET that sinks current:

```text
Load +V  ─────── LED Load ─────── OUTx (SINK)
                                       │
                                       └── GND (OM-16LED)
```

### Recommended Wiring Steps

- Connect the load voltage (up to 60 VDC) to the Vx terminal of the bank.
- Connect the output pin (OUTx) to the negative side of the LED load.
- Tie power supply GND to OM-16LED GND.
- Ensure the supply can support the total current of that bank.

**⚠️ Important Grounding Requirement**:
All load power supplies must share **a common ground** with the OM-16LED.
This is required for the low-side drivers and internal logic to reference the same electrical 0 V.

## 10. Safety Notes

- Do not exceed **60 VDC** load voltage
- Ensure load currents stay within **8 A per channel**
- Maintain proper RS-485 termination and shielding
- Verify adequate heat dissipation when operating at high currents

## 11. Contact & Support

For questions, feedback, or technical assistance regarding the Open Modbus OM-16LED, the following support channels are available:

### 📧 Email Support

For general inquiries, firmware questions, or troubleshooting:
[contact@sszczep.dev](mailto:contact@sszczep.dev)

### 🛠 GitHub Repository

Issue tracker, source code, hardware design files, and community contributions:
[https://github.com/OpenModbus/OM-16LED](https://github.com/OpenModbus/OM-16LED)

### 💬 Open Modbus Discussions

Share projects, ask for help, or suggest features:
[https://github.com/orgs/OpenModbus/discussions](https://github.com/orgs/OpenModbus/discussions)

### 📝 Bug Reports & Feature Requests

Submit issues directly via GitHub Issues:
[https://github.com/OpenModbus/OM-16LED/issues](https://github.com/OpenModbus/OM-16LED/issues)
