#ifndef __CLOCK_MENU_H__
#define __CLOCK_MENU_H__

#include <MenuItemInterface.h>

class ClockMenu : public MenuItemInterface {
public:
    ClockMenu() : MenuItemInterface("Clock") {}

    void optionsMenu(void);
    void showSubMenu(void);
    void drawIcon(float scale);
    const char *getCategory() const override { return "CHRONOMETRY"; }
    const char *getDescription() const override { return "RTC & NTP Sync"; }
    bool hasTheme() { return bruceConfig.theme.clock; }
    const String& themePath() override { return bruceConfig.theme.paths.clock; }
};

#endif
