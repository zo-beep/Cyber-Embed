#include "display.h"
#include "core/wifi/webInterface.h" // for server
#include "core/wifi/wg.h"           //for isConnectedWireguard to print wireguard lock
#include "mykeyboard.h"
#include "settings.h" //for timeStr
#include "utils.h"
#include <JPEGDecoder.h>
#include <interface.h> //for charging ischarging to print charging indicator
#include <memory>

#define MAX_MENU_SIZE (int)(tftHeight / 25)

// Send the ST7789 into or out of sleep mode
void panelSleep(bool on) {
#if defined(ST7789_2_DRIVER) || defined(ST7789_DRIVER)
    if (on) {
        tft.writecommand(0x10); // SLPIN: panel off
        delay(5);
    } else {
        tft.writecommand(0x11); // SLPOUT: panel on
        delay(120);
    }
#endif
    // Disables tft writings on the display
    tft.setSleepMode(on);
}

bool __attribute__((weak)) isCharging() { return false; }
/***************************************************************************************
** Function name: displayScrollingText
** Description:   Scroll large texts into screen
***************************************************************************************/
void displayScrollingText(const String &text, Opt_Coord &coord, bool highlight) {
    int len = text.length();
    String displayText = text + "        "; // Add spaces for smooth looping
    int scrollLen = len + 8;                // Full text plus space buffer
    static int i = 0;
    static long _lastmillis = 0;
    static String _lastText = "";

    if (_lastText != text) {
        i = 0;
        _lastText = text;
        _lastmillis = millis() + 500;
    }

    tft.setTextSize(FP);
    if (highlight) tft.setTextColor(coord.bgcolor, coord.fgcolor);
    else tft.setTextColor(coord.fgcolor, coord.bgcolor);

    if (len <= coord.size) {
        // Text fits within limit, no scrolling needed
        return;
    } else if (millis() > _lastmillis + 220) {
        String scrollingPart =
            displayText.substring(i, i + coord.size); // Display charLimit characters at a time
        tft.fillRect(
            coord.x,
            coord.y,
            coord.size * LW * FP,
            LH * FP + 2,
            highlight ? coord.fgcolor : coord.bgcolor
        ); // Clear display area
        tft.drawString(scrollingPart, coord.x, coord.y);
        if (i >= scrollLen - coord.size) i = -1; // Loop back
        _lastmillis = millis();
        i++;
        if (i == 1) _lastmillis = millis() + 800;
    }
}

/***************************************************************************************
** Function name: TouchFooter
** Description:   Draw touch screen footer
***************************************************************************************/
void TouchFooter(uint16_t color) {
#if defined(HAS_TOUCH)
    tft.drawRoundRect(5, tftHeight + 2, tftWidth - 10, 43, 5, color);
    tft.setTextColor(color);
    tft.setTextSize(FM);
    tft.drawCentreString("PREV", tftWidth / 6, tftHeight + 4, 1);
    tft.drawCentreString("SEL", tftWidth / 2, tftHeight + 4, 1);
    tft.drawCentreString("NEXT", 5 * tftWidth / 6, tftHeight + 4, 1);
#endif
}

/***************************************************************************************
** Function name: MegaFooter
** Description:   Draw mega footer
***************************************************************************************/
void MegaFooter(uint16_t color) {
#if defined(HAS_TOUCH)
    tft.drawRoundRect(5, tftHeight + 2, tftWidth - 10, 43, 5, color);
    tft.setTextColor(color);
    tft.setTextSize(FM);
    tft.drawCentreString("Exit", tftWidth / 6, tftHeight + 4, 1);
    tft.drawCentreString("UP", tftWidth / 2, tftHeight + 4, 1);
    tft.drawCentreString("DOWN", 5 * tftWidth / 6, tftHeight + 4, 1);
#endif
}

/***************************************************************************************
** Function name: resetTftDisplay
** Description:   set cursor to 0,0, screen and text to default color
***************************************************************************************/
void resetTftDisplay(int x, int y, uint16_t fc, int size, uint16_t bg, uint16_t screen) {
    tft.setCursor(x, y);
    tft.fillScreen(screen);
    tft.setTextSize(size);
    tft.setTextColor(fc, bg);
    tft.setTextDatum(0);
}

/***************************************************************************************
** Function name: setTftDisplay
** Description:   set cursor, font color, size and bg font color
***************************************************************************************/
void setTftDisplay(int x, int y, uint16_t fc, int size, uint16_t bg) {
    if (x >= 0 && y < 0) tft.setCursor(x, tft.getCursorY());      // if -1 on x, sets only y
    else if (x < 0 && y >= 0) tft.setCursor(tft.getCursorX(), y); // if -1 on y, sets only x
    else if (x >= 0 && y >= 0) tft.setCursor(x, y);               // if x and y > 0, sets both
    tft.setTextSize(size);
    tft.setTextColor(fc, bg);
}

void turnOffDisplay() { setBrightness(0, false); }

bool wakeUpScreen() {
    previousMillis = millis();
    if (isScreenOff) {
        isScreenOff = false;
        dimmer = false;
        getBrightness();
        vTaskDelay(pdMS_TO_TICKS(200));
        return true;
    } else if (dimmer) {
        dimmer = false;
        getBrightness();
        vTaskDelay(pdMS_TO_TICKS(200));
        return true;
    }
    return false;
}

/***************************************************************************************
** Function name: wrapText
** Description:   Wrap text to fit within a maximum width without dropping characters
***************************************************************************************/
std::vector<String> wrapText(const String &text, int maxCharsPerLine) {
    std::vector<String> lines;
    if (maxCharsPerLine <= 0) return lines;

    String remaining = text;
    while (remaining.length() > 0) {
        if ((int)remaining.length() <= maxCharsPerLine) {
            lines.push_back(remaining);
            break;
        }

        // Check if there is an explicit newline character within range
        int nlPos = remaining.indexOf('\n');
        if (nlPos != -1 && nlPos <= maxCharsPerLine) {
            lines.push_back(remaining.substring(0, nlPos));
            remaining = remaining.substring(nlPos + 1);
            continue;
        }

        // Find last space within maxCharsPerLine
        int splitPos = -1;
        for (int i = maxCharsPerLine; i >= 0; i--) {
            if (remaining[i] == ' ' || remaining[i] == '\t') {
                splitPos = i;
                break;
            }
        }

        if (splitPos <= 0) {
            // Check for delimiter hyphens or slashes
            for (int i = maxCharsPerLine - 1; i >= 1; i--) {
                if (remaining[i] == '-' || remaining[i] == '_' || remaining[i] == '/' || remaining[i] == ':') {
                    splitPos = i + 1;
                    break;
                }
            }
            if (splitPos <= 0) {
                splitPos = maxCharsPerLine;
            }
            lines.push_back(remaining.substring(0, splitPos));
            remaining = remaining.substring(splitPos);
        } else {
            lines.push_back(remaining.substring(0, splitPos));
            remaining = remaining.substring(splitPos + 1);
        }
    }
    return lines;
}

/***************************************************************************************
** Function name: displayRedStripe
** Description:   Display Alert card with multi-line text wrapping and proper line height
***************************************************************************************/
void displayRedStripe(const String &text, uint16_t fgcolor, uint16_t bgcolor) {
    int maxCharsFM = (tftWidth - 48) / (LW * FM); // ~22 chars
    int maxCharsFP = (tftWidth - 48) / (LW * FP); // ~45 chars

    int size = FP;
    std::vector<String> wrappedLines;

    if ((int)text.length() <= maxCharsFM) {
        size = FM;
        wrappedLines = wrapText(text, maxCharsFM);
    } else {
        size = FP;
        wrappedLines = wrapText(text, maxCharsFP);
    }

    int lineHeight = (size == FM) ? 18 : 12;
    int boxHeight = 22 + (wrappedLines.size() * lineHeight);
    if (boxHeight > tftHeight - 20) {
        size = FP;
        lineHeight = 12;
        wrappedLines = wrapText(text, maxCharsFP);
        boxHeight = 22 + (wrappedLines.size() * lineHeight);
    }

    int boxWidth = tftWidth - 28;
    int boxX = 14;
    int boxY = (tftHeight - boxHeight) / 2;

    tft.drawPixel(0, 0, 0);
    // Draw cyber tactical alert card
    tft.fillRoundRect(boxX, boxY, boxWidth, boxHeight, 4, bruceConfig.bgColor);
    tft.drawRoundRect(boxX, boxY, boxWidth, boxHeight, 4, bgcolor);
    tft.drawRoundRect(boxX + 2, boxY + 2, boxWidth - 4, boxHeight - 4, 3, bgcolor);

    // Accent corner ticks
    tft.fillRect(boxX, boxY, 8, 3, fgcolor);
    tft.fillRect(boxX, boxY, 3, 8, fgcolor);
    tft.fillRect(boxX + boxWidth - 8, boxY, 8, 3, fgcolor);
    tft.fillRect(boxX + boxWidth - 3, boxY, 3, 8, fgcolor);
    tft.fillRect(boxX, boxY + boxHeight - 3, 8, 3, fgcolor);
    tft.fillRect(boxX, boxY + boxHeight - 8, 3, 8, fgcolor);
    tft.fillRect(boxX + boxWidth - 8, boxY + boxHeight - 3, 8, 3, fgcolor);
    tft.fillRect(boxX + boxWidth - 3, boxY + boxHeight - 8, 3, 8, fgcolor);

    tft.setTextColor(fgcolor, bruceConfig.bgColor);
    tft.setTextSize(size);

    int startY = boxY + 11;
    for (size_t i = 0; i < wrappedLines.size(); i++) {
        tft.drawCentreString(wrappedLines[i], tftWidth / 2, startY + i * lineHeight, 1);
    }
}

void drawButton(
    int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color, const char *text, bool inverted = false
) {
    if (inverted) {
        tft.fillRoundRect(x, y, w, h, 3, color);
        tft.setTextColor(bruceConfig.bgColor, color);
    } else {
        tft.fillRoundRect(x, y, w, h, 3, bruceConfig.bgColor);
        tft.drawRoundRect(x, y, w, h, 3, color);
        tft.setTextColor(color, bruceConfig.bgColor);
    }
    tft.setTextSize(FP);
    tft.drawCentreString(text, x + w / 2, y + (h - 8) / 2, 1);
}

