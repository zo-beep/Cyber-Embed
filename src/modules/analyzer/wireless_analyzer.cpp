#include "wireless_analyzer.h"
#include "core/display.h"
#include "core/radio_mem.h"
#include "core/settings.h"
#include "core/utils.h"
#include "core/wifi/wifi_common.h"
#include "modules/NRF24/nrf_common.h"
#include "modules/ble/ble_common.h"
#include "modules/rf/rf_utils.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <NimBLEDevice.h>
#include <WiFi.h>

WirelessAnalyzer& wirelessAnalyzer = WirelessAnalyzer::getInstance();

WirelessAnalyzer::WirelessAnalyzer() {
    _latestSnapshot.timestamp = millis();
    for (int i = 0; i < 32; i++) {
        _latestSnapshot.activityHistory[i] = -95;
    }
    loadBaselineFromLittleFS();
}

String WirelessAnalyzer::getAuthModeStr(uint8_t authMode) {
    switch (authMode) {
        case WIFI_AUTH_OPEN:            return "OPEN";
        case WIFI_AUTH_WEP:             return "WEP";
        case WIFI_AUTH_WPA_PSK:         return "WPA";
        case WIFI_AUTH_WPA2_PSK:        return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENT";
        case WIFI_AUTH_WPA3_PSK:        return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3";
        case WIFI_AUTH_WAPI_PSK:        return "WAPI";
        case WIFI_AUTH_OWE:             return "OWE";
        default:                        return "UNKNOWN";
    }
}

String WirelessAnalyzer::getRelativeTimeStr(uint32_t pastMillis) {
    if (pastMillis == 0) return "Just now";
    uint32_t diff = (millis() - pastMillis) / 1000;
    if (diff < 5) return "Just now";
    if (diff < 60) return String(diff) + "s ago";
    uint32_t mins = diff / 60;
    if (mins < 60) return String(mins) + "m ago";
    uint32_t hours = mins / 60;
    return String(hours) + "h ago";
}

void WirelessAnalyzer::updateActivityHistory(int8_t level) {
    for (int i = 0; i < 31; i++) {
        _latestSnapshot.activityHistory[i] = _latestSnapshot.activityHistory[i + 1];
    }
    _latestSnapshot.activityHistory[31] = level;
}

void WirelessAnalyzer::performWifiScan() {
    _latestSnapshot.wifi.state = "SCANNING";
    _latestSnapshot.wifi.count = 0;
    _latestSnapshot.wifi.peakRssi = -128;
    _latestSnapshot.wifi.info1 = "Scanning...";
    _latestSnapshot.wifi.info2 = "";
    memset(_latestSnapshot.wifiChannelDist, 0, sizeof(_latestSnapshot.wifiChannelDist));

    // Remove old Wi-Fi AP devices while keeping BLE devices
    std::vector<WirelessDevice> updatedDevices;
    for (const auto& dev : _latestSnapshot.devices) {
        if (dev.type != DEV_TYPE_WIFI_AP) {
            updatedDevices.push_back(dev);
        }
    }
    _latestSnapshot.devices = updatedDevices;

    int numNets = WiFi.scanNetworks(false, true); // Scan including hidden
    if (numNets < 0) {
        _latestSnapshot.wifi.detected = false;
        _latestSnapshot.wifi.state = "SCAN ERR";
        _latestSnapshot.wifi.info1 = "Scan Failed";
        return;
    }

    _latestSnapshot.wifi.detected = true;
    _latestSnapshot.wifi.count = numNets;
    _latestSnapshot.wifi.state = (numNets > 0) ? "ACTIVE" : "IDLE";

    String strongestSsid = "";
    int8_t peakRssi = -128;
    uint8_t dominantChannel = 0;
    uint8_t maxChCount = 0;

    for (int i = 0; i < numNets; i++) {
        WirelessDevice dev;
        dev.type = DEV_TYPE_WIFI_AP;
        dev.name = WiFi.SSID(i);
        if (dev.name.length() == 0) dev.name = "<Hidden SSID>";
        dev.identifier = WiFi.BSSIDstr(i);
        dev.rssi = WiFi.RSSI(i);
        dev.channel = WiFi.channel(i);
        dev.extra = getAuthModeStr(WiFi.encryptionType(i));
        dev.firstSeen = millis();
        dev.lastSeen = millis();

        if (dev.channel >= 1 && dev.channel <= 13) {
            _latestSnapshot.wifiChannelDist[dev.channel]++;
            if (_latestSnapshot.wifiChannelDist[dev.channel] > maxChCount) {
                maxChCount = _latestSnapshot.wifiChannelDist[dev.channel];
                dominantChannel = dev.channel;
            }
        }

        if (dev.rssi > peakRssi) {
            peakRssi = dev.rssi;
            strongestSsid = dev.name;
        }

        _latestSnapshot.devices.push_back(dev);
    }

    _latestSnapshot.wifi.peakRssi = (numNets > 0) ? peakRssi : -128;
    if (numNets > 0) {
        _latestSnapshot.wifi.info1 = String(numNets) + " APs (Top: " + strongestSsid.substring(0, 10) + ")";
        _latestSnapshot.wifi.info2 = "Peak CH " + String(dominantChannel) + " (" + String(peakRssi) + "dBm)";
        updateActivityHistory(peakRssi);
    } else {
        _latestSnapshot.wifi.info1 = "0 Networks Found";
        _latestSnapshot.wifi.info2 = "CH 1-13 Clean";
    }

    WiFi.scanDelete();
}

