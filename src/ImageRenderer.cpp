#include "ImageRenderer.h"

#include <algorithm>

namespace ImageRenderer {

namespace {

constexpr uint16_t EXPECTED_WIDTH = 800;
constexpr uint16_t EXPECTED_HEIGHT = 480;

static uint16_t readLe16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

static uint32_t readLe32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

static int32_t readLe32s(const uint8_t* p) {
  return static_cast<int32_t>(readLe32(p));
}

static bool isLight(uint8_t r, uint8_t g, uint8_t b) {
  // Simple luma approximation; threshold tuned for black/white palettes.
  const uint16_t y = static_cast<uint16_t>(r) + static_cast<uint16_t>(g) + static_cast<uint16_t>(b);
  return y > (255u * 3u / 2u);
}

}  // namespace

BmpResult renderBmp(const uint8_t* bmpData, const size_t size, EInkDisplay& display) {
  if (bmpData == nullptr || size < 54) {
    return BmpResult::INVALID_SIZE;
  }

  // BITMAPFILEHEADER (14 bytes)
  const uint16_t bfType = readLe16(bmpData + 0);
  if (bfType != 0x4D42) {  // "BM"
    return BmpResult::INVALID_SIGNATURE;
  }
  const uint32_t bfOffBits = readLe32(bmpData + 10);
  if (bfOffBits >= size) {
    return BmpResult::INVALID_SIZE;
  }

  // DIB header (expect BITMAPINFOHEADER = 40)
  const uint32_t dibSize = readLe32(bmpData + 14);
  if (dibSize != 40) {
    return BmpResult::INVALID_FORMAT;
  }

  const int32_t width = readLe32s(bmpData + 18);
  const int32_t height = readLe32s(bmpData + 22);
  const uint16_t planes = readLe16(bmpData + 26);
  const uint16_t bitCount = readLe16(bmpData + 28);
  const uint32_t compression = readLe32(bmpData + 30);

  if (planes != 1) {
    return BmpResult::INVALID_FORMAT;
  }
  if (compression != 0) {
    return BmpResult::INVALID_FORMAT;
  }
  if (bitCount != 1) {
    return BmpResult::INVALID_BIT_DEPTH;
  }
  if (width != static_cast<int32_t>(EXPECTED_WIDTH) || (height != static_cast<int32_t>(EXPECTED_HEIGHT))) {
    // Only bottom-up 800x480 supported for MVP.
    if (width == static_cast<int32_t>(EXPECTED_WIDTH) && height == -static_cast<int32_t>(EXPECTED_HEIGHT)) {
      return BmpResult::UNSUPPORTED_ORIENTATION;
    }
    return BmpResult::INVALID_DIMENSIONS;
  }

  // Palette is 2 entries * 4 bytes, immediately after headers.
  const size_t paletteOffset = 14 + 40;
  const size_t paletteSize = 8;
  if (size < paletteOffset + paletteSize) {
    return BmpResult::INVALID_PALETTE;
  }
  const uint8_t b0 = bmpData[paletteOffset + 0];
  const uint8_t g0 = bmpData[paletteOffset + 1];
  const uint8_t r0 = bmpData[paletteOffset + 2];
  const uint8_t b1 = bmpData[paletteOffset + 4];
  const uint8_t g1 = bmpData[paletteOffset + 5];
  const uint8_t r1 = bmpData[paletteOffset + 6];

  // If palette[0] is lighter than palette[1], BMP's 0 bits represent white; invert so 1=white in framebuffer.
  const bool needsInversion = isLight(r0, g0, b0) && !isLight(r1, g1, b1);

  const uint32_t rowSize = ((EXPECTED_WIDTH + 31u) / 32u) * 4u;
  const uint32_t pixelBytesNeeded = rowSize * EXPECTED_HEIGHT;
  if (bfOffBits + pixelBytesNeeded > size) {
    return BmpResult::INVALID_SIZE;
  }

  uint8_t* framebuffer = display.getFrameBuffer();
  if (!framebuffer) {
    return BmpResult::BUFFER_OVERFLOW;
  }

  const uint8_t* pixelData = bmpData + bfOffBits;

  for (uint32_t srcRow = 0; srcRow < EXPECTED_HEIGHT; ++srcRow) {
    const uint32_t dstRow = (EXPECTED_HEIGHT - 1u) - srcRow;
    const uint8_t* src = pixelData + (srcRow * rowSize);
    uint8_t* dst = framebuffer + (dstRow * EInkDisplay::DISPLAY_WIDTH_BYTES);

    for (uint32_t i = 0; i < EInkDisplay::DISPLAY_WIDTH_BYTES; ++i) {
      uint8_t v = src[i];
      if (needsInversion) {
        v = static_cast<uint8_t>(~v);
      }
      dst[i] = v;
    }
  }

  display.displayBuffer(EInkDisplay::FAST_REFRESH, false);
  return BmpResult::SUCCESS;
}

}  // namespace ImageRenderer
