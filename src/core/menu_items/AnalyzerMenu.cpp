#include "AnalyzerMenu.h"
#include "core/display.h"
#include "core/utils.h"
#include "modules/analyzer/analyzer_ui.h"
#include "modules/analyzer/wireless_analyzer.h"

void AnalyzerMenu::optionsMenu() {
    options.clear();
    options.push_back({"Live Dashboard", analyzer_dashboard});
    options.push_back({"Run Full Scan", [=]() {
        tft.fillScreen(bruceConfig.bgColor);
        drawMainBorder();
        progressHandler(10, 100, "Scanning Environment...");
        WirelessAnalyzer::getInstance().performUnifiedScan([](const String& stage, int progress) {
            progressHandler(progress, 100, stage);
        });
        analyzer_dashboard();
    }});
    options.push_back({"Live Monitor", analyzer_live_monitor});
    options.push_back({"Baseline & Changes", analyzer_baseline_menu});
    options.push_back({"Device Explorer", analyzer_device_explorer});
    options.push_back({"Wi-Fi Channel Map", analyzer_channel_map});
    options.push_back({"Session Report", analyzer_report_menu});

    addOptionToMainMenu();
    loopOptions(options, MENU_TYPE_SUBMENU, "Wireless Analyzer");
}

void AnalyzerMenu::drawIcon(float scale) {
    clearIconArea();

    int radius1 = scale * 8;
    int radius2 = scale * 16;
    int radius3 = scale * 24;

    // Center sweep blip
    tft.fillCircle(iconCenterX, iconCenterY, scale * 3, bruceConfig.priColor);

    // Radar concentric grid rings
    tft.drawCircle(iconCenterX, iconCenterY, radius1, bruceConfig.priColor);
    tft.drawCircle(iconCenterX, iconCenterY, radius2, bruceConfig.priColor);
    tft.drawCircle(iconCenterX, iconCenterY, radius3, bruceConfig.priColor);

    // Crosshairs
    tft.drawFastHLine(iconCenterX - radius3 - 4, iconCenterY, (radius3 + 4) * 2, bruceConfig.priColor);
    tft.drawFastVLine(iconCenterX, iconCenterY - radius3 - 4, (radius3 + 4) * 2, bruceConfig.priColor);

    // Diagonal sweep beam
    int sweepX = iconCenterX + (radius2 * 7) / 10;
    int sweepY = iconCenterY - (radius2 * 7) / 10;
    tft.drawLine(iconCenterX, iconCenterY, sweepX, sweepY, bruceConfig.priColor);

    // Radar target blips
    tft.fillCircle(iconCenterX + scale * 10, iconCenterY - scale * 8, scale * 2, bruceConfig.priColor);
    tft.fillCircle(iconCenterX - scale * 12, iconCenterY + scale * 10, scale * 2, bruceConfig.priColor);
}