void WirelessAnalyzer::performBleScan() {
    _latestSnapshot.ble.state = "SCANNING";
    _latestSnapshot.ble.count = 0;
    _latestSnapshot.ble.peakRssi = -128;
    _latestSnapshot.ble.info1 = "Scanning...";
    _latestSnapshot.ble.info2 = "";

    // Remove old BLE devices while keeping Wi-Fi devices
    std::vector<WirelessDevice> updatedDevices;
    for (const auto& dev : _latestSnapshot.devices) {
        if (dev.type != DEV_TYPE_BLE) {
            updatedDevices.push_back(dev);
        }
    }
    _latestSnapshot.devices = updatedDevices;

    if (!radioHasMemForBle()) {
        _latestSnapshot.ble.detected = false;
        _latestSnapshot.ble.state = "LOW RAM";
        _latestSnapshot.ble.info1 = "RAM Limit";
        return;
    }

    bool wasBleActive = BLEConnected || (BLEDevice::getServer() != nullptr);

    if (!ble_scan_setup() || pBLEScan == nullptr) {
        _latestSnapshot.ble.detected = false;
        _latestSnapshot.ble.state = "INIT ERR";
        _latestSnapshot.ble.info1 = "BLE Init Failed";
        return;
    }

    pBLEScan->clearResults();

    int8_t peakRssi = -128;
    String strongestName = "";
    int devCount = 0;

    try {
        BLEScanResults foundDevices = pBLEScan->getResults(2000, false); // 2-second scan
        devCount = foundDevices.getCount();
        int maxToRead = min(devCount, 60);

        for (int i = 0; i < maxToRead; i++) {
            const NimBLEAdvertisedDevice *advDev = foundDevices.getDevice(i);
            if (!advDev) continue;

            WirelessDevice dev;
            dev.type = DEV_TYPE_BLE;
            dev.name = advDev->getName().c_str();
            if (dev.name.length() == 0) dev.name = "<Unknown BLE>";
            dev.identifier = advDev->getAddress().toString().c_str();
            dev.rssi = advDev->getRSSI();
            dev.channel = 0;
            dev.extra = "BLE Adv";
            if (advDev->haveManufacturerData()) {
                dev.extra = "MfgData";
            } else if (advDev->haveServiceUUID()) {
                dev.extra = "ServiceUUID";
            }
            dev.firstSeen = millis();
            dev.lastSeen = millis();

            if (dev.rssi > peakRssi) {
                peakRssi = dev.rssi;
                strongestName = dev.name;
            }

            _latestSnapshot.devices.push_back(dev);
        }
    } catch (...) {
        _latestSnapshot.ble.detected = false;
        _latestSnapshot.ble.state = "SCAN ERR";
        _latestSnapshot.ble.info1 = "Scan Exception";
    }

    if (pBLEScan) {
        pBLEScan->stop();
        pBLEScan->clearResults();
    }

    if (!wasBleActive) {
        stopBLEStack();
    }

    _latestSnapshot.ble.detected = true;
    _latestSnapshot.ble.count = devCount;
    _latestSnapshot.ble.state = (devCount > 0) ? "ACTIVE" : "IDLE";
    _latestSnapshot.ble.peakRssi = (devCount > 0) ? peakRssi : -128;

    if (devCount > 0) {
        _latestSnapshot.ble.info1 = String(devCount) + " Devs (Top: " + strongestName.substring(0, 10) + ")";
        _latestSnapshot.ble.info2 = "Signal: " + String(peakRssi) + " dBm";
        updateActivityHistory(peakRssi);
    } else {
        _latestSnapshot.ble.info1 = "0 Devices Found";
        _latestSnapshot.ble.info2 = "No Adv Signals";
    }
}

