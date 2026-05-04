# Stubborn Stamina Max User Manual

## Overview

Stubborn Stamina Max is the exact product name for the qUPS-P-BC v2.0 smart battery-buffered UPS HAT. It is designed to protect Raspberry Pi and other 5V DC devices by providing a seamless transition to battery backup and a safe shutdown path during power failures.

## Key Features

- Battery-buffered UPS for Raspberry Pi compatibility
- Supports Li-ion, Li-Po, LiFePo4, and Sodium-ion batteries
- Dual power input: USB-C 5.0–5.2V or industrial terminal 5.2–28V
- Maximum continuous load up to 3.5A
- Safe-Start AUTO mode to avoid reboot loops
- Quick switchover time: 100–300 μs
- Configurable GPIO handshake pins and power level thresholds

## Safety Regulations

### Personal Safety

- The Stubborn Stamina Max contains an energy storage system that may remain energized even when disconnected from mains power.
- The unit has no user-serviceable parts other than the battery. Repairs must be carried out by the manufacturer or an accredited service center.

### Product Safety

- Protect the product from extreme temperatures and direct sunlight.
- Keep the unit dry for at least 24 hours before installation.
- Avoid installation near conductive liquids or plastic materials that may cause short circuits.
- Only power the device through the USB-C connector or the industrial input terminal. Do not power the protected device from another source through the 40-pin connector.
- Do not use more than one qUPS unit on the same 40-pin connector.

### Precautions

- The system operates from low voltage but may still heat up if a short circuit occurs due to foreign objects.
- Use warm, well-ventilated environments and avoid mechanical stress to wiring.

## Introduction

The Stubborn Stamina Max is part of the Stubborn family and belongs to the Stubborn Stamina product line. It is optimized for uninterruptible operation of Raspberry Pi compatible single-board computers and any 5V DC device that draws up to 2.5A.

The battery-backed design enables extended runtime during outages, depending on the battery capacity and the connected device's current consumption.

## Commissioning

### 1. Power Supply

- USB-C input: 5.0V–5.2V DC, minimum 2A, 3A recommended.
- Industrial input: 5.2V–28.0V DC via the polarity-protected terminal.
- Use a quality power supply capable of supporting the battery charge current plus the protected device load.

### 2. Connections

#### 2.1 Raspberry Pi and SBCs

If the protected device has a Raspberry Pi compatible 40-pin header, plug the Stubborn Stamina Max directly onto the header.

#### 2.2 Other Devices

Any 5V DC device may be powered through the appropriate pins on the 40-pin connector. Verify the pinout before wiring.

#### 2.3 Powering the Unit

- Only use one power source at a time: USB-C or industrial terminal.
- The power indicator LED lights when external power is present.
- Never connect both power inputs to separate supplies simultaneously.

### 3. Battery Connection

- The battery is connected via the dedicated battery connector.
- Use a maximum cable size of 0.75 mm² (AWG18).
- The qUPS may draw up to 7.5A from the battery; choose a battery capable of this discharge current.
- Incorrect polarity is indicated by the bad polarity LED.

### 4. Temperature Sensing

- The optional NTC temperature sensor connects to the dedicated connector.
- If not using temperature sensing, short-circuit the NTC terminal.
- The qUPS will stop charging the battery if the measured temperature is outside safe limits.

### 5. GPIO and Reset

- All GPIO pins on the 40-pin header remain accessible except for the three pins assigned to PFO, LIM, and SHD.
- The reset connector can be used to create a momentary power interruption with a push button or switch.
- Reset wiring must be a simple contact closure, not a voltage signal.

## Setup

### 1. DIP Switch Configuration

The six-position setup switch configures:
- GPIO pin assignment (GT1 / GT2)
- Charge current (1A or 2A)
- Detection level thresholds (LS1 / LS2)
- Battery chemistry selection (Li-ion/LiPo vs LiFePo4/Sodium-ion)

