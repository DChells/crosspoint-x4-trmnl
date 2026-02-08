#include "ErrorDisplay.h"

#include <stdio.h>

#include "TextDraw.h"

namespace ErrorDisplay {

void showNoSdCard(EInkDisplay& display) {
    display.clearScreen(0xFF);

    const char* title = "ERROR";
    const char* message = "Insert SD Card";

    int16_t centerY = static_cast<int16_t>(EInkDisplay::DISPLAY_HEIGHT / 2);
    TextDraw::drawCenteredString(display, title, centerY - 32);
    TextDraw::drawCenteredString(display, message, centerY + 8);

    display.displayBuffer(EInkDisplay::FULL_REFRESH);
}

void showNoConfig(EInkDisplay& display) {
    display.clearScreen(0xFF);

    const char* title = "ERROR";
    const char* message = "Config file missing";
    const char* path = "Expected: /trmnl-config.json";

    int16_t centerY = static_cast<int16_t>(EInkDisplay::DISPLAY_HEIGHT / 2);
    TextDraw::drawCenteredString(display, title, centerY - 48);
    TextDraw::drawCenteredString(display, message, centerY - 8);
    TextDraw::drawCenteredString(display, path, centerY + 24);

    display.displayBuffer(EInkDisplay::FULL_REFRESH);
}

void showWiFiError(EInkDisplay& display, const char* ssid) {
    display.clearScreen(0xFF);

    const char* title = "ERROR";
    char message[64];
    snprintf(message, sizeof(message), "WiFi failed: %s", ssid);

    int16_t centerY = static_cast<int16_t>(EInkDisplay::DISPLAY_HEIGHT / 2);
    TextDraw::drawCenteredString(display, title, centerY - 24);
    TextDraw::drawCenteredString(display, message, centerY + 16);

    display.displayBuffer(EInkDisplay::FULL_REFRESH);
}

void showApiError(EInkDisplay& display, int httpCode) {
    display.clearScreen(0xFF);

    const char* title = "ERROR";
    char message[64];
    snprintf(message, sizeof(message), "API Error: %d", httpCode);

    int16_t centerY = static_cast<int16_t>(EInkDisplay::DISPLAY_HEIGHT / 2);
    TextDraw::drawCenteredString(display, title, centerY - 24);
    TextDraw::drawCenteredString(display, message, centerY + 16);

    display.displayBuffer(EInkDisplay::FULL_REFRESH);
}

void showGenericError(EInkDisplay& display, const char* message) {
    display.clearScreen(0xFF);

    const char* title = "ERROR";

    int16_t centerY = static_cast<int16_t>(EInkDisplay::DISPLAY_HEIGHT / 2);
    TextDraw::drawCenteredString(display, title, centerY - 24);
    TextDraw::drawCenteredString(display, message, centerY + 16);

    display.displayBuffer(EInkDisplay::FULL_REFRESH);
}

}  // namespace ErrorDisplay