int8_t displayMessage(
    const char *message, const char *leftButton, const char *centerButton, const char *rightButton,
    uint16_t color
) {
    int cardW = tftWidth - 24;
    int cardH = tftHeight - 24;
    int cardX = 12;
    int cardY = 12;

    tft.fillRoundRect(cardX, cardY, cardW, cardH, 4, bruceConfig.bgColor);
    tft.drawRoundRect(cardX, cardY, cardW, cardH, 4, color);
    tft.drawRoundRect(cardX + 2, cardY + 2, cardW - 4, cardH - 4, 3, color);

    // Title tag
    tft.fillRect(cardX + 12, cardY, 80, 3, color);
    tft.setTextColor(color, bruceConfig.bgColor);
    tft.setTextSize(FP);
    tft.drawString("SYSTEM ALERT", cardX + 10, cardY + 6);
    tft.drawFastHLine(cardX + 6, cardY + 16, cardW - 12, color);

    // Message text wrapping
    int maxCharsPerLine = (cardW - 24) / (LW * FP);
    std::vector<String> msgLines = wrapText(String(message), maxCharsPerLine);

    tft.setTextSize(FP);
    tft.setTextColor(0xFFFF, bruceConfig.bgColor);

    int startY = cardY + 24;
    for (size_t i = 0; i < msgLines.size() && i < 6; i++) {
        tft.drawCentreString(msgLines[i], tftWidth / 2, startY + i * 13, 1);
    }

    int16_t buttonHeight = 18;
    int16_t buttonY = cardY + cardH - buttonHeight - 8;

    int8_t totalButtons = (leftButton ? 1 : 0) + (centerButton ? 1 : 0) + (rightButton ? 1 : 0);
    int8_t selected = 0;
    bool redraw = true;

    int16_t buttonWidth = (totalButtons > 0) ? (cardW - 24 - (totalButtons - 1) * 8) / totalButtons : 60;
    if (buttonWidth > 90) buttonWidth = 90;

    while (true) {
        if (check(PrevPress) || check(EscPress) || check(UpPress)) {
            selected = (selected - 1 + totalButtons) % totalButtons;
            redraw = true;
        }
        if (check(NextPress) || check(DownPress)) {
            selected = (selected + 1) % totalButtons;
            redraw = true;
        }
        if (check(SelPress)) { break; }

        int8_t index = 0;

        if (redraw) {
            int totalBtnAreaW = totalButtons * buttonWidth + (totalButtons - 1) * 8;
            int startBtnX = cardX + (cardW - totalBtnAreaW) / 2;

            if (leftButton) {
                drawButton(
                    startBtnX + index * (buttonWidth + 8),
                    buttonY,
                    buttonWidth,
                    buttonHeight,
                    color,
                    leftButton,
                    selected == index
                );
                index++;
            }

            if (centerButton) {
                drawButton(
                    startBtnX + index * (buttonWidth + 8),
                    buttonY,
                    buttonWidth,
                    buttonHeight,
                    color,
                    centerButton,
                    selected == index
                );
                index++;
            }

            if (rightButton) {
                drawButton(
                    startBtnX + index * (buttonWidth + 8),
                    buttonY,
                    buttonWidth,
                    buttonHeight,
                    color,
                    rightButton,
                    selected == index
                );
            }
            redraw = false;
        }

        delay(10);
    }

    return selected;
}

void displayError(const String &txt, bool waitKeyPress) {
    displayRedStripe("[!] ERROR: " + txt, TFT_WHITE, TFT_RED);
    Serial.println("ERR: " + txt);
#ifndef HAS_SCREEN
    return;
#endif
    delay(200);
    while (waitKeyPress && !check(AnyKeyPress)) vTaskDelay(10 / portTICK_PERIOD_MS);
}

void displayWarning(const String &txt, bool waitKeyPress) {
    displayRedStripe("[!] " + txt, TFT_YELLOW, 0xFDC0);
    Serial.println("WARN: " + txt);
#ifndef HAS_SCREEN
    return;
#endif
    delay(200);
    while (waitKeyPress && !check(AnyKeyPress)) vTaskDelay(10 / portTICK_PERIOD_MS);
}

void displayInfo(const String &txt, bool waitKeyPress) {
    displayRedStripe("[i] " + txt, TFT_CYAN, 0x07FF);
    Serial.println("INFO: " + txt);
#ifndef HAS_SCREEN
    return;
#endif

    delay(200);
    while (waitKeyPress && !check(AnyKeyPress)) vTaskDelay(10 / portTICK_PERIOD_MS);
}

void displaySuccess(const String &txt, bool waitKeyPress) {
    displayRedStripe("[OK] " + txt, TFT_GREEN, 0x07E0);
    Serial.println("SUCCESS: " + txt);
#ifndef HAS_SCREEN
    return;
#endif
    delay(200);
    while (waitKeyPress && !check(AnyKeyPress)) vTaskDelay(10 / portTICK_PERIOD_MS);
}

void displayTextLine(const String &txt, bool waitKeyPress) {
    displayRedStripe(txt, bruceConfig.priColor, bruceConfig.priColor);
    Serial.println("MESSAGE: " + txt);
#ifndef HAS_SCREEN
    return;
#endif
    delay(200);
    while (waitKeyPress && !check(AnyKeyPress)) vTaskDelay(10 / portTICK_PERIOD_MS);
}

void setPadCursor(int16_t padx, int16_t pady) {
    for (int y = 0; y < pady; y++) tft.println();
    tft.setCursor(padx * BORDER_PAD_X, tft.getCursorY());
}

void padprintf(int16_t padx, const char *format, ...) {
    char buffer[64];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    tft.setCursor(padx * BORDER_PAD_X, tft.getCursorY());
    tft.printf("%s", buffer);
}
void padprintf(const char *format, ...) {
    char buffer[64];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    tft.setCursor(BORDER_PAD_X, tft.getCursorY());
    tft.printf("%s", buffer);
}

void padprint(const String &s, int16_t padx) {
    tft.setCursor(padx * BORDER_PAD_X, tft.getCursorY());
    tft.print(s);
}
void padprint(const char str[], int16_t padx) {
    tft.setCursor(padx * BORDER_PAD_X, tft.getCursorY());
    tft.print(str);
}
void padprint(char c, int16_t padx) {
    tft.setCursor(padx * BORDER_PAD_X, tft.getCursorY());
    tft.print(c);
}
void padprint(unsigned char b, int base, int16_t padx) {
    tft.setCursor(padx * BORDER_PAD_X, tft.getCursorY());
    tft.print(b, base);
}
void padprint(int n, int base, int16_t padx) {
    tft.setCursor(padx * BORDER_PAD_X, tft.getCursorY());
    tft.print(n, base);
}
void padprint(unsigned int n, int base, int16_t padx) {
    tft.setCursor(padx * BORDER_PAD_X, tft.getCursorY());
    tft.print(n, base);
}
void padprint(long n, int base, int16_t padx) {
    tft.setCursor(padx * BORDER_PAD_X, tft.getCursorY());
    tft.print(n, base);
}
void padprint(unsigned long n, int base, int16_t padx) {
    tft.setCursor(padx * BORDER_PAD_X, tft.getCursorY());
    tft.print(n, base);
}
void padprint(long long n, int base, int16_t padx) {
    tft.setCursor(padx * BORDER_PAD_X, tft.getCursorY());
    tft.print(n, base);
}
void padprint(unsigned long long n, int base, int16_t padx) {
    tft.setCursor(padx * BORDER_PAD_X, tft.getCursorY());
    tft.print(n, base);
}
void padprint(double n, int digits, int16_t padx) {
    tft.setCursor(padx * BORDER_PAD_X, tft.getCursorY());
    tft.print(n, digits);
}

void padprintln(const String &s, int16_t padx) {
    if (s.isEmpty()) {
        tft.setCursor(padx * BORDER_PAD_X, tft.getCursorY());
        tft.println(s);
        return;
    }

    String buff;
    size_t start = 0;
    int _maxCharsInLine = (tftWidth - (padx + 1) * BORDER_PAD_X) / (FP * LW);

    // automatically split into multiple lines
    while (!(buff = s.substring(start, start + _maxCharsInLine)).isEmpty()) {
        tft.setCursor(padx * BORDER_PAD_X, tft.getCursorY());
        tft.println(buff);
        start += buff.length();
    }
}
void padprintln(const char str[], int16_t padx) {
    if (strcmp(str, "") == 0) {
        tft.setCursor(padx * BORDER_PAD_X, tft.getCursorY());
        tft.println(str);
        return;
    }

    String buff;
    size_t start = 0;
    int _maxCharsInLine = (tftWidth - (padx + 1) * BORDER_PAD_X) / (FP * LW);

    // automatically split into multiple lines
    while (!(buff = String(str).substring(start, start + _maxCharsInLine)).isEmpty()) {
        tft.setCursor(padx * BORDER_PAD_X, tft.getCursorY());
        tft.println(buff);
        start += buff.length();
    }
}
void padprintln(char c, int16_t padx) {
    tft.setCursor(padx * BORDER_PAD_X, tft.getCursorY());
    tft.println(c);
}
void padprintln(unsigned char b, int base, int16_t padx) {
    tft.setCursor(padx * BORDER_PAD_X, tft.getCursorY());
    tft.println(b, base);
}
void padprintln(int n, int base, int16_t padx) {
    tft.setCursor(padx * BORDER_PAD_X, tft.getCursorY());
    tft.println(n, base);
}
void padprintln(unsigned int n, int base, int16_t padx) {
    tft.setCursor(padx * BORDER_PAD_X, tft.getCursorY());
    tft.println(n, base);
}
void padprintln(long n, int base, int16_t padx) {
    tft.setCursor(padx * BORDER_PAD_X, tft.getCursorY());
    tft.println(n, base);
}
void padprintln(unsigned long n, int base, int16_t padx) {
    tft.setCursor(padx * BORDER_PAD_X, tft.getCursorY());
    tft.println(n, base);
}
void padprintln(long long n, int base, int16_t padx) {
    tft.setCursor(padx * BORDER_PAD_X, tft.getCursorY());
    tft.println(n, base);
}
void padprintln(unsigned long long n, int base, int16_t padx) {
    tft.setCursor(padx * BORDER_PAD_X, tft.getCursorY());
    tft.println(n, base);
}
void padprintln(double n, int digits, int16_t padx) {
    tft.setCursor(padx * BORDER_PAD_X, tft.getCursorY());
    tft.println(n, digits);
}

