#include "main_menu.h"
#include "display.h"
#include "utils.h"
#include <globals.h>

MainMenu::MainMenu() {
    _menuItems = {
        &analyzerMenu,
        &investigationMenu,
        &findingsMenu,
        &wifiMenu,
        &bleMenu,
        &rfMenu,
        &nrf24Menu,
        &irMenu,
        &gpsMenu,
        &rfidMenu,
        &fileMenu,
#if !defined(LITE_VERSION) && !defined(DISABLE_INTERPRETER)
        &scriptsMenu,
#endif
        &clockMenu,
        &othersMenu,
        &configMenu,
    };

    _totalItems = _menuItems.size();
}

MainMenu::~MainMenu() {}

void MainMenu::begin(void) {
    returnToMenu = false;
    options = {};

    std::vector<String> l = bruceConfig.disabledMenus;
    std::vector<MenuItemInterface *> enabledItems;

    for (int i = 0; i < _totalItems; i++) {
        String itemName = _menuItems[i]->getName();
        if (find(l.begin(), l.end(), itemName) == l.end()) { // If menu item is not disabled
            enabledItems.push_back(_menuItems[i]);
        }
    }

    int totalEnabled = enabledItems.size();

    for (int i = 0; i < totalEnabled; i++) {
        MenuItemInterface *itemObj = enabledItems[i];
        String itemName = itemObj->getName();

        int prevIdx = (i - 1 + totalEnabled) % totalEnabled;
        int nextIdx = (i + 1) % totalEnabled;
        String prevName = enabledItems[prevIdx]->getName();
        String nextName = enabledItems[nextIdx]->getName();

        itemObj->setNavContext(i, totalEnabled, prevName, nextName);

        options.push_back(
            {// selected lambda
             itemName,
             [itemObj]() { itemObj->optionsMenu(); },
             false,                                  // selected = false
             [](void *menuItem, bool shouldRender) -> bool { // render lambda
                 if (!shouldRender) return false;
                 drawMainBorder(false);

                 MenuItemInterface *obj = static_cast<MenuItemInterface *>(menuItem);
                 float scale = float((float)tftWidth / (float)240);
                 if (bruceConfigPins.rotation & 0b01) scale = float((float)tftHeight / (float)135);
                 obj->draw(scale);
#if defined(HAS_TOUCH)
                 TouchFooter();
#endif
                 return true;
             },
             itemObj
            }
        );
    }
    _currentIndex = loopOptions(options, MENU_TYPE_MAIN, "Main Menu", _currentIndex);
};

/*********************************************************************
**  Function: hideAppsMenu
**  Menu to Hide or show menus
**********************************************************************/

void MainMenu::hideAppsMenu() {
    auto items = this->getItems();
    int index = 0;
RESTART: // using gotos to avoid stackoverflow after many choices
    options.clear();
    for (auto item : items) {
        String label = item->getName();
        std::vector<String> l = bruceConfig.disabledMenus;
        bool enabled = find(l.begin(), l.end(), label) == l.end();
        options.push_back(
            {label,
             [this, label, enabled]() {
                 if (enabled) bruceConfig.addDisabledMenu(label);
                 else bruceConfig.removeDisabledMenu(label);
             },
             enabled}
        );
    }
    options.push_back({"Show All", [=]() { bruceConfig.disabledMenus.clear(); }, true});
    addOptionToMainMenu();
    index = loopOptions(options, index);
    bruceConfig.saveFile();
    if (!returnToMenu) goto RESTART;
}
