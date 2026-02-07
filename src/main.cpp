#include <Arduino.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <esp_ota_ops.h>

#include "ConfigLoader.h"
#include "ErrorDisplay.h"
#include "ImageRenderer.h"
#include "ApiClient.h"
#include "ButtonHandler.h"

// SDK Libraries
#include "../lib/EInkDisplay/include/EInkDisplay.h"
#include "../lib/SDCardManager/include/SDCardManager.h"
#include "../lib/InputManager/include/InputManager.h"
#include "../lib/BatteryMonitor/include/BatteryMonitor.h"

// Global Objects
// EPD_SCLK=8, EPD_MOSI=10, EPD_CS=21, EPD_DC=4, EPD_RST=5, EPD_BUSY=6
EInkDisplay display(8, 10, 21, 4, 5, 6);
SDCardManager sdManager;
InputManager inputManager;
// GPIO0 for battery voltage ADC
BatteryMonitor batteryMonitor(0);
ButtonHandler buttonHandler(inputManager);

// Deep Sleep Wake Causes
#define WAKE_PIN_POWER 3  // GPIO3 for power button wake

void enterDeepSleep(uint64_t sleepSeconds) {
    // Configure wake sources
    esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000ULL);
    esp_deep_sleep_enable_gpio_wakeup((1ULL << WAKE_PIN_POWER), ESP_GPIO_WAKEUP_GPIO_LOW);
    
    Serial.printf("Entering deep sleep for %llu seconds...\n", sleepSeconds);
    Serial.flush();
    esp_deep_sleep_start();
}

void setup() {
    Serial.begin(115200);
    delay(1000); // Allow serial to stabilize
    Serial.println("\n=== CrossPoint X4 Terminal Starting ===");

    // 1. Initialize SD Card (Must be before Display)
    if (!sdManager.begin()) {
        Serial.println("SD Card initialization failed!");
        // We can't show error yet as display isn't init, but we'll try to init display next
    }

    // 2. Load Configuration
    ConfigResult configResult = ConfigLoader::load();
    const TrmnlConfig& config = ConfigLoader::getConfig();

    // 3. Initialize Display
    display.begin();
    display.clearScreen();

    // 4. Check critical errors
    if (configResult.error != ConfigError::SUCCESS) {
        Serial.printf("Config Error: %s\n", configResult.errorMessage.c_str());
        if (configResult.error == ConfigError::SD_NOT_READY) {
            ErrorDisplay::showNoSdCard(display);
        } else {
            ErrorDisplay::showNoConfig(display);
        }
        // ErrorDisplay functions handle the screen refresh internally
        enterDeepSleep(1800); // Sleep 30 mins on error
        return;
    }

    // 5. Initialize other peripherals
    inputManager.begin();
    // BatteryMonitor does not have begin()
    ApiClient::setBatteryMonitor(&batteryMonitor);

    // 6. Check Buttons (Quick exit check)
    buttonHandler.pollOnce();
    if (buttonHandler.shouldExit()) {
        Serial.println("Back button pressed, returning to CrossPoint...");
        buttonHandler.handleBackButton(config); // This will restart if needed
    }

    // 7. Connect to WiFi
    Serial.printf("Connecting to WiFi: %s\n", config.wifiSsid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false); // IMPORTANT: Don't save to NVS
    WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());

    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi Connection Failed!");
        ErrorDisplay::showWiFiError(display, config.wifiSsid.c_str());
        // ErrorDisplay functions handle the screen refresh internally
        enterDeepSleep(300); // Retry in 5 mins
        return;
    }

    // 8. Fetch Image
    Serial.println("Fetching display data...");
    DisplayFetchResult fetchResult = ApiClient::fetchDisplay(config);

    if (fetchResult.result.error != ApiError::SUCCESS) {
        Serial.printf("API Error: %s\n", fetchResult.result.errorMessage.c_str());
        if (fetchResult.result.error == ApiError::HTTP_REQUEST_FAILED || 
            fetchResult.result.error == ApiError::TIMEOUT) {
             ErrorDisplay::showGenericError(display, "Connection Failed");
        } else {
            ErrorDisplay::showApiError(display, fetchResult.result.httpStatus);
        }
        // ErrorDisplay functions handle the screen refresh internally
        enterDeepSleep(config.refreshInterval);
        return;
    }

    // 9. Handle Response
    if (fetchResult.trmnlStatus == TrmnlStatus::NO_UPDATE) {
        Serial.println("No update needed (Status 202)");
        // Optional: Draw a small indicator or just sleep?
        // For now, just sleep to save power
    } else {
        // 10. Render Image
        Serial.println("Rendering image...");
        ImageRenderer::BmpResult renderResult = ImageRenderer::renderBmp(
            fetchResult.imageData.data(),
            fetchResult.imageData.size(),
            display
        );

        if (renderResult != ImageRenderer::BmpResult::SUCCESS) {
            Serial.printf("Render Error Code: %d\n", (int)renderResult);
            ErrorDisplay::showGenericError(display, "Image Render Failed");
        } else {
             display.displayBuffer();
        }
    }

    // 11. Sleep
    Serial.printf("Update complete. Sleeping for %u seconds.\n", fetchResult.refreshRate);
    enterDeepSleep(fetchResult.refreshRate);
}

void loop() {
    // Should never be reached in normal operation
    // If we're here, something went wrong with sleep
    delay(10000);
    enterDeepSleep(60);
}
