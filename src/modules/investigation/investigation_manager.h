#ifndef __INVESTIGATION_MANAGER_H__
#define __INVESTIGATION_MANAGER_H__

#include "investigation_types.h"
#include "modules/analyzer/wireless_analyzer.h"
#include <functional>

class InvestigationManager {
public:
    static InvestigationManager& getInstance() {
        static InvestigationManager instance;
        return instance;
    }

    // Session lifecycle
    bool hasActiveSession() const { return _activeSession.isActive; }
    InvestigationSession& getActiveSession() { return _activeSession; }
    const InvestigationSession& getActiveSession() const { return _activeSession; }

    bool startNewSession(const String& name, bool attachBaseline = true);
    bool endSession(bool saveToStorage = true);
    void discardActiveSession();

    // Observation & Event logging
    void addEvent(InvestigationEventType type, const String& title, const String& summary, const String& details = "");
    void addNote(const String& noteText);
    void recordScanResults(std::function<void(const String& stage, int progressPercent)> onProgress = nullptr);

    // Baseline diff
    ComparisonResult compareSessionWithBaseline();

    // LittleFS Persistence
    std::vector<InvestigationSummary> listSavedSessions();
    bool loadSession(const String& id, InvestigationSession& outSession);
    bool saveSession(const InvestigationSession& session);
    bool deleteSession(const String& id);
    bool deleteAllSessions();
    String getNextSessionId();

    // Utilities & formatting
    uint32_t getSessionElapsedMs() const;
    static String getFormattedDuration(uint32_t durationMs);
    static String getFormattedTime(uint32_t offsetMs);
    static String getEventTypeTag(InvestigationEventType type);

private:
    InvestigationManager();
    ~InvestigationManager() = default;

    InvestigationManager(const InvestigationManager&) = delete;
    InvestigationManager& operator=(const InvestigationManager&) = delete;

    void updateSessionCounters();
    void autoSaveActiveSession();

    InvestigationSession _activeSession;
    uint32_t _nextEventId = 1;
    uint32_t _nextNoteId = 1;
};

extern InvestigationManager& investigationManager;

#endif // __INVESTIGATION_MANAGER_H__
