#pragma once

#include <Arduino.h>

#include "../lib/EInkDisplay/include/EInkDisplay.h"

namespace TextDraw {

void drawChar(EInkDisplay& display, char c, int16_t x, int16_t y);
void drawString(EInkDisplay& display, const char* str, int16_t x, int16_t y);
void drawCenteredString(EInkDisplay& display, const char* str, int16_t y);

}  // namespace TextDraw
