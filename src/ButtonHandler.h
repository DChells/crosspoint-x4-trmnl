#pragma once

#include <InputManager.h>

#include "ConfigLoader.h"

class ButtonHandler {
public:
    ButtonHandler(InputManager& inputManager);
    
    void pollOnce();
    bool shouldExit() const;
    void handleBackButton(const TrmnlConfig& config);

private:
    InputManager& _inputManager;
    bool _exitRequested;
    
    void returnToLauncher();
};
