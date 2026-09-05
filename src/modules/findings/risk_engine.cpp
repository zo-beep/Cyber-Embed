#include "risk_engine.h"
#include <LittleFS.h>

RiskEngine& riskEngine = RiskEngine::getInstance();

RiskEngine::RiskEngine() {
    loadFromLittleFS();
}

const char* RiskEngine::getRiskLabel(RiskLevel risk) {
    switch (risk) {
        case RISK_HIGH: return "HIGH";
        case RISK_MED:  return "MED ";
        case RISK_LOW:  return "LOW ";
        case RISK_INFO: return "INFO";
        default:        return "INFO";
    }
}

uint16_t RiskEngine::getRiskColor(RiskLevel risk) {
    switch (risk) {
        case RISK_HIGH: return 0xF800; // Red
        case RISK_MED:  return 0xFD60; // Orange / Amber
        case RISK_LOW:  return 0x07FF; // Cyan
        case RISK_INFO: return 0x8410; // Slate Grey
        default:        return 0xCE79; // Off-white
    }
}

const char* RiskEngine::getCategoryLabel(FindingCategory cat) {
    switch (cat) {
        case FINDING_CAT_WIFI:   return "Wi-Fi";
        case FINDING_CAT_BLE:    return "BLE";
        case FINDING_CAT_SUBGHZ: return "Sub-GHz";
        case FINDING_CAT_NRF24:  return "nRF24";
        case FINDING_CAT_SYSTEM: return "System";
        default:                 return "General";
    }
}

String RiskEngine::getNextFindingId() {
    char buf[16];
    snprintf(buf, sizeof(buf), "F-%03u", (unsigned int)_nextIdCounter++);
    return String(buf);
}

void RiskEngine::addOrUpdateFinding(const SecurityFinding& newFinding) {
    // Check if a finding with the same deduplication key already exists
    for (auto& existing : _findings) {
        if (existing.dedupeKey == newFinding.dedupeKey && newFinding.dedupeKey.length() > 0) {
            // Update mutable telemetry fields without resetting acknowledgement or notes
            existing.timestampMs = newFinding.timestampMs;
            existing.source = newFinding.source;
            existing.rssi = newFinding.rssi;
            existing.evidence = newFinding.evidence;
            existing.description = newFinding.description;
            if (newFinding.risk > existing.risk) {
                existing.risk = newFinding.risk;
            }
            saveToLittleFS();
            return;
        }
    }

    // Limit maximum retained findings to 50
    if (_findings.size() >= 50) {
        // Remove the oldest acknowledged finding, or oldest info finding
        int removeIdx = -1;
        for (size_t i = 0; i < _findings.size(); i++) {
            if (_findings[i].acknowledged) {
                removeIdx = i;
                break;
            }
        }
        if (removeIdx < 0) removeIdx = 0;
        _findings.erase(_findings.begin() + removeIdx);
    }

    SecurityFinding toAdd = newFinding;
    toAdd.id = getNextFindingId();
    _findings.push_back(toAdd);
    saveToLittleFS();
}

