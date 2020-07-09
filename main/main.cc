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

  RainbowFX(size_t width, size_t height);
  ~RainbowFX();

  void BeginRender(size_t render_batch_size);
  void Render(uint32_t* pixels);

 private:
  size_t width_;
  size_t height_;
  uint32_t* ss_pixels_;

  uint32_t* ss_pointer_ = nullptr;
};

RainbowFX::RainbowFX(size_t width, size_t height)
    : width_(width),
      height_(height),
      ss_pixels_(reinterpret_cast<uint32_t*>(
          malloc(width * kSuperSampling * height * kSuperSampling *
                 kSuperSamplingBitsPerPixel / sizeof(uint32_t)))) {}

RainbowFX::~RainbowFX() {
  // TODO: Why doesn't new[] work?
  free(ss_pixels_);
}

void RainbowFX::BeginRender(size_t render_batch_pixels) {
  assert(render_batch_pixels == kRenderBatchPixels);
  ss_pointer_ = ss_pixels_;
}

void RainbowFX::Render(uint32_t* pixels) {
  // TODO: Supersample down to 1x.
  for (size_t i = 0; i < kRenderBatchPixels / 2; i++) {
    *pixels++ = *ss_pointer_++;
  }
}

extern "C" void app_main() {
  SetupI2C();
  SetupSPI();

  auto display = Display::Create();
  auto distance_sensor = DistanceSensor::Create();

  RainbowFX rainbow_fx(display->Width(), display->Height());
  rainbow_fx.BeginRender(display->RenderBatchSize());
  display->Render([&](uint32_t* pixels) { rainbow_fx.Render(pixels); });

  distance_sensor->Start(100);

  while (true) {
    uint32_t distance_mm;
    if (distance_sensor->GetDistanceMM(distance_mm))
      printf("%.2f cm\n", distance_mm / 10.f);

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }

  esp_restart();
}