void WirelessAnalyzer::sampleSubGhz() {
    if (bruceConfigPins.rfModule == CC1101_SPI_MODULE) {
        if (initRfModule("rx", bruceConfigPins.rfFreq)) {
            if (ELECHOUSE_cc1101.getCC1101()) {
                _latestSnapshot.subghz.detected = true;
                vTaskDelay(pdMS_TO_TICKS(10));
                int rssi = ELECHOUSE_cc1101.getRssi();
                _latestSnapshot.subghz.peakRssi = rssi;
                _latestSnapshot.subghz.state = (rssi > -80) ? "ACTIVE" : "READY";
                _latestSnapshot.subghz.info1 = String(bruceConfigPins.rfFreq, 2) + " MHz";
                _latestSnapshot.subghz.info2 = String(rssi) + " dBm";
                updateActivityHistory(rssi);
            } else {
                _latestSnapshot.subghz.detected = false;
                _latestSnapshot.subghz.state = "NOT FOUND";
                _latestSnapshot.subghz.info1 = "CC1101 Offline";
                _latestSnapshot.subghz.info2 = "--";
            }
            deinitRfModule();
        } else {
            _latestSnapshot.subghz.detected = false;
            _latestSnapshot.subghz.state = "INIT ERR";
            _latestSnapshot.subghz.info1 = "SPI Bus Error";
            _latestSnapshot.subghz.info2 = "--";
        }
    } else {
        // Single-pin module / GPIO
        _latestSnapshot.subghz.detected = true;
        _latestSnapshot.subghz.state = "GPIO RX";
        _latestSnapshot.subghz.info1 = String(bruceConfigPins.rfFreq, 2) + " MHz";
        _latestSnapshot.subghz.info2 = "Pin " + String(bruceConfigPins.rfRx);
    }
}

void WirelessAnalyzer::sampleNrf24() {
    if (nrf_start(NRF_MODE_SPI)) {
        _latestSnapshot.nrf24.detected = true;
        NRFradio.setAutoAck(false);
        NRFradio.disableCRC();
        NRFradio.setAddressWidth(2);

        int activeCh = 0;
        for (int ch = 0; ch < 80; ch += 8) {
            NRFradio.setChannel(ch);
            NRFradio.startListening();
            delayMicroseconds(120);
            NRFradio.stopListening();
            if (NRFradio.testRPD()) {
                activeCh++;
            }
        }

        _latestSnapshot.nrf24.count = activeCh;
        _latestSnapshot.nrf24.state = (activeCh > 0) ? "ACTIVE" : "READY";
        _latestSnapshot.nrf24.info1 = "2.4 GHz Band";
        _latestSnapshot.nrf24.info2 = (activeCh > 0) ? (String(activeCh) + " Act Channels") : "Carrier Idle";

        NRFradio.powerDown();
    } else {
        _latestSnapshot.nrf24.detected = false;
        _latestSnapshot.nrf24.state = "NOT FOUND";
        _latestSnapshot.nrf24.count = 0;
        _latestSnapshot.nrf24.info1 = "No Module";
        _latestSnapshot.nrf24.info2 = "--";
    }
}

