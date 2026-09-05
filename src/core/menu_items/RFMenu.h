#ifndef __RF_MENU_H__
#define __RF_MENU_H__

#include <MenuItemInterface.h>

class RFMenu : public MenuItemInterface {
public:
    RFMenu() : MenuItemInterface("RF") {}

    void optionsMenu(void);
    void drawIcon(float scale);
    const char *getCategory() const override { return "SUB-GHZ CC1101"; }
    const char *getDescription() const override { return "Rx/Tx Transceiver"; }
    bool hasTheme() { return bruceConfig.theme.rf; }
    const String& themePath() override { return bruceConfig.theme.paths.rf; }

private:
    void configMenu(void);
};

#endif
