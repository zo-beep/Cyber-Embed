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
