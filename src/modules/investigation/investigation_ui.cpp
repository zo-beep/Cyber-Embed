#include "investigation_ui.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/settings.h"
#include "core/utils.h"
#include "modules/analyzer/analyzer_ui.h"
#include "modules/findings/risk_engine.h"

// Forward declarations
static void drawActiveInvestigationHUD(const InvestigationSession& session);

void investigation_home() {
    returnToMenu = false;
    auto& mgr = InvestigationManager::getInstance();

    options.clear();

    if (mgr.hasActiveSession()) {
        const auto& active = mgr.getActiveSession();
        String currentLabel = "Current: " + active.name + " (" + InvestigationManager::getFormattedDuration(mgr.getSessionElapsedMs()) + ")";
        options.push_back({currentLabel, investigation_active_dashboard});
        options.push_back({"New Investigation", investigation_create_dialog});
        options.push_back({"Saved Investigations", investigation_saved_list});
        options.push_back({"Purge Intel Data", investigation_purge_dialog});
    } else {
        options.push_back({"New Investigation", investigation_create_dialog});
        options.push_back({"Saved Investigations", investigation_saved_list});
        options.push_back({"Purge Intel Data", investigation_purge_dialog});
    }

    addOptionToMainMenu();

    loopOptions(options, MENU_TYPE_SUBMENU, "Investigation Mode");
}

void investigation_create_dialog() {
    returnToMenu = false;
    auto& mgr = InvestigationManager::getInstance();

    if (mgr.hasActiveSession()) {
        // If an active session exists, prompt user
        options.clear();
        options.push_back({"End Current & Start New", [=, &mgr]() {
            mgr.endSession(true);
            displaySuccess("Previous session saved", true);
            investigation_create_dialog();
        }});
        options.push_back({"Discard Current & Start New", [=, &mgr]() {
            mgr.discardActiveSession();
            investigation_create_dialog();
        }});
        options.push_back({"Cancel", [=]() { return; }});
        loopOptions(options, MENU_TYPE_SUBMENU, "Active Session Exists");
        return;
    }

    options.clear();
    const char* presetNames[] = {
        "SITE_ALPHA",
        "SITE_BRAVO",
        "LAB_TEST",
        "OFFICE_AUDIT",
        "PERIMETER",
        "FIELD_SCAN"
    };

    for (int i = 0; i < 6; i++) {
        String name = presetNames[i];
        options.push_back({name, [=, &mgr]() {
            // Baseline attachment choice
            options.clear();
            options.push_back({"Attach Current Baseline", [=, &mgr]() {
                tft.fillScreen(bruceConfig.bgColor);
                drawMainBorder();
                progressHandler(10, 100, "Starting Investigation...");
                mgr.startNewSession(name, true);
                progressHandler(100, 100, "Session Active");
                vTaskDelay(pdMS_TO_TICKS(150));
                investigation_active_dashboard();
            }});
            options.push_back({"Start Without Baseline", [=, &mgr]() {
                tft.fillScreen(bruceConfig.bgColor);
                drawMainBorder();
                progressHandler(10, 100, "Starting Investigation...");
                mgr.startNewSession(name, false);
                progressHandler(100, 100, "Session Active");
                vTaskDelay(pdMS_TO_TICKS(150));
                investigation_active_dashboard();
            }});
            options.push_back({"Cancel", [=]() { return; }});
            loopOptions(options, MENU_TYPE_SUBMENU, "Baseline Attachment");
        }});
    }

    options.push_back({"Back", [=]() { return; }});
    loopOptions(options, MENU_TYPE_SUBMENU, "Target / Site Name");
}

static void drawActiveInvestigationHUD(const InvestigationSession& session) {
    auto& mgr = InvestigationManager::getInstance();
    uint32_t elapsed = mgr.getSessionElapsedMs();

    tft.fillScreen(bruceConfig.bgColor);
    drawCyberHeader("INVESTIGATION // " + session.id, session.name, true);

    // 1. Session Telemetry Card (x=6, y=23, w=308, h=68)
    drawCyberCard(6, 23, 308, 68, bruceConfig.priColor, 0, "SESSION TELEMETRY");

    tft.setTextSize(FP);
    // Duration Timer (Top Right of Card)
    tft.setTextColor(0x07E0, bruceConfig.bgColor); // Green
    tft.drawRightString("+" + InvestigationManager::getFormattedDuration(elapsed), 304, 27, 1);

    // Row 1: Wi-Fi & BLE counts (y=39)
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawString("Wi-Fi APs:", 14, 39);
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    tft.drawString(String(session.totalWifiObserved) + " observed", 78, 39);

    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawString("BLE Devs:", 164, 39);
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    tft.drawString(String(session.totalBleObserved) + " observed", 226, 39);

    // Row 2: Sub-GHz & nRF24 status (y=53)
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawString("Sub-GHz:", 14, 53);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawString(session.lastSubghz.detected ? session.lastSubghz.state : "IDLE", 78, 53);

    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawString("nRF24:", 164, 53);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawString(session.lastNrf24.detected ? session.lastNrf24.state : "IDLE", 226, 53);

    // Row 3: Event & Notes badge (y=67)
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawString("Events: " + String(session.totalEvents) + "  |  Notes: " + String(session.totalNotes) + 
                   (session.hasBaseline ? "  |  Baseline: ATTACHED" : "  |  Baseline: NONE"), 14, 67);

    // 2. Actions Prompt Card (x=6, y=94, w=308, h=58)
    drawCyberCard(6, 94, 308, 58, 0x4228, 0, "ACTIVE WORKSPACE");
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    tft.drawString("Press OK to open Investigation Actions Menu", 14, 110);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawString("Sweep Record  |  Timeline  |  Devices  |  Notes", 14, 126);

    drawCyberFooter("ROT LIVE", "OK WORKSPACE", "ESC BACK");
}

