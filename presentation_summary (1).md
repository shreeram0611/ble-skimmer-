# Bluetooth Skimmer / Rogue Device Scanner
### A Cybersecurity IoT Project — Project Summary

## Abstract
Bluetooth-based card skimmers are hidden devices planted on ATMs and
payment terminals that steal card data and transmit it over Bluetooth
to a nearby receiver. This project builds a low-cost handheld/desk
scanner that detects the behavioral signature of such a hidden device
— a signal that stays present, strong, and unmoving over time — and
raises an audible and visual alert, without connecting to or
extracting data from any device.

## Problem statement
There is no simple, affordable way for an ordinary person or small
shop owner to check whether a card reader has a hidden Bluetooth
skimmer attached to it before using it.

## Objective
Build a portable ESP32-based scanner that passively monitors nearby
Bluetooth Low Energy (BLE) advertisements and flags devices exhibiting
skimmer-like behavior: strong signal strength sustained across many
consecutive scan cycles, indicating a stationary hidden transmitter
rather than a passing phone or wearable.

## System overview
| Layer | Role |
|---|---|
| ESP32 DevKit | Runs BLE scan cycles and detection logic |
| 0.96" OLED display | Live readout: device count, tracked count, alert status |
| Buzzer + LED | Audible/visual alarm on detection |
| Trusted device whitelist | Prevents false alarms from the user's own phone/earbuds |

## How it works
1. The ESP32 repeatedly scans for BLE advertisement broadcasts (every ~5 seconds).
2. Every unfamiliar device's MAC address, signal strength (RSSI), and
   how many consecutive cycles it has been seen in are tracked.
3. A device is flagged as suspicious only if it meets **both**
   conditions: strong signal (close proximity) **and** persistent
   presence across multiple cycles — the pattern of something fixed
   in place, not someone walking by.
4. On detection, the buzzer sounds, the LED flashes, and the OLED
   switches from "Monitoring..." to "ALERT: possible skimmer."

## Bill of materials

| Component | Qty | Approx cost |
|---|---|---|
| ESP32 DevKit | 1 | ₹400 |
| 0.96" I2C OLED (SSD1306) | 1 | ₹120 |
| Active buzzer | 1 | ₹15 |
| LED + 220Ω resistor | 1 | ₹10 |
| Breadboard + jumper wires | 1 set | ₹60 |
| **Total** | | **~₹605** |

## Applications
- Personal safety check before using unfamiliar ATMs or card machines
- Small retail/shop owners periodically checking their own terminals
- General awareness/education tool about wireless-based fraud techniques

## Advantages
- Low cost, single board, no internet/cloud dependency
- Doesn't rely on knowing a specific skimmer's signature — detects the
  *behavior pattern* instead, so it generalizes to unknown devices
- Live on-device feedback via OLED, no phone app or laptop required to operate

## Limitations
- Heuristic-based, not certainty-based: a legitimate stationary device
  (smart TV, fixed BLE beacon) can trigger a false positive
- Only detects Bluetooth-based skimmers, not magnetic-stripe or purely
  hardware-based skimmers with no wireless component
- Detection thresholds (signal strength, persistence duration) may
  need tuning for different environments

## Future scope
- Combine with a database of known skimmer BLE service signatures for
  higher-confidence detection
- Add a mobile app companion for logging and mapping detected devices
  over time
- Extend detection to WiFi-based skimmers using the same persistence
  heuristic

## Conclusion
This project demonstrates that meaningful, low-cost wireless security
tooling doesn't require expensive commercial hardware — a single
₹600 microcontroller board, applying the right behavioral heuristic
to publicly broadcast BLE data, can meaningfully raise awareness of a
real, underserved fraud risk.
