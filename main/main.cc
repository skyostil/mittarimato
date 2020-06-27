#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "distance_sensor.h"
#include "display.h"
#include "i2c.h"
#include "spi.h"

extern "C" void app_main() {
  SetupI2C();
  SetupSPI();

  auto display = Display::Create();
  auto distance_sensor = DistanceSensor::Create();
  distance_sensor->Start(100);

  while (true) {
    uint32_t distance_mm;
    if (distance_sensor->GetDistanceMM(distance_mm))
      printf("%.2f cm\n", distance_mm / 10.f);

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }

  esp_restart();
}