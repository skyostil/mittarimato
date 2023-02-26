#include "rainbow_fx.h"

#include <esp_system.h>

#include "font.h"
#include "sprites.h"
#include "util.h"

RainbowFX::RainbowFX() {
  Clear();
}

RainbowFX::~RainbowFX() = default;

void IRAM_ATTR RainbowFX::Clear() {
  for (auto& pixel : backbuffer_pixels_)
    pixel = 0;
  // int index = 0;
  // for (size_t y = 0; y < kHeight; y++) {
  //  for (size_t x = 0; x < kWidth / 2; x++) {
  //    backbuffer_pixels_[index++] = ((y / 8) & 0xf) | ((y / 8) << 4);
  //  }
  //}
}

void IRAM_ATTR RainbowFX::Fade() {
  for (uint8_t& pair : backbuffer_pixels_) {
    uint8_t p0 = pair & 0b00001111;
    uint8_t p1 = pair & 0b11110000;
    p0 = p0 ? p0 - 0b00000001 : p0;
    p1 = p1 ? p1 - 0b00010000 : p1;
    pair = p0 | p1;
  }
}

void IRAM_ATTR RainbowFX::Move(int16_t delta) {
  if (delta > 0) {
    if (delta >= kHeight)
      delta = kHeight - 1;
    const uint8_t* src = &backbuffer_pixels_[delta * kWidth / 2];
    uint8_t* dst = &backbuffer_pixels_[0];
    std::copy(src, src + kWidth * (kHeight - delta) / 2, dst);
  } else {
    delta = -delta;
    if (delta >= kHeight)
      delta = kHeight - 1;
    const uint8_t* src = &backbuffer_pixels_[0];
    uint8_t* dst = &backbuffer_pixels_[delta * kWidth / 2];
    std::copy(src, src + kWidth * (kHeight - delta) / 2, dst);
  }
}

const Glyph* IRAM_ATTR RainbowFX::DrawGlyph(uint8_t glyph,
                                            int pos_x,
                                            int pos_y) {
  if (glyph < kFirstGlyph || glyph > kLastGlyph)
    return nullptr;
  const auto& g = kGlyphs[glyph - kFirstGlyph];
  const uint32_t* glyph_bits = &kGlyphData[g.offset];
  for (size_t y = 0; y < g.height; y++) {
    uint8_t* dest = &backbuffer_pixels_[((pos_y + y) * kWidth + pos_x) / 2];
    for (size_t x = 0; x < g.width; x += 32) {
      for (uint8_t px = 0; px < 32; px += 2) {
        if (*glyph_bits & (1u << (31 - px)))
          *dest |= 0x0f;
        if (*glyph_bits & (1u << (31 - px - 1)))
          *dest |= 0xf0;
        dest++;
      }
      glyph_bits++;
    }
  }
  return &g;
}

void IRAM_ATTR RainbowFX::MeasureText(const char* text,
                                      uint16_t& w,
                                      uint16_t& h) {
  w = 0;
  h = 0;
  while (*text) {
    const auto& g = kGlyphs[*text - kFirstGlyph];
    w += g.width;
    h = std::max(h, static_cast<uint16_t>(g.height));
    text++;
  }
}

void IRAM_ATTR RainbowFX::BeginRender() {
  backbuffer_ptr_ = &backbuffer_pixels_[0];
  render_column_ = 0;
}

/*
void IRAM_ATTR RainbowFX::Render(uint32_t* pixels) {
  if (kSuperSampling == 1) {
    for (size_t i = 0; i < Display::kRenderBatchPixels *
                               Display::kBitsPerPixel / (sizeof(uint32_t) * 8);
         i++) {
      // Each backbuffer byte expands into two 16 bit pixels.
      uint8_t pair = *backbuffer_ptr_++;
      uint16_t p0 = UnexplodeRGB565(kPalette[pair & 0b00001111]);
      uint16_t p1 = UnexplodeRGB565(kPalette[(pair & 0b11110000) >> 4]);
      p0 = __builtin_bswap16(p0);
      p1 = __builtin_bswap16(p1);
      *pixels++ = p0 | (p1 << 16);
    }
  } else if (kSuperSampling == 2) {
    for (size_t i = 0; i < Display::kRenderBatchPixels *
                               Display::kBitsPerPixel / (sizeof(uint32_t) * 8);
         i++) {
      uint8_t pair;
      // Each backbuffer byte expands into two 16 bit pixels. Combine 4
      // backbuffer pixels into one output pixel.
      pair = *backbuffer_ptr_;
      uint32_t p0 = kPalette[pair & 0b00001111];
      uint32_t p1 = kPalette[(pair & 0b11110000) >> 4];

      pair = *(backbuffer_ptr_++ + kWidth / 2);
      uint32_t p2 = kPalette[pair & 0b00001111];
      uint32_t p3 = kPalette[(pair & 0b11110000) >> 4];

      pair = *backbuffer_ptr_;
      uint32_t p4 = kPalette[pair & 0b00001111];
      uint32_t p5 = kPalette[(pair & 0b11110000) >> 4];

      pair = *(backbuffer_ptr_++ + kWidth / 2);
      uint32_t p6 = kPalette[pair & 0b00001111];
      uint32_t p7 = kPalette[(pair & 0b11110000) >> 4];

      uint16_t b0 = UnexplodeRGB565((p0 + p1 + p2 + p3) >> 2);
      uint16_t b1 = UnexplodeRGB565((p4 + p5 + p6 + p7) >> 2);

      b0 = __builtin_bswap16(b0);
      b1 = __builtin_bswap16(b1);
      *pixels++ = b0 | (b1 << 16);
    }

    render_column_ += Display::kRenderBatchPixels;
    if (render_column_ >= Display::kWidth) {
      backbuffer_ptr_ += kWidth * kBackbufferBitsPerPixel / 8;
      render_column_ = 0;
    }
  }
}
*/