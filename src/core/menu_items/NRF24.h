#ifndef __NRF24_MENU_H__
#define __NRF24_MENU_H__

#include <MenuItemInterface.h>

class NRF24Menu : public MenuItemInterface {
public:
    NRF24Menu() : MenuItemInterface("NRF24") {}

    void optionsMenu(void);
    void configMenu(void);
    void drawIcon(float scale);
    const char *getCategory() const override { return "NRF24L01+ 2.4G"; }
    const char *getDescription() const override { return "Jammer & Sniffer"; }
    bool hasTheme() { return bruceConfig.theme.nrf; }
    const String& themePath() override { return bruceConfig.theme.paths.nrf; }
};

#endif
