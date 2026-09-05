#ifndef __FINDINGS_MENU_H__
#define __FINDINGS_MENU_H__

#include <MenuItemInterface.h>

class FindingsMenu : public MenuItemInterface {
public:
    FindingsMenu() : MenuItemInterface("Findings") {}

    void optionsMenu(void) override;
    void drawIcon(float scale) override;
    const char *getCategory() const override { return "SECURITY AUDIT"; }
    const char *getDescription() const override { return "Risk & Findings Engine"; }
    bool hasTheme() override { return false; }
    const String& themePath() override { static String empty = ""; return empty; }
};

#endif // __FINDINGS_MENU_H__