void investigation_active_dashboard() {
    returnToMenu = false;
    auto& mgr = InvestigationManager::getInstance();

    if (!mgr.hasActiveSession()) {
        displayInfo("No active investigation", true);
        return;
    }

    uint32_t lastHUD = 0;

    while (!returnToMenu) {
        if (check(EscPress)) {
            break;
        }

        // Live HUD update every 1000ms
        if (millis() - lastHUD >= 1000) {
            lastHUD = millis();
            drawActiveInvestigationHUD(mgr.getActiveSession());
        }

        if (check(SelPress)) {
            // Open workspace action menu
            options.clear();
            options.push_back({"Scan & Record Observations", [=, &mgr]() {
                tft.fillScreen(bruceConfig.bgColor);
                drawMainBorder();
                progressHandler(10, 100, "Sweep & Ingest Observations...");
                mgr.recordScanResults([](const String& stage, int progress) {
                    progressHandler(progress, 100, stage);
                });
                displaySuccess("Observations Ingested!", true);
            }});

            options.push_back({"Timeline (" + String(mgr.getActiveSession().totalEvents) + " Events)", [=, &mgr]() {
                investigation_timeline_view(mgr.getActiveSession(), false);
            }});

            options.push_back({"Devices (" + String(mgr.getActiveSession().observedDevices.size()) + " Unique)", [=, &mgr]() {
                investigation_devices_view(mgr.getActiveSession(), false);
            }});

            options.push_back({"Changes & Baseline Diff", [=, &mgr]() {
                investigation_changes_view(mgr.getActiveSession(), false);
            }});

            options.push_back({"Notes (" + String(mgr.getActiveSession().totalNotes) + ")", [=, &mgr]() {
                investigation_notes_view(mgr.getActiveSession(), false);
            }});

            options.push_back({"End Session", investigation_end_dialog});
            options.push_back({"Back to HUD", [=]() { return; }});

            loopOptions(options, MENU_TYPE_SUBMENU, "Investigation Actions");

            // Force HUD redraw when returning to dashboard
            lastHUD = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

void investigation_timeline_view(const InvestigationSession& session, bool isReadOnly) {
    returnToMenu = false;

    if (session.events.empty()) {
        displayInfo("No events recorded yet", true);
        return;
    }

    options.clear();
    // Show events in reverse chronological order (newest first)
    for (int i = (int)session.events.size() - 1; i >= 0; i--) {
        const auto& ev = session.events[i];
        String timeStr = "+" + InvestigationManager::getFormattedTime(ev.timestampMs);
        String tag = InvestigationManager::getEventTypeTag(ev.type);

        String title = truncateForWidth(ev.title, 90, FP);
        String label = timeStr + " " + tag + " " + title;
        while (label.length() < 24) label += " ";
        label += truncateForWidth(ev.summary, 90, FP);

        options.push_back({label, [ev]() {
            investigation_event_details(ev);
        }});
    }

    options.push_back({"Back", [=]() { return; }});

    loopOptions(options, MENU_TYPE_SUBMENU, "Event Timeline");
}

void investigation_event_details(const InvestigationEvent& ev) {
    tft.fillScreen(bruceConfig.bgColor);
    drawCyberHeader("EVENT // INSPECT", InvestigationManager::getEventTypeTag(ev.type), true);

    drawCyberCard(6, 23, 308, 129, bruceConfig.priColor, 0, "EVENT DETAILS");

    tft.setTextSize(FP);
    int y = 39;
    const int lineH = 14;

    // Time & Type
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawString("TIME:", 14, y);
    tft.setTextColor(0x07E0, bruceConfig.bgColor);
    tft.drawString("+" + InvestigationManager::getFormattedTime(ev.timestampMs) + " from start", 74, y);
    y += lineH;

    // Title
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawString("TITLE:", 14, y);
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    tft.drawString(truncateForWidth(ev.title, 220, FP), 74, y);
    y += lineH;

    // Summary
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawString("SUMMARY:", 14, y);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawString(truncateForWidth(ev.summary, 220, FP), 74, y);
    y += lineH + 2;

    // Details / Metadata
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawString("DATA:", 14, y);
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    
    // Print lines from details string without clipping outside card
    String remaining = ev.details;
    int maxLines = 4;
    while (remaining.length() > 0 && maxLines > 0) {
        int idx = remaining.indexOf('\n');
        String line = (idx >= 0) ? remaining.substring(0, idx) : remaining;
        tft.drawString(truncateForWidth(line, 220, FP), 74, y);
        y += lineH;
        maxLines--;
        if (idx >= 0) remaining = remaining.substring(idx + 1);
        else break;
    }

    drawCyberFooter("", "OK / ESC RETURN", "");

    delay(300);
    while (!check(SelPress) && !check(EscPress)) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void investigation_devices_view(const InvestigationSession& session, bool isReadOnly) {
    returnToMenu = false;

    if (session.observedDevices.empty()) {
        displayInfo("No observations recorded yet", true);
        return;
    }

    options.clear();
    for (size_t i = 0; i < session.observedDevices.size(); i++) {
        const auto& dev = session.observedDevices[i];
        String name = truncateForWidth(dev.name, 100, FP);
        String label = (dev.type == DEV_TYPE_WIFI_AP ? "[W] " : "[B] ") + name;
        while (label.length() < 20) label += " ";
        label += String(dev.rssi) + "dBm";

        options.push_back({label, [dev]() {
            analyzer_device_details(dev);
        }});
    }

    options.push_back({"Back", [=]() { return; }});

    loopOptions(options, MENU_TYPE_SUBMENU, "Observed Devices");
}

void investigation_changes_view(const InvestigationSession& session, bool isReadOnly) {
    analyzer_compare_view();
}

void investigation_notes_view(InvestigationSession& session, bool isReadOnly) {
    returnToMenu = false;

    options.clear();
    if (!isReadOnly) {
        options.push_back({"Add Note / Annotation", investigation_add_note_dialog});
    }

    if (session.notes.empty()) {
        if (isReadOnly) {
            displayInfo("No notes saved in this session", true);
            return;
        }
    } else {
        for (int i = (int)session.notes.size() - 1; i >= 0; i--) {
            const auto& nt = session.notes[i];
            String timeStr = "+" + InvestigationManager::getFormattedTime(nt.timestampMs);
            String label = timeStr + " - " + truncateForWidth(nt.text, 160, FP);
            options.push_back({label, [nt]() {
                String msg = "NOTE #" + String(nt.id) + "\nTime: +" + 
                             InvestigationManager::getFormattedTime(nt.timestampMs) + 
                             "\n\n" + nt.text;
                displayInfo(msg, true);
            }});
        }
    }

    options.push_back({"Back", [=]() { return; }});

    loopOptions(options, MENU_TYPE_SUBMENU, "Session Notes");
}

void investigation_add_note_dialog() {
    returnToMenu = false;

    options.clear();
    const char* quickNotes[] = {
        "Suspicious AP observed",
        "Strong BLE beacon detected",
        "Elevated Sub-GHz RF spike",
        "2.4GHz carrier activity",
        "Perimeter walk completed",
        "Device went offline",
        "High RSSI target identified",
        "Manual audit checkpoint"
    };

    for (int i = 0; i < 8; i++) {
        String txt = quickNotes[i];
        options.push_back({txt, [txt]() {
            InvestigationManager::getInstance().addNote(txt);
            displaySuccess("Note Recorded!", true);
        }});
    }

    options.push_back({"Cancel", [=]() { return; }});

    loopOptions(options, MENU_TYPE_SUBMENU, "Select Note / Tag");
}

void investigation_end_dialog() {
    returnToMenu = false;
    auto& mgr = InvestigationManager::getInstance();

    if (!mgr.hasActiveSession()) return;

    const auto& session = mgr.getActiveSession();
    uint32_t elapsed = mgr.getSessionElapsedMs();

    tft.fillScreen(bruceConfig.bgColor);
    drawCyberHeader("CONCLUDE // SESSION", session.name, true);

    drawCyberCard(6, 23, 308, 129, bruceConfig.priColor, 0, "SESSION SUMMARY");

    tft.setTextSize(FP);
    int y = 39;
    const int lineH = 15;

    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawString("DURATION:", 14, y);
    tft.setTextColor(0x07E0, bruceConfig.bgColor);
    tft.drawString(InvestigationManager::getFormattedDuration(elapsed), 94, y);
    y += lineH;

    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawString("TOTAL EVENTS:", 14, y);
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    tft.drawString(String(session.totalEvents), 94, y);
    y += lineH;

    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawString("Wi-Fi APs:", 14, y);
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    tft.drawString(String(session.totalWifiObserved), 94, y);
    y += lineH;

    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawString("BLE DEVS:", 14, y);
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    tft.drawString(String(session.totalBleObserved), 94, y);
    y += lineH;

    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawString("NOTES:", 14, y);
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    tft.drawString(String(session.totalNotes), 94, y);
    y += lineH;

    options.clear();
    options.push_back({"Save to LittleFS & End", [=, &mgr]() {
        mgr.endSession(true);
        displaySuccess("Investigation Saved!", true);
        returnToMenu = true;
    }});

    options.push_back({"Discard & End", [=, &mgr]() {
        mgr.discardActiveSession();
        displayInfo("Session discarded", true);
        returnToMenu = true;
    }});

    options.push_back({"Resume Session", [=]() { return; }});

    loopOptions(options, MENU_TYPE_SUBMENU, "Conclude Session?");
}

void investigation_saved_list() {
    returnToMenu = false;
    auto& mgr = InvestigationManager::getInstance();

    std::vector<InvestigationSummary> list = mgr.listSavedSessions();

    if (list.empty()) {
        displayInfo("No saved investigations", true);
        return;
    }

    options.clear();
    for (const auto& item : list) {
        String dur = InvestigationManager::getFormattedDuration(item.durationMs);
        String name = truncateForWidth(item.name, 70, FP);
        String label = item.id + " " + name;
        while (label.length() < 17) label += " ";
        label += dur + " (" + String(item.totalEvents) + "ev)";

        options.push_back({label, [item]() {
            investigation_saved_viewer(item.id);
        }});
    }

    options.push_back({"[Purge All Saved]", investigation_purge_dialog});
    options.push_back({"Back", [=]() { return; }});

    loopOptions(options, MENU_TYPE_SUBMENU, "Saved Investigations");
}

void investigation_saved_viewer(const String& id) {
    returnToMenu = false;
    auto& mgr = InvestigationManager::getInstance();

    InvestigationSession session;
    if (!mgr.loadSession(id, session)) {
        displayError("Failed to load session", true);
        return;
    }

    options.clear();
    options.push_back({"Timeline (" + String(session.totalEvents) + " Events)", [session]() {
        investigation_timeline_view(session, true);
    }});

    options.push_back({"Observed Devices (" + String(session.observedDevices.size()) + ")", [session]() {
        investigation_devices_view(session, true);
    }});

    options.push_back({"Notes (" + String(session.totalNotes) + ")", [session]() {
        InvestigationSession mutableSession = session;
        investigation_notes_view(mutableSession, true);
    }});

    options.push_back({"Delete Investigation", [=, &mgr]() {
        mgr.deleteSession(id);
        displaySuccess("Deleted " + id, true);
        returnToMenu = true;
    }});

    options.push_back({"Back", [=]() { return; }});

    String title = session.id + " // " + truncateForWidth(session.name, 110, FP);
    loopOptions(options, MENU_TYPE_SUBMENU, title.c_str());
}

void investigation_purge_dialog() {
    returnToMenu = false;
    auto& mgr = InvestigationManager::getInstance();
    auto& analyzer = WirelessAnalyzer::getInstance();
    auto& re = RiskEngine::getInstance();

    options.clear();
    options.push_back({"Confirm Purge (Erase All)", [=, &mgr, &analyzer, &re]() {
        tft.fillScreen(bruceConfig.bgColor);
        drawMainBorder();
        progressHandler(20, 100, "Purging Investigations...");
        mgr.deleteAllSessions();

        progressHandler(50, 100, "Purging Baseline & Reports...");
        analyzer.clearBaseline();
        analyzer.deleteReportFromLittleFS();
        analyzer.clearSnapshot();

        progressHandler(80, 100, "Purging Findings...");
        re.clearAllFindings();

        progressHandler(100, 100, "Purge Complete");
        vTaskDelay(pdMS_TO_TICKS(300));
        displaySuccess("All Intel Data Purged!", true);
        returnToMenu = true;
    }});

    options.push_back({"Erase Investigations Only", [=, &mgr]() {
        mgr.deleteAllSessions();
        displaySuccess("Investigations Purged", true);
    }});

    options.push_back({"Erase Baseline Only", [=, &analyzer]() {
        analyzer.clearBaseline();
        analyzer.deleteReportFromLittleFS();
        displaySuccess("Baseline Purged", true);
    }});

    options.push_back({"Erase Findings Only", [=, &re]() {
        re.clearAllFindings();
        displaySuccess("Findings Purged", true);
    }});

    options.push_back({"Cancel", [=]() { return; }});

    loopOptions(options, MENU_TYPE_SUBMENU, "Purge Intel Data");
}

