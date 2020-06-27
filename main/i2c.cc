#include "i2c.h"

void SetupI2C() {
  constexpr i2c_config_t kConfig = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = kI2CPinSDA,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_io_num = kI2CPinSCL,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .clk_stretch_tick = 300,
  };
  // Note: ESP32 requires the following commands in the opposite order (config
  // before install).
  i2c_driver_install(kI2CPort, I2C_MODE_MASTER);
  i2c_param_config(kI2CPort, &kConfig);
}
