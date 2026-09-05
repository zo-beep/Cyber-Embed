#ifndef __WIFI_MENU_H__
#define __WIFI_MENU_H__

#include <MenuItemInterface.h>

class WifiMenu : public MenuItemInterface {
public:
    WifiMenu() : MenuItemInterface("WiFi") {}

    void optionsMenu(void);
    void drawIcon(float scale);
    const char *getCategory() const override { return "802.11 RECON"; }
    const char *getDescription() const override { return "Sniffer & Attacks"; }
    bool hasTheme() { return bruceConfig.theme.wifi; }
    const String& themePath() override { return bruceConfig.theme.paths.wifi; }

private:
    void configMenu(void);
};

#endif
