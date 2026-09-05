#ifndef __FILE_MENU_H__
#define __FILE_MENU_H__

#include <MenuItemInterface.h>

class FileMenu : public MenuItemInterface {
public:
    FileMenu() : MenuItemInterface("Files") {}

    void optionsMenu(void);
    void drawIcon(float scale);
    const char *getCategory() const override { return "STORAGE SYSTEM"; }
    const char *getDescription() const override { return "LittleFS & SD"; }
    bool hasTheme() { return bruceConfig.theme.files; }
    const String& themePath() override { return bruceConfig.theme.paths.files; }
};

#endif
