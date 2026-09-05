#include "findings_ui.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/settings.h"
#include "core/utils.h"
#include "modules/analyzer/wireless_analyzer.h"
#include "modules/investigation/investigation_manager.h"
#include "modules/investigation/investigation_ui.h"

void findings_home() {
    returnToMenu = false;
    auto& re = RiskEngine::getInstance();

    // Auto-evaluate current live observations if any
    auto& analyzer = WirelessAnalyzer::getInstance();
    const auto& snap = analyzer.getLatestSnapshot();
    if (!snap.devices.empty()) {
        re.evaluateSnapshot(snap, analyzer.hasBaseline());
    }

    auto& inv = InvestigationManager::getInstance();
    if (inv.hasActiveSession()) {
        re.evaluateInvestigation(inv.getActiveSession());
    }

    options.clear();

    std::vector<SecurityFinding> list = re.getFindings();

    if (list.empty()) {
        options.push_back({"Evaluate Wireless Environment", [=, &re, &analyzer]() {
            tft.fillScreen(bruceConfig.bgColor);
            drawMainBorder();
            progressHandler(10, 100, "Scanning & Evaluating...");
            analyzer.performUnifiedScan([](const String& stage, int progress) {
                progressHandler(progress, 100, stage);
            });
            re.evaluateSnapshot(analyzer.getLatestSnapshot(), analyzer.hasBaseline());
            displaySuccess("Evaluation Complete", true);
            findings_home();
        }});
        if (inv.hasActiveSession()) {
            options.push_back({"Evaluate Active Investigation", [=, &re, &inv]() {
                re.evaluateInvestigation(inv.getActiveSession());
                displaySuccess("Investigation Evaluated", true);
                findings_home();
            }});
        }
        options.push_back({"Purge Stored Intel", investigation_purge_dialog});
        addOptionToMainMenu();
        loopOptions(options, MENU_TYPE_SUBMENU, "Findings (0)");
        return;
    }

    // Populate findings in list
    for (const auto& item : list) {
        String rTag = String(RiskEngine::getRiskLabel(item.risk));
        while (rTag.length() < 5) rTag += " ";

        String title = truncateForWidth(item.title, 90, FP);
        String tgt = truncateForWidth(item.target, 70, FP);

        String label = rTag + " " + title;
        while (label.length() < 22) label += " ";
        label += tgt;

        if (item.acknowledged) {
            label += " [ACK]";
        }

        options.push_back({label, [item]() {
            finding_details_view(item);
        }});
    }

    // Management options at the bottom of the list
    options.push_back({"Rescan & Evaluate", [=, &re, &analyzer]() {
        tft.fillScreen(bruceConfig.bgColor);
        drawMainBorder();
        progressHandler(10, 100, "Scanning & Evaluating...");
        analyzer.performUnifiedScan([](const String& stage, int progress) {
            progressHandler(progress, 100, stage);
        });
        re.evaluateSnapshot(analyzer.getLatestSnapshot(), analyzer.hasBaseline());
        displaySuccess("Risk Engine Evaluated", true);
        findings_home();
    }});

    options.push_back({"Clear All Findings", [=, &re]() {
        re.clearAllFindings();
        displayInfo("Findings Cleared", true);
        findings_home();
    }});

    options.push_back({"Purge All Intel Data", investigation_purge_dialog});

    addOptionToMainMenu();

    String title = "FINDINGS [" + String(list.size()) + "] // " + String(RiskEngine::getRiskLabel(re.getHighestRisk()));
    loopOptions(options, MENU_TYPE_SUBMENU, title.c_str());
}

