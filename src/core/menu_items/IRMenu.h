#ifndef __IR_MENU_H__
#define __IR_MENU_H__

#include <MenuItemInterface.h>

class IRMenu : public MenuItemInterface {
public:
    IRMenu() : MenuItemInterface("IR") {}

    void optionsMenu(void);
    void drawIcon(float scale);
    const char *getCategory() const override { return "OPTICAL INFRARED"; }
    const char *getDescription() const override { return "Remote & Cloner"; }
    bool hasTheme() { return bruceConfig.theme.ir; }
    const String& themePath() override { return bruceConfig.theme.paths.ir; }

private:
    void configMenu(void);
};

#endif
