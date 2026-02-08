#pragma once

#include <Arduino.h>
#include <EInkDisplay.h>

namespace ErrorDisplay {

/**
 * Display an error screen when no SD card is detected.
 * Shows "Insert SD Card" message centered on the display.
 *
 * @param display Reference to the EInkDisplay instance
 */
void showNoSdCard(EInkDisplay& display);

/**
 * Display an error screen when the config file is missing.
 * Shows "Config file missing" message with expected path.
 *
 * @param display Reference to the EInkDisplay instance
 */
void showNoConfig(EInkDisplay& display);

/**
 * Display an error screen when WiFi connection fails.
 * Shows "WiFi failed" message with the SSID that failed.
 *
 * @param display Reference to the EInkDisplay instance
 * @param ssid The WiFi SSID that failed to connect
 */
void showWiFiError(EInkDisplay& display, const char* ssid);

/**
 * Display an error screen when an API request fails.
 * Shows "API Error" message with the HTTP status code.
 *
 * @param display Reference to the EInkDisplay instance
 * @param httpCode The HTTP status code returned
 */
void showApiError(EInkDisplay& display, int httpCode);

/**
 * Display a generic error screen with a custom message.
 * Centers the provided message on the display.
 *
 * @param display Reference to the EInkDisplay instance
 * @param message Custom error message to display
 */
void showGenericError(EInkDisplay& display, const char* message);

}  // namespace ErrorDisplay
