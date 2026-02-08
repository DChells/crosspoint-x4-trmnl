#include <Arduino.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <esp_ota_ops.h>
#include <driver/gpio.h>

#include "ConfigLoader.h"
#include "ErrorDisplay.h"
#include "ImageRenderer.h"
#include "ApiClient.h"
#include "ButtonHandler.h"
#include "TextDraw.h"

// SDK Libraries
#include "../lib/EInkDisplay/include/EInkDisplay.h"
#include "../lib/SDCardManager/include/SDCardManager.h"
#include "../lib/InputManager/include/InputManager.h"
#include "../lib/BatteryMonitor/include/BatteryMonitor.h"

// Global Objects
// EPD_SCLK=8, EPD_MOSI=10, EPD_CS=21, EPD_DC=4, EPD_RST=5, EPD_BUSY=6
EInkDisplay display(8, 10, 21, 4, 5, 6);
InputManager inputManager;
// GPIO0 for battery voltage ADC
BatteryMonitor batteryMonitor(0);
ButtonHandler buttonHandler(inputManager);

// Deep Sleep Wake Causes
#define WAKE_PIN_POWER 3  // GPIO3 for power button wake

#ifndef TRMNL_SAFE_BOOT_MS
#define TRMNL_SAFE_BOOT_MS 8000
#endif

#ifndef TRMNL_MIN_UPTIME_BEFORE_SLEEP_MS
#define TRMNL_MIN_UPTIME_BEFORE_SLEEP_MS 12000
#endif

static void waitForSerialBrief() {
    const uint32_t start = millis();
    while (!Serial && (millis() - start) < 2000) {
        delay(10);
    }
}

static void ensureUptimeBeforeSleep() {
    const uint32_t now = millis();
    if (now < TRMNL_MIN_UPTIME_BEFORE_SLEEP_MS) {
        delay(TRMNL_MIN_UPTIME_BEFORE_SLEEP_MS - now);
    }
}

static void holdUsbWindow(const char* reason) {
    (void)reason;
    // Give a window to attach serial / reflash before sleeping.
    delay(TRMNL_SAFE_BOOT_MS);
}

static void enterDeepSleep(uint64_t sleepSeconds) {
#ifdef TRMNL_NO_SLEEP
    Serial.printf("TRMNL_NO_SLEEP=1: skipping deep sleep (requested %llu seconds)\n", sleepSeconds);
    (void)sleepSeconds;
    return;
#else
    ensureUptimeBeforeSleep();

    // Configure GPIO3 as input with pull-up to avoid floating level.
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << WAKE_PIN_POWER),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Configure wake sources
    esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000ULL);
    esp_deep_sleep_enable_gpio_wakeup((1ULL << WAKE_PIN_POWER), ESP_GPIO_WAKEUP_GPIO_LOW);
    
    Serial.printf("Entering deep sleep for %llu seconds...\n", sleepSeconds);
    Serial.flush();
    esp_deep_sleep_start();
#endif
}

static void returnToCrossPoint() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* next = esp_ota_get_next_update_partition(running);
    if (running == nullptr || next == nullptr || next == running || next->address == running->address) {
        esp_restart();
        return;
    }
    if (esp_ota_set_boot_partition(next) != ESP_OK) {
        esp_restart();
        return;
    }
    esp_restart();
}

enum class MenuAction { START, EXIT, RETRY };

static MenuAction showBootMenu(const TrmnlConfig& config, const ConfigResult& configResult, const bool allowAutoStart) {
    (void)config;

    display.clearScreen(0xFF);
    TextDraw::drawCenteredString(display, "TRMNL DASHBOARD", 80);

    const bool cfgOk = (configResult.error == ConfigError::SUCCESS);
    if (cfgOk) {
        TextDraw::drawCenteredString(display, "CONFIRM: START", 200);
        TextDraw::drawCenteredString(display, "BACK: EXIT", 220);
        if (allowAutoStart) {
            TextDraw::drawCenteredString(display, "AUTO-START IN 8s", 260);
        } else {
            TextDraw::drawCenteredString(display, "AUTO-START DISABLED", 260);
        }
    } else {
        TextDraw::drawCenteredString(display, "CONFIG ERROR", 190);
        TextDraw::drawCenteredString(display, "CONFIRM: RETRY", 230);
        TextDraw::drawCenteredString(display, "BACK: EXIT", 250);
    }

    display.displayBuffer(EInkDisplay::FAST_REFRESH, false);

    const uint32_t start = millis();
    while (true) {
        inputManager.update();

        if (inputManager.wasPressed(InputManager::BTN_BACK)) {
            return MenuAction::EXIT;
        }
        if (inputManager.wasPressed(InputManager::BTN_CONFIRM)) {
            return cfgOk ? MenuAction::START : MenuAction::RETRY;
        }

        if (cfgOk && allowAutoStart && (millis() - start) >= 8000) {
            return MenuAction::START;
        }

        delay(20);
    }
}

