#ifndef __INVESTIGATION_MENU_H__
#define __INVESTIGATION_MENU_H__

#include <MenuItemInterface.h>

class InvestigationMenu : public MenuItemInterface {
public:
    InvestigationMenu() : MenuItemInterface("Investigation") {}

    void optionsMenu(void) override;
    void drawIcon(float scale) override;
    const char *getCategory() const override { return "RECON & AUDIT"; }
    const char *getDescription() const override { return "Session Workspace"; }
    bool hasTheme() override { return false; }
    const String& themePath() override { static String empty = ""; return empty; }
};

#endif // __INVESTIGATION_MENU_H__
