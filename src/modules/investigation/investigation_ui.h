#ifndef __INVESTIGATION_UI_H__
#define __INVESTIGATION_UI_H__

#include "investigation_types.h"
#include "investigation_manager.h"

void investigation_home();
void investigation_create_dialog();
void investigation_active_dashboard();
void investigation_timeline_view(const InvestigationSession& session, bool isReadOnly = false);
void investigation_event_details(const InvestigationEvent& ev);
void investigation_devices_view(const InvestigationSession& session, bool isReadOnly = false);
void investigation_changes_view(const InvestigationSession& session, bool isReadOnly = false);
void investigation_notes_view(InvestigationSession& session, bool isReadOnly = false);
void investigation_add_note_dialog();
void investigation_end_dialog();
void investigation_saved_list();
void investigation_saved_viewer(const String& id);
void investigation_purge_dialog();

#endif // __INVESTIGATION_UI_H__
