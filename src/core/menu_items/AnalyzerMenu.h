#ifndef __ANALYZER_MENU_H__
#define __ANALYZER_MENU_H__

#include <MenuItemInterface.h>

class AnalyzerMenu : public MenuItemInterface {
public:
    AnalyzerMenu() : MenuItemInterface("Analyzer") {}

    void optionsMenu(void) override;
    void drawIcon(float scale) override;
    const char *getCategory() const override { return "RECON & AUDIT"; }
    const char *getDescription() const override { return "Wireless Overview"; }
    bool hasTheme() override { return false; }
    const String& themePath() override { static String empty = ""; return empty; }
};

#endif // __ANALYZER_MENU_H__