/*********************************************************************
**  Function: loopOptions
**  Where you choose among the options in menu
**********************************************************************/
int loopOptions(
    std::vector<Option> &options, uint8_t menuType, const char *subText, int index, bool interpreter
) {
    if (options.empty()) return -1;

    auto findFirstEnabled = [&]() -> int {
        for (size_t i = 0; i < options.size(); i++) {
            if (options[i].enabled) return static_cast<int>(i);
        }
        return -1;
    };

    auto findNextEnabled = [&](int start, int step) -> int {
        if (options.empty()) return -1;
        int size = static_cast<int>(options.size());
        int idx = start;
        for (int i = 0; i < size; i++) {
            idx = (idx + step + size) % size;
            if (options[idx].enabled) return idx;
        }
        return -1;
    };

    if (index < 0 || index >= static_cast<int>(options.size())) index = 0;
    if (!options[index].enabled) {
        int firstEnabled = findFirstEnabled();
        if (firstEnabled < 0) return -1;
        index = firstEnabled;
    }

    Opt_Coord coord;
    bool redraw = true;
    bool exit = false;
    int menuSize = options.size();
    int devModeCounter = 0;
    static unsigned long _clock_bat_timer = millis();
    if (options.size() > MAX_MENU_SIZE) { menuSize = MAX_MENU_SIZE; }
    if (index >= (int)options.size()) index = 0;
    bool firstRender = true;
    unsigned long menuOpenTs =
        0; // timestamp when this menu was first rendered (per-invocation, not shared across nested menus)
    drawMainBorder();
    while (1) {
        // Check for shutdown before drawing menu to avoid drawing a black bar on the screen
        if (exit) break;
        if (menuType == MENU_TYPE_MAIN) {
            checkReboot();
            if (devModeCounter >= 5 && !bruceConfig.devMode) {
                bruceConfig.setDevMode(true);
                displayInfo("Dev Mode Enabled", true);
            }
            if (millis() - _clock_bat_timer > 30000) {
                _clock_bat_timer = millis();
                drawStatusBar(); // update clock and battery status each 30s
            }
        }

        if (redraw) {
            menuOptionType = menuType; // updates menutype to the remote controller
            menuOptionLabel = subText;
            // update the hovered
            for (auto &opt : options) opt.hovered = false;
            options[index].hovered = true;

            bool renderedByLambda = false;
            if (options[index].hover)
                renderedByLambda = options[index].hover(options[index].hoverPointer, true);

            if (!renderedByLambda) {
                if (menuType == MENU_TYPE_SUBMENU) drawSubmenu(index, options, subText);
                else
                    coord = drawOptions(
                        index,
                        options,
                        bruceConfig.priColor,
                        bruceConfig.secColor,
                        bruceConfig.bgColor,
                        firstRender
                    );
            }
            if (firstRender) menuOpenTs = millis();
            firstRender = false;
            redraw = false;
        }

        // handleSerialCommands(); // always use serial task for it
#ifdef HAS_KEYBOARD
        // Only process shortcuts on the main menu; the menuType check must short-circuit
        // checkShortcutPress() so a held key can't re-fire inside the submenu it just opened.
        // Break so the caller rebuilds the menu, since the shortcut refilled the shared global
        // `options` (like selecting an option does) rather than repainting stale options.
        if (menuType == MENU_TYPE_MAIN && checkShortcutPress()) break;
#endif

        if (menuType == MENU_TYPE_REGULAR) {
            String txt = options[index].label;
            displayScrollingText(txt, coord, true);
        }

// Checks ESC Press first, to not exit after PrevPress is processed
// PrevPress condition is a StickCPlus workaround, as it uses the same button for Prev and Esc
// Same happens to Core and some other boards
#ifdef HAS_3_BUTTONS
        if (EscPress && PrevPress) EscPress = false;
#endif
        if (menuType != MENU_TYPE_MAIN && check(EscPress)) {
            index = -1;
            break;
        }

#ifdef HAS_ENCODER
        int32_t rotarySteps = drainRotarySteps();
        if (rotarySteps != 0) {
            check(PrevPress);
            check(NextPress);
            check(UpPress);
            check(DownPress);
            devModeCounter = 0;
            while (rotarySteps > 0) {
                int prevEnabled = findNextEnabled(index, -1);
                if (prevEnabled < 0) break;
                index = prevEnabled;
                rotarySteps--;
                redraw = true;
            }
            while (rotarySteps < 0) {
                int nextEnabled = findNextEnabled(index, +1);
                if (nextEnabled < 0) break;
                if (!bruceConfig.devMode && nextEnabled <= index) devModeCounter++;
                index = nextEnabled;
                rotarySteps++;
                redraw = true;
            }
            vTaskDelay(4 / portTICK_PERIOD_MS);
            PrevPress = false;
            NextPress = false;
            UpPress = false;
            DownPress = false;
        } else
#endif
        {
            if (PrevPress || check(UpPress)) {
                devModeCounter = 0;
#ifdef HAS_KEYBOARD
                check(PrevPress);
                int prevEnabled = findNextEnabled(index, -1);
                if (prevEnabled >= 0) index = prevEnabled;
                redraw = true;
#else
                long _tmp = millis();
#ifndef HAS_ENCODER // T-Embed doesn't need it
                LongPress = true;
                while (PrevPress && menuType != MENU_TYPE_MAIN) {
                    if (millis() - _tmp > 200)
                        tft.drawArc(
                            tftWidth / 2,
                            tftHeight / 2,
                            25,
                            15,
                            0,
                            360 * (millis() - (_tmp + 200)) / 500,
                            getColorVariation(bruceConfig.priColor),
                            bruceConfig.bgColor
                        );
                    vTaskDelay(10 / portTICK_RATE_MS);
                }
                tft.drawArc(
                    tftWidth / 2, tftHeight / 2, 25, 15, 0, 360, bruceConfig.bgColor, bruceConfig.bgColor
                );
                LongPress = false;
#endif
                if (millis() - _tmp > 700) { // longpress detected to exit
                    index = -1;
                    break;
                } else {
                    check(PrevPress);
                    int prevEnabled = findNextEnabled(index, -1);
                    if (prevEnabled >= 0) index = prevEnabled;
                    redraw = true;
                }
#endif
            }
            /* DW Btn to next item */
            if (check(NextPress) || check(DownPress)) {
                int nextEnabled = findNextEnabled(index, +1);
                if (nextEnabled >= 0) {
                    if (!bruceConfig.devMode && nextEnabled <= index) devModeCounter++;
                    index = nextEnabled;
                }
                redraw = true;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }

        /* Select and run function
        forceMenuOption is set by a SerialCommand to force a selection within the menu
        */
        // Prevent immediate selection if the SEL button was already being held when the menu opened.
        // Allow a short grace period for the user to release the button first.
        static const unsigned long MENU_SELECT_IGNORE_MS = 600; // ms to ignore SEL after menu opens

        if (forceMenuOption >= 0 || (millis() - menuOpenTs > MENU_SELECT_IGNORE_MS && check(SelPress))) {
            uint16_t chosen = index;
            if (forceMenuOption >= 0) {
                chosen = forceMenuOption;
                forceMenuOption = -1; // reset SerialCommand navigation option
                Serial.print("Forcely ");
            }
            if (chosen >= options.size() || !options[chosen].enabled) continue;
            Serial.println("Selected: " + String(options[chosen].label));
            if (options[chosen].operation) {
                options[chosen].operation();
            }
            break;
        }
        // interpreter_start -> running the interpreter
        // interpreter -> loopOptions helper inside the Javascript
        if (interpreter_state > 0 && !interpreter) { break; }
    }

    RotaryNetSteps = 0; // reset rotary steps to avoid unexpected jumps in the next menu
    return index;
}

/***************************************************************************************
** Function name: truncateForWidth
** Description:   Dynamically measure and truncate text with ellipsis to prevent clipping
***************************************************************************************/
String truncateForWidth(const String &str, int maxPixelWidth, int textSize) {
    if (str.length() == 0 || maxPixelWidth <= 0) return "";
    
    // Quick estimation for standard fixed font 1 (glyph width = 6 * textSize)
    int glyphWidth = 6 * textSize;
    int maxChars = maxPixelWidth / glyphWidth;
    if (maxChars <= 0) return "";
    
    if ((int)str.length() <= maxChars) {
        return str;
    }
    
    if (maxChars <= 2) {
        return str.substring(0, maxChars);
    }
    
    return str.substring(0, maxChars - 2) + "..";
}

String padOrTruncate(const String &str, int maxChars, bool padRight) {
    if ((int)str.length() > maxChars) {
        if (maxChars <= 2) return str.substring(0, maxChars);
        return str.substring(0, maxChars - 2) + "..";
    }
    if (!padRight) return str;
    String res = str;
    while ((int)res.length() < maxChars) res += " ";
    return res;
}

/***************************************************************************************
** Function name: drawCyberCard
** Description:   Crisp tactical container card with corner accents
***************************************************************************************/
void drawCyberCard(int x, int y, int w, int h, uint16_t borderColor, uint16_t fillColor, const char *cornerTag) {
    if (fillColor != 0x0000 || bruceConfig.bgColor != 0x0000) {
        tft.fillRoundRect(x, y, w, h, 3, fillColor == 0x0000 ? bruceConfig.bgColor : fillColor);
    }
    tft.drawRoundRect(x, y, w, h, 3, borderColor);

    // Accent corner ticks (3px)
    tft.fillRect(x, y, 4, 2, borderColor);
    tft.fillRect(x, y, 2, 4, borderColor);
    tft.fillRect(x + w - 4, y, 4, 2, borderColor);
    tft.fillRect(x + w - 2, y, 2, 4, borderColor);
    tft.fillRect(x, y + h - 2, 4, 2, borderColor);
    tft.fillRect(x, y + h - 4, 2, 4, borderColor);
    tft.fillRect(x + w - 4, y + h - 2, 4, 2, borderColor);
    tft.fillRect(x + w - 2, y + h - 4, 2, 4, borderColor);

    if (cornerTag) {
        tft.setTextSize(FP);
        tft.setTextColor(borderColor, bruceConfig.bgColor);
        tft.drawString(cornerTag, x + 8, y + 4);
        tft.drawFastHLine(x + 6, y + 14, w - 12, borderColor);
    }
}

/***************************************************************************************
** Function name: drawCyberScrollbar
** Description:   Crisp scroll indicator for lists
***************************************************************************************/
void drawCyberScrollbar(int x, int y, int h, int currentIndex, int totalItems, int itemsPerPage) {
    if (totalItems <= itemsPerPage) return;

    tft.fillRect(x - 1, y, 3, h, bruceConfig.bgColor);
    tft.drawFastVLine(x, y, h, 0x31A6); // dark slate track

    int thumbH = h * itemsPerPage / totalItems;
    if (thumbH < 8) thumbH = 8;
    if (thumbH > h) thumbH = h;

    int maxScroll = totalItems - itemsPerPage;
    if (maxScroll < 1) maxScroll = 1;
    int scrollOffset = currentIndex;
    if (scrollOffset > maxScroll) scrollOffset = maxScroll;

    int thumbY = y + (h - thumbH) * scrollOffset / maxScroll;
    tft.fillRect(x - 1, thumbY, 3, thumbH, bruceConfig.priColor);
}

/***************************************************************************************
** Function name: drawCyberHeader
** Description:   Standardized tactical header with breadcrumb and status
***************************************************************************************/
void drawCyberHeader(const String &title, const String &category, bool showBattery) {
    tft.fillRect(0, 0, tftWidth, 21, bruceConfig.bgColor);
    tft.drawFastHLine(0, 20, tftWidth, 0x31A6);

    tft.setTextSize(FP);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);

    String t = title;
    t.toUpperCase();
    if (!t.startsWith("CYBER") && !t.startsWith("INVESTIGATION") && !t.startsWith("ANALYZER") && !t.startsWith("FINDING") && !t.startsWith("WIFI") && !t.startsWith("BLE") && !t.startsWith("FILES")) {
        t = "CYBER // " + t;
    }

    // Reserved right-side anchor:
    // If showBattery is true: battery widget spans x = 248..312 (width=64px)
    // Category is placed to the left of battery: rightAnchor = tftWidth - 74 (246 on 320px)
    // If showBattery is false: category can go up to tftWidth - 8 (312 on 320px)
    const int rightAnchor = showBattery ? (tftWidth - 74) : (tftWidth - 8);

    int catWidth = 0;
    if (category.length() > 0) {
        String catStr = "[" + category + "]";
        catWidth = catStr.length() * 6; // font size 1 = 6px per char
        tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
        tft.drawRightString(catStr, rightAnchor, 6, 1);
    }

    int reservedRight = showBattery ? 74 : 8;
    if (catWidth > 0) {
        reservedRight += (catWidth + 8);
    }
    int maxTitleWidth = tftWidth - reservedRight - 8; // 8px left margin
    if (maxTitleWidth < 50) maxTitleWidth = 50;

    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawString(truncateForWidth(t, maxTitleWidth, FP), 8, 6);

    if (showBattery) {
        drawBatteryStatus(getBattery());
    }
}

/***************************************************************************************
** Function name: drawCyberFooter
** Description:   Standardized tactical footer / action bar
***************************************************************************************/
void drawCyberFooter(const String &left, const String &center, const String &right) {
    tft.fillRect(0, 154, tftWidth, 16, bruceConfig.bgColor);
    tft.drawFastHLine(0, 154, tftWidth, 0x31A6);
    tft.setTextSize(FP);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);

    if (left.length() > 0) {
        String l = left;
        if (l.startsWith("[") && l.endsWith("]")) l = l.substring(1, l.length() - 1);
        tft.drawString(l, 8, 158);
    }
    if (center.length() > 0) {
        String c = center;
        if (c.startsWith("[") && c.endsWith("]")) c = c.substring(1, c.length() - 1);
        tft.drawCentreString(c, tftWidth / 2, 158, 1);
    }
    if (right.length() > 0) {
        String r = right;
        if (r.startsWith("[") && r.endsWith("]")) r = r.substring(1, r.length() - 1);
        tft.drawRightString(r, tftWidth - 8, 158, 1);
    }
}