void RiskEngine::evaluateDevice(const WirelessDevice& dev, const String& source, bool hasBaseline, bool isNew) {
    uint32_t now = millis();

    if (dev.type == DEV_TYPE_WIFI_AP) {
        // 1. Insecure Open AP rule
        String extraUpper = dev.extra;
        extraUpper.toUpperCase();
        if (extraUpper.indexOf("OPEN") >= 0 || extraUpper.indexOf("NONE") >= 0 || extraUpper.indexOf("AUTH_OPEN") >= 0) {
            SecurityFinding f;
            f.dedupeKey = "WIFI:OPEN:" + dev.identifier;
            f.risk = RISK_MED;
            f.category = FINDING_CAT_WIFI;
            f.title = "Insecure Open AP";
            f.description = "Access point has no wireless encryption configured.";
            f.timestampMs = now;
            f.source = source;
            f.target = dev.name.length() > 0 ? dev.name : dev.identifier;
            f.rssi = dev.rssi;
            f.evidence = "BSSID: " + dev.identifier + " | CH: " + String(dev.channel) + " | " + String(dev.rssi) + " dBm [OPEN]";
            addOrUpdateFinding(f);
        }

        // 2. Unknown / Baseline-diff AP rule
        if (hasBaseline && isNew) {
            SecurityFinding f;
            f.dedupeKey = "WIFI:NEW:" + dev.identifier;
            f.risk = RISK_MED;
            f.category = FINDING_CAT_WIFI;
            f.title = "Unknown AP";
            f.description = "A previously unseen access point was observed during assessment.";
            f.timestampMs = now;
            f.source = source;
            f.target = dev.name.length() > 0 ? dev.name : dev.identifier;
            f.rssi = dev.rssi;
            f.evidence = "Not in attached baseline. CH: " + String(dev.channel) + " | Signal: " + String(dev.rssi) + " dBm";
            addOrUpdateFinding(f);
        }

        // 3. Close physical proximity AP rule
        if (dev.rssi >= -45) {
            SecurityFinding f;
            f.dedupeKey = "WIFI:PROX:" + dev.identifier;
            f.risk = RISK_LOW;
            f.category = FINDING_CAT_WIFI;
            f.title = "High Proximity AP";
            f.description = "Access point is in immediate physical proximity.";
            f.timestampMs = now;
            f.source = source;
            f.target = dev.name.length() > 0 ? dev.name : dev.identifier;
            f.rssi = dev.rssi;
            f.evidence = "Signal strength: " + String(dev.rssi) + " dBm (Threshold >= -45 dBm)";
            addOrUpdateFinding(f);
        }
    } else if (dev.type == DEV_TYPE_BLE) {
        // 1. New BLE Device rule
        if (hasBaseline && isNew) {
            SecurityFinding f;
            f.dedupeKey = "BLE:NEW:" + dev.identifier;
            f.risk = RISK_LOW;
            f.category = FINDING_CAT_BLE;
            f.title = "New BLE Device";
            f.description = "A new Bluetooth LE device was observed not present in baseline.";
            f.timestampMs = now;
            f.source = source;
            f.target = dev.name.length() > 0 ? dev.name : dev.identifier;
            f.rssi = dev.rssi;
            f.evidence = "MAC: " + dev.identifier + " | Signal: " + String(dev.rssi) + " dBm";
            addOrUpdateFinding(f);
        }

        // 2. High Proximity BLE Beacon
        if (dev.rssi >= -48) {
            SecurityFinding f;
            f.dedupeKey = "BLE:PROX:" + dev.identifier;
            f.risk = RISK_LOW;
            f.category = FINDING_CAT_BLE;
            f.title = "Proximity BLE Beacon";
            f.description = "Strong Bluetooth LE beacon detected in immediate perimeter.";
            f.timestampMs = now;
            f.source = source;
            f.target = dev.name.length() > 0 ? dev.name : dev.identifier;
            f.rssi = dev.rssi;
            f.evidence = "Signal strength: " + String(dev.rssi) + " dBm (Threshold >= -48 dBm)";
            addOrUpdateFinding(f);
        }
    }
}

void RiskEngine::evaluateSubsystems(const SubsystemStatus& subghz, const SubsystemStatus& nrf24, const String& source) {
    uint32_t now = millis();

    // 1. Sub-GHz elevated activity
    if (subghz.detected && subghz.state != "IDLE") {
        SecurityFinding f;
        f.dedupeKey = "SUBGHZ:ACTIVITY:" + subghz.state;
        f.risk = RISK_LOW;
        f.category = FINDING_CAT_SUBGHZ;
        f.title = "RF Activity Shift";
        f.description = "Elevated Sub-GHz radio activity observed in environment.";
        f.timestampMs = now;
        f.source = source;
        f.target = subghz.state;
        f.rssi = subghz.peakRssi;
        f.evidence = subghz.info1 + " " + subghz.info2 + " | Peak RSSI: " + String(subghz.peakRssi) + " dBm";
        addOrUpdateFinding(f);
    }

    // 2. nRF24 elevated carrier activity
    if (nrf24.detected && nrf24.state != "IDLE") {
        SecurityFinding f;
        f.dedupeKey = "NRF24:ACTIVITY:" + nrf24.state;
        f.risk = RISK_LOW;
        f.category = FINDING_CAT_NRF24;
        f.title = "2.4GHz Activity Shift";
        f.description = "Elevated 2.4GHz ISM carrier activity detected.";
        f.timestampMs = now;
        f.source = source;
        f.target = nrf24.state;
        f.rssi = 0;
        f.evidence = nrf24.info1 + " " + nrf24.info2;
        addOrUpdateFinding(f);
    }
}

void RiskEngine::evaluateSnapshot(const AnalyzerSnapshot& snap, bool hasBaseline) {
    String source = "Wireless Analyzer";
    for (const auto& dev : snap.devices) {
        evaluateDevice(dev, source, hasBaseline, dev.isNew);
    }
    evaluateSubsystems(snap.subghz, snap.nrf24, source);
}