void finding_details_view(const SecurityFinding& finding) {
    auto& re = RiskEngine::getInstance();
    SecurityFinding cur = finding;
    re.getFindingById(finding.id, cur);

    bool inDetails = true;
    while (inDetails && !returnToMenu) {
        tft.fillScreen(bruceConfig.bgColor);
        drawCyberHeader("FINDING // " + cur.id, RiskEngine::getRiskLabel(cur.risk), true);

        uint16_t cardBorder = RiskEngine::getRiskColor(cur.risk);
        drawCyberCard(6, 23, 308, 129, cardBorder, 0, RiskEngine::getCategoryLabel(cur.category));

        tft.setTextSize(FP);
        int y = 39;
        const int lineH = 14;

        // 1. Title & Risk
        tft.setTextColor(cardBorder, bruceConfig.bgColor);
        tft.drawString("TITLE:", 14, y);
        tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
        tft.drawString(truncateForWidth(cur.title, 220, FP), 74, y);
        y += lineH;

        // 2. Target
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawString("TARGET:", 14, y);
        tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
        tft.drawString(truncateForWidth(cur.target, 220, FP), 74, y);
        y += lineH;

        // 3. Source & Signal
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawString("SOURCE:", 14, y);
        tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
        tft.drawString(truncateForWidth(cur.source, 140, FP), 74, y);

        if (cur.rssi != 0) {
            tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
            tft.drawString("RSSI:", 220, y);
            tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
            tft.drawString(String(cur.rssi) + "dBm", 256, y);
        }
        y += lineH;

        // 4. Status
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawString("STATUS:", 14, y);
        if (cur.acknowledged) {
            tft.setTextColor(0x07E0, bruceConfig.bgColor);
            tft.drawString("ACKNOWLEDGED [OK]", 74, y);
        } else {
            tft.setTextColor(0xF800, bruceConfig.bgColor);
            tft.drawString("UNACKNOWLEDGED", 74, y);
        }
        y += lineH + 2;

        // 5. Evidence
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawString("REASON:", 14, y);
        tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);

        String remaining = cur.evidence.length() > 0 ? cur.evidence : cur.description;
        int maxLines = 3;
        while (remaining.length() > 0 && maxLines > 0) {
            int idx = remaining.indexOf('\n');
            String line = (idx >= 0) ? remaining.substring(0, idx) : remaining;
            tft.drawString(truncateForWidth(line, 224, FP), 74, y);
            y += lineH;
            maxLines--;
            if (idx >= 0) remaining = remaining.substring(idx + 1);
            else break;
        }

        // 6. Analyst Note
        if (cur.note.length() > 0 && y <= 136) {
            tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
            tft.drawString("NOTE: " + truncateForWidth(cur.note, 220, FP), 14, y);
        }

        drawCyberFooter("ROT ACTIONS", "OK ACK/UNACK", "ESC BACK");

        vTaskDelay(pdMS_TO_TICKS(300));

        while (true) {
            if (check(EscPress)) {
                inDetails = false;
                break;
            }

            if (check(SelPress)) {
                // Quick toggle acknowledge
                re.toggleAcknowledge(cur.id);
                re.getFindingById(cur.id, cur);
                break; // Re-render card
            }

#ifdef HAS_ENCODER
            int32_t rotarySteps = drainRotarySteps();
            if (rotarySteps != 0 || check(NextPress) || check(PrevPress)) {
                // Open actions menu for this finding
                options.clear();
                String ackLabel = cur.acknowledged ? "Mark Unacknowledged" : "Acknowledge Finding";
                options.push_back({ackLabel, [=, &re, &cur]() {
                    re.toggleAcknowledge(cur.id);
                    re.getFindingById(cur.id, cur);
                }});

                options.push_back({"Add / Update Note", [cur]() {
                    finding_add_note_dialog(cur.id);
                }});

                options.push_back({"Delete Finding", [=, &re, &inDetails]() {
                    re.deleteFinding(cur.id);
                    displaySuccess("Finding Deleted", true);
                    inDetails = false;
                }});

                options.push_back({"Back to Inspect", [=]() { return; }});

                loopOptions(options, MENU_TYPE_SUBMENU, "Finding Actions");
                re.getFindingById(cur.id, cur);
                break; // Re-render card
            }
#endif

            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

void finding_add_note_dialog(const String& findingId) {
    returnToMenu = false;
    auto& re = RiskEngine::getInstance();

    options.clear();
    const char* presetNotes[] = {
        "Verified False Positive",
        "Authorized Infrastructure",
        "Rogue Device Suspect",
        "Under Active Audit",
        "High Priority Target",
        "Temporary Test Asset",
        "Cleared - Known Signal"
    };

    for (int i = 0; i < 7; i++) {
        String txt = presetNotes[i];
        options.push_back({txt, [=, &re]() {
            re.addNoteToFinding(findingId, txt);
            displaySuccess("Note Saved!", true);
        }});
    }

    options.push_back({"Cancel", [=]() { return; }});

    loopOptions(options, MENU_TYPE_SUBMENU, "Annotate Finding");
}
