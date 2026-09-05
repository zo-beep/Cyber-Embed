#ifndef __FINDINGS_UI_H__
#define __FINDINGS_UI_H__

#include "findings_types.h"
#include "risk_engine.h"

void findings_home();
void finding_details_view(const SecurityFinding& finding);
void finding_add_note_dialog(const String& findingId);

#endif // __FINDINGS_UI_H__
