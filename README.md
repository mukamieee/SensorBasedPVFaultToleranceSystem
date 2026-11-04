# Off-Grid Solar PV Fault Detection & Protection System

An edge-computing diagnostic node built on the Arduino Mega 2560. This system actively monitors solar photovoltaic telemetry to detect hardware degradation (thermal hotspots) and reverse current leakage, utilizing a non-blocking C++ state machine to physically isolate the circuit via an electromechanical relay.

## System Architecture
* **Microcontroller:** Arduino Mega 2560
* **Sensors:** INA219 (Current/Voltage), NTC Thermistor (Thermal $\Delta T$)
* **Actuation:** 5V Electromechanical Relay (10A DC)
* **Storage:** MicroSD SPI Module (FAT32 Offline Logging)
* **UI:** 0.96" I2C OLED & 4x4 Membrane Keypad

## Core Features
1. **Continuous Background Monitoring:** Evaluates sensor arrays without blocking the main loop.
2. **Time-Based Escalation:** Implements a 30-second Grace Period for passing environmental anomalies before triggering a Hard Lockout requiring technician intervention.
3. **Offline Event Database:** Automatically logs telemetry (`data.txt`), access attempts (`access.txt`), and fault resolutions (`events.txt`).
4. **Secure State Machine:** Completely hides system status behind a PIN-locked UI during physical danger states.

