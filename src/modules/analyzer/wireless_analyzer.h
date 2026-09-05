#ifndef __WIRELESS_ANALYZER_H__
#define __WIRELESS_ANALYZER_H__

#include "analyzer_types.h"
#include <functional>

class WirelessAnalyzer {
public:
    static WirelessAnalyzer& getInstance() {
        static WirelessAnalyzer instance;
        return instance;
    }

    // Core scanning orchestrators
    void performUnifiedScan(std::function<void(const String& stage, int progressPercent)> onProgress = nullptr);
    void performWifiScan();
    void performBleScan();
    void sampleSubGhz();
    void sampleNrf24();

    // Baseline management & Persistence
    void setBaseline();
    void clearBaseline();
    bool hasBaseline() const { return _baseline.isSet; }
    const BaselineSnapshot& getBaseline() const { return _baseline; }
    bool saveBaselineToLittleFS(const String& path = "/analyzer_baseline.json");
    bool loadBaselineFromLittleFS(const String& path = "/analyzer_baseline.json");
    bool deleteBaselineFromLittleFS(const String& path = "/analyzer_baseline.json");
    ComparisonResult compareWithBaseline() const;

    // Snapshot & activity data
    AnalyzerSnapshot& getLatestSnapshot() { return _latestSnapshot; }
    const AnalyzerSnapshot& getLatestSnapshot() const { return _latestSnapshot; }
    void clearSnapshot();
    void updateActivityHistory(int8_t level);

    // Storage & Session reporting
    bool saveReportToLittleFS(const String& path = "/analyzer_report.txt");
    String getSessionReportText() const;
    bool hasSavedReport(const String& path = "/analyzer_report.txt") const;
    bool deleteReportFromLittleFS(const String& path = "/analyzer_report.txt");

    // Helper formatting
    static String getAuthModeStr(uint8_t authMode);
    static String getRelativeTimeStr(uint32_t pastMillis);

private:
    WirelessAnalyzer();
    ~WirelessAnalyzer() = default;

    WirelessAnalyzer(const WirelessAnalyzer&) = delete;
    WirelessAnalyzer& operator=(const WirelessAnalyzer&) = delete;

    AnalyzerSnapshot _latestSnapshot;
    BaselineSnapshot _baseline;
};

extern WirelessAnalyzer& wirelessAnalyzer;

#endif // __WIRELESS_ANALYZER_H__