/***************************************************************************************
** Function name: progressHandler
** Description:   Technical progress HUD bar
***************************************************************************************/
void progressHandler(int progress, size_t total, const String &message) {
    int cardW = tftWidth - 40;
    int cardH = 70;
    int cardX = 20;
    int cardY = (tftHeight - cardH) / 2;

    int pct = (total > 0) ? (progress * 100 / total) : 0;
    if (pct > 100) pct = 100;

    int barW = cardW - 24;
    int fillW = (total > 0) ? map(progress, 0, total, 0, barW) : 0;
    if (fillW > barW) fillW = barW;

    if (progress <= 1 || fillW < 3) {
        tft.fillRoundRect(cardX, cardY, cardW, cardH, 4, bruceConfig.bgColor);
        tft.drawRoundRect(cardX, cardY, cardW, cardH, 4, bruceConfig.priColor);
        tft.drawRoundRect(cardX + 2, cardY + 2, cardW - 4, cardH - 4, 3, bruceConfig.priColor);

        // Header tag
        tft.setTextSize(FP);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.drawString("TASK IN PROGRESS", cardX + 10, cardY + 8);

        tft.setTextSize(FM);
        tft.drawCentreString(message, tftWidth / 2, cardY + 22, 1);

        // Progress bar track
        tft.drawRoundRect(cardX + 10, cardY + 44, barW + 4, 16, 2, bruceConfig.priColor);
    }

    // Segmented progress fill
    if (fillW > 0) {
        tft.fillRect(cardX + 12, cardY + 46, fillW, 12, bruceConfig.priColor);
    }

    // Percentage text
    tft.setTextSize(FP);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawRightString(String(pct) + "%", cardX + cardW - 10, cardY + 8, 1);
}

/***************************************************************************************
** Function name: drawOptions
** Description:   Context options modal picker (Tactical Confirmation / Option Picker)
***************************************************************************************/
Opt_Coord drawOptions(
    int index, std::vector<Option> &options, uint16_t fgcolor, uint16_t selcolor, uint16_t bgcolor,
    bool firstRender
) {
    Opt_Coord coord;
    int menuSize = options.size();
    const int VISIBLE_ITEMS = 5;
    const int ROW_H = 21;

    int cardW = tftWidth - 28;
    int cardH = VISIBLE_ITEMS * ROW_H + 24;
    int cardX = 14;
    int cardY = (tftHeight - cardH) / 2;

    if (firstRender) {
        tft.fillRoundRect(cardX, cardY, cardW, cardH, 3, bgcolor);
        tft.drawRoundRect(cardX, cardY, cardW, cardH, 3, fgcolor);

        tft.setTextSize(FP);
        tft.setTextColor(fgcolor, bgcolor);
        tft.drawString("SELECT OPTION", cardX + 10, cardY + 5);
        tft.drawFastHLine(cardX + 6, cardY + 16, cardW - 12, fgcolor);
    }

    int start = 0;
    if (index >= VISIBLE_ITEMS) {
        start = index - VISIBLE_ITEMS + 1;
    }
    if (start + VISIBLE_ITEMS > menuSize && menuSize >= VISIBLE_ITEMS) {
        start = menuSize - VISIBLE_ITEMS;
    }
    if (start < 0) start = 0;

    int startY = cardY + 18;
    int maxTextPixelW = cardW - 38;

    for (int cont = 0; cont < VISIBLE_ITEMS && (start + cont) < menuSize; cont++) {
        int i = start + cont;
        int rowY = startY + cont * ROW_H;

        String label = String(options[i].label);
        String truncLabel = truncateForWidth(label, maxTextPixelW, FP);

        if (i == index) {
            tft.fillRect(cardX + 4, rowY, cardW - 14, 19, bruceConfig.priColor);
            tft.setTextColor(bgcolor, bruceConfig.priColor);
            tft.setTextSize(FP);
            tft.drawString(">", cardX + 8, rowY + 5);

            coord.x = cardX + 20;
            coord.y = rowY + 5;
            coord.size = maxTextPixelW / (LW * FP);
            coord.fgcolor = fgcolor;
            coord.bgcolor = bgcolor;

            tft.drawString(truncLabel, cardX + 20, rowY + 5);
        } else {
            tft.fillRect(cardX + 4, rowY, cardW - 14, 19, bgcolor);
            if (options[i].selected) tft.setTextColor(selcolor, bgcolor);
            else if (!options[i].enabled) tft.setTextColor(TFT_DARKGREY, bgcolor);
            else tft.setTextColor(fgcolor, bgcolor);

            tft.setTextSize(FP);
            tft.drawString(truncLabel, cardX + 20, rowY + 5);
        }
    }

    // Scrollbar inside modal
    drawCyberScrollbar(cardX + cardW - 6, startY, VISIBLE_ITEMS * ROW_H, index, menuSize, VISIBLE_ITEMS);

#if defined(HAS_TOUCH)
    TouchFooter();
#endif
    return coord;
}

/***************************************************************************************
** Function name: drawSubmenu
** Description:   Tactical 6-Row Information-Dense List Submenu (Pattern B)
***************************************************************************************/
void drawSubmenu(int index, std::vector<Option> &options, const char *title) {
    int menuSize = options.size();
    const int VISIBLE_ITEMS = TACTICAL_LIST_VISIBLE_ITEMS; // 6 items
    const int ROW_H = TACTICAL_LIST_ROW_H;                  // 21px
    const int START_Y = TACTICAL_CONTENT_TOP + 2;          // 24px

    // Format item counter badge
    String counter = "[" + String(index + 1 < 10 ? "0" : "") + String(index + 1) + "/" +
                     String(menuSize < 10 ? "0" : "") + String(menuSize) + "]";

    // Tactical header
    drawCyberHeader(title, counter, true);

    // Calculate scrolling window
    int start = 0;
    if (index >= VISIBLE_ITEMS) {
        start = index - VISIBLE_ITEMS + 1;
    }
    if (start + VISIBLE_ITEMS > menuSize && menuSize >= VISIBLE_ITEMS) {
        start = menuSize - VISIBLE_ITEMS;
    }
    if (start < 0) start = 0;

    for (int cont = 0; cont < VISIBLE_ITEMS; cont++) {
        int i = start + cont;
        int rowY = START_Y + cont * ROW_H;

        if (i >= menuSize) {
            tft.fillRect(4, rowY, tftWidth - 14, ROW_H, bruceConfig.bgColor);
            continue;
        }

        bool isSelected = (i == index);
        String label = options[i].label;

        // Check for toggle suffix
        bool hasToggle = false;
        bool toggleOn = false;
        String cleanLabel = label;
        if (cleanLabel.endsWith(":ON") || cleanLabel.endsWith(" ON") || cleanLabel.endsWith(": ON")) {
            hasToggle = true;
            toggleOn = true;
            if (cleanLabel.endsWith(": ON")) cleanLabel = cleanLabel.substring(0, cleanLabel.length() - 4);
            else if (cleanLabel.endsWith(":ON")) cleanLabel = cleanLabel.substring(0, cleanLabel.length() - 3);
            else if (cleanLabel.endsWith(" ON")) cleanLabel = cleanLabel.substring(0, cleanLabel.length() - 3);
        } else if (cleanLabel.endsWith(":OFF") || cleanLabel.endsWith(" OFF") || cleanLabel.endsWith(": OFF")) {
            hasToggle = true;
            toggleOn = false;
            if (cleanLabel.endsWith(": OFF")) cleanLabel = cleanLabel.substring(0, cleanLabel.length() - 5);
            else if (cleanLabel.endsWith(":OFF")) cleanLabel = cleanLabel.substring(0, cleanLabel.length() - 4);
            else if (cleanLabel.endsWith(" OFF")) cleanLabel = cleanLabel.substring(0, cleanLabel.length() - 4);
        }

        int maxLabelPixels = hasToggle ? (tftWidth - 96) : (tftWidth - 54);
        String truncLabel = truncateForWidth(cleanLabel, maxLabelPixels, FP);
        String idxStr = String(i + 1 < 10 ? "0" : "") + String(i + 1);

        if (isSelected) {
            // Selected row: solid accent pill
            tft.fillRect(4, rowY, tftWidth - 14, 19, bruceConfig.priColor);

            tft.setTextColor(bruceConfig.bgColor, bruceConfig.priColor);
            tft.setTextSize(FP);
            tft.drawString(">", 8, rowY + 5);

            // Index prefix & label
            tft.drawString(idxStr, 18, rowY + 5);
            tft.drawString(truncLabel, 36, rowY + 5);

            if (hasToggle) {
                tft.fillRoundRect(tftWidth - 44, rowY + 2, 32, 15, 2, toggleOn ? 0x07E0 : 0x4228);
                tft.setTextColor(toggleOn ? 0x0000 : 0xFFFF, toggleOn ? 0x07E0 : 0x4228);
                tft.drawCentreString(toggleOn ? "ON" : "OFF", tftWidth - 28, rowY + 5, 1);
            }
        } else {
            // Inactive row: subtle divider line
            tft.fillRect(4, rowY, tftWidth - 14, 19, bruceConfig.bgColor);
            tft.drawFastHLine(6, rowY + 19, tftWidth - 18, 0x18E3);

            tft.setTextSize(FP);
            tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
            tft.drawString(idxStr, 18, rowY + 5);

            if (!options[i].enabled) {
                tft.setTextColor(TFT_DARKGREY, bruceConfig.bgColor);
            } else if (options[i].selected) {
                tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
            } else {
                tft.setTextColor(0xCE79, bruceConfig.bgColor); // Slate near-white
            }
            tft.drawString(truncLabel, 36, rowY + 5);

            if (hasToggle) {
                tft.drawRoundRect(tftWidth - 44, rowY + 2, 32, 15, 2, toggleOn ? 0x07E0 : 0x4228);
                tft.setTextColor(toggleOn ? 0x07E0 : 0x8430, bruceConfig.bgColor);
                tft.drawCentreString(toggleOn ? "ON" : "OFF", tftWidth - 28, rowY + 5, 1);
            }
        }
    }

    // Scrollbar on right edge
    drawCyberScrollbar(tftWidth - 6, START_Y, VISIBLE_ITEMS * ROW_H, index, menuSize, VISIBLE_ITEMS);

    // Tactical footer
    drawCyberFooter("ROT NAV", "OK SELECT", "ESC BACK");

#if defined(HAS_TOUCH)
    TouchFooter();
#endif
}



