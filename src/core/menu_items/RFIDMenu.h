#ifndef __RFID_MENU_H__
#define __RFID_MENU_H__

#include <MenuItemInterface.h>

class RFIDMenu : public MenuItemInterface {
public:
    RFIDMenu() : MenuItemInterface("RFID") {}

    void optionsMenu(void);
    void drawIcon(float scale);
    const char *getCategory() const override { return "PN532 NFC/RFID"; }
    const char *getDescription() const override { return "Mifare & Cloner"; }
    bool hasTheme() { return bruceConfig.theme.rfid; }
    const String& themePath() override { return bruceConfig.theme.paths.rfid; }

private:
    void configMenu(void);
};

#endif
