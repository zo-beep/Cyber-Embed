#include "investigation_manager.h"
#include "modules/findings/risk_engine.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

InvestigationManager& investigationManager = InvestigationManager::getInstance();

InvestigationManager::InvestigationManager() {
    _activeSession.isActive = false;
}

uint32_t InvestigationManager::getSessionElapsedMs() const {
    if (!_activeSession.isActive || _activeSession.startTime == 0) {
        return _activeSession.durationMs;
    }
    return millis() - _activeSession.startTime;
}

String InvestigationManager::getFormattedDuration(uint32_t durationMs) {
    uint32_t totalSec = durationMs / 1000;
    uint32_t hours = totalSec / 3600;
    uint32_t mins = (totalSec % 3600) / 60;
    uint32_t secs = totalSec % 60;

    char buf[16];
    if (hours > 0) {
        snprintf(buf, sizeof(buf), "%02u:%02u:%02u", (unsigned int)hours, (unsigned int)mins, (unsigned int)secs);
    } else {
        snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned int)mins, (unsigned int)secs);
    }
    return String(buf);
}

String InvestigationManager::getFormattedTime(uint32_t offsetMs) {
    uint32_t totalSec = offsetMs / 1000;
    uint32_t mins = totalSec / 60;
    uint32_t secs = totalSec % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned int)mins, (unsigned int)secs);
    return String(buf);
}

String InvestigationManager::getEventTypeTag(InvestigationEventType type) {
    switch (type) {
        case EVENT_TYPE_SESSION_START:   return "[INIT]";
        case EVENT_TYPE_SCAN_SUMMARY:    return "[SCAN]";
        case EVENT_TYPE_DEVICE_NEW:      return "[NEW+]";
        case EVENT_TYPE_DEVICE_GONE:     return "[GONE-]";
        case EVENT_TYPE_DEVICE_MODIFIED: return "[MOD~]";
        case EVENT_TYPE_SUBGHZ_SHIFT:    return "[RF]";
        case EVENT_TYPE_NRF24_SHIFT:     return "[NRF]";
        case EVENT_TYPE_BASELINE_DIFF:   return "[DIFF]";
        case EVENT_TYPE_NOTE:            return "[NOTE]";
        case EVENT_TYPE_SESSION_END:     return "[END]";
        default:                         return "[INFO]";
    }
}

String InvestigationManager::getNextSessionId() {
    std::vector<InvestigationSummary> list = listSavedSessions();
    int highestNum = 0;
    for (const auto& s : list) {
        if (s.id.startsWith("INV_")) {
            int num = s.id.substring(4).toInt();
            if (num > highestNum) highestNum = num;
        }
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "INV_%03d", highestNum + 1);
    return String(buf);
}

bool InvestigationManager::startNewSession(const String& name, bool attachBaseline) {
    _activeSession = InvestigationSession();
    _activeSession.id = getNextSessionId();
    _activeSession.name = (name.length() > 0) ? name : "SITE_ALPHA";
    _activeSession.startTime = millis();
    _activeSession.durationMs = 0;
    _activeSession.isActive = true;
    _activeSession.hasBaseline = attachBaseline;

    auto& analyzer = WirelessAnalyzer::getInstance();
    if (attachBaseline && analyzer.hasBaseline()) {
        _activeSession.baselineName = "SAVED_BASELINE";
    }

    _nextEventId = 1;
    _nextNoteId = 1;

    addEvent(EVENT_TYPE_SESSION_START, "Investigation Started", "Session: " + _activeSession.name, "ID: " + _activeSession.id);

    // Initial environment sweep
    recordScanResults(nullptr);

    autoSaveActiveSession();
    return true;
}

void InvestigationManager::addEvent(InvestigationEventType type, const String& title, const String& summary, const String& details) {
    if (!_activeSession.isActive) return;

    InvestigationEvent ev;
    ev.id = _nextEventId++;
    ev.timestampMs = getSessionElapsedMs();
    ev.type = type;
    ev.title = title;
    ev.summary = summary;
    ev.details = details;

    _activeSession.events.push_back(ev);
    _activeSession.totalEvents = _activeSession.events.size();
}