void RiskEngine::evaluateInvestigation(const InvestigationSession& session) {
    String source = "Investigation: " + session.name;
    for (const auto& dev : session.observedDevices) {
        evaluateDevice(dev, source, session.hasBaseline, dev.isNew);
    }
    evaluateSubsystems(session.lastSubghz, session.lastNrf24, source);
}

std::vector<SecurityFinding> RiskEngine::getFindings() const {
    return _findings;
}

size_t RiskEngine::getTotalFindings() const {
    return _findings.size();
}

size_t RiskEngine::getUnackFindingsCount() const {
    size_t count = 0;
    for (const auto& f : _findings) {
        if (!f.acknowledged) count++;
    }
    return count;
}

RiskLevel RiskEngine::getHighestRisk() const {
    RiskLevel highest = RISK_INFO;
    for (const auto& f : _findings) {
        if (!f.acknowledged && f.risk > highest) {
            highest = f.risk;
        }
    }
    return highest;
}

bool RiskEngine::getFindingById(const String& id, SecurityFinding& outFinding) const {
    for (const auto& f : _findings) {
        if (f.id == id) {
            outFinding = f;
            return true;
        }
    }
    return false;
}

bool RiskEngine::toggleAcknowledge(const String& id) {
    for (auto& f : _findings) {
        if (f.id == id) {
            f.acknowledged = !f.acknowledged;
            saveToLittleFS();
            return true;
        }
    }
    return false;
}

bool RiskEngine::addNoteToFinding(const String& id, const String& note) {
    for (auto& f : _findings) {
        if (f.id == id) {
            f.note = note;
            saveToLittleFS();
            return true;
        }
    }
    return false;
}

bool RiskEngine::deleteFinding(const String& id) {
    for (size_t i = 0; i < _findings.size(); i++) {
        if (_findings[i].id == id) {
            _findings.erase(_findings.begin() + i);
            saveToLittleFS();
            return true;
        }
    }
    return false;
}

void RiskEngine::clearAllFindings() {
    _findings.clear();
    _nextIdCounter = 1;
    if (LittleFS.exists("/findings.json")) {
        LittleFS.remove("/findings.json");
    }
}

bool RiskEngine::saveToLittleFS() {
    File file = LittleFS.open("/findings.json", "w");
    if (!file) return false;

    JsonDocument doc;
    JsonArray arr = doc["findings"].to<JsonArray>();

    for (const auto& f : _findings) {
        JsonObject obj = arr.add<JsonObject>();
        obj["id"] = f.id;
        obj["k"] = f.dedupeKey;
        obj["r"] = (int)f.risk;
        obj["c"] = (int)f.category;
        obj["t"] = f.title;
        obj["d"] = f.description;
        obj["ts"] = f.timestampMs;
        obj["s"] = f.source;
        obj["tgt"] = f.target;
        obj["rssi"] = f.rssi;
        obj["ev"] = f.evidence;
        obj["ack"] = f.acknowledged;
        obj["nt"] = f.note;
    }

    doc["nextId"] = _nextIdCounter;
    serializeJson(doc, file);
    file.close();
    return true;
}

bool RiskEngine::loadFromLittleFS() {
    if (!LittleFS.exists("/findings.json")) return false;

    File file = LittleFS.open("/findings.json", "r");
    if (!file) return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    if (err) return false;

    _findings.clear();
    _nextIdCounter = doc["nextId"] | 1;

    JsonArray arr = doc["findings"].as<JsonArray>();
    for (JsonObject obj : arr) {
        SecurityFinding f;
        f.id = (const char*)(obj["id"] | "");
        f.dedupeKey = (const char*)(obj["k"] | "");
        f.risk = (RiskLevel)(obj["r"] | 0);
        f.category = (FindingCategory)(obj["c"] | 0);
        f.title = (const char*)(obj["t"] | "");
        f.description = (const char*)(obj["d"] | "");
        f.timestampMs = obj["ts"] | 0;
        f.source = (const char*)(obj["s"] | "");
        f.target = (const char*)(obj["tgt"] | "");
        f.rssi = obj["rssi"] | 0;
        f.evidence = (const char*)(obj["ev"] | "");
        f.acknowledged = obj["ack"] | false;
        f.note = (const char*)(obj["nt"] | "");
        _findings.push_back(f);
    }
    return true;
}