void WirelessAnalyzer::performUnifiedScan(std::function<void(const String& stage, int progressPercent)> onProgress) {
    uint32_t startTime = millis();

    if (onProgress) onProgress("Scanning Wi-Fi 802.11...", 15);
    performWifiScan();

    if (onProgress) onProgress("Scanning Bluetooth LE...", 50);
    performBleScan();

    if (onProgress) onProgress("Sampling Sub-GHz RF...", 80);
    sampleSubGhz();

    if (onProgress) onProgress("Checking nRF24 Band...", 95);
    sampleNrf24();

    _latestSnapshot.timestamp = millis();
    _latestSnapshot.scanDurationMs = millis() - startTime;

    // Baseline diff analysis
    _latestSnapshot.newWifiCount = 0;
    _latestSnapshot.newBleCount = 0;

    if (_baseline.isSet) {
        for (auto& dev : _latestSnapshot.devices) {
            bool foundInBaseline = false;
            for (const auto& baseId : _baseline.knownIdentifiers) {
                if (dev.identifier.equalsIgnoreCase(baseId)) {
                    foundInBaseline = true;
                    break;
                }
            }
            dev.isNew = !foundInBaseline;
            if (dev.isNew) {
                if (dev.type == DEV_TYPE_WIFI_AP) _latestSnapshot.newWifiCount++;
                else if (dev.type == DEV_TYPE_BLE) _latestSnapshot.newBleCount++;
            }
        }
    } else {
        for (auto& dev : _latestSnapshot.devices) {
            dev.isNew = false;
        }
    }

    if (onProgress) onProgress("Scan Complete", 100);
}

void WirelessAnalyzer::setBaseline() {
    _baseline.knownIdentifiers.clear();
    _baseline.devices = _latestSnapshot.devices;
    _baseline.subghz = _latestSnapshot.subghz;
    _baseline.nrf24 = _latestSnapshot.nrf24;
    _baseline.wifiCount = _latestSnapshot.wifi.count;
    _baseline.bleCount = _latestSnapshot.ble.count;
    _baseline.timestamp = millis();
    _baseline.isSet = true;

    for (const auto& dev : _latestSnapshot.devices) {
        _baseline.knownIdentifiers.push_back(dev.identifier);
    }

    // Reset isNew flags on current snapshot
    for (auto& dev : _latestSnapshot.devices) {
        dev.isNew = false;
    }
    _latestSnapshot.newWifiCount = 0;
    _latestSnapshot.newBleCount = 0;

    saveBaselineToLittleFS();
}

void WirelessAnalyzer::clearBaseline() {
    _baseline.knownIdentifiers.clear();
    _baseline.devices.clear();
    _baseline.subghz = SubsystemStatus();
    _baseline.nrf24 = SubsystemStatus();
    _baseline.wifiCount = 0;
    _baseline.bleCount = 0;
    _baseline.timestamp = 0;
    _baseline.isSet = false;

    for (auto& dev : _latestSnapshot.devices) {
        dev.isNew = false;
    }
    _latestSnapshot.newWifiCount = 0;
    _latestSnapshot.newBleCount = 0;

    deleteBaselineFromLittleFS();
}

bool WirelessAnalyzer::saveBaselineToLittleFS(const String& path) {
    if (!_baseline.isSet) return false;
    File file = LittleFS.open(path.c_str(), "w");
    if (!file) return false;

    JsonDocument doc;
    doc["timestamp"] = _baseline.timestamp;
    doc["wifiCount"] = _baseline.wifiCount;
    doc["bleCount"] = _baseline.bleCount;

    JsonObject subObj = doc["subghz"].to<JsonObject>();
    subObj["detected"] = _baseline.subghz.detected;
    subObj["state"] = _baseline.subghz.state;
    subObj["peakRssi"] = _baseline.subghz.peakRssi;
    subObj["info1"] = _baseline.subghz.info1;
    subObj["info2"] = _baseline.subghz.info2;

    JsonObject nrfObj = doc["nrf24"].to<JsonObject>();
    nrfObj["detected"] = _baseline.nrf24.detected;
    nrfObj["state"] = _baseline.nrf24.state;
    nrfObj["count"] = _baseline.nrf24.count;
    nrfObj["info1"] = _baseline.nrf24.info1;
    nrfObj["info2"] = _baseline.nrf24.info2;

    JsonArray devArr = doc["devices"].to<JsonArray>();
    for (const auto& dev : _baseline.devices) {
        JsonObject dObj = devArr.add<JsonObject>();
        dObj["t"] = (int)dev.type;
        dObj["n"] = dev.name;
        dObj["id"] = dev.identifier;
        dObj["r"] = dev.rssi;
        dObj["ch"] = dev.channel;
        dObj["ex"] = dev.extra;
    }

    serializeJson(doc, file);
    file.close();
    return true;
}

