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

inline constexpr uint16_t PackRGB565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

inline constexpr uint16_t PackRGB565(uint32_t rgb) {
  return PackRGB565((rgb & 0xff0000) >> 16, (rgb & 0xff00) >> 8, rgb & 0xff);
}

// from 16 bits:          rrrrrggggggbbbbb
//   to 25 bits: 000rrrrr000gggggg000bbbbb
inline constexpr uint32_t ExplodeRGB565(uint16_t rgb) {
  return (rgb & 0b11111) | ((rgb & 0b11111100000) << 3) |
         ((rgb & 0b1111100000000000) << 6);
}

// from 25 bits: 000rrrrr000gggggg000bbbbb
//   to 16 bits:          rrrrrggggggbbbbb
inline constexpr uint16_t UnexplodeRGB565(uint32_t rgb) {
  return (rgb & 0b11111) | ((rgb & 0b11111100000000) >> 3) |
         ((rgb & 0b1111100000000000000000) >> 6);
}