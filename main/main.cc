#include <stdio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>

#include "distance_sensor.h"

extern "C" void app_main() {
  puts("Hello world!\n");
  DistanceSensor distance_sensor;

  vTaskDelay(1000 / portTICK_PERIOD_MS);
  esp_restart();
}