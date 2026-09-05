#include "nrf_jammer.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "nrf_common.h"
#include <globals.h>

static void shuffleChannels(uint8_t *arr, size_t count) {
    for (size_t i = count - 1; i > 0; i--) {
        size_t j = esp_random() % (i + 1);
        uint8_t tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
    }
}

struct JamModeItem {
    const char *displayName;
    const char *uartCommand;
    byte *channels;
    size_t count;
};

static void renderJammerStatusHUD(int nrfOnline, uint8_t hoppingMode) {
    int hudY = 42;
    int hudH = 14;
    tft.fillRect(6, hudY, tftWidth - 12, hudH, bruceConfig.bgColor);

    tft.setTextSize(FP);
    tft.setTextColor(0x07E0, bruceConfig.bgColor);
    tft.drawString("STATUS: " + String(nrfOnline) + " ACTIVE", 10, hudY + 2);

    tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
    tft.drawRightString(hoppingMode == 0 ? "HOP: SEQ" : "HOP: FHSS", tftWidth - 10, hudY + 2, 1);

    tft.drawFastHLine(6, hudY + hudH, tftWidth - 12, 0x31A6);
}

static void renderJammerModeList(int modeIndex, int modeCount, const JamModeItem *modes) {
    int listStartY = 59;
    int listEndY = tftHeight - 18;
    int availableH = listEndY - listStartY;
    int lineH = 14;
    int maxVisible = availableH / lineH;
    if (maxVisible < 1) maxVisible = 1;

    int visibleCount = (modeCount < maxVisible) ? modeCount : maxVisible;

    int topIndex = 0;
    if (modeIndex >= visibleCount) {
        topIndex = modeIndex - visibleCount + 1;
    }
    if (topIndex + visibleCount > modeCount) {
        topIndex = modeCount - visibleCount;
    }
    if (topIndex < 0) topIndex = 0;

    int listW = tftWidth - 16;
    if (modeCount > visibleCount) {
        listW = tftWidth - 24;
    }

    tft.setTextSize(FP);
    for (int i = 0; i < visibleCount; i++) {
        int mIdx = topIndex + i;
        int itemY = listStartY + i * lineH;
        bool isSelected = (mIdx == modeIndex);

        if (isSelected) {
            tft.fillRoundRect(8, itemY, listW, lineH - 1, 3, bruceConfig.priColor);
            tft.setTextColor(bruceConfig.bgColor, bruceConfig.priColor);
            tft.drawString("> " + String(modes[mIdx].displayName), 12, itemY + 2);
            tft.drawRightString(String(modes[mIdx].count) + " CH", 8 + listW - 6, itemY + 2, 1);
        } else {
            tft.fillRect(8, itemY, listW, lineH, bruceConfig.bgColor);
            tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
            tft.drawString("  " + String(modes[mIdx].displayName), 12, itemY + 2);
            tft.setTextColor(bruceConfig.secColor, bruceConfig.bgColor);
            tft.drawRightString(String(modes[mIdx].count) + " CH", 8 + listW - 6, itemY + 2, 1);
        }
    }

    int drawnH = visibleCount * lineH;
    if (drawnH < availableH) {
        tft.fillRect(8, listStartY + drawnH, listW, availableH - drawnH, bruceConfig.bgColor);
    }

    if (modeCount > visibleCount) {
        drawCyberScrollbar(tftWidth - 12, listStartY, visibleCount * lineH, modeIndex, modeCount, visibleCount);
    }
}

