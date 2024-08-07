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
#include "rainbow_fx.h"

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
    int x = 24 + h / 16 % 64;
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
  esp_set_cpu_freq(ESP_CPU_FREQ_160M);

  distance_sensor->Start(100);
  uint32_t frame = 0;
  uint32_t distance_mm = 0;
  uint32_t display_mm = 0;
  uint32_t stable_mm = 0;
  int stable_count = 0;
  int fail_count = 0;
  int awake_count = 0;
  bool sleeping = false;
  constexpr int kSleepThresholdFrames = 60 * 5;
  constexpr int kFadeFrames = 60;
  constexpr int kMaxWakeTimeFrames = 60 * 30;

  while (true) {
    if (distance_sensor->GetDistanceMM(distance_mm)) {
      WDT_FEED();
      fail_count = 0;
    } else {
      fail_count++;
      if (fail_count > 32) {
        printf("Too many failures, rebooting...\n");
        esp_restart();
      }
    }

    int16_t delta = distance_mm - display_mm;
    if (delta) {
      display_mm += delta / 4;
    } else {
      display_mm = distance_mm;
    }
    if (std::abs(static_cast<int32_t>(distance_mm) - static_cast<int32_t>(stable_mm)) < 7) {
      stable_count++;
    } else {
      stable_count = 0;
      stable_mm = distance_mm;
      if (sleeping) {
        esp_set_cpu_freq(ESP_CPU_FREQ_160M);
        sleeping = false;
        display->Enable(true);
        awake_count = 0;
      }
    }
    if (!sleeping)
      awake_count++;
    if (stable_count > kSleepThresholdFrames || awake_count > kMaxWakeTimeFrames) {
      if (!sleeping) {
        esp_set_cpu_freq(ESP_CPU_FREQ_80M);
        sleeping = true;
        display->Enable(false);
      }
    } else if (stable_count > kSleepThresholdFrames - kFadeFrames) {
      if (stable_count % 3 == 0)
        rainbow_fx->Fade();
    } else {
      Render(*rainbow_fx, display_mm);
    }

    if (sleeping) {
      vTaskDelay(250 / portTICK_PERIOD_MS);
    } else {
      rainbow_fx->BeginRender();
      display->Render([&](uint32_t* pixels)
                          IRAM_ATTR { rainbow_fx->Render(pixels); });
    }
    frame++;
  }

  esp_restart();
}
