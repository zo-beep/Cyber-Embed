#include "InvestigationMenu.h"
#include "core/display.h"
#include "core/utils.h"
#include "modules/investigation/investigation_ui.h"

void InvestigationMenu::optionsMenu() {
    investigation_home();
}

void InvestigationMenu::drawIcon(float scale) {
    clearIconArea();

    int w = scale * 36;
    int h = scale * 46;
    int x = iconCenterX - w / 2;
    int y = iconCenterY - h / 2;

    // Dossier / Clipboard outline
    tft.drawRoundRect(x, y, w, h, 3, bruceConfig.priColor);
    tft.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 2, bruceConfig.priColor);

    // Top clip badge
    int clipW = scale * 16;
    int clipH = scale * 6;
    tft.fillRect(iconCenterX - clipW / 2, y - 2, clipW, clipH, bruceConfig.priColor);

    // Document header lines
    tft.drawFastHLine(x + scale * 6, y + scale * 12, w - scale * 12, bruceConfig.secColor);
    tft.drawFastHLine(x + scale * 6, y + scale * 18, w - scale * 16, bruceConfig.secColor);
    tft.drawFastHLine(x + scale * 6, y + scale * 24, w - scale * 12, bruceConfig.secColor);

    // Magnifying glass lens / Crosshair on bottom right of dossier
    int lensR = scale * 11;
    int lensX = iconCenterX + scale * 6;
    int lensY = iconCenterY + scale * 8;

    tft.fillCircle(lensX, lensY, lensR, bruceConfig.bgColor);
    tft.drawCircle(lensX, lensY, lensR, bruceConfig.priColor);
    tft.drawCircle(lensX, lensY, lensR - 1, bruceConfig.priColor);

    // Crosshairs inside lens
    tft.drawFastHLine(lensX - lensR + 3, lensY, (lensR - 3) * 2, bruceConfig.priColor);
    tft.drawFastVLine(lensX, lensY - lensR + 3, (lensR - 3) * 2, bruceConfig.priColor);

    // Magnifier handle
    int hStartX = lensX + (lensR * 7) / 10;
    int hStartY = lensY + (lensR * 7) / 10;
    int hEndX = hStartX + scale * 8;
    int hEndY = hStartY + scale * 8;
    tft.drawLine(hStartX, hStartY, hEndX, hEndY, bruceConfig.priColor);
    tft.drawLine(hStartX + 1, hStartY, hEndX + 1, hEndY, bruceConfig.priColor);
}
