#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "display.h"
#include "distance_sensor.h"
#include "i2c.h"
#include "spi.h"

class RainbowFX {
 public:
  static constexpr int kSuperSampling = 1;
  static constexpr int kSuperSamplingBitsPerPixel = 16;

  // Must match display.
  static constexpr uint8_t kRenderBatchPixels = 32;

  RainbowFX();
  ~RainbowFX();

  void BeginRender();
  void Render(uint32_t* pixels);

 private:
  size_t width_;
  size_t height_;
  std::array<uint32_t,
             Display::kWidth * Display::kHeight * Display::kBitsPerPixel / 16 /
                 sizeof(uint32_t)>
      ss_pixels_;

  uint32_t* ss_pointer_ = nullptr;
};

RainbowFX::RainbowFX() {
  for (size_t y = 0; y < Display::kHeight; y++) {
    for (size_t x = 0; x < Display::kWidth; x++) {
      uint16_t r = ((1 << 5) - 1) * (((x % 16) < 8) ? 1 : 0);
      uint16_t g = ((1 << 6) - 1) * (((x % 32) < 16) ? 1 : 0);
      uint16_t b = ((1 << 5) - 1) * (((y % 16) < 8) ? 1 : 0);
      ss_pixels_[(y * Display::kWidth + x)] = r | (g << 5) | (b << 11);
    }
  }
}
RainbowFX::~RainbowFX() = default;

void RainbowFX::BeginRender() {
  ss_pointer_ = &ss_pixels_[0];
}

void RainbowFX::Render(uint32_t* pixels) {
  // TODO: Supersample down to 1x.
  for (size_t i = 0; i < Display::kRenderBatchPixels / 2; i++) {
    *pixels++ = *ss_pointer_++;
  }
}

extern "C" void app_main() {
  SetupI2C();
  SetupSPI();

  auto display = std::unique_ptr<Display>(new Display());
  auto distance_sensor = DistanceSensor::Create();

  auto rainbow_fx = std::unique_ptr<RainbowFX>(new RainbowFX());
  rainbow_fx->BeginRender();
  display->Render([&](uint32_t* pixels) { rainbow_fx->Render(pixels); });

  distance_sensor->Start(100);

  while (true) {
    uint32_t distance_mm;
    if (distance_sensor->GetDistanceMM(distance_mm))
      printf("%.2f cm\n", distance_mm / 10.f);

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }

  esp_restart();
}