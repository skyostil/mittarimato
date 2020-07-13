#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "display.h"
#include "distance_sensor.h"
#include "font.h"
#include "i2c.h"
#include "spi.h"
#include "util.h"

// clang-format off
// Based on https://lospec.com/palette-list/sweetie-16
static constexpr DRAM_ATTR std::array<uint16_t, 16> palette_ = {
  PackRGB565(0x1a1c2c * 0),
  PackRGB565(0x5d275d),
  PackRGB565(0xb13e53),
  PackRGB565(0xef7d57),
  PackRGB565(0xffcd75),
  PackRGB565(0xa7f070),
  PackRGB565(0x38b764),
  PackRGB565(0x257179),
  PackRGB565(0x29366f),
  PackRGB565(0x3b5dc9),
  PackRGB565(0x41a6f6),
  PackRGB565(0x73eff7),
  PackRGB565(0x333c57),
  PackRGB565(0x566c86),
  PackRGB565(0x94b0c2),
  PackRGB565(0xf4f4f4),
};
// clang-format on

class RainbowFX {
 public:
  static constexpr auto kBackbufferBitsPerPixel = 4;
  static constexpr int kSuperSampling = 2;
  static constexpr int kSuperSamplingBitsPerPixel = 16;
  static constexpr auto kWidth = Display::kWidth * kSuperSampling;
  static constexpr auto kHeight = Display::kHeight * kSuperSampling;

  RainbowFX();
  ~RainbowFX();

  void BeginRender();
  void Render(uint32_t* pixels);

  void Clear();
  const Glyph* DrawGlyph(uint8_t glyph, int x, int y);

 private:
  // The backbuffer is 4 bits per pixel (paletted).
  std::array<uint8_t, kWidth * kHeight * kBackbufferBitsPerPixel / 8>
      backbuffer_pixels_ __attribute__((aligned)) = {};

  const uint8_t* backbuffer_ptr_ = nullptr;
  uint8_t render_column_ = 0;
};

RainbowFX::RainbowFX() {
  for (size_t y = 0; y < kHeight; y++) {
    for (size_t x = 0; x < kWidth; x++) {
      // uint16_t r = ((1 << 5) - 1) * (((x % 16) < 8) ? 1 : 0);
      // uint16_t g = ((1 << 6) - 1) * (((x % 32) < 16) ? 1 : 0);
      // uint16_t b = ((1 << 5) - 1) * (((y % 16) < 8) ? 1 : 0);
      auto index = (y * kWidth + x) * kBackbufferBitsPerPixel / 8;
      // backbuffer_pixels_[index] |= (x ^ y) >> 2;
      if (x == 0)
        backbuffer_pixels_[index] |= 0x0f;
      if (x == kWidth - 1)
        backbuffer_pixels_[index] |= 0xf0;
      if (y == 0 || y == kHeight - 1)
        backbuffer_pixels_[index] |= 0xff;
    }
  }

  Clear();
}
RainbowFX::~RainbowFX() = default;

void RainbowFX::Clear() {
  for (auto& pixel : backbuffer_pixels_)
    pixel = 0;
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

void inline IRAM_ATTR RainbowFX::BeginRender() {
  backbuffer_ptr_ = &backbuffer_pixels_[0];
  render_column_ = 0;
}

void inline IRAM_ATTR RainbowFX::Render(uint32_t* pixels) {
  if (kSuperSampling == 1) {
    for (size_t i = 0; i < Display::kRenderBatchPixels *
                               Display::kBitsPerPixel / (sizeof(uint32_t) * 8);
         i++) {
      // Each backbuffer byte expands into two 16 bit pixels.
      uint8_t pair = *backbuffer_ptr_++;
      uint16_t p0 = palette_[pair & 0b00001111];
      uint16_t p1 = palette_[(pair & 0b11110000) >> 4];
      *pixels++ = p0 | (p1 << 16) | 0xf000f000;
    }
  } else if (kSuperSampling == 2) {
    for (size_t i = 0; i < Display::kRenderBatchPixels *
                               Display::kBitsPerPixel / (sizeof(uint32_t) * 8);
         i++) {
      uint8_t pair;
      // Each backbuffer byte expands into two 16 bit pixels. Combine 4
      // backbuffer pixels into one output pixel.
      pair = *backbuffer_ptr_;
      uint16_t p0 = palette_[pair & 0b00001111];
      uint16_t p1 = palette_[(pair & 0b11110000) >> 4];

      pair = *(backbuffer_ptr_++ + kWidth / 2);
      uint16_t p2 = palette_[pair & 0b00001111];
      uint16_t p3 = palette_[(pair & 0b11110000) >> 4];

      pair = *backbuffer_ptr_;
      uint16_t p4 = palette_[pair & 0b00001111];
      uint16_t p5 = palette_[(pair & 0b11110000) >> 4];

      pair = *(backbuffer_ptr_++ + kWidth / 2);
      uint16_t p6 = palette_[pair & 0b00001111];
      uint16_t p7 = palette_[(pair & 0b11110000) >> 4];
      // TODO: Blend.
      *pixels++ = p0 | (p1 << 16) | p2 | (p3 << 16) | p4 | (p5 << 16) | p6 |
                  (p7 << 16);
    }

    render_column_ += Display::kRenderBatchPixels;
    if (render_column_ >= Display::kWidth) {
      backbuffer_ptr_ += kWidth * kBackbufferBitsPerPixel / 8;
      render_column_ = 0;
    }
  }
}

extern "C" void app_main() {
  SetupI2C();
  SetupSPI();

  auto display = std::unique_ptr<Display>(new Display());
  auto distance_sensor = DistanceSensor::Create();

  auto rainbow_fx = std::unique_ptr<RainbowFX>(new RainbowFX());
  Benchmark([&]() IRAM_ATTR {
    rainbow_fx->BeginRender();
    display->Render([&](uint32_t* pixels)
                        IRAM_ATTR { rainbow_fx->Render(pixels); });
  });
  printf("heap free: %d\n", esp_get_free_heap_size());

  distance_sensor->Start(100);

  while (true) {
    uint32_t distance_mm;
    if (distance_sensor->GetDistanceMM(distance_mm)) {
      // printf("%.2f cm\n", distance_mm / 10.f);
      char buf[16];
      itoa(distance_mm / 10, buf, 10);

      rainbow_fx->Clear();
      int x = 16;
      int y = 16;
      for (auto c : buf) {
        if (!c)
          break;
        auto glyph = rainbow_fx->DrawGlyph(c, x, y);
        if (!glyph)
          break;
        x += glyph->width;
      }
      rainbow_fx->BeginRender();
      display->Render([&](uint32_t* pixels)
                          IRAM_ATTR { rainbow_fx->Render(pixels); });
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }

  esp_restart();
}