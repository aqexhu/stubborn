# Stubborn Stamina Max Technical Datasheet

## Product Overview

Stubborn Stamina Max is the documentation name for qUPS-P-BC v2.0, a professional-grade battery-buffered UPS HAT for Raspberry Pi and other 5V DC systems. The battery-backed design allows extended runtime during outages and supports multiple battery chemistries.

## Technical Specifications

### Input
- **USB-C Input**: 5.0V – 5.2V DC
- **Terminal Input**: 5.2V – 28.0V DC (polarity protected)
- **Recommended Supply**: Minimum 3A for stable operation

### Output
- **Output Voltage**: 5.0V – 5.2V DC
- **Max Continuous Load**: 3.5A (17.5W)
- **Battery Discharge Limit**: 7.5A

### Charging
- **Charge Current**: 1A or 2A selectable via DIP switch
- **Switchover Time**: 100 – 300 μs

## Supported Battery Technologies

- **Li-ion / Li-Po**: 3.0V – 4.2V
- **LiFePo4**: 2.5V – 3.6V
- **Sodium-ion**: 1.8V – 4.0V

### Temperature Ranges
- **Li-ion / Li-Po**: 0°C to +40°C
- **LiFePo4**: -20°C to +60°C
- **Sodium-ion**: -30°C to +60°C

## GPIO and Communication

The Stubborn Stamina Max uses configurable GPIO pin assignments via GT1/GT2 DIP switches. The available signals are:

- **PFO (Power Fail Out)**: indicates external power status
- **LIM (Limit)**: indicates the battery has reached the safe threshold
- **SHD (Shutdown)**: handshake signal for safe shutdown/power-on sequencing

### DIP Switch Mapping

- **GT1=ON, GT2=OFF**
  - PFO: GPIO17 / Pin 11
  - LIM: GPIO27 / Pin 13
  - SHD: GPIO22 / Pin 15

- **GT1=OFF, GT2=ON**
  - PFO: GPIO23 / Pin 16
  - LIM: GPIO24 / Pin 18
  - SHD: GPIO25 / Pin 22

- **GT1=ON, GT2=ON**
  - PFO: GPIO5 / Pin 29
  - LIM: GPIO6 / Pin 31
  - SHD: GPIO26 / Pin 37

- **GT1=OFF, GT2=OFF**
  - Communication disabled

## LED Indicators

- **External Power**: Green when external input is present
- **Bad Polarity**: Red if the battery is connected with reverse polarity
- **Max**: Green when the battery is fully charged
- **Min**: Green when sufficient energy is available for boot and shutdown
- **Safe**: Yellow when battery reaches safe threshold
- **Low**: Red when battery charge is depleted and shutdown is imminent

## Runtime Estimates (4000mAh LiFePo4)

| Raspberry Pi Model | No Load [min] | 50% Load [min] | 100% Load [min] |
|--------------------|---------------|----------------|-----------------|
| Raspberry Pi 2     | 530           | 365            | 280             |
| Raspberry Pi 3     | 450           | 250            | 220             |
| Raspberry Pi 4     | 312           | 190            | 144             |
| Raspberry Pi 5     | 304           | 168            | 114             |

> Final runtimes depend on battery capacity, age, and ambient temperature.

## Operational Features

- **Safe-Start Logic**: In AUTO mode, the system waits until the battery has enough energy to complete a boot and shutdown cycle.
- **Anti-Reboot Loop**: Prevents repeated restart loops during brownouts.
- **Threshold Tuning**: A potentiometer sets the external power detection threshold to compensate for cable voltage drop.

## Usage Notes

- The unit is optimized for Raspberry Pi 4 and Raspberry Pi 5 devices.
- The battery connector supports up to 0.75 mm² wire.
- Ensure the battery can supply the maximum 7.5A discharge current.
- For industrial or extreme temperature use, LiFePo4 or Sodium-ion batteries are recommended.

## Software Support

- Recommended software: `qups-guard` daemon for safe shutdown and power management.
- Repository: https://github.com/aqexhu/qups-guard

## Compliance

- CE
- FCC Part 15 Class B
- RoHS
- REACH

## Product Information

- **Model**: Stubborn Stamina Max
- **Product Code**: qUPS-P-BC v2.0
- **Release**: 2024
- **Manufacturer**: AQEX Electronics
- **Website**: https://aqex.eu/qups-p-bc-raspberry-pi-ups-hat-with-battery.html
