#include <esp_system.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "display.h"
#include "distance_sensor.h"
#include "font.h"
#include "i2c.h"
#include "spi.h"
#include "sprites.h"
#include "util.h"

// clang-format off
// Based on https://lospec.com/palette-list/sweetie-16
static constexpr DRAM_ATTR std::array<uint32_t, 16> palette_ = {
  ExplodeRGB565(PackRGB565(0x1a1c2c * 0)),
  ExplodeRGB565(PackRGB565(0x5d275d)),
  ExplodeRGB565(PackRGB565(0xb13e53)),
  ExplodeRGB565(PackRGB565(0xef7d57)),
  ExplodeRGB565(PackRGB565(0xffcd75)),
  ExplodeRGB565(PackRGB565(0xa7f070)),
  ExplodeRGB565(PackRGB565(0x38b764)),
  ExplodeRGB565(PackRGB565(0x257179)),
  ExplodeRGB565(PackRGB565(0x29366f)),
  ExplodeRGB565(PackRGB565(0x3b5dc9)),
  ExplodeRGB565(PackRGB565(0x41a6f6)),
  ExplodeRGB565(PackRGB565(0x73eff7)),
  ExplodeRGB565(PackRGB565(0x333c57)),
  ExplodeRGB565(PackRGB565(0x566c86)),
  ExplodeRGB565(PackRGB565(0x94b0c2)),
  ExplodeRGB565(PackRGB565(0xf4f4f4)),
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
  void Fade();
  void Move(int16_t delta);
  const Glyph* DrawGlyph(uint8_t glyph, int x, int y);
  void MeasureText(const char*, uint16_t& w, uint16_t& h);

  struct DefaultDrawTraits {
    static constexpr bool kBlend = false;
    static constexpr bool kScale2x = kSuperSampling == 2;
  };
  struct BlendDrawTraits {
    static constexpr bool kBlend = true;
    static constexpr bool kScale2x = kSuperSampling == 2;
  };
  struct BlendDrawTraits1X {
    static constexpr bool kBlend = true;
    static constexpr bool kScale2x = false;
  };
  template <typename DrawTraits = DefaultDrawTraits>
  void DrawSprite(const Sprite& sprite, int x, int y);

 private:
  // The backbuffer is 4 bits per pixel (paletted).
  std::array<uint8_t, kWidth * kHeight * kBackbufferBitsPerPixel / 8>
      backbuffer_pixels_ __attribute__((aligned)) = {};

  const uint8_t* backbuffer_ptr_ = nullptr;
  uint8_t render_column_ = 0;
};

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

template <typename DrawTraits>
void IRAM_ATTR RainbowFX::DrawSprite(const Sprite& sprite,
                                     int pos_x,
                                     int pos_y) {
  const uint8_t* sprite_bits = &kSpriteData[sprite.offset];
  int width = sprite.width;
  int height = sprite.height;
  if (pos_y < 0) {
    sprite_bits += -pos_y * (sprite.width / 2);
    height -= -pos_y;
    pos_y = 0;
  }
  if (DrawTraits::kScale2x) {
    if (pos_y + height > kHeight / 2) {
      height = kHeight / 2 - pos_y;
    }
  } else {
    if (pos_y + height > kHeight) {
      height = kHeight - pos_y;
    }
  }
  if (height < 0)
    return;
  for (size_t y = 0; y < height; y++) {
    uint8_t* dest = &backbuffer_pixels_[((pos_y + y) * kWidth + pos_x) / 2];
    if (DrawTraits::kScale2x) {
      dest = &backbuffer_pixels_[(2 * (pos_y + y) * kWidth + pos_x) / 2];
    }
    for (size_t x = 0; x < width; x += 2) {
      if (DrawTraits::kBlend) {
        uint8_t p0 = *sprite_bits & 0x0f;
        uint8_t p1 = *sprite_bits & 0xf0;
        if (p0) {
          *dest &= 0xf0;
          *dest |= p0;
        }
        if (p1) {
          *dest &= 0x0f;
          *dest |= p1;
        }
      } else {
        *dest = *sprite_bits;
      }
      if (DrawTraits::kScale2x) {
        uint8_t p0 = *dest & 0x0f;
        uint8_t p1 = *dest & 0xf0;
        p0 |= p0 << 4;
        p1 |= p1 >> 4;
        dest[0] = p0;
        dest[(kWidth / 2)] = p0;
        dest[1] = p1;
        dest[(kWidth / 2) + 1] = p1;
        dest += 2;
      } else {
        dest++;
      }
      sprite_bits++;
    }
  }
}

