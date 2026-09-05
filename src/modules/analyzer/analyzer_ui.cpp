#include "analyzer_ui.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/scrollableTextArea.h"
#include "core/settings.h"
#include "core/utils.h"
#include "modules/NRF24/nrf_spectrum.h"
#include "modules/rf/rf_scan.h"
#include "modules/rf/rf_spectrum.h"
#include "modules/rf/rf_waterfall.h"
#include "modules/investigation/investigation_ui.h"

// Forward declarations
static void drawDashboardView(const AnalyzerSnapshot& snap, bool hasBaseline, int selectedSection = 0);

void analyzer_dashboard() {
    returnToMenu = false;
    auto& analyzer = WirelessAnalyzer::getInstance();

    // If no scan has been performed yet, run an initial scan with progress HUD
    if (analyzer.getLatestSnapshot().devices.empty() && 
        analyzer.getLatestSnapshot().wifi.state == "IDLE" && 
        analyzer.getLatestSnapshot().ble.state == "IDLE") {
        tft.fillScreen(bruceConfig.bgColor);
        drawMainBorder();
        progressHandler(10, 100, "Init Wireless Scanner...");
        analyzer.performUnifiedScan([](const String& stage, int progress) {
            progressHandler(progress, 100, stage);
        });
    }

    int selectedSection = 0; // 0: Wi-Fi (default), 1: BLE, 2: Sub-GHz, 3: nRF24, 4: Baseline
    const int numSections = 5;
    bool needsRedraw = true;
    uint32_t lastPulse = 0;

    while (!returnToMenu) {
        if (check(EscPress)) {
            break;
        }

        if (needsRedraw) {
            drawDashboardView(analyzer.getLatestSnapshot(), analyzer.hasBaseline(), selectedSection);
            needsRedraw = false;
        }

        // Sub-GHz live RSSI pulse sampling every 1000ms while on dashboard
        if (millis() - lastPulse > 1000) {
            lastPulse = millis();
            analyzer.sampleSubGhz();
            const auto& snap = analyzer.getLatestSnapshot();
            
            // Sub-GHz Card Content (clean refresh inside card bounds: x=8, y=80, w=144, h=22)
            tft.fillRect(10, 82, 142, 20, bruceConfig.bgColor);
            tft.setTextSize(FP);
            tft.setTextColor(snap.subghz.detected ? bruceConfig.priColor : 0x8430, bruceConfig.bgColor);
            tft.drawString(truncateForWidth(snap.subghz.state, 138, FP), 12, 82);
            tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
            tft.drawString(truncateForWidth(snap.subghz.info1 + " " + snap.subghz.info2, 138, FP), 12, 92);
        }

#ifdef HAS_ENCODER
        int32_t rotarySteps = drainRotarySteps();
        if (rotarySteps != 0) {
            check(PrevPress);
            check(NextPress);
            check(UpPress);
            check(DownPress);
            while (rotarySteps > 0) {
                selectedSection = (selectedSection - 1 + numSections) % numSections;
                rotarySteps--;
                needsRedraw = true;
            }
            while (rotarySteps < 0) {
                selectedSection = (selectedSection + 1) % numSections;
                rotarySteps++;
                needsRedraw = true;
            }
        }
#endif

        if (!needsRedraw) {
            if (check(NextPress) || check(DownPress)) {
                selectedSection = (selectedSection + 1) % numSections;
                needsRedraw = true;
            } else if (check(PrevPress) || check(UpPress)) {
                selectedSection = (selectedSection - 1 + numSections) % numSections;
                needsRedraw = true;
            }
        }

        if (check(SelPress)) {
            switch (selectedSection) {
                case 0: analyzer_wifi_section(); break;
                case 1: analyzer_ble_section(); break;
                case 2: analyzer_subghz_section(); break;
                case 3: analyzer_nrf24_section(); break;
                case 4: analyzer_baseline_menu(); break;
            }
            needsRedraw = true;
        }

        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

static void drawDashboardView(const AnalyzerSnapshot& snap, bool hasBaseline, int selectedSection) {
    tft.fillScreen(bruceConfig.bgColor);
    drawCyberHeader("ANALYZER", "OVERVIEW", true);

    // 1. Wi-Fi Card (Top-Left: x=6, y=24, w=150, h=39)
    drawCyberCard(6, 24, 150, 39, (selectedSection == 0) ? bruceConfig.priColor : 0x4228, 0, (selectedSection == 0) ? "> 802.11 WIFI" : "802.11 WIFI");
    if (selectedSection == 0) tft.drawRect(7, 25, 148, 37, bruceConfig.priColor);
    tft.setTextSize(FP);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawString(String(snap.wifi.count) + " APs Detected", 12, 39);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawString(truncateForWidth(snap.wifi.info2.length() > 0 ? snap.wifi.info2 : "CH 1-13 Idle", 138, FP), 12, 49);

    // 2. BLE Card (Top-Right: x=164, y=24, w=150, h=39)
    drawCyberCard(164, 24, 150, 39, (selectedSection == 1) ? bruceConfig.priColor : 0x4228, 0, (selectedSection == 1) ? "> BLUETOOTH LE" : "BLUETOOTH LE");
    if (selectedSection == 1) tft.drawRect(165, 25, 148, 37, bruceConfig.priColor);
    tft.setTextSize(FP);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawString(String(snap.ble.count) + " Devices", 170, 39);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawString(truncateForWidth(snap.ble.info2.length() > 0 ? snap.ble.info2 : "No Adv Signals", 138, FP), 170, 49);

    // 3. Sub-GHz CC1101 Card (Mid-Left: x=6, y=67, w=150, h=39)
    drawCyberCard(6, 67, 150, 39, (selectedSection == 2) ? bruceConfig.priColor : 0x4228, 0, (selectedSection == 2) ? "> SUB-GHZ RF" : "SUB-GHZ RF");
    if (selectedSection == 2) tft.drawRect(7, 68, 148, 37, bruceConfig.priColor);
    tft.setTextSize(FP);
    tft.setTextColor(snap.subghz.detected ? bruceConfig.priColor : 0x8430, bruceConfig.bgColor);
    tft.drawString(truncateForWidth(snap.subghz.state, 138, FP), 12, 82);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawString(truncateForWidth(snap.subghz.info1 + " " + snap.subghz.info2, 138, FP), 12, 92);

    // 4. nRF24 Card (Mid-Right: x=164, y=67, w=150, h=39)
    drawCyberCard(164, 67, 150, 39, (selectedSection == 3) ? bruceConfig.priColor : 0x4228, 0, (selectedSection == 3) ? "> NRF24 2.4G" : "NRF24 2.4G");
    if (selectedSection == 3) tft.drawRect(165, 68, 148, 37, bruceConfig.priColor);
    tft.setTextSize(FP);
    tft.setTextColor(snap.nrf24.detected ? bruceConfig.priColor : 0x8430, bruceConfig.bgColor);
    tft.drawString(truncateForWidth(snap.nrf24.state, 138, FP), 170, 82);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawString(truncateForWidth(snap.nrf24.info2, 138, FP), 170, 92);

    // 5. Baseline & Change Section (Bottom: x=6, y=110, w=308, h=38)
    drawCyberCard(6, 110, 308, 38, (selectedSection == 4) ? bruceConfig.priColor : 0x4228, 0, (selectedSection == 4) ? "> BASELINE & CHANGE" : "BASELINE & CHANGE");
    if (selectedSection == 4) tft.drawRect(7, 111, 306, 36, bruceConfig.priColor);

    tft.setTextSize(FP);
    if (hasBaseline) {
        auto& base = WirelessAnalyzer::getInstance().getBaseline();
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawString("STATUS:", 14, 128);
        tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
        tft.drawString("SAVED (" + String(base.wifiCount) + "W/" + String(base.bleCount) + "B)", 66, 128);

        int totalNew = snap.newWifiCount + snap.newBleCount;
        if (totalNew > 0) {
            tft.setTextColor(TFT_RED, bruceConfig.bgColor);
            tft.drawRightString("[!] +" + String(totalNew) + " NEW", 304, 128, 1);
        } else {
            tft.setTextColor(0x07E0, bruceConfig.bgColor); // Green
            tft.drawRightString("MATCH BASELINE", 304, 128, 1);
        }
    } else {
        tft.setTextColor(0x8430, bruceConfig.bgColor);
        tft.drawString("NO BASELINE SAVED", 14, 128);
        tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
        tft.drawRightString("[OK: CREATE]", 304, 128, 1);
    }

    drawCyberFooter("ROT SECTION", "OK OPEN", "ESC BACK");
}

void analyzer_wifi_section() {
    auto& analyzer = WirelessAnalyzer::getInstance();
    const auto& snap = analyzer.getLatestSnapshot();

    options.clear();
    int wifiCount = 0;
    for (size_t i = 0; i < snap.devices.size(); i++) {
        const auto& dev = snap.devices[i];
        if (dev.type != DEV_TYPE_WIFI_AP) continue;
        wifiCount++;

        String name = truncateForWidth(dev.name, 100, FP);
        while (name.length() < 17) name += " ";
        String label = name + String(dev.rssi) + "dBm CH" + String(dev.channel);
        if (dev.isNew) label += " [NEW]";

        options.push_back({label, [dev]() {
            analyzer_device_details(dev);
        }});
    }

    if (wifiCount == 0) {
        options.push_back({"Scan Wi-Fi Networks", [=, &analyzer]() {
            tft.fillScreen(bruceConfig.bgColor);
            drawMainBorder();
            progressHandler(10, 100, "Scanning Wi-Fi APs...");
            analyzer.performWifiScan();
            progressHandler(100, 100, "Wi-Fi Scan Done");
            vTaskDelay(pdMS_TO_TICKS(200));
            analyzer_wifi_section();
        }});
    } else {
        options.push_back({"Re-Scan Wi-Fi Networks", [=, &analyzer]() {
            tft.fillScreen(bruceConfig.bgColor);
            drawMainBorder();
            progressHandler(10, 100, "Scanning Wi-Fi APs...");
            analyzer.performWifiScan();
            progressHandler(100, 100, "Wi-Fi Scan Done");
            vTaskDelay(pdMS_TO_TICKS(200));
            analyzer_wifi_section();
        }});
    }

    options.push_back({"Wi-Fi Channel Map", analyzer_channel_map});
    options.push_back({"Back", nullptr});

    loopOptions(options, MENU_TYPE_SUBMENU, "Wi-Fi Networks");
}

void analyzer_ble_section() {
    auto& analyzer = WirelessAnalyzer::getInstance();
    const auto& snap = analyzer.getLatestSnapshot();

    options.clear();
    int bleCount = 0;
    for (size_t i = 0; i < snap.devices.size(); i++) {
        const auto& dev = snap.devices[i];
        if (dev.type != DEV_TYPE_BLE) continue;
        bleCount++;

        String name = truncateForWidth(dev.name, 120, FP);
        while (name.length() < 20) name += " ";
        String label = name + String(dev.rssi) + "dBm";
        if (dev.isNew) label += " [NEW]";

        options.push_back({label, [dev]() {
            analyzer_device_details(dev);
        }});
    }

    if (bleCount == 0) {
        options.push_back({"Scan BLE Devices", [=, &analyzer]() {
            tft.fillScreen(bruceConfig.bgColor);
            drawMainBorder();
            progressHandler(10, 100, "Scanning Bluetooth LE...");
            analyzer.performBleScan();
            progressHandler(100, 100, "BLE Scan Done");
            vTaskDelay(pdMS_TO_TICKS(200));
            analyzer_ble_section();
        }});
    } else {
        options.push_back({"Re-Scan BLE Devices", [=, &analyzer]() {
            tft.fillScreen(bruceConfig.bgColor);
            drawMainBorder();
            progressHandler(10, 100, "Scanning Bluetooth LE...");
            analyzer.performBleScan();
            progressHandler(100, 100, "BLE Scan Done");
            vTaskDelay(pdMS_TO_TICKS(200));
            analyzer_ble_section();
        }});
    }

    options.push_back({"Back", nullptr});

    loopOptions(options, MENU_TYPE_SUBMENU, "BLE Devices");
}

void analyzer_subghz_section() {
    auto& analyzer = WirelessAnalyzer::getInstance();

    options.clear();
    options.push_back({"CC1101 Spectrum Analyzer", rf_spectrum});
    options.push_back({"Frequency Scanner", [=]() { rf_scan(0, 0, -1); }});
    options.push_back({"Waterfall Display", rf_waterfall});
    options.push_back({"Sample Signal & RSSI", [=, &analyzer]() {
        analyzer.sampleSubGhz();
        const auto& snap = analyzer.getLatestSnapshot();
        String msg = "State: " + snap.subghz.state + "\nFreq: " + snap.subghz.info1 + "\nSignal: " + snap.subghz.info2;
        displayInfo(msg, true);
    }});
    options.push_back({"Back", nullptr});

    loopOptions(options, MENU_TYPE_SUBMENU, "Sub-GHz Analyzer");
}

void analyzer_nrf24_section() {
    auto& analyzer = WirelessAnalyzer::getInstance();

    options.clear();
    options.push_back({"nRF24 Spectrum Analyzer", nrf_spectrum});
    options.push_back({"Active Channels Scan", [=, &analyzer]() {
        analyzer.sampleNrf24();
        const auto& snap = analyzer.getLatestSnapshot();
        String msg = "State: " + snap.nrf24.state + "\nInfo: " + snap.nrf24.info2;
        displayInfo(msg, true);
    }});
    options.push_back({"Back", nullptr});

    loopOptions(options, MENU_TYPE_SUBMENU, "nRF24 Analyzer");
}

void analyzer_overview_section() {
    auto& analyzer = WirelessAnalyzer::getInstance();

    options.clear();
    options.push_back({"Run Full Environment Scan", [=, &analyzer]() {
        tft.fillScreen(bruceConfig.bgColor);
        drawMainBorder();
        progressHandler(10, 100, "Scanning Environment...");
        analyzer.performUnifiedScan([](const String& stage, int progress) {
            progressHandler(progress, 100, stage);
        });
    }});
    options.push_back({"Investigation Mode", investigation_home});
    options.push_back({"Baseline Management", analyzer_baseline_menu});
    options.push_back({"Live Monitor Mode", analyzer_live_monitor});
    options.push_back({"Device Explorer (" + String(analyzer.getLatestSnapshot().devices.size()) + ")", analyzer_device_explorer});
    options.push_back({"Wi-Fi Channel Map", analyzer_channel_map});
    options.push_back({"Session Report (LittleFS)", analyzer_report_menu});
    options.push_back({"Back", nullptr});

    loopOptions(options, MENU_TYPE_SUBMENU, "Analyzer Actions");
}

void analyzer_baseline_menu() {
    returnToMenu = false;
    auto& analyzer = WirelessAnalyzer::getInstance();
    const auto& base = analyzer.getBaseline();

    options.clear();

    if (!analyzer.hasBaseline()) {
        options.push_back({"Create Baseline (Scan Now)", [=, &analyzer]() {
            tft.fillScreen(bruceConfig.bgColor);
            drawMainBorder();
            progressHandler(10, 100, "Capturing Baseline...");
            analyzer.performUnifiedScan([](const String& stage, int progress) {
                progressHandler(progress, 100, stage);
            });
            analyzer.setBaseline();
            displaySuccess("Baseline saved to LittleFS!", true);
            analyzer_baseline_menu();
        }});
        options.push_back({"Save Current Scan as Baseline", [=, &analyzer]() {
            analyzer.setBaseline();
            displaySuccess("Saved to LittleFS!", true);
            analyzer_baseline_menu();
        }});
    } else {
        options.push_back({"Compare Changes", analyzer_compare_view});
        options.push_back({"Update Baseline (Scan Now)", [=, &analyzer]() {
            tft.fillScreen(bruceConfig.bgColor);
            drawMainBorder();
            progressHandler(10, 100, "Updating Baseline...");
            analyzer.performUnifiedScan([](const String& stage, int progress) {
                progressHandler(progress, 100, stage);
            });
            analyzer.setBaseline();
            displaySuccess("Baseline updated in LittleFS!", true);
            analyzer_baseline_menu();
        }});
        options.push_back({"Delete Baseline", [=, &analyzer]() {
            analyzer.clearBaseline();
            displayInfo("Baseline deleted", true);
            analyzer_baseline_menu();
        }});
    }

    options.push_back({"Back", nullptr});

    String title = analyzer.hasBaseline() ? 
        ("Baseline [" + String(base.wifiCount) + "W/" + String(base.bleCount) + "B]") : 
        "Baseline (Empty)";
    loopOptions(options, MENU_TYPE_SUBMENU, title.c_str());
}

void analyzer_compare_view() {
    returnToMenu = false;
    auto& analyzer = WirelessAnalyzer::getInstance();

    if (!analyzer.hasBaseline()) {
        displayInfo("No baseline saved yet", true);
        return;
    }

    // Perform live scan before comparison to ensure freshest telemetry
    tft.fillScreen(bruceConfig.bgColor);
    drawMainBorder();
    progressHandler(10, 100, "Comparing with Baseline...");
    analyzer.performUnifiedScan([](const String& stage, int progress) {
        progressHandler(progress, 100, stage);
    });

    ComparisonResult diff = analyzer.compareWithBaseline();

    options.clear();

    // 1. Wi-Fi Option
    String wifiLabel = "Wi-Fi: ";
    if (diff.wifiChanges.empty()) {
        wifiLabel += "No Changes (" + String(analyzer.getLatestSnapshot().wifi.count) + " APs)";
    } else {
        wifiLabel += "+" + String(diff.wifiNewCount) + " New, -" + String(diff.wifiGoneCount) + " Gone";
        if (diff.wifiModCount > 0) wifiLabel += ", ~" + String(diff.wifiModCount) + " Chg";
    }
    options.push_back({wifiLabel, [diff]() {
        if (diff.wifiChanges.empty()) {
            displayInfo("Wi-Fi matches baseline", true);
            return;
        }
        options.clear();
        for (const auto& chg : diff.wifiChanges) {
            String tag = (chg.changeType == CHANGE_NEW) ? "[+] NEW " : 
                         (chg.changeType == CHANGE_GONE) ? "[-] GONE " : "[~] CHG ";
            String label = tag + chg.name.substring(0, 12);
            while (label.length() < 22) label += " ";
            label += chg.details.substring(0, 18);
            options.push_back({label, [chg]() {
                String msg = "DEVICE: " + chg.name + "\nBSSID: " + chg.identifier + "\n";
                if (chg.changeType == CHANGE_NEW) {
                    msg += "STATUS: Newly Discovered\n" + chg.details;
                } else if (chg.changeType == CHANGE_GONE) {
                    msg += "STATUS: Departed / Offline\n" + chg.details;
                } else {
                    msg += "STATUS: Modified\n" + chg.details;
                }
                displayInfo(msg, true);
            }});
        }
        options.push_back({"Back", nullptr});
        loopOptions(options, MENU_TYPE_SUBMENU, "Wi-Fi Changes");
    }});

    // 2. BLE Option
    String bleLabel = "BLE: ";
    if (diff.bleChanges.empty()) {
        bleLabel += "No Changes (" + String(analyzer.getLatestSnapshot().ble.count) + " Devs)";
    } else {
        bleLabel += "+" + String(diff.bleNewCount) + " New, -" + String(diff.bleGoneCount) + " Gone";
        if (diff.bleModCount > 0) bleLabel += ", ~" + String(diff.bleModCount) + " Chg";
    }
    options.push_back({bleLabel, [diff]() {
        if (diff.bleChanges.empty()) {
            displayInfo("BLE matches baseline", true);
            return;
        }
        options.clear();
        for (const auto& chg : diff.bleChanges) {
            String tag = (chg.changeType == CHANGE_NEW) ? "[+] NEW " : 
                         (chg.changeType == CHANGE_GONE) ? "[-] GONE " : "[~] CHG ";
            String label = tag + chg.name.substring(0, 12);
            while (label.length() < 22) label += " ";
            label += chg.details.substring(0, 18);
            options.push_back({label, [chg]() {
                String msg = "DEVICE: " + chg.name + "\nADDR: " + chg.identifier + "\n";
                if (chg.changeType == CHANGE_NEW) {
                    msg += "STATUS: Newly Discovered\n" + chg.details;
                } else if (chg.changeType == CHANGE_GONE) {
                    msg += "STATUS: Departed / Offline\n" + chg.details;
                } else {
                    msg += "STATUS: Modified\n" + chg.details;
                }
                displayInfo(msg, true);
            }});
        }
        options.push_back({"Back", nullptr});
        loopOptions(options, MENU_TYPE_SUBMENU, "BLE Changes");
    }});

    // 3. Sub-GHz Option
    String subghzLabel = "Sub-GHz: " + diff.subghzChange.description;
    options.push_back({subghzLabel, [diff]() {
        String msg = "SUB-GHZ RF COMPARISON\n\n";
        msg += "Baseline: " + diff.subghzChange.baselineState + "\n";
        msg += "Current:  " + diff.subghzChange.currentState + "\n\n";
        msg += "Result: " + diff.subghzChange.description;
        displayInfo(msg, true);
    }});

    // 4. nRF24 Option
    String nrfLabel = "nRF24: " + diff.nrf24Change.description;
    options.push_back({nrfLabel, [diff]() {
        String msg = "nRF24 BAND COMPARISON\n\n";
        msg += "Baseline: " + diff.nrf24Change.baselineState + "\n";
        msg += "Current:  " + diff.nrf24Change.currentState + "\n\n";
        msg += "Result: " + diff.nrf24Change.description;
        displayInfo(msg, true);
    }});

    options.push_back({"Back", nullptr});

    loopOptions(options, MENU_TYPE_SUBMENU, "Change Detection");
}

void analyzer_live_monitor() {
    returnToMenu = false;
    auto& analyzer = WirelessAnalyzer::getInstance();

    tft.fillScreen(bruceConfig.bgColor);
    drawCyberHeader("ANALYZER // MONITOR", "SWEEP #1", true);
    drawCyberFooter("ROT PAUSE", "OK RESCAN", "ESC EXIT");

    uint32_t lastFastSweep = 0;
    uint32_t lastSlowSweep = 0;
    uint32_t cycleCount = 0;

    while (!returnToMenu) {
        if (check(EscPress)) {
            break;
        }

        uint32_t now = millis();

        // Fast sweep (Sub-GHz & nRF24): every 600ms
        if (now - lastFastSweep >= 600) {
            lastFastSweep = now;
            analyzer.sampleSubGhz();
            analyzer.sampleNrf24();
        }

        // Slow sweep (Wi-Fi & BLE): every 3500ms
        if (now - lastSlowSweep >= 3500) {
            lastSlowSweep = now;
            cycleCount++;
            drawCyberHeader("ANALYZER // MONITOR", "SWEEP #" + String(cycleCount), true);

            analyzer.performWifiScan();
            analyzer.performBleScan();

            // Re-evaluate baseline diff
            if (analyzer.hasBaseline()) {
                auto& snap = analyzer.getLatestSnapshot();
                snap.newWifiCount = 0;
                snap.newBleCount = 0;
                const auto& base = analyzer.getBaseline();
                for (auto& dev : snap.devices) {
                    bool found = false;
                    for (const auto& bId : base.knownIdentifiers) {
                        if (dev.identifier.equalsIgnoreCase(bId)) {
                            found = true;
                            break;
                        }
                    }
                    dev.isNew = !found;
                    if (dev.isNew) {
                        if (dev.type == DEV_TYPE_WIFI_AP) snap.newWifiCount++;
                        else if (dev.type == DEV_TYPE_BLE) snap.newBleCount++;
                    }
                }
            }
        }

        const auto& snap = analyzer.getLatestSnapshot();
        drawCyberCard(6, 23, 308, 129, bruceConfig.priColor, 0, "LIVE TELEMETRY");

        tft.setTextSize(FP);
        int y = 40;
        const int lineH = 22;

        // 1. Wi-Fi
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawString("Wi-Fi:", 14, y);
        int wifiFill = map(constrain((int)snap.wifi.count, 0, 20), 0, 20, 0, 80);
        tft.drawRect(70, y + 2, 82, 7, bruceConfig.priColor);
        tft.fillRect(71, y + 3, wifiFill, 5, bruceConfig.priColor);
        tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
        tft.drawString(String(snap.wifi.count) + " APs", 160, y);
        tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
        tft.drawString(truncateForWidth(snap.wifi.info2, 90, FP), 220, y);
        y += lineH;

        // 2. BLE
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawString("BLE:", 14, y);
        int bleFill = map(constrain((int)snap.ble.count, 0, 20), 0, 20, 0, 80);
        tft.drawRect(70, y + 2, 82, 7, bruceConfig.priColor);
        tft.fillRect(71, y + 3, bleFill, 5, bruceConfig.priColor);
        tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
        tft.drawString(String(snap.ble.count) + " DEVs", 160, y);
        tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
        tft.drawString(truncateForWidth(snap.ble.info2, 90, FP), 220, y);
        y += lineH;

        // 3. Sub-GHz
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawString("Sub-GHz:", 14, y);
        int rfFill = snap.subghz.detected ? 70 : 10;
        tft.drawRect(70, y + 2, 82, 7, snap.subghz.detected ? 0x07E0 : 0x4228);
        tft.fillRect(71, y + 3, rfFill, 5, snap.subghz.detected ? 0x07E0 : 0x4228);
        tft.setTextColor(snap.subghz.detected ? 0x07E0 : 0x8430, bruceConfig.bgColor);
        tft.drawString(snap.subghz.state, 160, y);
        tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
        tft.drawString(truncateForWidth(snap.subghz.info1, 90, FP), 220, y);
        y += lineH;

        // 4. nRF24
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawString("nRF24:", 14, y);
        int nrfFill = snap.nrf24.detected ? 70 : 10;
        tft.drawRect(70, y + 2, 82, 7, snap.nrf24.detected ? 0x07E0 : 0x4228);
        tft.fillRect(71, y + 3, nrfFill, 5, snap.nrf24.detected ? 0x07E0 : 0x4228);
        tft.setTextColor(snap.nrf24.detected ? 0x07E0 : 0x8430, bruceConfig.bgColor);
        tft.drawString(snap.nrf24.state, 160, y);
        tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
        tft.drawString(truncateForWidth(snap.nrf24.info2, 90, FP), 220, y);
        y += lineH;

        // Summary footer inside card
        tft.drawFastHLine(10, 130, 300, 0x31A6);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawString("BASELINE:", 14, 136);
        if (analyzer.hasBaseline()) {
            int totalNew = snap.newWifiCount + snap.newBleCount;
            if (totalNew > 0) {
                tft.setTextColor(TFT_RED, bruceConfig.bgColor);
                tft.drawString("[!] +" + String(totalNew) + " NEW SIGNALS OBSERVED", 80, 136);
            } else {
                tft.setTextColor(0x07E0, bruceConfig.bgColor);
                tft.drawString("MATCHING RECORDED BASELINE", 80, 136);
            }
        } else {
            tft.setTextColor(0x8430, bruceConfig.bgColor);
            tft.drawString("NO BASELINE SAVED", 80, 136);
        }

        drawCyberFooter("ROT PAUSE", "OK RESCAN", "ESC EXIT");
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void analyzer_device_explorer() {
    returnToMenu = false;
    auto& analyzer = WirelessAnalyzer::getInstance();
    const auto& snap = analyzer.getLatestSnapshot();

    if (snap.devices.empty()) {
        displayInfo("No devices scanned yet", true);
        return;
    }

    options.clear();
    for (size_t i = 0; i < snap.devices.size(); i++) {
        const auto& dev = snap.devices[i];
        String name = truncateForWidth(dev.name, 100, FP);
        String label = (dev.type == DEV_TYPE_WIFI_AP ? "[W] " : "[B] ") + name;
        while (label.length() < 20) label += " ";
        label += String(dev.rssi) + "dBm";
        if (dev.isNew) label += " [NEW]";

        options.push_back({label, [dev]() {
            analyzer_device_details(dev);
        }});
    }
    options.push_back({"Back", nullptr});

    loopOptions(options, MENU_TYPE_SUBMENU, "Device Explorer");
}

void analyzer_device_details(const WirelessDevice& dev) {
    tft.fillScreen(bruceConfig.bgColor);
    drawCyberHeader("DEVICE // INSPECT", dev.type == DEV_TYPE_WIFI_AP ? "802.11 AP" : "BLE DEV", true);

    drawCyberCard(6, 23, 308, 129, bruceConfig.priColor, 0, "SPECIFICATIONS");

    tft.setTextSize(FP);
    int y = 39;
    const int lineH = 15;

    // 1. Name
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawString("NAME:", 14, y);
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    tft.drawString(truncateForWidth(dev.name, 220, FP), 84, y);
    y += lineH;

    // 2. Type & Identifier
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawString("TYPE:", 14, y);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawString(dev.type == DEV_TYPE_WIFI_AP ? "Wi-Fi 802.11 AP" : "Bluetooth LE", 84, y);
    y += lineH;

    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawString("ADDR:", 14, y);
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    tft.drawString(dev.identifier, 84, y);
    y += lineH;

    // 3. Signal RSSI with visual bar
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawString("RSSI:", 14, y);
    tft.setTextColor(TFT_WHITE, bruceConfig.bgColor);
    tft.drawString(String(dev.rssi) + " dBm", 84, y);

    // RSSI mini gauge (no collision with text)
    int gaugeX = 154;
    int gaugeW = 60;
    int gaugeH = 6;
    tft.drawRect(gaugeX, y + 2, gaugeW, gaugeH, bruceConfig.priColor);
    int fillW = map(constrain((int)dev.rssi, -100, -30), -100, -30, 0, gaugeW - 2);
    tft.fillRect(gaugeX + 1, y + 3, fillW, gaugeH - 2, bruceConfig.priColor);
    y += lineH;

    // 4. Channel & Extra
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawString("CHANNEL:", 14, y);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    if (dev.type == DEV_TYPE_WIFI_AP) {
        tft.drawString(String(dev.channel) + " (2.4 GHz Band)", 84, y);
    } else {
        tft.drawString("2.4 GHz BLE Adv", 84, y);
    }
    y += lineH;

    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawString("EXTRA:", 14, y);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawString(truncateForWidth(dev.extra.length() > 0 ? dev.extra : "None", 220, FP), 84, y);
    y += lineH;

    // 5. Baseline status
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawString("STATUS:", 14, y);
    if (dev.isNew) {
        tft.setTextColor(TFT_RED, bruceConfig.bgColor);
        tft.drawString("NEW (NOT IN BASELINE)", 84, y);
    } else {
        tft.setTextColor(0x07E0, bruceConfig.bgColor);
        tft.drawString("RECORDED BASELINE", 84, y);
    }

    drawCyberFooter("", "OK / ESC RETURN", "");

    delay(300);
    while (!check(SelPress) && !check(EscPress)) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void analyzer_channel_map() {
    returnToMenu = false;
    auto& analyzer = WirelessAnalyzer::getInstance();
    const auto& snap = analyzer.getLatestSnapshot();

    tft.fillScreen(bruceConfig.bgColor);
    drawCyberHeader("WIFI // CHANNELS", "1-13 SPECTRUM", true);

    drawCyberCard(6, 23, 308, 129, bruceConfig.priColor, 0, "2.4GHz CONGESTION");

    // Find max AP count across channels
    uint8_t maxCount = 1;
    uint8_t congestedCh = 1;
    for (int ch = 1; ch <= 13; ch++) {
        if (snap.wifiChannelDist[ch] > maxCount) {
            maxCount = snap.wifiChannelDist[ch];
            congestedCh = ch;
        }
    }

    // Recommendation summary at top
    uint8_t bestCh = 1;
    uint8_t minCount = snap.wifiChannelDist[1];
    if (snap.wifiChannelDist[6] < minCount) { minCount = snap.wifiChannelDist[6]; bestCh = 6; }
    if (snap.wifiChannelDist[11] < minCount) { minCount = snap.wifiChannelDist[11]; bestCh = 11; }

    tft.setTextSize(FP);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawString("Best: CH " + String(bestCh) + "  |  Busy: CH " + String(congestedCh), 14, 38);

    // Draw 13 channel bars
    const int startX = 18;
    const int startY = 122;
    const int maxBarH = 54;
    const int barWidth = 16;
    const int gap = 6;

    for (int ch = 1; ch <= 13; ch++) {
        int x = startX + (ch - 1) * (barWidth + gap);
        uint8_t count = snap.wifiChannelDist[ch];
        int barH = (count * maxBarH) / maxCount;
        if (count > 0 && barH < 4) barH = 4;

        // Bar frame
        tft.drawRect(x, startY - barH, barWidth, barH, bruceConfig.priColor);
        if (ch == 1 || ch == 6 || ch == 11) {
            // Main non-overlapping channels highlighted
            tft.fillRect(x + 1, startY - barH + 1, barWidth - 2, barH - 2, bruceConfig.priColor);
        } else {
            tft.fillRect(x + 1, startY - barH + 1, barWidth - 2, barH - 2, 0x4228);
        }

        // Count above bar
        if (count > 0) {
            tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
            tft.drawCentreString(String(count), x + barWidth / 2, startY - barH - 10, 1);
        }

        // Channel label below
        tft.setTextColor(ch == congestedCh ? TFT_RED : bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawCentreString(String(ch), x + barWidth / 2, startY + 3, 1);
    }

    drawCyberFooter("", "OK / ESC RETURN", "");

    delay(300);
    while (!check(SelPress) && !check(EscPress)) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void analyzer_report_menu() {
    returnToMenu = false;
    auto& analyzer = WirelessAnalyzer::getInstance();

    options.clear();
    options.push_back({"Save Report to LittleFS", [=, &analyzer]() {
        if (analyzer.saveReportToLittleFS("/analyzer_report.txt")) {
            displaySuccess("Saved to /analyzer_report.txt", true);
        } else {
            displayError("Failed to save report", true);
        }
    }});

    options.push_back({"View Live Report", [=, &analyzer]() {
        ScrollableTextArea sta("SESSION REPORT");
        sta.fromString(analyzer.getSessionReportText());
        sta.show();
    }});

    if (analyzer.hasSavedReport("/analyzer_report.txt")) {
        options.push_back({"View Saved Report", [=]() {
            File f = LittleFS.open("/analyzer_report.txt", "r");
            if (f) {
                String content = f.readString();
                f.close();
                ScrollableTextArea sta("SAVED REPORT");
                sta.fromString(content);
                sta.show();
            } else {
                displayError("Failed to open report file", true);
            }
        }});
    }

    options.push_back({"Back", [=]() { return; }});

    loopOptions(options, MENU_TYPE_SUBMENU, "Session Report");
}