void drawStatusBar() {
    // Clear top status bar
    tft.fillRect(0, 0, tftWidth, 21, bruceConfig.bgColor);
    tft.drawFastHLine(0, 20, tftWidth, bruceConfig.priColor);
    tft.drawPixel(0, 20, bruceConfig.priColor);
    tft.drawPixel(tftWidth - 1, 20, bruceConfig.priColor);

    // Brand tag on left
    tft.setTextSize(FP);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawString("CYBER-EMBED", 8, 5);

    // Clock / Version right-aligned before battery widget
    const int rightAnchor = tftWidth - 74;
    if (clock_set) {
#if defined(HAS_RTC)
        updateTimeStr(_rtc.getTimeStruct());
#else
        updateTimeStr(rtc.getTimeStruct());
#endif
        tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
        tft.drawRightString(timeStr, rightAnchor, 5, 1);
    } else {
        tft.setTextColor(0x8430, bruceConfig.bgColor);
        tft.drawRightString("v" + String(BRUCE_VERSION), rightAnchor, 5, 1);
    }

    // Subsystem status pills in center
    int iconCount = 0;
    bool showSD = sdcardMounted;
    bool showGPS = gpsConnected;
    bool showWifi = (WiFi.getMode() != 0);
    bool showWeb = isWebUIActive;
    bool showBLE = BLEConnected;
    bool showWG = isConnectedWireguard;

    if (showSD) iconCount++;
    if (showGPS) iconCount++;
    if (showWifi) iconCount++;
    if (showWeb) iconCount++;
    if (showBLE) iconCount++;
    if (showWG) iconCount++;

    if (iconCount > 0) {
        const int IW = 14;
        const int GAP = 4;
        int sx = 88;
        int iy = 3;
        int idx = 0;

        if (showSD) {
            drawSdSmall(sx + idx * (IW + GAP), iy);
            idx++;
        }
        if (showGPS) {
            drawGpsSmall(sx + idx * (IW + GAP), iy);
            idx++;
        }
        if (showWifi) {
            drawWifiSmall(sx + idx * (IW + GAP), iy);
            idx++;
        }
        if (showWeb) {
            drawWebUISmall(sx + idx * (IW + GAP), iy);
            idx++;
        }
        if (showBLE) {
            drawBLESmall(sx + idx * (IW + GAP), iy);
            idx++;
        }
        if (showWG) {
            drawWireguardStatus(sx + idx * (IW + GAP), iy);
            idx++;
        }
    }

    // Battery status indicator on right
    drawBatteryStatus(getBattery());
}

void drawMainBorder(bool clear) {
    if (clear) {
        tft.drawPixel(0, 0, 0);
        tft.fillScreen(bruceConfig.bgColor);
    }
    drawStatusBar();

#if defined(HAS_TOUCH)
    TouchFooter();
#endif
}

void drawMainBorderWithTitle(const String &title, bool clear) {
    drawMainBorder(clear);
    printTitle(title);
}

void printTitle(const String &title) {
    String t = title;
    t.toUpperCase();
    drawCyberHeader(t);
}

void printSubtitle(const String &subtitle, bool withLine) {
    int16_t cursorX = (tftWidth - (subtitle.length() * FP * LW)) / 2;
    if (cursorX < 8) cursorX = 8;
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.setTextSize(FP);

    tft.drawString(subtitle, cursorX, 44);

    if (withLine) {
        int lineW = subtitle.length() * FP * LW;
        if (lineW > tftWidth - 16) lineW = tftWidth - 16;
        tft.drawFastHLine(cursorX, 54, lineW, bruceConfig.priColor);
    }
}

void printFootnote(const String &text) {
    tft.setTextSize(FP);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawRightString(text, tftWidth - BORDER_PAD_X, tftHeight - 12, SMOOTH_FONT);
}

void printCenterFootnote(const String &text) {
    tft.fillRect(10, tftHeight - 16, tftWidth - 20, 14, bruceConfig.bgColor);
    tft.setTextSize(FP);
    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawCentreString(text, tftWidth / 2, tftHeight - 12, SMOOTH_FONT);
}

void drawBatteryStatus(uint8_t bat) {
    if (bat == 0) return;

    bool charging = isCharging();

    uint16_t barcolor = 0x07E0; // Green
    if (bat < 20) barcolor = TFT_RED;
    else if (bat < 40) barcolor = 0xFDC0; // Amber

    // Battery geometry (furthest right safe position inside 320x170 display)
    const int bw = 22;
    const int bh = 12;
    const int by = 4; // Vertically centered in 20px header (4..16)
    const int capW = 2;
    const int capH = 6;
    const int rightPad = 8; // Margin from screen right edge (x=320)

    // Battery body x coordinate (bx = 320 - 8 - 2 - 22 = 288)
    const int bx = tftWidth - rightPad - capW - bw;

    // Clear background for battery widget region (including readout text) to prevent stale pixels
    tft.fillRect(bx - 36, 0, bw + capW + 36 + rightPad, 20, bruceConfig.bgColor);

    // Battery outline
    tft.drawRoundRect(bx, by, bw, bh, 2, bruceConfig.priColor);
    // Terminal cap (vertically centered at y=7..13)
    tft.fillRect(bx + bw, by + 3, capW, capH, bruceConfig.priColor);

    // Level fill
    const int innerW = bw - 4; // 18px
    const int innerH = bh - 4; // 8px
    int fillW = (innerW * bat) / 100;
    if (fillW < 1 && bat > 0) fillW = 1;
    if (fillW > innerW) fillW = innerW;
    tft.fillRect(bx + 2, by + 2, innerW, innerH, bruceConfig.bgColor);
    tft.fillRect(bx + 2, by + 2, fillW, innerH, barcolor);

    // Readout text (CHG / bat%) placed right-aligned to left of battery icon with 4px gap
    tft.setTextSize(FP);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    if (charging) {
        tft.drawRightString("CHG", bx - 4, by + 2, 1);
    } else {
        tft.drawRightString(String(bat) + "%", bx - 4, by + 2, 1);
    }
}

void drawWireguardStatus(int x, int y) {
    tft.fillRect(x, y, 14, 14, bruceConfig.bgColor);
    if (isConnectedWireguard) {
        tft.drawRoundRect(x + 3, y + 1, 8, 6, 2, bruceConfig.priColor);
        tft.fillRoundRect(x + 2, y + 5, 10, 7, 1, bruceConfig.priColor);
    } else {
        tft.drawRoundRect(x + 3, y + 1, 8, 6, 2, 0x4228);
        tft.drawRoundRect(x + 2, y + 5, 10, 7, 1, 0x4228);
    }
}

/***************************************************************************************
** Function name: listFiles
** Description:   Tactical File Manager row renderer (Pattern B)
***************************************************************************************/
#define MAX_ITEMS 6
Opt_Coord listFiles(int index, std::vector<FileList> fileList) {
    Opt_Coord coord;
    tft.drawPixel(0, 0, bruceConfig.bgColor);

    const int ROW_H = TACTICAL_LIST_ROW_H;
    const int START_Y = TACTICAL_CONTENT_TOP + 2;

    int arraySize = fileList.size();
    String counter = "[" + String(index + 1 < 10 ? "0" : "") + String(index + 1) + "/" +
                     String(arraySize < 10 ? "0" : "") + String(arraySize) + "]";

    bool isSD = sdcardMounted;
    drawCyberHeader(isSD ? "FILES // SD CARD" : "FILES // LITTLEFS", counter, true);

    int start = 0;
    if (index >= MAX_ITEMS) {
        start = index - MAX_ITEMS + 1;
    }
    if (start + MAX_ITEMS > arraySize && arraySize >= MAX_ITEMS) {
        start = arraySize - MAX_ITEMS;
    }
    if (start < 0) start = 0;

    int maxFilenamePixels = tftWidth - 84;

    for (int cont = 0; cont < MAX_ITEMS; cont++) {
        int i = start + cont;
        int rowY = START_Y + cont * ROW_H;

        if (i >= arraySize) {
            tft.fillRect(4, rowY, tftWidth - 14, ROW_H, bruceConfig.bgColor);
            continue;
        }

        bool isSelected = (i == index);
        String filename = fileList[i].filename;

        // Tag badge for file type
        String tag = "[FILE]";
        uint16_t tagColor = 0x8430;

        if (fileList[i].folder) {
            tag = "[DIR]";
            tagColor = 0xFDC0; // Amber
        } else if (fileList[i].operation) {
            tag = "[..]";
            tagColor = bruceConfig.secColor;
        } else if (filename.endsWith(".txt") || filename.endsWith(".ducky")) {
            tag = "[DUCK]";
            tagColor = 0x07E0; // Green
        } else if (filename.endsWith(".js") || filename.endsWith(".mqjs")) {
            tag = "[JS]";
            tagColor = 0xFFE0; // Yellow
        } else if (filename.endsWith(".sub") || filename.endsWith(".raw") || filename.endsWith(".rf")) {
            tag = "[RF]";
            tagColor = 0x07FF; // Cyan
        } else if (filename.endsWith(".nfc") || filename.endsWith(".mif")) {
            tag = "[NFC]";
            tagColor = 0xF81F; // Magenta
        } else if (filename.endsWith(".ir")) {
            tag = "[IR]";
            tagColor = 0xFC40; // Orange
        } else if (filename.endsWith(".bin") || filename.endsWith(".hex")) {
            tag = "[BIN]";
            tagColor = 0x96FE; // Light blue
        }

        String truncName = truncateForWidth(filename, maxFilenamePixels, FP);

        if (isSelected) {
            tft.fillRect(4, rowY, tftWidth - 14, 19, bruceConfig.priColor);
            tft.setTextColor(bruceConfig.bgColor, bruceConfig.priColor);
            tft.setTextSize(FP);
            tft.drawString(">", 8, rowY + 5);

            // Badge
            tft.drawString(tag, 18, rowY + 5);

            // Filename
            tft.drawString(truncName, 64, rowY + 5);

            coord.x = 64;
            coord.y = rowY + 5;
            coord.size = maxFilenamePixels / (LW * FP);
            coord.fgcolor = bruceConfig.priColor;
            coord.bgcolor = bruceConfig.bgColor;
        } else {
            tft.fillRect(4, rowY, tftWidth - 14, 19, bruceConfig.bgColor);
            tft.drawFastHLine(6, rowY + 19, tftWidth - 18, 0x18E3);

            // Badge
            tft.setTextSize(FP);
            tft.setTextColor(tagColor, bruceConfig.bgColor);
            tft.drawString(tag, 18, rowY + 5);

            // Filename
            tft.setTextColor(fileList[i].folder ? 0xFFFF : 0xCE79, bruceConfig.bgColor);
            tft.drawString(truncName, 64, rowY + 5);
        }
    }

    // Scrollbar on right edge
    drawCyberScrollbar(tftWidth - 6, START_Y, MAX_ITEMS * ROW_H, index, arraySize, MAX_ITEMS);

    // Tactical footer
    drawCyberFooter("ROT NAV", "OK OPEN", "ESC BACK");

    return coord;
}

// desenhos do menu principal, sprite "draw" com 80x80 pixels

void drawSdSmall(int x, int y) {
    tft.fillRect(x, y, 16, 16, bruceConfig.bgColor);
    tft.drawLine(x + 3, y + 2, x + 3, y + 14, bruceConfig.priColor);
    tft.drawLine(x + 3, y + 14, x + 13, y + 14, bruceConfig.priColor);
    tft.drawLine(x + 13, y + 14, x + 13, y + 5, bruceConfig.priColor);
    tft.drawLine(x + 13, y + 5, x + 10, y + 2, bruceConfig.priColor);
    tft.drawLine(x + 10, y + 2, x + 3, y + 2, bruceConfig.priColor);

    tft.drawLine(x + 5, y + 4, x + 5, y + 6, bruceConfig.priColor);
    tft.drawLine(x + 7, y + 4, x + 7, y + 6, bruceConfig.priColor);
    tft.drawLine(x + 9, y + 4, x + 9, y + 6, bruceConfig.priColor);
    tft.drawLine(x + 11, y + 5, x + 11, y + 6, bruceConfig.priColor);
}

