#ifndef __MENU_ITEM_INTERFACE_H__
#define __MENU_ITEM_INTERFACE_H__

#include "core/display.h"
#include <globals.h>

class MenuItemInterface {
public:
    virtual ~MenuItemInterface() = default;
    virtual void optionsMenu(void) = 0;
    virtual void drawIcon(float scale = 1) = 0;
    virtual void drawIconImg() {
        drawImg(
            *bruceConfig.themeFS(),
            bruceConfig.getThemeItemImg(themePath()),  // const String& - no heap alloc
            0,
            imgCenterY,
            true,
            bruceConfig.theme.gifDuration,
            false
        );
    }
    virtual bool hasTheme() = 0;
    virtual const String& themePath() = 0;

    bool checkTheme() { return hasTheme() && themePath().length() > 0; }
    String getName() const { return String(_name); }
    virtual const char *getCategory() const { return "SYSTEM"; }
    virtual const char *getDescription() const { return "Module Interface"; }

    void setNavContext(int index, int total, const String &prev, const String &next) {
        _navIndex = index;
        _navTotal = total;
        _navPrev = prev;
        _navNext = next;
    }

    void draw(float scale = 1) {
        drawHud(scale, _navIndex, _navTotal, _navPrev, _navNext);
    }

    void drawHud(
        float scale, int currentIndex, int totalItems, const String &prevName, const String &nextName
    ) {
        if (rotation != bruceConfigPins.rotation) resetCoordinates();

        if (!checkTheme()) {
            tft.fillRect(0, 21, tftWidth, tftHeight - 21, bruceConfig.bgColor);

            // Left module tactical HUD card
            drawCyberCard(6, 23, 196, 129, bruceConfig.priColor, 0, getCategory());

            // Center vector icon inside left card with safe vertical scaling
            drawIcon(scale * 0.65f);

            // Module name & description inside card without any text overlap
            tft.setTextSize(FM);
            tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
            tft.drawCentreString(truncateForWidth(getName(), 180, FM), 104, 92, 1);

            tft.setTextSize(FP);
            tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
            tft.drawCentreString(truncateForWidth(getDescription(), 180, FP), 104, 114, 1);

            // Right navigation carousel card
            drawCyberCard(206, 23, 108, 129, bruceConfig.priColor, 0, "CAROUSEL");

            // Position counter
            String pos = "[" + String(currentIndex + 1 < 10 ? "0" : "") + String(currentIndex + 1) + "/" +
                         String(totalItems < 10 ? "0" : "") + String(totalItems) + "]";
            tft.setTextSize(FP);
            tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
            tft.drawRightString(pos, 306, 27, 1);

            // Previous item
            tft.setTextColor(0x632C, bruceConfig.bgColor);
            tft.drawCentreString("^ PREV", 260, 42, 1);
            tft.drawCentreString(truncateForWidth(prevName, 96, FP), 260, 52, 1);

            // Active highlighted item
            tft.fillRect(209, 66, 102, 21, bruceConfig.priColor);
            tft.setTextColor(bruceConfig.bgColor, bruceConfig.priColor);
            tft.setTextSize(FP);
            tft.drawCentreString(truncateForWidth(getName(), 96, FP), 260, 72, 1);

            // Next item
            tft.setTextColor(0x632C, bruceConfig.bgColor);
            tft.drawCentreString(truncateForWidth(nextName, 96, FP), 260, 94, 1);
            tft.drawCentreString("v NEXT", 260, 106, 1);

            // Footer hint
            drawCyberFooter("ROT NAV", "OK OPEN", "CYBER-EMBED");
        } else {
            if (bruceConfig.theme.label)
                drawTitle(scale); // If using .GIF, labels are draw after complete, which takes some time
            drawIconImg();
            if (bruceConfig.theme.label) drawTitle(scale); // Makes sure to draw over the image
        }
        drawStatusBar();
    }

    void drawArrows(float scale = 1) {
        // Preserved for backwards compatibility
    }

    void drawTitle(float scale = 1) {
        int titleY = iconCenterY + iconAreaH / 2 + FG;

        tft.setTextSize(FM);
        tft.drawPixel(0, 0, 0);
        tft.fillRect(arrowAreaX, titleY, tftWidth - 2 * arrowAreaX, LH * FM, bruceConfig.bgColor);
        int nchars = (tftWidth - 16) / (LW * FM);
        tft.drawCentreString(getName().substring(0, nchars), iconCenterX, titleY, 1);
    }

protected:
    const char *_name = "";
    uint8_t rotation = ROTATION;

    int _navIndex = 0;
    int _navTotal = 1;
    String _navPrev = "";
    String _navNext = "";

    int iconAreaH = 48;
    int iconAreaW = 180;

    int iconCenterX = 104;
    int iconCenterY = 64;
    int imgCenterY = 13;

    int iconAreaX = 14;
    int iconAreaY = 40;

    int arrowAreaX = BORDER_PAD_X;
    int arrowAreaW = 20;

    MenuItemInterface(const char *name) : _name(name) {}

    void clearIconArea(void) {
        if (tftWidth > tftHeight) {
            tft.fillRect(10, 40, 188, 50, bruceConfig.bgColor);
        } else {
            tft.fillRect(iconAreaX, iconAreaY, iconAreaW, iconAreaH, bruceConfig.bgColor);
        }
    }
    void clearImgArea(void) { tft.fillRect(7, 27, tftWidth - 14, tftHeight - 34, bruceConfig.bgColor); }
    void resetCoordinates(void) {
        if (tftWidth > tftHeight) {
            iconCenterX = 104;
            iconCenterY = 64;
            iconAreaX = 14;
            iconAreaY = 40;
            iconAreaW = 180;
            iconAreaH = 48;
        } else {
            iconCenterX = tftWidth / 2;
            iconCenterY = tftHeight / 2;
            iconAreaX = 10;
            iconAreaY = 30;
            iconAreaW = tftWidth - 20;
            iconAreaH = 80;
        }

        arrowAreaX = BORDER_PAD_X;
        arrowAreaW = 20;

        rotation = bruceConfigPins.rotation;
    }

private:
};

#endif // __MENU_ITEM_INTERFACE_H__
