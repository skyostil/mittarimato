#pragma once

#include <FreeRTOS.h>
#include <freertos/task.h>

template <typename Lambda>
void Benchmark(Lambda&& lambda, int steps = 100) {
  auto start = xTaskGetTickCount();
  for (int i = 0; i < steps; i++)
    lambda();
  auto diff = static_cast<float>(xTaskGetTickCount() - start) / steps;
  printf("%.2f ticks, %.2f ms\n", diff, (1000 / xPortGetTickRateHz()) * diff);
}