void InvestigationManager::addNote(const String& noteText) {
    if (!_activeSession.isActive || noteText.length() == 0) return;

    InvestigationNote note;
    note.id = _nextNoteId++;
    note.timestampMs = getSessionElapsedMs();
    note.text = noteText;

    _activeSession.notes.push_back(note);
    _activeSession.totalNotes = _activeSession.notes.size();

    addEvent(EVENT_TYPE_NOTE, "Field Note Added", noteText.substring(0, 20), noteText);
    autoSaveActiveSession();
}

void InvestigationManager::updateSessionCounters() {
    int wCount = 0;
    int bCount = 0;
    for (const auto& dev : _activeSession.observedDevices) {
        if (dev.type == DEV_TYPE_WIFI_AP) wCount++;
        else if (dev.type == DEV_TYPE_BLE) bCount++;
    }
    _activeSession.totalWifiObserved = wCount;
    _activeSession.totalBleObserved = bCount;
    _activeSession.totalEvents = _activeSession.events.size();
    _activeSession.totalNotes = _activeSession.notes.size();
}

void InvestigationManager::recordScanResults(std::function<void(const String& stage, int progressPercent)> onProgress) {
    if (!_activeSession.isActive) return;

    auto& analyzer = WirelessAnalyzer::getInstance();
    analyzer.performUnifiedScan(onProgress);

    const auto& snap = analyzer.getLatestSnapshot();

    int newDevicesThisScan = 0;

    // 1. Process discovered devices
    for (const auto& currDev : snap.devices) {
        WirelessDevice* matched = nullptr;
        for (auto& obsDev : _activeSession.observedDevices) {
            if (obsDev.type == currDev.type && obsDev.identifier.equalsIgnoreCase(currDev.identifier)) {
                matched = &obsDev;
                break;
            }
        }

        if (!matched) {
            // New device observed for the first time during this investigation
            WirelessDevice newDev = currDev;
            newDev.firstSeen = getSessionElapsedMs();
            newDev.lastSeen = getSessionElapsedMs();
            _activeSession.observedDevices.push_back(newDev);
            newDevicesThisScan++;

            String typeStr = (currDev.type == DEV_TYPE_WIFI_AP) ? "Wi-Fi AP" : "BLE Device";
            String summ = currDev.name.substring(0, 14) + " (" + String(currDev.rssi) + "dBm)";
            String det = "Type: " + typeStr + "\nName: " + currDev.name + "\nAddr: " + currDev.identifier + 
                         "\nRSSI: " + String(currDev.rssi) + " dBm";
            if (currDev.type == DEV_TYPE_WIFI_AP) {
                det += "\nChannel: " + String(currDev.channel) + "\nSecurity: " + currDev.extra;
            } else {
                det += "\nPayload: " + currDev.extra;
            }

            addEvent(EVENT_TYPE_DEVICE_NEW, "New " + typeStr, summ, det);
        } else {
            // Existing device update
            matched->lastSeen = getSessionElapsedMs();
            int rssiDiff = abs((int)currDev.rssi - (int)matched->rssi);
            if (rssiDiff >= 15) {
                matched->rssi = currDev.rssi;
                String typeStr = (currDev.type == DEV_TYPE_WIFI_AP) ? "Wi-Fi" : "BLE";
                addEvent(EVENT_TYPE_DEVICE_MODIFIED, typeStr + " Signal Shift", 
                         currDev.name.substring(0, 12) + " " + String(currDev.rssi) + "dBm",
                         "Device: " + currDev.name + "\nAddr: " + currDev.identifier + "\nNew RSSI: " + 
                         String(currDev.rssi) + " dBm (Delta: " + String(rssiDiff) + " dBm)");
            }
        }
    }

    // 2. Sub-GHz telemetry
    if (snap.subghz.detected) {
        if (_activeSession.lastSubghz.detected) {
            int rssiDiff = abs((int)snap.subghz.peakRssi - (int)_activeSession.lastSubghz.peakRssi);
            if (rssiDiff >= 10 || snap.subghz.state != _activeSession.lastSubghz.state) {
                addEvent(EVENT_TYPE_SUBGHZ_SHIFT, "Sub-GHz RF Shift", 
                         snap.subghz.info1 + " " + snap.subghz.info2, 
                         "RF Activity changed on " + snap.subghz.info1 + "\nState: " + snap.subghz.state + 
                         "\nRSSI: " + snap.subghz.info2);
            }
        }
        _activeSession.lastSubghz = snap.subghz;
    }

    // 3. nRF24 telemetry
    if (snap.nrf24.detected) {
        if (_activeSession.lastNrf24.detected) {
            if (snap.nrf24.count != _activeSession.lastNrf24.count) {
                addEvent(EVENT_TYPE_NRF24_SHIFT, "nRF24 Band Shift", 
                         snap.nrf24.info2, 
                         "2.4 GHz active carriers: " + String(snap.nrf24.count) + " channels\nState: " + snap.nrf24.state);
            }
        }
        _activeSession.lastNrf24 = snap.nrf24;
    }

    // 4. Log Scan Sweep event
    addEvent(EVENT_TYPE_SCAN_SUMMARY, "Sweep Completed", 
             String(snap.wifi.count) + " APs, " + String(snap.ble.count) + " BLE, +" + String(newDevicesThisScan) + " New",
             "Wi-Fi: " + String(snap.wifi.count) + " APs\nBLE: " + String(snap.ble.count) + " Devs\nSub-GHz: " + 
             snap.subghz.info1 + " " + snap.subghz.info2 + "\nnRF24: " + snap.nrf24.info2);

    updateSessionCounters();
    autoSaveActiveSession();

    // Trigger Risk Engine evaluation on new evidence
    RiskEngine::getInstance().evaluateInvestigation(_activeSession);
}