bool WirelessAnalyzer::loadBaselineFromLittleFS(const String& path) {
    if (!LittleFS.exists(path.c_str())) return false;
    File file = LittleFS.open(path.c_str(), "r");
    if (!file) return false;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) return false;

    _baseline.timestamp = doc["timestamp"] | 0;
    _baseline.wifiCount = doc["wifiCount"] | 0;
    _baseline.bleCount = doc["bleCount"] | 0;

    JsonObject subObj = doc["subghz"];
    if (!subObj.isNull()) {
        _baseline.subghz.detected = subObj["detected"] | false;
        _baseline.subghz.state = (const char*)(subObj["state"] | "IDLE");
        _baseline.subghz.peakRssi = subObj["peakRssi"] | -128;
        _baseline.subghz.info1 = (const char*)(subObj["info1"] | "");
        _baseline.subghz.info2 = (const char*)(subObj["info2"] | "");
    }

    JsonObject nrfObj = doc["nrf24"];
    if (!nrfObj.isNull()) {
        _baseline.nrf24.detected = nrfObj["detected"] | false;
        _baseline.nrf24.state = (const char*)(nrfObj["state"] | "IDLE");
        _baseline.nrf24.count = nrfObj["count"] | 0;
        _baseline.nrf24.info1 = (const char*)(nrfObj["info1"] | "");
        _baseline.nrf24.info2 = (const char*)(nrfObj["info2"] | "");
    }

    _baseline.devices.clear();
    _baseline.knownIdentifiers.clear();
    JsonArray devArr = doc["devices"].as<JsonArray>();
    for (JsonObject dObj : devArr) {
        WirelessDevice dev;
        dev.type = (WirelessDeviceType)(dObj["t"] | 0);
        dev.name = (const char*)(dObj["n"] | "");
        dev.identifier = (const char*)(dObj["id"] | "");
        dev.rssi = dObj["r"] | -128;
        dev.channel = dObj["ch"] | 0;
        dev.extra = (const char*)(dObj["ex"] | "");
        _baseline.devices.push_back(dev);
        _baseline.knownIdentifiers.push_back(dev.identifier);
    }

    _baseline.isSet = true;
    return true;
}

bool WirelessAnalyzer::deleteBaselineFromLittleFS(const String& path) {
    if (LittleFS.exists(path.c_str())) {
        return LittleFS.remove(path.c_str());
    }
    return true;
}

