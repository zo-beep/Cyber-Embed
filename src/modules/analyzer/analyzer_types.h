#ifndef __ANALYZER_TYPES_H__
#define __ANALYZER_TYPES_H__

#include <Arduino.h>
#include <vector>

enum WirelessDeviceType {
    DEV_TYPE_UNKNOWN = 0,
    DEV_TYPE_WIFI_AP,
    DEV_TYPE_WIFI_CLIENT,
    DEV_TYPE_BLE
};

struct WirelessDevice {
    WirelessDeviceType type = DEV_TYPE_UNKNOWN;
    String name;
    String identifier; // MAC / BSSID
    int8_t rssi = -128;
    uint8_t channel = 0;
    String extra;      // Security / UUID / Manufacturer
    uint32_t firstSeen = 0; // millis timestamp
    uint32_t lastSeen = 0;
    bool isNew = false;
};

struct SubsystemStatus {
    bool detected = false;
    String state = "IDLE"; // "READY", "ACTIVE", "IDLE", "SCANNING", "NOT FOUND"
    int count = 0;
    int8_t peakRssi = -128;
    String info1; // Top device / active info
    String info2; // Channel / Frequency info
};

struct BaselineSnapshot {
    uint32_t timestamp = 0;
    std::vector<String> knownIdentifiers;
    std::vector<WirelessDevice> devices;
    SubsystemStatus subghz;
    SubsystemStatus nrf24;
    int wifiCount = 0;
    int bleCount = 0;
    bool isSet = false;
};

struct AnalyzerSnapshot {
    uint32_t timestamp = 0;
    uint32_t scanDurationMs = 0;
    SubsystemStatus wifi;
    SubsystemStatus ble;
    SubsystemStatus subghz;
    SubsystemStatus nrf24;
    std::vector<WirelessDevice> devices;
    uint8_t wifiChannelDist[14] = {0}; // Channels 1-13
    int8_t activityHistory[32] = {0};  // Rolling RF/activity levels (-128 to 0 dBm or level 0-100)
    int newWifiCount = 0;
    int newBleCount = 0;
};

// Change detection types
enum ChangeType {
    CHANGE_NONE = 0,
    CHANGE_NEW,       // + NEW
    CHANGE_GONE,      // - GONE
    CHANGE_MODIFIED   // ~ CHANGED (RSSI delta, channel shift, etc.)
};

struct DeviceChange {
    WirelessDeviceType type = DEV_TYPE_UNKNOWN;
    ChangeType changeType = CHANGE_NONE;
    String name;
    String identifier;
    int8_t baselineRssi = -128;
    int8_t currentRssi = -128;
    uint8_t channel = 0;
    String details;
};

struct SubsystemChange {
    String name;          // "Sub-GHz", "nRF24"
    bool hasChanged = false;
    String baselineState;
    String currentState;
    String description;   // e.g. "Elevated (+16 dBm)", "Active Carriers", "No Change"
};

struct ComparisonResult {
    uint32_t compareTime = 0;
    std::vector<DeviceChange> wifiChanges;
    std::vector<DeviceChange> bleChanges;
    SubsystemChange subghzChange;
    SubsystemChange nrf24Change;

    int wifiNewCount = 0;
    int wifiGoneCount = 0;
    int wifiModCount = 0;

    int bleNewCount = 0;
    int bleGoneCount = 0;
    int bleModCount = 0;

    int totalWifiChanges() const { return (int)wifiChanges.size(); }
    int totalBleChanges() const { return (int)bleChanges.size(); }
    bool hasAnyChanges() const {
        return !wifiChanges.empty() || !bleChanges.empty() || subghzChange.hasChanged || nrf24Change.hasChanged;
    }
};

#endif // __ANALYZER_TYPES_H__
