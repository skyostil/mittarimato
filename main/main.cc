#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "display.h"
#include "distance_sensor.h"
#include "i2c.h"
#include "spi.h"
#include "util.h"

class RainbowFX {
 public:
  static constexpr auto kBitsPerPixel = Display::kBitsPerPixel;
  static constexpr int kSuperSampling = 1;
  static constexpr int kSuperSamplingBitsPerPixel = 16;
  static constexpr auto kWidth = Display::kWidth * kSuperSampling;
  static constexpr auto kHeight = Display::kHeight * kSuperSampling;

  // Must match display.
  static constexpr uint8_t kRenderBatchPixels = 32;

  RainbowFX();
  ~RainbowFX();

  void BeginRender();
  void Render(uint32_t* pixels);

 private:
  std::array<uint16_t, kWidth * kHeight * kBitsPerPixel / 8 / sizeof(uint16_t)>
      backbuffer_pixels_ __attribute__((aligned));

  const uint32_t* backbuffer_ptr_ = nullptr;
};

RainbowFX::RainbowFX() {
  for (size_t y = 0; y < kHeight; y++) {
    for (size_t x = 0; x < kWidth; x++) {
      uint16_t r = ((1 << 5) - 1) * (((x % 16) < 8) ? 1 : 0);
      uint16_t g = ((1 << 6) - 1) * (((x % 32) < 16) ? 1 : 0);
      uint16_t b = ((1 << 5) - 1) * (((y % 16) < 8) ? 1 : 0);
      backbuffer_pixels_[(y * kWidth + x)] = r | (g << 5) | (b << 11);
    }
  }
}
RainbowFX::~RainbowFX() = default;

void RainbowFX::BeginRender() {
  backbuffer_ptr_ = reinterpret_cast<const uint32_t*>(&backbuffer_pixels_[0]);
}

inline void RainbowFX::Render(uint32_t* pixels) {
  // TODO: Supersample down to 1x.
  if (kSuperSampling == 1) {
    for (size_t i = 0; i < Display::kRenderBatchPixels * kBitsPerPixel /
                               (sizeof(uint32_t) * 8);
         i++) {
      *pixels++ = *backbuffer_ptr_++;
    }
  }
}

extern "C" void app_main() {
  SetupI2C();
  SetupSPI();

  auto display = std::unique_ptr<Display>(new Display());
  auto distance_sensor = DistanceSensor::Create();

  auto rainbow_fx = std::unique_ptr<RainbowFX>(new RainbowFX());
  Benchmark([&] {
    rainbow_fx->BeginRender();
    display->Render([&](uint32_t* pixels) { rainbow_fx->Render(pixels); });
  });
  printf("heap free: %d\n", esp_get_free_heap_size());

  distance_sensor->Start(100);

  while (true) {
    uint32_t distance_mm;
    if (distance_sensor->GetDistanceMM(distance_mm))
      printf("%.2f cm\n", distance_mm / 10.f);

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }

  esp_restart();
}