#### Communication Pin Mapping

- GT1=ON, GT2=OFF: PFO → GPIO17 (Pin 11), LIM → GPIO27 (Pin 13), SHD → GPIO22 (Pin 15)
- GT1=OFF, GT2=ON: PFO → GPIO23 (Pin 16), LIM → GPIO24 (Pin 18), SHD → GPIO25 (Pin 22)
- GT1=ON, GT2=ON: PFO → GPIO5 (Pin 29), LIM → GPIO6 (Pin 31), SHD → GPIO26 (Pin 37)
- GT1=OFF, GT2=OFF: communication disabled

#### Detection Level Fine-Tuning

Use LS1 and LS2 to adjust the charge threshold for the connected Raspberry Pi model and battery capacity:
- Pi 2: LS1 OFF, LS2 OFF
- Pi 3: LS1 OFF, LS2 OFF
- Pi 4: LS1 OFF, LS2 OFF
- Pi 5: LS1 OFF, LS2 ON

#### Charge Current

- OFF: 1A charge current
- ON: 2A charge current

Use the lower setting if the external supply cannot support the protected device plus battery charging.

#### Battery Chemistry

- Set for Li-ion/LiPo or LiFePo4/Sodium-ion according to the installed battery.
- This selection is critical to protect the battery and to ensure correct charge behavior.

### 2. Input Threshold Potentiometer

The potentiometer adjusts the external power detection threshold. If set too high, the device may chatter between modes. If set too low, the Stubborn Stamina Max may switch to battery power too late during an outage.

### 3. Mode Switch

- OFF: removes power from the protected device.
- ON: powers the protected device immediately, regardless of battery state.
- AUTO: delays startup until sufficient battery energy is available for a safe boot and shutdown cycle.

## Usage

### Battery Assembly

1. Set the mode selector to OFF.
2. Install the spacers and battery holder onto the Raspberry Pi.
3. Secure the battery holder with the supplied screws.
4. Connect the battery to the battery connector, verifying polarity.
5. Place the battery into the holder and secure it with the clamping rings.
6. Mount the qUPS on the Raspberry Pi and fasten with M2.5 screws.
7. Switch the mode selector to ON or AUTO.

### Battery Disassembly

Perform the assembly steps in reverse order and set the mode selector to OFF before removing the battery.

### Normal Operation

With a correct setup, the system charges the battery when external power is available and switches automatically to battery power if the input is lost.

### AUTO Mode Behavior

- The system waits until the battery has enough energy for a full boot and safe shutdown.
- This avoids boot-loop behavior when the battery is deeply discharged.
- Startup delay may vary from a few seconds to several minutes.

### ON Mode Behavior

- The protected device starts immediately.
- This mode is not recommended for continuous use because it may allow abrupt shutdowns if the battery becomes critically low.

### Battery Voltage and Shutdown Behavior

- Measured battery voltage may differ from actual cell voltage due to internal resistance.
- During shutdown, voltage rebound can occur and delay proper power-off.
- If needed, configure a shutdown delay in the software to ensure the system remains below the cutoff voltage after shutdown.

### Deep Discharge and Storage

- The controller disconnects the battery at approximately 2.5V to protect the cells.
- Even when switched off, the unit draws a small current and can discharge the battery over several days.
- For long-term storage, disconnect one battery lead from the connector.

## Troubleshooting

- If the unit does not start, verify power input, battery connection, and mode switch position.
- If the bad polarity LED is lit, immediately disconnect the battery and verify wiring.
- If the system flashes the low-charge LED during startup, reduce the load or adjust the input threshold potentiometer.
- Use AUTO mode for the safest boot behavior.

## Software Support

- Recommended software: `qups-guard` daemon for safe shutdown and battery management.
- Repository: https://github.com/aqexhu/qups-guard

## Compliance

- Follow local regulations for battery handling and disposal.
- Only use approved battery types and charging configurations.
