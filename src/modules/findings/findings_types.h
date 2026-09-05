#ifndef __FINDINGS_TYPES_H__
#define __FINDINGS_TYPES_H__

#include <Arduino.h>
#include <vector>

enum RiskLevel {
    RISK_INFO = 0,
    RISK_LOW  = 1,
    RISK_MED  = 2,
    RISK_HIGH = 3
};

enum FindingCategory {
    FINDING_CAT_WIFI = 0,
    FINDING_CAT_BLE,
    FINDING_CAT_SUBGHZ,
    FINDING_CAT_NRF24,
    FINDING_CAT_SYSTEM
};

struct SecurityFinding {
    String id;                 // e.g. "F-001"
    String dedupeKey;          // Unique hash/key to prevent duplicates (e.g. "WIFI:AA:BB:CC:NEW")
    RiskLevel risk = RISK_INFO;
    FindingCategory category = FINDING_CAT_WIFI;
    String title;              // e.g. "Unknown AP", "New BLE Device", "Insecure Open AP"
    String description;        // Summary explanation
    uint32_t timestampMs = 0;  // Milliseconds or uptime timestamp
    String source;             // e.g. "Investigation: SITE_ALPHA", "Wireless Analyzer"
    String target;             // SSID / BLE Name / Frequency / Identifier
    int rssi = 0;              // Signal strength if applicable
    String evidence;           // e.g. "AP was not present in the attached baseline."
    bool acknowledged = false;
    String note;               // Optional analyst annotation
};

#endif // __FINDINGS_TYPES_H__