ComparisonResult InvestigationManager::compareSessionWithBaseline() {
    auto& analyzer = WirelessAnalyzer::getInstance();
    return analyzer.compareWithBaseline();
}

bool InvestigationManager::endSession(bool saveToStorage) {
    if (!_activeSession.isActive) return false;

    _activeSession.durationMs = getSessionElapsedMs();
    _activeSession.isActive = false;

    addEvent(EVENT_TYPE_SESSION_END, "Session Concluded", 
             "Duration: " + getFormattedDuration(_activeSession.durationMs), 
             "Total Events: " + String(_activeSession.totalEvents) + "\nWi-Fi APs: " + 
             String(_activeSession.totalWifiObserved) + "\nBLE Devs: " + String(_activeSession.totalBleObserved));

    updateSessionCounters();

    // Evaluate final session findings
    RiskEngine::getInstance().evaluateInvestigation(_activeSession);

    if (saveToStorage) {
        saveSession(_activeSession);
    }

    return true;
}

void InvestigationManager::discardActiveSession() {
    _activeSession.isActive = false;
    _activeSession = InvestigationSession();
}

// ---------------- LittleFS Persistence ----------------

void InvestigationManager::autoSaveActiveSession() {
    if (!_activeSession.isActive) return;
    saveSession(_activeSession);
}

