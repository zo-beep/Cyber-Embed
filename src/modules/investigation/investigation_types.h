#ifndef __INVESTIGATION_TYPES_H__
#define __INVESTIGATION_TYPES_H__

#include <Arduino.h>
#include <vector>
#include "modules/analyzer/analyzer_types.h"

enum InvestigationEventType {
    EVENT_TYPE_SESSION_START = 0,
    EVENT_TYPE_SCAN_SUMMARY,
    EVENT_TYPE_DEVICE_NEW,
    EVENT_TYPE_DEVICE_GONE,
    EVENT_TYPE_DEVICE_MODIFIED,
    EVENT_TYPE_SUBGHZ_SHIFT,
    EVENT_TYPE_NRF24_SHIFT,
    EVENT_TYPE_BASELINE_DIFF,
    EVENT_TYPE_NOTE,
    EVENT_TYPE_SESSION_END
};

struct InvestigationEvent {
    uint32_t id = 0;
    uint32_t timestampMs = 0; // Milliseconds elapsed since session start
    InvestigationEventType type = EVENT_TYPE_NOTE;
    String title;             // Short headline: "Wi-Fi Discovered", "BLE Beacon", "RF Activity Shift"
    String summary;           // e.g. "HomeNet_5G CH6 -54dBm"
    String details;           // Full metadata: BSSID, Encryption, RSSI, Frequency, etc.
};

struct InvestigationNote {
    uint32_t id = 0;
    uint32_t timestampMs = 0;
    String text;
};

struct InvestigationSession {
    String id;                 // e.g. "INV_001"
    String name;               // e.g. "SITE_A", "LAB_TEST"
    uint32_t startTime = 0;    // millis() timestamp when started
    uint32_t durationMs = 0;   // total elapsed duration
    bool isActive = false;     // true if session is live
    bool hasBaseline = false;
    String baselineName;

    // Statistics
    int totalWifiObserved = 0;
    int totalBleObserved = 0;
    int totalEvents = 0;
    int totalNotes = 0;

    // Recorded observations (unique devices seen during this session)
    std::vector<WirelessDevice> observedDevices;

    // Chronological event log
    std::vector<InvestigationEvent> events;

    // User session notes
    std::vector<InvestigationNote> notes;

    // Last recorded subsystem states
    SubsystemStatus lastSubghz;
    SubsystemStatus lastNrf24;
};

struct InvestigationSummary {
    String id;
    String name;
    uint32_t startTime = 0;
    uint32_t durationMs = 0;
    int totalEvents = 0;
    int wifiCount = 0;
    int bleCount = 0;
    int notesCount = 0;
    String filename;
};

#endif // __INVESTIGATION_TYPES_H__
