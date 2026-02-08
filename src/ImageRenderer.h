#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <EInkDisplay.h>

namespace ImageRenderer {

/**
 * Result codes for BMP rendering operations.
 */
enum class BmpResult {
  SUCCESS,
  INVALID_SIGNATURE,
  INVALID_SIZE,
  INVALID_FORMAT,
  INVALID_DIMENSIONS,
  INVALID_BIT_DEPTH,
  UNSUPPORTED_ORIENTATION,
  INVALID_PALETTE,
  BUFFER_OVERFLOW
};

/**
 * Render a 1-bit monochrome BMP image to the EInk display.
 *
 * Supports:
 * - 800x480 resolution only
 * - 1-bit monochrome (black/white) BMPs
 * - Bottom-up orientation (standard BMP storage)
 * - Proper BMP row padding (4-byte boundary)
 * - Automatic palette inversion detection
 *
 * @param bmpData Pointer to BMP data buffer
 * @param size Size of BMP data in bytes
 * @param display Reference to EInkDisplay instance
 * @return BmpResult Result code indicating success or failure reason
 */
BmpResult renderBmp(const uint8_t* bmpData, size_t size, EInkDisplay& display);

}  // namespace ImageRenderer
