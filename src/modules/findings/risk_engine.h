#ifndef __RISK_ENGINE_H__
#define __RISK_ENGINE_H__

#include "findings_types.h"
#include "modules/analyzer/analyzer_types.h"
#include "modules/investigation/investigation_types.h"
#include <ArduinoJson.h>

class RiskEngine {
public:
    static RiskEngine& getInstance() {
        static RiskEngine instance;
        return instance;
    }

    // Evaluation API
    void evaluateInvestigation(const InvestigationSession& session);
    void evaluateSnapshot(const AnalyzerSnapshot& snap, bool hasBaseline);
    void evaluateDevice(const WirelessDevice& dev, const String& source, bool hasBaseline, bool isNew);
    void evaluateSubsystems(const SubsystemStatus& subghz, const SubsystemStatus& nrf24, const String& source);

    // Query API
    std::vector<SecurityFinding> getFindings() const;
    size_t getTotalFindings() const;
    size_t getUnackFindingsCount() const;
    RiskLevel getHighestRisk() const;
    bool getFindingById(const String& id, SecurityFinding& outFinding) const;

    // Modification API
    bool toggleAcknowledge(const String& id);
    bool addNoteToFinding(const String& id, const String& note);
    bool deleteFinding(const String& id);
    void clearAllFindings();

    // Persistence (LittleFS)
    bool saveToLittleFS();
    bool loadFromLittleFS();

    // Formatting & presentation helpers
    static const char* getRiskLabel(RiskLevel risk);
    static uint16_t getRiskColor(RiskLevel risk);
    static const char* getCategoryLabel(FindingCategory cat);

private:
    RiskEngine();
    ~RiskEngine() = default;

    RiskEngine(const RiskEngine&) = delete;
    RiskEngine& operator=(const RiskEngine&) = delete;

    void addOrUpdateFinding(const SecurityFinding& finding);
    String getNextFindingId();

    std::vector<SecurityFinding> _findings;
    uint32_t _nextIdCounter = 1;
};

extern RiskEngine& riskEngine;

#endif // __RISK_ENGINE_H__