ComparisonResult WirelessAnalyzer::compareWithBaseline() const {
    ComparisonResult result;
    result.compareTime = millis();

    if (!_baseline.isSet) return result;

    // 1. Wi-Fi Comparison
    for (const auto& currDev : _latestSnapshot.devices) {
        if (currDev.type != DEV_TYPE_WIFI_AP) continue;

        const WirelessDevice* matchedBase = nullptr;
        for (const auto& baseDev : _baseline.devices) {
            if (baseDev.type == DEV_TYPE_WIFI_AP && currDev.identifier.equalsIgnoreCase(baseDev.identifier)) {
                matchedBase = &baseDev;
                break;
            }
        }

        if (!matchedBase) {
            DeviceChange chg;
            chg.type = DEV_TYPE_WIFI_AP;
            chg.changeType = CHANGE_NEW;
            chg.name = currDev.name;
            chg.identifier = currDev.identifier;
            chg.currentRssi = currDev.rssi;
            chg.channel = currDev.channel;
            chg.details = "CH" + String(currDev.channel) + " " + String(currDev.rssi) + "dBm";
            result.wifiChanges.push_back(chg);
            result.wifiNewCount++;
        } else {
            int rssiDiff = abs((int)currDev.rssi - (int)matchedBase->rssi);
            bool chDiff = (currDev.channel != matchedBase->channel);
            if (rssiDiff >= 15 || chDiff) {
                DeviceChange chg;
                chg.type = DEV_TYPE_WIFI_AP;
                chg.changeType = CHANGE_MODIFIED;
                chg.name = currDev.name;
                chg.identifier = currDev.identifier;
                chg.baselineRssi = matchedBase->rssi;
                chg.currentRssi = currDev.rssi;
                chg.channel = currDev.channel;
                chg.details = String(currDev.rssi) + "dBm (was " + String(matchedBase->rssi) + "dBm)";
                if (chDiff) chg.details += " CH" + String(currDev.channel) + " (was " + String(matchedBase->channel) + ")";
                result.wifiChanges.push_back(chg);
                result.wifiModCount++;
            }
        }
    }

    for (const auto& baseDev : _baseline.devices) {
        if (baseDev.type != DEV_TYPE_WIFI_AP) continue;

        bool foundInCurrent = false;
        for (const auto& currDev : _latestSnapshot.devices) {
            if (currDev.type == DEV_TYPE_WIFI_AP && baseDev.identifier.equalsIgnoreCase(currDev.identifier)) {
                foundInCurrent = true;
                break;
            }
        }

        if (!foundInCurrent) {
            DeviceChange chg;
            chg.type = DEV_TYPE_WIFI_AP;
            chg.changeType = CHANGE_GONE;
            chg.name = baseDev.name;
            chg.identifier = baseDev.identifier;
            chg.baselineRssi = baseDev.rssi;
            chg.channel = baseDev.channel;
            chg.details = "Last CH" + String(baseDev.channel) + " " + String(baseDev.rssi) + "dBm";
            result.wifiChanges.push_back(chg);
            result.wifiGoneCount++;
        }
    }

    // 2. BLE Comparison
    for (const auto& currDev : _latestSnapshot.devices) {
        if (currDev.type != DEV_TYPE_BLE) continue;

        const WirelessDevice* matchedBase = nullptr;
        for (const auto& baseDev : _baseline.devices) {
            if (baseDev.type == DEV_TYPE_BLE && currDev.identifier.equalsIgnoreCase(baseDev.identifier)) {
                matchedBase = &baseDev;
                break;
            }
        }

        if (!matchedBase) {
            DeviceChange chg;
            chg.type = DEV_TYPE_BLE;
            chg.changeType = CHANGE_NEW;
            chg.name = currDev.name;
            chg.identifier = currDev.identifier;
            chg.currentRssi = currDev.rssi;
            chg.details = String(currDev.rssi) + "dBm " + currDev.extra;
            result.bleChanges.push_back(chg);
            result.bleNewCount++;
        } else {
            int rssiDiff = abs((int)currDev.rssi - (int)matchedBase->rssi);
            if (rssiDiff >= 15) {
                DeviceChange chg;
                chg.type = DEV_TYPE_BLE;
                chg.changeType = CHANGE_MODIFIED;
                chg.name = currDev.name;
                chg.identifier = currDev.identifier;
                chg.baselineRssi = matchedBase->rssi;
                chg.currentRssi = currDev.rssi;
                chg.details = String(currDev.rssi) + "dBm (was " + String(matchedBase->rssi) + "dBm)";
                result.bleChanges.push_back(chg);
                result.bleModCount++;
            }
        }
    }

    for (const auto& baseDev : _baseline.devices) {
        if (baseDev.type != DEV_TYPE_BLE) continue;

        bool foundInCurrent = false;
        for (const auto& currDev : _latestSnapshot.devices) {
            if (currDev.type == DEV_TYPE_BLE && baseDev.identifier.equalsIgnoreCase(currDev.identifier)) {
                foundInCurrent = true;
                break;
            }
        }

        if (!foundInCurrent) {
            DeviceChange chg;
            chg.type = DEV_TYPE_BLE;
            chg.changeType = CHANGE_GONE;
            chg.name = baseDev.name;
            chg.identifier = baseDev.identifier;
            chg.baselineRssi = baseDev.rssi;
            chg.details = "Last signal: " + String(baseDev.rssi) + "dBm";
            result.bleChanges.push_back(chg);
            result.bleGoneCount++;
        }
    }

    // 3. Sub-GHz Comparison
    result.subghzChange.name = "Sub-GHz RF";
    result.subghzChange.baselineState = _baseline.subghz.info1 + " (" + _baseline.subghz.info2 + ")";
    result.subghzChange.currentState = _latestSnapshot.subghz.info1 + " (" + _latestSnapshot.subghz.info2 + ")";
    if (_baseline.subghz.detected && _latestSnapshot.subghz.detected) {
        int rssiDelta = (int)_latestSnapshot.subghz.peakRssi - (int)_baseline.subghz.peakRssi;
        if (abs(rssiDelta) >= 10 || (_latestSnapshot.subghz.state != _baseline.subghz.state)) {
            result.subghzChange.hasChanged = true;
            result.subghzChange.description = (rssiDelta > 0) ? 
                ("Elevated (+" + String(rssiDelta) + " dBm)") : 
                ("Activity Shift (" + String(rssiDelta) + " dBm)");
        } else {
            result.subghzChange.hasChanged = false;
            result.subghzChange.description = "Normal (No Change)";
        }
    } else {
        result.subghzChange.hasChanged = false;
        result.subghzChange.description = "No Module / Inactive";
    }

    // 4. nRF24 Comparison
    result.nrf24Change.name = "nRF24 2.4G";
    result.nrf24Change.baselineState = _baseline.nrf24.info2;
    result.nrf24Change.currentState = _latestSnapshot.nrf24.info2;
    if (_baseline.nrf24.detected && _latestSnapshot.nrf24.detected) {
        int chDelta = _latestSnapshot.nrf24.count - _baseline.nrf24.count;
        if (chDelta != 0 || (_latestSnapshot.nrf24.state != _baseline.nrf24.state)) {
            result.nrf24Change.hasChanged = true;
            result.nrf24Change.description = (chDelta > 0) ? 
                ("+" + String(chDelta) + " Active Channels") : 
                (String(chDelta) + " Active Channels");
        } else {
            result.nrf24Change.hasChanged = false;
            result.nrf24Change.description = "Normal (No Change)";
        }
    } else {
        result.nrf24Change.hasChanged = false;
        result.nrf24Change.description = "No Module / Inactive";
    }

    return result;
}