void drawWifiSmall(int x, int y) {
    tft.fillRect(x, y, 16, 16, bruceConfig.bgColor);
    tft.fillCircle(x + 8, y + 13, 1, bruceConfig.priColor);
    tft.drawArc(x + 8, y + 13, 4, 6, 135, 225, bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawArc(x + 8, y + 13, 9, 11, 135, 225, bruceConfig.priColor, bruceConfig.bgColor);
}

void drawWebUISmall(int x, int y) {
    tft.fillRect(x, y, 16, 16, bruceConfig.bgColor);
    tft.drawCircle(x + 8, y + 8, 6, bruceConfig.priColor);
    tft.drawLine(x + 3, y + 4, x + 13, y + 4, bruceConfig.priColor);
    tft.drawLine(x + 2, y + 8, x + 14, y + 8, bruceConfig.priColor);
    tft.drawLine(x + 3, y + 12, x + 13, y + 12, bruceConfig.priColor);
}

void drawBLESmall(int x, int y) {
    tft.fillRect(x, y, 16, 16, bruceConfig.bgColor);
    tft.drawWideLine(x + 8, y + 8, x + 4, y + 4, 2, bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawWideLine(x + 8, y + 8, x + 4, y + 12, 2, bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawTriangle(x + 8, y + 8, x + 8, y + 2, x + 12, y + 5, bruceConfig.priColor);
    tft.drawTriangle(x + 8, y + 8, x + 8, y + 14, x + 12, y + 11, bruceConfig.priColor);
}

void drawBLE_beacon(int x, int y, uint16_t color) {
    tft.fillRect(x, y, 40, 80, bruceConfig.bgColor);
    tft.drawWideLine(40 + x, 53 + y, 2 + x, 26 + y, 5, color, bruceConfig.bgColor);
    tft.drawWideLine(40 + x, 26 + y, 2 + x, 53 + y, 5, color, bruceConfig.bgColor);
    tft.drawWideLine(40 + x, 53 + y, 20 + x, 68 + y, 5, color, bruceConfig.bgColor);
    tft.drawWideLine(40 + x, 26 + y, 20 + x, 12 + y, 5, color, bruceConfig.bgColor);
    tft.drawWideLine(20 + x, 12 + y, 20 + x, 68 + y, 5, color, bruceConfig.bgColor);
    tft.fillTriangle(40 + x, 26 + y, 20 + x, 40 + y, 20 + x, 12 + y, color);
    tft.fillTriangle(40 + x, 53 + y, 20 + x, 40 + y, 20 + x, 68 + y, color);
}

void drawGPS(int x, int y) {
    tft.fillRect(x, y, 80, 80, bruceConfig.bgColor);
    tft.drawEllipse(40 + x, 70 + y, 15, 8, bruceConfig.priColor);
    tft.drawArc(40 + x, 25 + y, 23, 7, 0, 340, bruceConfig.priColor, bruceConfig.bgColor);
    tft.fillTriangle(40 + x, 70 + y, 20 + x, 64 + y, 60 + x, 64 + y, bruceConfig.priColor);
}

void drawGpsSmall(int x, int y) {
    tft.fillRect(x, y, 16, 16, bruceConfig.bgColor);
    tft.drawEllipse(x + 8, y + 13, 4, 3, bruceConfig.priColor);
    tft.drawArc(x + 8, y + 5, 5, 2, 0, 360, bruceConfig.priColor, bruceConfig.bgColor);
    tft.fillTriangle(x + 8, y + 14, x + 4, y + 8, x + 12, y + 8, bruceConfig.priColor);
}

void drawCreditCard(int x, int y) {
    tft.fillRect(x, y, 70, 50, bruceConfig.bgColor);
    tft.fillRoundRect(x + 5, y + 5, 60, 40, 5, bruceConfig.priColor);
    tft.fillRect(x + 5, y + 15, 60, 10, getColorVariation(bruceConfig.priColor, 3, -1));
    tft.fillRect(x + 10, y + 30, 12, 10, getColorVariation(bruceConfig.priColor, 3, 1));
    tft.drawRect(x + 10, y + 30, 12, 10, getColorVariation(bruceConfig.priColor, 5, -1));
    tft.drawRect(x + 10 + 4, y + 30, 4, 10, getColorVariation(bruceConfig.priColor, 5, -1));
    tft.drawRect(x + 10, y + 33, 5, 4, getColorVariation(bruceConfig.priColor, 5, -1));
    tft.drawRect(x + 17, y + 33, 5, 4, getColorVariation(bruceConfig.priColor, 5, -1));
    tft.fillRect(x + 30, y + 35, 30, 5, getColorVariation(bruceConfig.priColor, 5, 1));
}

void drawMfkey32Icon(int x, int y) {
    tft.drawRect(x + 2, y + 15, 24, 40, bruceConfig.priColor);
    tft.drawRect(x + 5, y + 18, 18, 12, bruceConfig.priColor);
    tft.drawRect(x + 5, y + 34, 18, 18, bruceConfig.priColor);
    tft.drawLine(x + 5, y + 40, x + 22, y + 40, bruceConfig.priColor);
    tft.drawLine(x + 5, y + 46, x + 22, y + 46, bruceConfig.priColor);
    tft.drawLine(x + 11, y + 34, x + 11, y + 51, bruceConfig.priColor);
    tft.drawLine(x + 17, y + 34, x + 17, y + 51, bruceConfig.priColor);
    tft.drawRect(x + 30, y + 10, 25, 35, bruceConfig.priColor);
    int startX = x + 32;
    int startY = y + 12;
    int endX = x + 52;
    int endY = y + 32;
    int step = 2;
    int turns = 0;

    while (startX <= endX && startY <= endY && turns < 3) {
        for (int i = startX; i <= endX; i++) { tft.drawPixel(i, startY, bruceConfig.priColor); }
        startY += step;
        for (int i = startY; i <= endY; i++) { tft.drawPixel(endX, i, bruceConfig.priColor); }
        endX -= step;
        for (int i = endX; i >= startX; i--) { tft.drawPixel(i, endY, bruceConfig.priColor); }
        endY -= step;
        for (int i = endY; i >= startY; i--) { tft.drawPixel(startX, i, bruceConfig.priColor); }
        startX += step;
        turns++;
    }
    tft.fillRect(x + 40, y + 36, 6, 6, getColorVariation(bruceConfig.priColor, 3, 1));
}

void drawMfkey64Icon(int x, int y) {
    drawMfkey32Icon(x, y);
    tft.fillRoundRect(x + 40, y + 6, 24, 14, 4, bruceConfig.bgColor);
    tft.drawRoundRect(x + 40, y + 6, 24, 14, 4, getColorVariation(bruceConfig.priColor, 3, -1));
    tft.drawCircle(x + 48, y + 12, 4, getColorVariation(bruceConfig.priColor, 3, -1));
}

// ####################################################################################################
//  Draw a JPEG on the TFT, images will be cropped on the right/bottom sides if they do not fit
// ####################################################################################################
//  from:
//  https://github.com/Bodmer/TFT_eSPI/blob/master/examples/Generic/ESP32_SDcard_jpeg/ESP32_SDcard_jpeg.ino
//  This function assumes xpos,ypos is a valid screen coordinate. For convenience images that do not
//  fit totally on the screen are cropped to the nearest MCU size and may leave right/bottom borders.
void jpegRender(int xpos, int ypos) {

    // jpegInfo(); // Print information from the JPEG file (could comment this line out)

    uint16_t *pImg;
    uint16_t mcu_w = JpegDec.MCUWidth;
    uint16_t mcu_h = JpegDec.MCUHeight;
    uint32_t max_x = JpegDec.width;
    uint32_t max_y = JpegDec.height;

    bool swapBytes = tft.getSwapBytes();
    tft.setSwapBytes(true);

    // Jpeg images are draw as a set of image block (tiles) called Minimum Coding Units (MCUs)
    // Typically these MCUs are 16x16 pixel blocks
    // Determine the width and height of the right and bottom edge image blocks
    uint32_t min_w = jpg_min(mcu_w, max_x % mcu_w);
    uint32_t min_h = jpg_min(mcu_h, max_y % mcu_h);

    // save the current image block size
    uint32_t win_w = mcu_w;
    uint32_t win_h = mcu_h;

    // save the coordinate of the right and bottom edges to assist image cropping
    // to the screen size
    max_x += xpos;
    max_y += ypos;

    // Fetch data from the file, decode and display
    tft.fillRect(xpos, ypos, JpegDec.width, JpegDec.height, TFT_BLACK);
    while (JpegDec.read()) {   // While there is more data in the file
        pImg = JpegDec.pImage; // Decode a MCU (Minimum Coding Unit, typically a 8x8 or 16x16 pixel block)

        // Calculate coordinates of top left corner of current MCU
        int mcu_x = JpegDec.MCUx * mcu_w + xpos;
        int mcu_y = JpegDec.MCUy * mcu_h + ypos;

        // check if the image block size needs to be changed for the right edge
        if (mcu_x + mcu_w <= max_x) win_w = mcu_w;
        else win_w = min_w;

        // check if the image block size needs to be changed for the bottom edge
        if (mcu_y + mcu_h <= max_y) win_h = mcu_h;
        else win_h = min_h;

        // copy pixels into a contiguous block
        if (win_w != mcu_w) {
            uint16_t *cImg;
            int p = 0;
            cImg = pImg + win_w;
            for (int h = 1; h < win_h; h++) {
                p += mcu_w;
                for (int w = 0; w < win_w; w++) {
                    *cImg = *(pImg + w + p);
                    cImg++;
                }
            }
        }

        // calculate how many pixels must be drawn
        uint32_t mcu_pixels = win_w * win_h;

        // draw image MCU block only if it will fit on the screen
        if ((mcu_x + win_w) <= tft.width() && (mcu_y + win_h) <= tft.height())
            tft.pushImage(mcu_x, mcu_y, win_w, win_h, pImg);
        else if ((mcu_y + win_h) > tft.height())
            JpegDec.abort(); // Image has run off bottom of screen so abort decoding
    }

    tft.setSwapBytes(swapBytes);
}

bool showJpeg(FS &fs, const String &filename, int x, int y, bool center) {
    // record the current time so we can measure how long it takes to draw an image
    uint32_t drawTime = millis();
    File picture;
    if (fs.exists(filename)) picture = fs.open(filename, FILE_READ);
    else return false;

    const size_t data_size = picture.size();

    // Alloc memory into heap
    uint8_t *data_array = new uint8_t[data_size];
    if (data_array == nullptr) {
        // Fail allocating memory
        picture.close();
        delete[] data_array;
        return false;
    }

    uint8_t data;
    int i = 0;
    byte line_len = 0;

    while (picture.available()) {
        data = picture.read();
        data_array[i] = data;
        i++;

        // print array on Serial
        /*
        Serial.print("0x");
        if (abs(data) < 16) {
          Serial.print("0");
        }

        Serial.print(data, HEX);
        Serial.print(","); // Add value and comma
        line_len++;
        if (line_len >= 32) {
          line_len = 0;
          Serial.println();
        }
        */
    }

    picture.close();

    bool decoded = false;
    if (data_array) {
        decoded = JpegDec.decodeArray(data_array, data_size);
    } else {
        displayError(filename + " Fail");
        delay(2500);
        delete[] data_array; // free heap before leaving
        return false;
    }

    if (decoded) {
        if (center) {
            x = x + (tftWidth - JpegDec.width) / 2;
            y = y + (tftHeight - JpegDec.height) / 2;
        }
        jpegRender(x, y);
    }
    // calculate how long it took to draw the image
    drawTime = millis() - drawTime; // Calculate the time it took

    // print the results to the serial port
    Serial.print("Total render time was    : ");
    Serial.print(drawTime);
    Serial.println(" ms");
    Serial.println("=====================================");

    delete[] data_array; // free heap before leaving
    return true;
}

bool showJpeg(const uint8_t *data_array, size_t data_size, int x, int y, bool center) {
    bool decoded = false;
    if (data_array) {
        decoded = JpegDec.decodeArray(data_array, data_size);
    } else {
        return false;
    }

    if (decoded) {
        if (center) {
            x = x + (tftWidth - JpegDec.width) / 2;
            y = y + (tftHeight - JpegDec.height) / 2;
        }
        jpegRender(x, y);
    }

    return true;
}

#if !defined(LITE_VERSION)
// ####################################################################################################
//  Draw a GIF on the TFT
//  derived from
//  https://github.com/bitbank2/AnimatedGIF/blob/master/examples/TFT_eSPI_memory/TFT_eSPI_memory.ino and
//  https://github.com/bitbank2/AnimatedGIF/blob/master/examples/best_practices_example/best_practices_example.ino
// ####################################################################################################

Gif::Gif() : gifPosition(0, 0) {}

Gif::~Gif() {
    if (gif != nullptr) {
        gif->close();
        delete gif;
        gif = nullptr;
    }
}

FS *Gif::GifFs = NULL;

void *Gif::openFile(const char *fname, int32_t *pSize) {
    File GifFile;

    if (GifFs != NULL) {
        GifFile = GifFs->open(fname);
    } else {
        if (SD.exists(fname)) GifFile = SD.open(fname);
        else if (LittleFS.exists(fname)) GifFile = LittleFS.open(fname);
    }

    File *FSGifFile = new File(GifFile);

    if (FSGifFile) {
        *pSize = FSGifFile->size();
        return (void *)FSGifFile;
    }
    return NULL;
}

void Gif::closeFile(void *pHandle) {
    File *f = static_cast<File *>(pHandle);
    if (f != NULL) { f->close(); }
    delete f;
}

int32_t Gif::readFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
    int32_t iBytesRead;
    iBytesRead = iLen;
    File *f = static_cast<File *>(pFile->fHandle);
    // Note: If you read a file all the way to the last byte, seek() stops working
    if ((pFile->iSize - pFile->iPos) < iLen)
        iBytesRead = pFile->iSize - pFile->iPos - 1; // <-- ugly work-around
    if (iBytesRead <= 0) return 0;
    iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
    pFile->iPos = f->position();
    return iBytesRead;
}

int32_t Gif::seekFile(GIFFILE *pFile, int32_t iPosition) {
    int i = micros();
    File *f = static_cast<File *>(pFile->fHandle);
    f->seek(iPosition);
    pFile->iPos = (int32_t)f->position();
    i = micros() - i;
    return pFile->iPos;
}

void Gif::GIFDraw(GIFDRAW *pDraw) {
    uint8_t *s;
    uint16_t *d, *usPalette, usTemp[tftWidth];
    int x, y, iWidth;

    GifPosition *position = (GifPosition *)(pDraw->pUser);

    iWidth = pDraw->iWidth;
    if (iWidth > tftWidth) iWidth = tftWidth;
    usPalette = pDraw->pPalette;
    y = pDraw->iY + pDraw->y; // current line

    s = pDraw->pPixels;
    if (pDraw->ucDisposalMethod == 2) { // restore to background color
        for (x = 0; x < iWidth; x++) {
            if (s[x] == pDraw->ucTransparent) s[x] = pDraw->ucBackground;
        }
        pDraw->ucHasTransparency = 0;
    }
    // Apply the new pixels to the main image
    if (pDraw->ucHasTransparency) { // if transparency used
        uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
        int x, iCount;
        pEnd = s + iWidth;
        x = 0;
        iCount = 0; // count non-transparent pixels
        while (x < iWidth) {
            c = ucTransparent - 1;
            d = usTemp;
            while (c != ucTransparent && s < pEnd) {
                c = *s++;
                if (c == ucTransparent) { // done, stop
                    s--;                  // back up to treat it like transparent
                } else {                  // opaque
                    *d++ = usPalette[c];
                    iCount++;
                }
            } // while looking for opaque pixels
            if (iCount) { // any opaque pixels?
                tft.drawPixel(0, 0, 0);
                tft.pushImage(pDraw->iX + x + position->x, y + position->y, iCount, 1, (uint16_t *)usTemp);
                x += iCount;
                iCount = 0;
            }
            // no, look for a run of transparent pixels
            c = ucTransparent;
            while (c == ucTransparent && s < pEnd) {
                c = *s++;
                if (c == ucTransparent) iCount++;
                else s--;
            }
            if (iCount) {
                x += iCount; // skip these
                iCount = 0;
            }
        }
    } else {
        s = pDraw->pPixels;
        // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
        for (x = 0; x < iWidth; x++) usTemp[x] = usPalette[*s++];
        tft.drawPixel(0, 0, 0);
        tft.pushImage(pDraw->iX + position->x, y + position->y, iWidth, 1, (uint16_t *)usTemp);
    }
} /* GIFDraw() */

bool Gif::openGIF(FS *fs, const char *filename) {
    if (fs != NULL) {
        GifFs = fs;
        if (!fs->exists(filename)) return false;
    } else {
        GifFs = NULL;
    }

    gif = new AnimatedGIF();
    gif->begin(BIG_ENDIAN_PIXELS);
    if (gif->open(filename, openFile, closeFile, readFile, seekFile, GIFDraw)) { return true; }

    log_e("GIF opening error: %d\n", gif->getLastError());
    return false;
}

// Play a single frame
// returns:
// 2 = skipped waiting for another frame
// 1 = good result and more frames exist
// 0 = no more frames exist, a frame may or may not have been played: use getLastError() and look for
// GIF_SUCCESS to know if a frame was played -1 = error
int Gif::playFrame(int x, int y, bool bSync) {
    if (gif == nullptr) return -1;

    gifPosition.x = x;
    gifPosition.y = y;
    return gif->playFrame(bSync, nullptr, &gifPosition);
}

int Gif::getLastError() { return gif->getLastError(); }

/*
 * playDurationMs:
 *  -1 : Play the GIF in an infinite loop
 *  0  : Play the GIF once
 * >0  : Play the GIF for the specified duration in milliseconds
 *       (e.g., 1000 = play for 1 second)
 */
bool showGif(
    FS *fs, const char *filename, int x, int y, bool center, int playDurationMs, bool resetButtonStatus
) {
    if (!fs->exists(filename)) return false;

    Gif gif;
    bool success = gif.openGIF(fs, filename);
    if (!success) { return false; }

    if (center) {
        x = x + (tftWidth - gif.getCanvasWidth()) / 2;
        y = y + (tftHeight - gif.getCanvasHeight()) / 2;
    }

    int result = 0;
    long timeStart = millis();
    do {
        result = gif.playFrame(x, y);
        if (result == -1) log_e("GIF playFrame error: %d\n", gif.getLastError());

        if (check(AnyKeyPress, resetButtonStatus)) break;

        if (playDurationMs > 0 && (millis() - timeStart) > playDurationMs) break;
        if (playDurationMs == 0 && result == 0) break;
    } while (result >= 0);

    return true;
}
#endif
/***************************************************************************************
** Function name: getComplementaryColor2
** Description:   Get simple complementary color in RGB565 format
***************************************************************************************/
uint16_t getComplementaryColor2(uint16_t color) {
    int r = 31 - ((color >> 11) & 0x1F);
    int g = 63 - ((color >> 5) & 0x3F);
    int b = 31 - (color & 0x1F);
    return (r << 11) | (g << 5) | b;
}
/***************************************************************************************
** Function name: getComplementaryColor
** Description:   Get complementary color in RGB565 format
***************************************************************************************/
uint16_t getComplementaryColor(uint16_t color) {
    double r = ((color >> 11) & 0x1F) / 31.0;
    double g = ((color >> 5) & 0x3F) / 63.0;
    double b = (color & 0x1F) / 31.0;

    double cmax = fmax(r, fmax(g, b));
    double cmin = fmin(r, fmin(g, b));
    double delta = cmax - cmin;

    double hue = 0.0;
    if (delta == 0) hue = 0.0;
    else if (cmax == r) hue = 60 * fmod((g - b) / delta, 6);
    else if (cmax == g) hue = 60 * ((b - r) / delta + 2);
    else hue = 60 * ((r - g) / delta + 4);

    if (hue < 0) hue += 360;

    double lightness = (cmax + cmin) / 2;
    double saturation = (delta == 0) ? 0 : delta / (1 - std::abs(2 * lightness - 1));

    double compHue = fmod(hue + 180, 360);

    double c = (1 - std::abs(2 * lightness - 1)) * saturation;
    double x = c * (1 - std::abs(fmod(compHue / 60, 2) - 1));
    double m = lightness - c / 2;

    double compR = 0, compG = 0, compB = 0;
    if (compHue >= 0 && compHue < 60) {
        compR = c;
        compG = x;
    } else if (compHue >= 60 && compHue < 120) {
        compR = x;
        compG = c;
    } else if (compHue >= 120 && compHue < 180) {
        compG = c;
        compB = x;
    } else if (compHue >= 180 && compHue < 240) {
        compG = x;
        compB = c;
    } else if (compHue >= 240 && compHue < 300) {
        compB = c;
        compR = x;
    } else {
        compB = x;
        compR = c;
    }

    uint16_t compl_color = uint8_t(compR * 31) << 11 | uint8_t(compG * 63) << 5 | uint8_t(compB * 31);

    // change black color
    if (compl_color == 0) compl_color = color - 0x1111;

    return compl_color;
}

/***************************************************************************************
** Function name: getColorVariation
** Description:   Get a variation of color in RGB565 format
***************************************************************************************/
uint16_t getColorVariation(uint16_t color, int delta, int direction) {
    uint8_t r = ((color >> 11) & 0x1F);
    uint8_t g = ((color >> 5) & 0x3F);
    uint8_t b = (color & 0x1F);

    float brightness = 0.299 * r / 31 + 0.587 * g / 63 + 0.114 * b / 31;

    if (direction < 0 || (direction == 0 && brightness >= 0.5)) {
        r = max(0, r - delta);
        g = max(0, g - 2 * delta);
        b = max(0, b - delta);
    } else {
        r = min(31, r + delta);
        g = min(63, g + 2 * delta);
        b = min(31, b + delta);
    }

    uint16_t compl_color = r << 11 | g << 5 | b;

    return compl_color;
}

// Draw BITMAP files
// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(fs::File &f) {
    uint16_t result = 0;                // Initialize to prevent undefined behavior
    ((uint8_t *)&result)[0] = f.read(); // LSB
    ((uint8_t *)&result)[1] = f.read(); // MSB
    return result;
}

uint32_t read32(fs::File &f) {
    uint32_t result = 0;                // Initialize to prevent undefined behavior
    ((uint8_t *)&result)[0] = f.read(); // LSB
    ((uint8_t *)&result)[1] = f.read();
    ((uint8_t *)&result)[2] = f.read();
    ((uint8_t *)&result)[3] = f.read(); // MSB
    return result;
}
bool drawBmp(FS &fs, const String &filename, int x, int y, bool center) {
    if ((x >= tft.width()) || (y >= tft.height())) return false;
    uint32_t startTime = millis();

    File bmpFS;

    // Open requested file on SD card
    bmpFS = fs.open(filename, "r");

    if (!bmpFS) {
        Serial.print("File not found");
        goto ERROR;
    }

    uint32_t seekOffset;
    uint16_t w, h, row, col;
    uint8_t r, g, b;

    if (read16(bmpFS) == 0x4D42) {
        read32(bmpFS);
        read32(bmpFS);
        seekOffset = read32(bmpFS);
        read32(bmpFS);
        w = read32(bmpFS);
        h = read32(bmpFS);
        if (center) {
            x = x + (tftWidth - w) / 2;
            y = y + (tftHeight - h) / 2;
        }

        if ((read16(bmpFS) == 1) && (read16(bmpFS) == 24) && (read32(bmpFS) == 0)) {
            y += h - 1;

            bool oldSwapBytes = tft.getSwapBytes();
            tft.setSwapBytes(true);
            bmpFS.seek(seekOffset);

            uint16_t padding = (4 - ((w * 3) & 3)) & 3;
            uint8_t lineBuffer[w * 3 + padding];

            for (row = 0; row < h; row++) {

                bmpFS.read(lineBuffer, sizeof(lineBuffer));
                uint8_t *bptr = lineBuffer;
                uint16_t *tptr = (uint16_t *)lineBuffer;
                // Convert 24 to 16-bit colours
                for (uint16_t col = 0; col < w; col++) {
                    b = *bptr++;
                    g = *bptr++;
                    r = *bptr++;
                    *tptr++ = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
                }

                // Push the pixel row to screen, pushImage will crop the line if needed
                // y is decremented as the BMP image is drawn bottom up
                tft.drawPixel(
                    0, 0, 0
                ); // shared TFT_Spi devices struggle to work, need call a line first sometimes
                tft.pushImage(x, y--, w, 1, (uint16_t *)lineBuffer);
            }
            tft.setSwapBytes(oldSwapBytes);
            Serial.print("BMP Loaded in ");
            Serial.print(millis() - startTime);
            Serial.println(" ms");
        } else {
            goto ERROR;
        }
    } else {
    ERROR:
        Serial.println("BMP format not recognized.");
        bmpFS.close();
        return false;
    }
    bmpFS.close();
    return true;
}

bool drawImg(
    FS &fs, const String &filename, int x, int y, bool center, int playDurationMs, bool resetButtonStatus
) {
    String ext = filename.substring(filename.lastIndexOf('.'));
    ext.toLowerCase();
    uint8_t fls = 2;         // 2 for Little FS
    if (&fs == &SD) fls = 0; // 0 for SD
    tft.imageToBin(fls, filename, x, y, center, playDurationMs);
    if (ext.endsWith("jpg")) return showJpeg(fs, filename, x, y, center);
    else if (ext.endsWith("bmp")) return drawBmp(fs, filename, x, y, center);
    else if (ext.endsWith("png")) return drawPNG(fs, filename, x, y, center);

#if !defined(LITE_VERSION)

    else if (ext.endsWith("gif"))
        return showGif(&fs, filename.c_str(), x, y, center, playDurationMs, resetButtonStatus);
#endif
    else log_e("Image not supported");

    return false;
}

#if !defined(LITE_VERSION)
/// Draw PNG files

#include <PNGdec.h>
#if TFT_WIDTH > TFT_HEIGHT
#define MAX_IMAGE_WIDTH TFT_WIDTH
#else
#define MAX_IMAGE_WIDTH TFT_HEIGHT
#endif
PNG *png = nullptr;
// Optional pointer to write decoded lines into a cached BIN file
static File *pngBinOut = nullptr;
static bool pngCacheOnly = false;
// Optionally use heap capabilities on ESP32 to pick the best memory region for the decoder
#if defined(ESP32)
#include <esp_heap_caps.h>
#endif
// Functions to access a file on the SD card
File myfile;
FS *_fs;

void *myOpen(const char *filename, int32_t *size) {
    // Serial.printf("Attempting to open %s\n", filename);
    myfile = _fs->open(filename);
    *size = myfile.size();
    return &myfile;
}
void myClose(void *handle) {
    if (myfile) myfile.close();
}
int32_t myRead(PNGFILE *handle, uint8_t *buffer, int32_t length) {
    if (!myfile) return 0;
    return myfile.read(buffer, length);
}
int32_t mySeek(PNGFILE *handle, int32_t position) {
    if (!myfile) return 0;
    return myfile.seek(position);
}
// Function to draw pixels to the display
int16_t xpos = 0;
int16_t ypos = 0;
int PNGDraw(PNGDRAW *pDraw) {
    uint16_t usPixels[MAX_IMAGE_WIDTH];
    // static uint16_t dmaBuffer[MAX_IMAGE_WIDTH]; // static so buffer persists after fn exit
    uint8_t r = ((uint16_t)bruceConfig.bgColor & 0xF800) >> 8;
    uint8_t g = ((uint16_t)bruceConfig.bgColor & 0x07E0) >> 3;
    uint8_t b = ((uint16_t)bruceConfig.bgColor & 0x001F) << 3;
    png->getLineAsRGB565(pDraw, usPixels, PNG_RGB565_BIG_ENDIAN, b << 16 | g << 8 | r);
    if (!pngCacheOnly) {
        tft.drawPixel(0, 0, 0);
        tft.drawPixel(0, 0, 0);
        tft.pushImage(xpos, ypos + pDraw->y, pDraw->iWidth, 1, usPixels);
    }
    if (pngBinOut) { pngBinOut->write((uint8_t *)usPixels, pDraw->iWidth * sizeof(uint16_t)); }
    return 1;
}

// Build a cache path alongside the PNG: <dir>/tmp/<basename>.bin
static String buildPngBinPath(const String &pngPath) {
    int slash = pngPath.lastIndexOf('/');
    String dir = (slash >= 0) ? pngPath.substring(0, slash) : "";
    String name = pngPath.substring(slash + 1);
    int dot = name.lastIndexOf('.');
    if (dot > 0) name = name.substring(0, dot);

    String tmpDir = dir.length() ? dir + "/tmp" : "/tmp";
    if (!tmpDir.startsWith("/")) tmpDir = "/" + tmpDir;

    return tmpDir + "/" + name + ".bin";
}

static bool ensureTmpDir(FS &fs, const String &binPath) {
    int slash = binPath.lastIndexOf('/');
    if (slash < 0) return false;
    String dir = binPath.substring(0, slash);
    if (fs.exists(dir)) return true;
    return fs.mkdir(dir);
}

// Render a previously cached BIN (RGB565 LE with 2-byte width/height header)
static bool drawPngBin(FS &fs, const String &binPath, int x, int y, bool center) {
    File f = fs.open(binPath, FILE_READ);
    if (!f) return false;

    uint16_t w = 0, h = 0;
    if (f.read((uint8_t *)&w, sizeof(uint16_t)) != sizeof(uint16_t) ||
        f.read((uint8_t *)&h, sizeof(uint16_t)) != sizeof(uint16_t)) {
        f.close();
        return false;
    }

    if (center) {
        x = x + (tftWidth - w) / 2;
        y = y + (tftHeight - h) / 2;
    }

    if (x >= tft.width() || y >= tft.height()) {
        f.close();
        return false;
    }

    std::unique_ptr<uint16_t[]> line(new (std::nothrow) uint16_t[w]);
    if (!line) {
        f.close();
        return false;
    }

    size_t rowBytes = w * sizeof(uint16_t);
    for (uint16_t row = 0; row < h; ++row) {
        if (f.read((uint8_t *)line.get(), rowBytes) != rowBytes) {
            f.close();
            return false;
        }
        tft.pushImage(x, y + row, w, 1, line.get());
    }

    f.close();
    return true;
}

bool drawPNG(FS &fs, const String &filename, int x, int y, bool center) {
    if ((x >= tft.width()) || (y >= tft.height())) return false;
    _fs = &fs;
    uint32_t dt = millis();

    String binPath = buildPngBinPath(filename);
    if (fs.exists(binPath)) {
        if (pngCacheOnly) return true; // cache already ready
        if (drawPngBin(fs, binPath, x, y, center)) return true;
        fs.remove(binPath); // stale cache, fall back to decode
    }

    // Allocate decoder only while drawing, then release to keep RAM available for Wi-Fi/AP usage
#if defined(ESP32)
    bool usedHeapCaps = true;
    void *mem = psramFound() ? heap_caps_malloc(sizeof(PNG), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
                             : heap_caps_malloc(sizeof(PNG), MALLOC_CAP_8BIT);
    if (!mem) {
        mem = malloc(sizeof(PNG));
        usedHeapCaps = false;
    }
#else
    void *mem = malloc(sizeof(PNG));
#endif
#if !defined(ESP32)
    bool usedHeapCaps = false;
#endif

    if (!mem) {
        Serial.println("Fail alloc PNG!");
        bruceConfig.theme.label = true;
        return false;
    }

    png = new (mem) PNG();
    int16_t rc = png->open(filename.c_str(), myOpen, myClose, myRead, mySeek, PNGDraw);
    if (rc == PNG_SUCCESS) {
        // Serial.printf("image specs: (%d x %d), %d bpp, pixel type: %d\n", png->getWidth(),
        // png->getHeight(), png->getBpp(), png->getPixelType());

        File binFile;
        if (ensureTmpDir(fs, binPath)) {
            binFile = fs.open(binPath, FILE_WRITE);
            if (binFile) {
                uint16_t w = png->getWidth();
                uint16_t h = png->getHeight();
                binFile.write((uint8_t *)&w, sizeof(uint16_t));
                binFile.write((uint8_t *)&h, sizeof(uint16_t));
                pngBinOut = &binFile;
            }
        }

        if (center) {
            xpos = x + (tftWidth - png->getWidth()) / 2;
            ypos = y + (tftHeight - png->getHeight()) / 2;
        }

        if (png->getWidth() > MAX_IMAGE_WIDTH) {
            Serial.println("Image too wide for allocated line buffer size!");
        } else {
            rc = png->decode(NULL, 0);
            png->close();
        }

        if (pngBinOut) {
            pngBinOut->close();
            pngBinOut = nullptr;
        } else {
            if (fs.exists(binPath) && rc != PNG_SUCCESS) fs.remove(binPath);
        }
        if (rc != PNG_SUCCESS && fs.exists(binPath)) { fs.remove(binPath); }

        // How long did rendering take...
        Serial.print("PNG Loaded in ");
        Serial.print(millis() - dt);
        Serial.println("ms");
    } else {
        // Decode/open failed, ensure no stale cache
        if (fs.exists(binPath)) fs.remove(binPath);
    }

    // Destroy placement-new object and free memory so RAM is available after rendering
    png->~PNG();
#if defined(ESP32)
    if (usedHeapCaps) heap_caps_free(mem);
    else free(mem);
#else
    free(mem);
#endif
    png = nullptr;

    return rc == PNG_SUCCESS;
}

// Prepare (or verify) the cached BIN for a PNG without rendering it on screen
bool preparePngBin(FS &fs, const String &filename) {
    bool previous = pngCacheOnly;
    pngCacheOnly = true;
    bool ok = drawPNG(fs, filename, 0, 0, false);
    pngCacheOnly = previous;
    return ok;
}
#else
bool preparePngBin(FS &fs, const String &filename) {
    log_w("PNG: Not supported in this version");
    return true;
}
bool drawPNG(FS &fs, const String &filename, int x, int y, bool center) {
    log_w("PNG: Not supported in this version");
    return false;
}
#endif
