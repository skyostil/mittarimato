#include <stdio.h>

#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "i2c.h"
#include "distance_sensor.h"

extern "C" void app_main() {
  puts("Hello world!\n");
  SetupI2C();

  auto distance_sensor = DistanceSensor::Create();
  distance_sensor->Start();
  while (true) {
    printf("%d\n", distance_sensor->GetDistanceMM());
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }

  vTaskDelay(1000 / portTICK_PERIOD_MS);
  esp_restart();
}