static bool connectWifiOrShowError(const TrmnlConfig& config) {
    Serial.printf("Connecting to WiFi: %s\n", config.wifiSsid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());

    const uint32_t startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt) < 20000) {
        delay(250);
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi Connection Failed!");
        ErrorDisplay::showWiFiError(display, config.wifiSsid.c_str());
        holdUsbWindow("wifi_error");
        return false;
    }

    return true;
}

static void runOnce(const TrmnlConfig& config) {
    if (!connectWifiOrShowError(config)) {
        return;
    }

    Serial.println("Fetching display data...");
    DisplayFetchResult fetchResult = ApiClient::fetchDisplay(config);

    if (fetchResult.result.error != ApiError::SUCCESS) {
        Serial.printf("API Error: %s\n", fetchResult.result.errorMessage.c_str());
        ErrorDisplay::showApiError(display, fetchResult.result.httpStatus);
        holdUsbWindow("api_error");
        return;
    }

    if (fetchResult.trmnlStatus == TrmnlStatus::NO_UPDATE) {
        Serial.println("No update needed (Status 202)");
        // In release mode, sleep until next refresh; in dev, return to menu.
        holdUsbWindow("no_update");
        enterDeepSleep(fetchResult.refreshRate);
        return;
    }

    Serial.println("Rendering image...");
    ImageRenderer::BmpResult renderResult = ImageRenderer::renderBmp(fetchResult.imageData.data(), fetchResult.imageData.size(), display);
    if (renderResult != ImageRenderer::BmpResult::SUCCESS) {
        Serial.printf("Render Error Code: %d\n", static_cast<int>(renderResult));
        ErrorDisplay::showGenericError(display, "Image Render Failed");
        holdUsbWindow("render_error");
        return;
    }

    Serial.printf("Update complete. Sleeping for %u seconds.\n", fetchResult.refreshRate);
    holdUsbWindow("before_sleep");
    enterDeepSleep(fetchResult.refreshRate);
}

void setup() {
    Serial.begin(115200);
    waitForSerialBrief();
    delay(250);
    Serial.println("\n=== CrossPoint X4 Terminal Starting ===");

    // SD/config MUST be read before display.begin() because display.begin() sets SPI MISO=-1.
    (void)SdMan.begin();
    const ConfigResult configResult = ConfigLoader::load();
    const TrmnlConfig& config = ConfigLoader::getConfig();

    display.begin();
    inputManager.begin();
    ApiClient::setBatteryMonitor(&batteryMonitor);

    bool allowAutoStart = true;

    for (;;) {
        const MenuAction action = showBootMenu(config, configResult, allowAutoStart);
        if (action == MenuAction::EXIT) {
            if (!config.standaloneMode) {
                returnToCrossPoint();
            }
            // If standalone, just reboot into the same firmware.
            esp_restart();
        }
        if (action == MenuAction::RETRY) {
            esp_restart();
        }

        // START
        if (configResult.error != ConfigError::SUCCESS) {
            // Can't proceed without a valid config.
            ErrorDisplay::showNoConfig(display);
            holdUsbWindow("config_error");
            continue;
        }

        runOnce(config);
        // If we reached here, we didn't deep sleep (dev mode or error). Return to menu.
        holdUsbWindow("back_to_menu");
        allowAutoStart = false;
    }
}

void loop() {
    // setup() contains the main loop.
    delay(1000);
}
