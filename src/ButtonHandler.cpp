#include "ButtonHandler.h"
#include <Arduino.h>
#include <esp_ota_ops.h>
#include <esp_system.h>

ButtonHandler::ButtonHandler(InputManager& inputManager) 
    : _inputManager(inputManager), _exitRequested(false) {
}

void ButtonHandler::pollOnce() {
    _inputManager.update();
    if (_inputManager.wasPressed(InputManager::BTN_BACK)) {
        _exitRequested = true;
    }
}

bool ButtonHandler::shouldExit() const {
    return _exitRequested;
}

void ButtonHandler::handleBackButton(const TrmnlConfig& config) {
    if (config.standaloneMode) {
        return;
    }
    
    if (_exitRequested) {
        returnToLauncher();
    }
}

void ButtonHandler::returnToLauncher() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* next = esp_ota_get_next_update_partition(running);

    // If we can't find a valid alternate slot, just restart in place.
    if (running == nullptr || next == nullptr || next == running || next->address == running->address) {
        esp_restart();
        return;
    }

    const esp_err_t err = esp_ota_set_boot_partition(next);
    if (err != ESP_OK) {
        esp_restart();
        return;
    }

    esp_restart();
}
