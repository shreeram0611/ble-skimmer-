# 🔵 BLE Skimmer / Rogue Device Scanner

An ESP32-S3 based IoT device that passively detects hidden Bluetooth (BLE) card skimmers by recognizing their behavior — a signal that stays close and stays put far longer than any ordinary passing phone, earbud, or smartwatch would.

Built as a low-cost, open-hardware alternative to commercial skimmer-detection tools used by fuel-dispensing inspectors, retail security teams, and law enforcement fraud units — for roughly ₹600 in parts.

> ⚠️ This project only **passively listens** to publicly broadcast BLE advertisements. It never connects to, pairs with, or extracts data from any nearby device.

---

## Table of Contents

- [How It Works](#how-it-works)
- [Architecture](#architecture)
- [Bill of Materials](#bill-of-materials)
- [Wiring](#wiring)
- [Setup](#setup)
- [Features](#features)
- [Testing](#testing)
- [Limitations](#limitations)
- [Future Scope](#future-scope)
- [License](#license)

---

## How It Works

1. The ESP32-S3 continuously scans for BLE advertisements in ~5 second cycles.
2. Every unfamiliar device is tracked by MAC address, signal strength (RSSI), and **real elapsed time** since it was first seen.
3. A device only reaches full "risk" once it has been **continuously present for ~10 minutes** at close range — the behavioral signature of a hidden, stationary transmitter rather than someone briefly nearby.
4. Devices that leave are automatically dropped from tracking after 1 minute of absence.
5. A risk score (0–100) combines **persistence (60%)** and **signal strength (40%)**. Crossing 80 triggers a buzzer + LED alert; 50–79 shows as "elevated risk" without alarming.
6. Everything is visible live on an OLED screen **and** a self-hosted WiFi dashboard — no app, no internet connection required.

---

## Architecture

![Architecture Diagram](images/architecture_diagram.svg)

---

## Bill of Materials

| Component | Approx. Cost (₹) | Purpose |
|---|---|---|
| ESP32-S3 DevKit | 400 | Runs BLE scanning, WiFi dashboard, and display logic simultaneously |
| 0.96" OLED (SSD1306, I2C) | 120 | Live on-device status readout |
| Active buzzer | 15 | Audible alarm |
| Breadboard + jumper wires | 60 | Prototyping, no soldering required |
| **Total** | **~₹595** | |

*(The alert LED uses the board's built-in `LED_BUILTIN` — no external LED needed.)*

---

## Wiring

![Wiring Diagram](images/wiring_diagram.svg)

| Component pin | ESP32-S3 pin |
|---|---|
| OLED — SDA | GPIO8 |
| OLED — SCL | GPIO9 |
| OLED — VCC | 3.3V |
| OLED — GND | GND |
| Buzzer — positive | GPIO4 |
| Buzzer — negative | GND |
| Alert LED | Onboard (`LED_BUILTIN`) |

Pin choices specifically avoid the ESP32-S3's reserved flash/PSRAM pins (GPIO26–32), strapping pins (GPIO0, 3, 45, 46), and native USB pins (GPIO19, 20).

---

## Setup

1. Install the **ESP32 board package** in Arduino IDE:
   `Tools → Board → Boards Manager → search "esp32" → install (by Espressif Systems)`
2. Select your board: `Tools → Board → ESP32S3 Dev Module`
3. Install two libraries via `Sketch → Include Library → Manage Libraries`:
   - **Adafruit GFX Library**
   - **Adafruit SSD1306**
4. Wire the components as shown above.
5. Open `ble_skimmer_scanner.ino`, update `trustedMacs[]` with your own phone's BLE MAC address so it doesn't trigger false alerts.
6. Upload, then open Serial Monitor at **115200 baud**.

---

## Features

- Passive, non-intrusive BLE scanning
- Real elapsed-time persistence tracking (not just scan-cycle counting)
- Weighted 0–100 risk scoring (persistence + signal strength)
- Automatic cleanup of devices no longer present
- Trusted device whitelisting
- Live OLED status display
- Local buzzer + LED alerting
- Self-hosted WiFi dashboard at `192.168.4.1` — no app or internet required
- Built-in serial test mode for instant demo without waiting 10 real minutes

---

## Testing

Type **`s`** into Serial Monitor and press Enter — this simulates a device that has already been present for the full persistence window, triggering the complete alert pathway (buzzer, LED, dashboard) instantly, without needing to wait or plant real test hardware.

---

## Limitations

- Detects **behavior**, not identity — a legitimate stationary BLE device (e.g. a smart speaker) can also trigger a high score.
- Only detects **BLE-based** skimmers — magnetic-stripe, deep-insert, WiFi, or cellular-exfiltration skimmers are outside its scope.
- Detection range is limited to a few meters (BLE's practical range).
- A proof-of-concept, not a certified/regulator-approved security product.

---

## Future Scope

- Cross-reference device names against known BLE skimmer chip signatures for identity-based confidence
- Persistent event logging (flash/SD card) with timestamps
- Remote alerts via Telegram/email
- Auto-whitelisting "learning mode" for permanently-present legitimate devices
- Extend the same persistence heuristic to WiFi-based skimmers

---

## License

This project is released under the MIT License — free to use, modify, and build upon.

---

*Built as a demonstration that meaningful wireless security tooling doesn't require expensive proprietary hardware.*