String WirelessAnalyzer::getSessionReportText() const {
    String report = "=== CYBER-EMBED WIRELESS REPORT ===\n";
    report += "Timestamp: " + String(millis() / 1000) + "s uptime\n";
    report += "Scan Duration: " + String(_latestSnapshot.scanDurationMs) + "ms\n\n";

    report += "[SUBSYSTEM SUMMARY]\n";
    report += "Wi-Fi:    " + _latestSnapshot.wifi.state + " (" + String(_latestSnapshot.wifi.count) + " APs, Peak: " + String(_latestSnapshot.wifi.peakRssi) + "dBm)\n";
    report += "BLE:      " + _latestSnapshot.ble.state + " (" + String(_latestSnapshot.ble.count) + " Devs, Peak: " + String(_latestSnapshot.ble.peakRssi) + "dBm)\n";
    report += "Sub-GHz:  " + _latestSnapshot.subghz.state + " (" + _latestSnapshot.subghz.info1 + ", " + _latestSnapshot.subghz.info2 + ")\n";
    report += "nRF24:    " + _latestSnapshot.nrf24.state + " (" + _latestSnapshot.nrf24.info2 + ")\n\n";

    if (_baseline.isSet) {
        report += "[BASELINE DIFF]\n";
        report += "Baseline: " + String(_baseline.wifiCount) + " Wi-Fi, " + String(_baseline.bleCount) + " BLE\n";
        report += "Changes:  +" + String(_latestSnapshot.newWifiCount) + " Wi-Fi, +" + String(_latestSnapshot.newBleCount) + " BLE\n\n";
    }

    report += "[DISCOVERED DEVICES (" + String(_latestSnapshot.devices.size()) + ")]\n";
    for (size_t i = 0; i < _latestSnapshot.devices.size(); i++) {
        const auto& d = _latestSnapshot.devices[i];
        report += String(i + 1) + ". [" + (d.type == DEV_TYPE_WIFI_AP ? "WIFI" : "BLE") + "] ";
        report += d.name + " (" + d.identifier + ") ";
        report += String(d.rssi) + "dBm ";
        if (d.channel > 0) report += "CH:" + String(d.channel) + " ";
        report += d.extra + (d.isNew ? " [NEW]" : "") + "\n";
    }

    report += "\n=== END OF REPORT ===\n";
    return report;
}

bool WirelessAnalyzer::saveReportToLittleFS(const String& path) {
    File file = LittleFS.open(path.c_str(), "w");
    if (!file) return false;
    String text = getSessionReportText();
    file.print(text);
    file.close();
    return true;
}

bool WirelessAnalyzer::hasSavedReport(const String& path) const {
    return LittleFS.exists(path.c_str());
}

bool WirelessAnalyzer::deleteReportFromLittleFS(const String& path) {
    if (LittleFS.exists(path.c_str())) {
        return LittleFS.remove(path.c_str());
    }
    return false;
}

void WirelessAnalyzer::clearSnapshot() {
    _latestSnapshot = AnalyzerSnapshot();
}
