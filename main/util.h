#pragma once

#include <FreeRTOS.h>
#include <freertos/task.h>

template <typename Lambda>
void IRAM_ATTR Benchmark(Lambda&& lambda, int steps = 100) {
  auto start = xTaskGetTickCount();
  for (int i = 0; i < steps; i++)
    lambda();
  auto diff = static_cast<float>(xTaskGetTickCount() - start) / steps;
  printf("%.2f ticks, %.2f ms\n", diff, (1000 / xPortGetTickRateHz()) * diff);
}

constexpr uint16_t PackRGB565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r << 0) & 0b0000000000011111) | ((g << 5) & 0b0000011111100000) |
         ((b << 11) & 0b1111100000000000);
}

constexpr uint16_t PackRGB565(uint32_t rgb) {
  return PackRGB565(rgb & 0xff, (rgb & 0xff00) >> 8, (rgb & 0xff0000) >> 16);
}