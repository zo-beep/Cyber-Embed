# ⚡ Cyber-Embed // Tactical Wireless Security & RF Intelligence

[![Firmware Version](https://img.shields.io/badge/version-v1.3.0-00E5FF.svg?style=flat-square)](https://github.com/)
[![Target Hardware](https://img.shields.io/badge/target-LilyGO_T--Embed_CC1101_Plus-FFA000.svg?style=flat-square)](https://github.com/)
[![Based on](https://img.shields.io/badge/lineage-Bruce_Firmware-7C4DFF.svg?style=flat-square)](https://github.com/pr3y/Bruce)
[![License](https://img.shields.io/badge/license-GPLv3-00E676.svg?style=flat-square)](LICENSE)

**Cyber-Embed** is an advanced, passive wireless security assessment instrument and RF intelligence firmware engineered specifically for the **LilyGO T-Embed CC1101 Plus (ESP32-S3)** with its 320×170 display and rotary encoder.

---

## 🕊️ Tribute to Bruce

Cyber-Embed stands on the shoulders of giants. It is built directly upon the versatile, community-driven foundation of [**Bruce**](https://github.com/pr3y/Bruce) (created by [@pr3y](https://github.com/pr3y) and its incredible open-source contributors).

While Bruce created the ultimate multi-radio swiss-army knife for ESP32 devices (spanning CC1101 Sub-GHz, nRF24, Wi-Fi, BLE, RFID/NFC, IR, and BadUSB), **Cyber-Embed** extends this legacy into an analytical, passive security research instrument—transforming raw packet sweeps into structured baselines, chronological investigation sessions, and explainable security findings.

---

## 🚀 Key Features in Cyber-Embed

```text
       ANALYZER                    BASELINE                  INVESTIGATION
   (Live Telemetry)             (Stored Golden)            (Session Evidence)
          │                            │                           │
          ▼                            ▼                           ▼
  ┌────────────────────────────────────────────────────────────────────────┐
  │                 CENTRALIZED RISK ENGINE EVALUATION LAYER               │
  │                                                                        │
  │  • Unfamiliar AP / Device not in Baseline  -> MED / LOW                │
  │  • Insecure Open / Unencrypted Wi-Fi AP   -> MED                      │
  │  • High Proximity Signal (>= -45 dBm)     -> LOW                      │
  │  • Elevated Sub-GHz Carrier Shift         -> LOW                      │
  │  • 2.4GHz ISM Band Activity Shift         -> LOW                      │
  └────────────────────────────────────┬───────────────────────────────────┘
                                       │
                         Deduplication & Persistence
                                       │
                                       ▼
                       ┌──────────────────────────────┐
                       │  LittleFS: /findings.json    │
                       └──────────────┬───────────────┘
                                      │
                                      ▼
                        UI 2.0 TACTICAL FINDINGS HUD
```

1. Tactical Console UI 2.0
Purpose-Built for 320×170: Strict layout zoning (Header, Content Safe Area, Action Footer) designed for high information density and single-detent rotary encoder navigation.
Dynamic Text & Metrics: Zero text clipping or font squash—every label cleanly wraps, truncates, or formats with measured bounding boxes.
Dedicated Status Bar: Non-overlapping battery gauge, percentage readout, RTC time anchor, and peripheral status indicators.

2. Multi-Band Wireless Analyzer
Cross-Spectrum Observation: Simultaneous passive monitoring across 2.4GHz Wi-Fi (Ch 1–13), Bluetooth Low Energy, Sub-GHz (CC1101), and 2.4GHz ISM (nRF24).
Golden Baseline Profile: Capture a snapshot of your environment (/analyzer_baseline.json) and run real-time differential change detection (New APs, Disappeared Peripherals, RSSI Spikes).
Session Reports: Export plain-text audit summaries directly to LittleFS (/analyzer_report.txt).

3. Investigation Mode
Session Profiles: Rapidly launch audit scenarios (OFFICE_AUDIT, PERIMETER, FIELD_SCAN, SITE_ALPHA, SITE_BRAVO, LAB_TEST).
Live Evidence Recording: Chronological timeline of events ([NEW_AP], [DISAPPEAR], [RSSI_SPIKE], [RF_TRIGGER]).
Field Notes & Quick Tags: Tag critical checkpoints and observations without typing on a physical keyboard.
Standalone Flash Persistence: All sessions are indexed and preserved on LittleFS without requiring an SD card.

4. Findings & Risk Engine (v1.3.0)
Heuristic Threat Triaging: Translates observations into clear, explainable security alerts tagged by risk level (HIGH, MED, LOW, INFO).
Zero-Spam Deduplication: Intelligently hashes repeated events (category:target:title) so refreshes never clutter the HUD.
Analyst Workflow: Acknowledge findings (✓ ACK), add status tags (CONFIRMED, FALSE_POS, CHECK_PHYSICAL), and manage findings directly from the device.

5. Instant Intel Purge & Sanitization
One-click confirmation dialog to wipe active assessment sessions, baseline golden images, report files, and cached security findings cleanly from LittleFS.

Hardware Specification
Platform: LilyGO T-Embed CC1101 Plus
MCU: Espressif ESP32-S3 (Dual-core Xtensa LX7, 240 MHz, 8MB PSRAM, 16MB Flash)
Display: 1.9" ST7789V IPS LCD (320×170 px)
Input: Rotary Encoder + Center Push Button + Dedicated Back Key
Radios & Sensors:
Texas Instruments CC1101 (300–928 MHz Sub-GHz Transceiver)
Nordic Semiconductor nRF24L01+ (2.4 GHz Transceiver)
NXP PN532 (NFC / RFID 13.56 MHz)
Onboard 2.4GHz Wi-Fi (802.11 b/g/n) & BLE 5.0
Infrared Receiver & Transmitter
Microphone & Speaker (I2S)

📦 Building & Flashing (Open in Powershell)
Compiled with PlatformIO:

bash
# Clone the repository
git clone https://github.com/your-username/Cyber-Embed.git
cd Cyber-Embed
# Build the firmware
pio run -e lilygo-t-embed-cc1101
# Flash to device via USB
pio run -e lilygo-t-embed-cc1101 -t upload

⚖️ License & Acknowledgements
Base Firmware: Derived from Bruce under GPLv3.
Special Thanks: @pr3y and the Bruce developer community for their incredible hardware support and RF libraries.
Disclaimer: This tool is designed strictly for authorized security research, educational audits, and defensive RF monitoring.