void nrf_jammer() {
    int OnX = 0;
    uint8_t hopping_mode = 0; // 0: sequential, 1: random
    NRF24_MODE mode = nrf_setMode();
    int NRFOnline = 1;

    byte Test_channels[] = {50, 52, 54, 56, 58, 60, 62, 64, 66, 68, 70, 72, 74, 76, 78, 80, 2,  4,  6,  8,
                            10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48};

    byte wifi_channels[] = {2, 7, 12, 17, 22, 27, 32, 37, 42, 47, 52, 57, 62, 67, 72, 77};

    byte ble_channels[] = {2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
                           22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41};

    byte ble_adv_priority[] = {37, 38, 39, 1, 2, 3, 25, 26, 27, 79, 80, 81};

    byte bluetooth_channels[] = {2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17,
                                 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33,
                                 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
                                 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65,
                                 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80};
    byte usb_channels[] = {32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64, 66, 68, 70};
    byte video_channels[] = {60, 62, 64, 66,  68,  70,  72,  74,  76,  78,  80,  82,  84,  86,  88,  90, 92,
                             94, 96, 98, 100, 102, 104, 106, 108, 110, 112, 114, 116, 118, 120, 122, 124};
    byte rc_channels[] = {1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31, 33, 35, 37, 39};

    byte full_channels[] = {1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,  16,
                            17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,
                            33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,
                            49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,
                            65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,
                            81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,  96,
                            97,  98,  99,  100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112,
                            113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124};
    byte zigbee_channels[] = {
        4,  5,  6,  // ch11
        9,  10, 11, // ch12
        14, 15, 16, // ch13
        19, 20, 21, // ch14
        24, 25, 26, // ch15
        29, 30, 31, // ch16
        34, 35, 36, // ch17
        39, 40, 41, // ch18
        44, 45, 46, // ch19
        49, 50, 51, // ch20
        54, 55, 56, // ch21
        59, 60, 61, // ch22
        64, 65, 66, // ch23
        69, 70, 71, // ch24
        74, 75, 76, // ch25
        79, 80, 81  // ch26
    };

    JamModeItem modes[] = {
        {"Test",         "Test",        Test_channels,      sizeof(Test_channels) / sizeof(Test_channels[0])          },
        {"WiFi",         "WiFi",        wifi_channels,      sizeof(wifi_channels) / sizeof(wifi_channels[0])          },
        {"BLE",          "BLEch",       ble_channels,       sizeof(ble_channels) / sizeof(ble_channels[0])            },
        {"BLE Adv Pri",  "BLEAdvPri",   ble_adv_priority,   sizeof(ble_adv_priority) / sizeof(ble_adv_priority[0])    },
        {"Bluetooth",    "Bluetooth",   bluetooth_channels, sizeof(bluetooth_channels) / sizeof(bluetooth_channels[0])},
        {"USB",          "USB",         usb_channels,       sizeof(usb_channels) / sizeof(usb_channels[0])            },
        {"Video Stream", "VideoStream", video_channels,     sizeof(video_channels) / sizeof(video_channels[0])        },
        {"RC",           "RC",          rc_channels,        sizeof(rc_channels) / sizeof(rc_channels[0])              },
        {"Zigbee",       "Zigbee",      zigbee_channels,    sizeof(zigbee_channels) / sizeof(zigbee_channels[0])      },
        {"Full",         "Full",        full_channels,      sizeof(full_channels) / sizeof(full_channels[0])          }
    };

    if (nrf_start(mode)) {
        int modeCount = sizeof(modes) / sizeof(modes[0]);
        int modeIndex = 0;
        int hopIndex = 0;
        bool need_reshuffle = true;
        uint8_t shuffled_idx[124];

        if (CHECK_NRF_SPI(mode)) {
            NRFradio.setPALevel(RF24_PA_MAX);
            NRFradio.startConstCarrier(RF24_PA_MAX, 50);
            NRFradio.setAddressWidth(5);
            NRFradio.setPayloadSize(2);
            if (!NRFradio.setDataRate(RF24_2MBPS)) {
                Serial.println("Failed to set data rate to 2Mbps, trying 1Mbps");
                if (!NRFradio.setDataRate(RF24_1MBPS)) {
                    Serial.println("Failed to set data rate to 1Mbps, trying 250kbps");
                    if (!NRFradio.setDataRate(RF24_250KBPS)) {
                        Serial.println("Failed to set data rate to 250kbps, giving up");
                    }
                }
            }
        }

        drawMainBorder(true);
        drawCyberHeader("NRF JAMMER", "2.4GHz");
        drawCyberFooter("[ROT: MODE]", "[SEL: HOP]", "[ESC: STOP]");
        renderJammerStatusHUD(NRFOnline, hopping_mode);
        renderJammerModeList(modeIndex, modeCount, modes);

        NRFSerial.println("RADIOS");
        vTaskDelay(50 / portTICK_PERIOD_MS);

        while (true) {
            if ((CHECK_NRF_UART(mode)) || (CHECK_NRF_BOTH(mode))) {
                if (OnX == 0) {
                    NRFSerial.println("RADIOS");
                    vTaskDelay(50 / portTICK_PERIOD_MS);
                }

                if (NRFSerial.available()) {
                    String incomingNRFs = NRFSerial.readStringUntil('\n');
                    incomingNRFs.trim();
                    if (incomingNRFs.length() == 1 && isDigit(incomingNRFs.charAt(0))) {
                        OnX = 1;
                        int newNRFOnline = incomingNRFs.toInt();
                        if (CHECK_NRF_BOTH(mode)) { newNRFOnline += 1; }
                        if (newNRFOnline != NRFOnline) {
                            NRFOnline = newNRFOnline;
                            renderJammerStatusHUD(NRFOnline, hopping_mode);
                        }
                    }
                }
            }

            hopIndex++;
            if (hopIndex >= (int)modes[modeIndex].count) {
                hopIndex = 0;
                need_reshuffle = true;
            }
            uint8_t ch_idx;
            if (hopping_mode == 1) {
                if (need_reshuffle) {
                    size_t cnt = modes[modeIndex].count;
                    for (size_t i = 0; i < cnt; i++) shuffled_idx[i] = i;
                    shuffleChannels(shuffled_idx, cnt);
                    need_reshuffle = false;
                }
                ch_idx = shuffled_idx[hopIndex];
            } else {
                ch_idx = hopIndex;
            }
            if (CHECK_NRF_SPI(mode)) { NRFradio.setChannel(modes[modeIndex].channels[ch_idx]); }

            bool modeChanged = false;

#ifdef HAS_ENCODER
            int32_t rotarySteps = drainRotarySteps();
            if (rotarySteps != 0) {
                check(PrevPress);
                check(NextPress);
                check(UpPress);
                check(DownPress);
                while (rotarySteps > 0) {
                    modeIndex = (modeIndex - 1 + modeCount) % modeCount;
                    rotarySteps--;
                    modeChanged = true;
                }
                while (rotarySteps < 0) {
                    modeIndex = (modeIndex + 1) % modeCount;
                    rotarySteps++;
                    modeChanged = true;
                }
            }
#endif

            if (!modeChanged) {
                if (check(NextPress) || check(DownPress)) {
                    modeIndex = (modeIndex + 1) % modeCount;
                    modeChanged = true;
                } else if (check(PrevPress) || check(UpPress)) {
                    modeIndex = (modeIndex - 1 + modeCount) % modeCount;
                    modeChanged = true;
                }
            }

            if (modeChanged) {
                hopIndex = 0;
                need_reshuffle = true;
                renderJammerModeList(modeIndex, modeCount, modes);
                if ((CHECK_NRF_UART(mode)) || (CHECK_NRF_BOTH(mode))) {
                    NRFSerial.println(modes[modeIndex].uartCommand);
                }
            }

            if (check(EscPress)) break;

            if (check(SelPress)) {
                hopping_mode = 1 - hopping_mode;
                hopIndex = 0;
                need_reshuffle = true;
                renderJammerStatusHUD(NRFOnline, hopping_mode);
            }
        }

        if (CHECK_NRF_SPI(mode)) NRFradio.stopConstCarrier();
        if ((CHECK_NRF_UART(mode)) || (CHECK_NRF_BOTH(mode))) { NRFSerial.println("OFF"); }

    } else {
        displayError("NRF24 not found", true);
    }
}

