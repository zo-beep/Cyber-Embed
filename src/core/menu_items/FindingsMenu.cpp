#include "FindingsMenu.h"
#include "core/display.h"
#include "core/utils.h"
#include "modules/findings/findings_ui.h"

void FindingsMenu::optionsMenu() {
    findings_home();
}

void FindingsMenu::drawIcon(float scale) {
    clearIconArea();

    int w = scale * 38;
    int h = scale * 46;
    int x = iconCenterX - w / 2;
    int y = iconCenterY - h / 2;

    // Tactical Shield Outer Contour
    // Top flat edge
    tft.drawFastHLine(x + 4, y, w - 8, bruceConfig.priColor);
    tft.drawFastHLine(x + 4, y + 1, w - 8, bruceConfig.priColor);

    // Top-left and top-right bevels
    tft.drawLine(x, y + 4, x + 4, y, bruceConfig.priColor);
    tft.drawLine(x + w, y + 4, x + w - 4, y, bruceConfig.priColor);

    // Left and right vertical sides
    tft.drawFastVLine(x, y + 4, h / 2, bruceConfig.priColor);
    tft.drawFastVLine(x + 1, y + 4, h / 2, bruceConfig.priColor);
    tft.drawFastVLine(x + w, y + 4, h / 2, bruceConfig.priColor);
    tft.drawFastVLine(x + w - 1, y + 4, h / 2, bruceConfig.priColor);

    // Bottom converging diagonals to point
    tft.drawLine(x, y + 4 + h / 2, iconCenterX, y + h, bruceConfig.priColor);
    tft.drawLine(x + 1, y + 4 + h / 2, iconCenterX, y + h - 1, bruceConfig.priColor);
    tft.drawLine(x + w, y + 4 + h / 2, iconCenterX, y + h, bruceConfig.priColor);
    tft.drawLine(x + w - 1, y + 4 + h / 2, iconCenterX, y + h - 1, bruceConfig.priColor);

    // Center Warning / Inspection Crosshair & Concentric Reticle
    int innerR = scale * 8;
    tft.drawCircle(iconCenterX, iconCenterY, innerR, bruceConfig.secColor);
    tft.fillCircle(iconCenterX, iconCenterY, scale * 3, bruceConfig.priColor);

    // Reticle crosshairs
    tft.drawFastHLine(iconCenterX - innerR - 3, iconCenterY, (innerR + 3) * 2, bruceConfig.priColor);
    tft.drawFastVLine(iconCenterX, iconCenterY - innerR - 3, (innerR + 3) * 2, bruceConfig.priColor);

    // Top banner alert notch
    tft.fillRect(iconCenterX - scale * 6, y + 4, scale * 12, scale * 3, bruceConfig.priColor);
}