bool InvestigationManager::saveSession(const InvestigationSession& session) {
    String filename = "/inv_" + session.id + ".json";
    File file = LittleFS.open(filename.c_str(), "w");
    if (!file) return false;

    JsonDocument doc;
    doc["id"] = session.id;
    doc["name"] = session.name;
    doc["startTime"] = session.startTime;
    doc["durationMs"] = session.isActive ? (millis() - session.startTime) : session.durationMs;
    doc["isActive"] = session.isActive;
    doc["hasBaseline"] = session.hasBaseline;
    doc["baselineName"] = session.baselineName;

    // Devices array
    JsonArray devArr = doc["devices"].to<JsonArray>();
    for (const auto& d : session.observedDevices) {
        JsonObject obj = devArr.add<JsonObject>();
        obj["t"] = (int)d.type;
        obj["n"] = d.name;
        obj["id"] = d.identifier;
        obj["r"] = d.rssi;
        obj["ch"] = d.channel;
        obj["ex"] = d.extra;
        obj["fs"] = d.firstSeen;
        obj["ls"] = d.lastSeen;
    }

    // Events array (limit to newest 80 to preserve flash & RAM)
    JsonArray evArr = doc["events"].to<JsonArray>();
    size_t startEv = (session.events.size() > 80) ? (session.events.size() - 80) : 0;
    for (size_t i = startEv; i < session.events.size(); i++) {
        const auto& ev = session.events[i];
        JsonObject obj = evArr.add<JsonObject>();
        obj["i"] = ev.id;
        obj["ts"] = ev.timestampMs;
        obj["t"] = (int)ev.type;
        obj["h"] = ev.title;
        obj["s"] = ev.summary;
        obj["d"] = ev.details;
    }

    // Notes array
    JsonArray noteArr = doc["notes"].to<JsonArray>();
    for (const auto& nt : session.notes) {
        JsonObject obj = noteArr.add<JsonObject>();
        obj["i"] = nt.id;
        obj["ts"] = nt.timestampMs;
        obj["txt"] = nt.text;
    }

    serializeJson(doc, file);
    file.close();

    // Update index file
    std::vector<InvestigationSummary> list = listSavedSessions();
    bool existsInIndex = false;
    for (auto& item : list) {
        if (item.id == session.id) {
            item.name = session.name;
            item.durationMs = doc["durationMs"];
            item.totalEvents = session.events.size();
            item.wifiCount = session.totalWifiObserved;
            item.bleCount = session.totalBleObserved;
            item.notesCount = session.notes.size();
            item.filename = filename;
            existsInIndex = true;
            break;
        }
    }

    if (!existsInIndex) {
        InvestigationSummary summary;
        summary.id = session.id;
        summary.name = session.name;
        summary.startTime = session.startTime;
        summary.durationMs = doc["durationMs"];
        summary.totalEvents = session.events.size();
        summary.wifiCount = session.totalWifiObserved;
        summary.bleCount = session.totalBleObserved;
        summary.notesCount = session.notes.size();
        summary.filename = filename;
        list.push_back(summary);
    }

    File idxFile = LittleFS.open("/inv_index.json", "w");
    if (idxFile) {
        JsonDocument idxDoc;
        JsonArray arr = idxDoc["sessions"].to<JsonArray>();
        for (const auto& item : list) {
            JsonObject sObj = arr.add<JsonObject>();
            sObj["id"] = item.id;
            sObj["name"] = item.name;
            sObj["st"] = item.startTime;
            sObj["dur"] = item.durationMs;
            sObj["ev"] = item.totalEvents;
            sObj["w"] = item.wifiCount;
            sObj["b"] = item.bleCount;
            sObj["nt"] = item.notesCount;
            sObj["fn"] = item.filename;
        }
        serializeJson(idxDoc, idxFile);
        idxFile.close();
    }

    return true;
}

std::vector<InvestigationSummary> InvestigationManager::listSavedSessions() {
    std::vector<InvestigationSummary> list;
    if (!LittleFS.exists("/inv_index.json")) return list;

    File file = LittleFS.open("/inv_index.json", "r");
    if (!file) return list;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    if (err) return list;

    JsonArray arr = doc["sessions"].as<JsonArray>();
    for (JsonObject obj : arr) {
        InvestigationSummary s;
        s.id = (const char*)(obj["id"] | "");
        s.name = (const char*)(obj["name"] | "");
        s.startTime = obj["st"] | 0;
        s.durationMs = obj["dur"] | 0;
        s.totalEvents = obj["ev"] | 0;
        s.wifiCount = obj["w"] | 0;
        s.bleCount = obj["b"] | 0;
        s.notesCount = obj["nt"] | 0;
        s.filename = (const char*)(obj["fn"] | "");

        // Only include if actual file exists
        if (LittleFS.exists(s.filename.c_str())) {
            list.push_back(s);
        }
    }
    return list;
}

