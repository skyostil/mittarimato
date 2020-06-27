#pragma once

#include <driver/gpio.h>
#include <driver/i2c.h>

constexpr static auto kI2CPort = I2C_NUM_0;
constexpr static gpio_num_t kI2CPinSDA = GPIO_NUM_4;
constexpr static gpio_num_t kI2CPinSCL = GPIO_NUM_5;

void SetupI2C();