void inline IRAM_ATTR RainbowFX::MeasureText(const char* text,
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
      uint16_t p0 = UnexplodeRGB565(palette_[pair & 0b00001111]);
      uint16_t p1 = UnexplodeRGB565(palette_[(pair & 0b11110000) >> 4]);
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
      uint32_t p0 = palette_[pair & 0b00001111];
      uint32_t p1 = palette_[(pair & 0b11110000) >> 4];

      pair = *(backbuffer_ptr_++ + kWidth / 2);
      uint32_t p2 = palette_[pair & 0b00001111];
      uint32_t p3 = palette_[(pair & 0b11110000) >> 4];

      pair = *backbuffer_ptr_;
      uint32_t p4 = palette_[pair & 0b00001111];
      uint32_t p5 = palette_[(pair & 0b11110000) >> 4];

      pair = *(backbuffer_ptr_++ + kWidth / 2);
      uint32_t p6 = palette_[pair & 0b00001111];
      uint32_t p7 = palette_[(pair & 0b11110000) >> 4];

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

void IRAM_ATTR Render(RainbowFX& rainbow_fx, uint32_t display_mm) {
  rainbow_fx.Clear();
  uint32_t bg_offset = display_mm / 8;
  const auto& bg_sprite = kSprites[4];
  rainbow_fx.DrawSprite(bg_sprite, 0, bg_offset % (RainbowFX::kHeight / 2));
  rainbow_fx.DrawSprite(
      bg_sprite, RainbowFX::kWidth / 2 - bg_sprite.width,
      bg_offset % (RainbowFX::kHeight / 2) - RainbowFX::kHeight / 2);

  const int kMaxHeightMM = 4000;
  int sprite = 0;
  for (int h = 0; h < kMaxHeightMM; h += 150) {
    int y = (static_cast<int>(display_mm) - h) / 2;
    int x = 32 + h / 16 % 64;
    if (y < -RainbowFX::kHeight)
      break;
    if (sprite % 7 == 0) {
      rainbow_fx.DrawSprite<RainbowFX::BlendDrawTraits1X>(kSprites[sprite % 5],
                                                          x * 2, y);
    } else {
      rainbow_fx.DrawSprite<RainbowFX::BlendDrawTraits>(kSprites[sprite % 4], x,
                                                        y);
    }
    sprite++;
  }

  char buf[16];
  itoa(display_mm / 10, buf, 10);

  uint16_t w, h;
  rainbow_fx.MeasureText(buf, w, h);
  // rainbow_fx->Clear();
  int x = RainbowFX::kWidth / 2 - w / 2;
  int y = RainbowFX::kHeight / 2 - h / 2;
  for (auto c : buf) {
    if (!c)
      break;
    auto glyph = rainbow_fx.DrawGlyph(c, x, y);
    if (!glyph)
      break;
    x += glyph->width;
  }
}

extern "C" void IRAM_ATTR app_main() {
  SetupI2C();
  SetupSPI();

  auto display = std::unique_ptr<Display>(new Display());
  auto distance_sensor = DistanceSensor::Create();

  auto rainbow_fx = std::unique_ptr<RainbowFX>(new RainbowFX());
#if 0
  Benchmark([&]() IRAM_ATTR {
    rainbow_fx->BeginRender();
    display->Render([&](uint32_t* pixels)
                        IRAM_ATTR { rainbow_fx->Render(pixels); });
  });
#endif
  printf("heap free: %d\n", esp_get_free_heap_size());

  distance_sensor->Start(100);
  uint32_t frame = 0;
  uint32_t distance_mm = 0;
  uint32_t display_mm = 0;
  int stable_count = 0;
  bool sleeping = false;
  constexpr int kSleepThresholdFrames = 60 * 5;
  constexpr int kFadeFrames = 60;

  while (true) {
    if (distance_sensor->GetDistanceMM(distance_mm)) {
      // printf("%.2f cm\n", distance_mm / 10.f);
      WDT_FEED();
    }

    int16_t delta = distance_mm - display_mm;
    if (delta) {
      display_mm += delta / 4;
      // rainbow_fx->Move(delta / 4);
    } else {
      display_mm = distance_mm;
    }
    if (abs(delta) <= 10) {
      stable_count++;
    } else {
      stable_count = 0;
      sleeping = false;
      display->Enable(true);
    }
    if (stable_count > kSleepThresholdFrames) {
      sleeping = true;
      display->Enable(false);
    } else if (stable_count > kSleepThresholdFrames - kFadeFrames) {
      if (stable_count % 3 == 0)
        rainbow_fx->Fade();
    } else {
      Render(*rainbow_fx, display_mm);
    }

    if (sleeping) {
      vTaskDelay(500 / portTICK_PERIOD_MS);
    } else {
      rainbow_fx->BeginRender();
      display->Render([&](uint32_t* pixels)
                          IRAM_ATTR { rainbow_fx->Render(pixels); });
    }
    frame++;
  }

  esp_restart();
}