bool InvestigationManager::loadSession(const String& id, InvestigationSession& outSession) {
    String filename = "/inv_" + id + ".json";
    if (!LittleFS.exists(filename.c_str())) return false;

    File file = LittleFS.open(filename.c_str(), "r");
    if (!file) return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    if (err) return false;

    outSession = InvestigationSession();
    outSession.id = (const char*)(doc["id"] | "");
    outSession.name = (const char*)(doc["name"] | "");
    outSession.startTime = doc["startTime"] | 0;
    outSession.durationMs = doc["durationMs"] | 0;
    outSession.isActive = doc["isActive"] | false;
    outSession.hasBaseline = doc["hasBaseline"] | false;
    outSession.baselineName = (const char*)(doc["baselineName"] | "");

    // Load devices
    JsonArray devArr = doc["devices"].as<JsonArray>();
    for (JsonObject obj : devArr) {
        WirelessDevice dev;
        dev.type = (WirelessDeviceType)(obj["t"] | 0);
        dev.name = (const char*)(obj["n"] | "");
        dev.identifier = (const char*)(obj["id"] | "");
        dev.rssi = obj["r"] | -128;
        dev.channel = obj["ch"] | 0;
        dev.extra = (const char*)(obj["ex"] | "");
        dev.firstSeen = obj["fs"] | 0;
        dev.lastSeen = obj["ls"] | 0;
        outSession.observedDevices.push_back(dev);
    }

    // Load events
    JsonArray evArr = doc["events"].as<JsonArray>();
    for (JsonObject obj : evArr) {
        InvestigationEvent ev;
        ev.id = obj["i"] | 0;
        ev.timestampMs = obj["ts"] | 0;
        ev.type = (InvestigationEventType)(obj["t"] | 0);
        ev.title = (const char*)(obj["h"] | "");
        ev.summary = (const char*)(obj["s"] | "");
        ev.details = (const char*)(obj["d"] | "");
        outSession.events.push_back(ev);
    }

    // Load notes
    JsonArray noteArr = doc["notes"].as<JsonArray>();
    for (JsonObject obj : noteArr) {
        InvestigationNote nt;
        nt.id = obj["i"] | 0;
        nt.timestampMs = obj["ts"] | 0;
        nt.text = (const char*)(obj["txt"] | "");
        outSession.notes.push_back(nt);
    }

    outSession.totalEvents = outSession.events.size();
    outSession.totalNotes = outSession.notes.size();
    int wCount = 0, bCount = 0;
    for (const auto& d : outSession.observedDevices) {
        if (d.type == DEV_TYPE_WIFI_AP) wCount++;
        else if (d.type == DEV_TYPE_BLE) bCount++;
    }
    outSession.totalWifiObserved = wCount;
    outSession.totalBleObserved = bCount;

    return true;
}

bool InvestigationManager::deleteSession(const String& id) {
    String filename = "/inv_" + id + ".json";
    if (LittleFS.exists(filename.c_str())) {
        LittleFS.remove(filename.c_str());
    }

    // If deleting active session
    if (_activeSession.isActive && _activeSession.id == id) {
        discardActiveSession();
    }

    // Update index
    std::vector<InvestigationSummary> list = listSavedSessions();
    std::vector<InvestigationSummary> updated;
    for (const auto& item : list) {
        if (item.id != id) {
            updated.push_back(item);
        }
    }

    File idxFile = LittleFS.open("/inv_index.json", "w");
    if (idxFile) {
        JsonDocument idxDoc;
        JsonArray arr = idxDoc["sessions"].to<JsonArray>();
        for (const auto& item : updated) {
            JsonObject sObj = arr.add<JsonObject>();
            sObj["id"] = item.id;
            sObj["name"] = item.name;
            sObj["st"] = item.startTime;
            sObj["dur"] = item.durationMs;
            sObj["ev"] = item.totalEvents;
            sObj["w"] = item.wifiCount;
            sObj["b"] = item.bleCount;
            sObj["nt"] = item.notesCount;
            sObj["fn"] = item.filename;
        }
        serializeJson(idxDoc, idxFile);
        idxFile.close();
    }

    return true;
}

bool InvestigationManager::deleteAllSessions() {
    discardActiveSession();
    std::vector<InvestigationSummary> list = listSavedSessions();
    for (const auto& item : list) {
        String filename = item.filename.isEmpty() ? ("/inv_" + item.id + ".json") : item.filename;
        if (LittleFS.exists(filename.c_str())) {
            LittleFS.remove(filename.c_str());
        }
    }
    if (LittleFS.exists("/inv_index.json")) {
        LittleFS.remove("/inv_index.json");
    }
    _nextEventId = 1;
    _nextNoteId = 1;
    return true;
}
