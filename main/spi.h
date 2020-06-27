#pragma once

#include <driver/gpio.h>
#include <driver/spi.h>

//constexpr static gpio_num_t kSPIPinSCLK = GPIO_NUM_14;  // D5 <--> SCL
//constexpr static gpio_num_t kSPIPinMOSI = GPIO_NUM_13;  // D7 <--> SDA/DIN
//constexpr static gpio_num_t kSPIPinRST = GPIO_NUM_12;   // D6 <--> RES
//constexpr static gpio_num_t kSPIPinDC = GPIO_NUM_15;    // D8 <--> D/C
//constexpr static gpio_num_t kSPIPinCS = GPIO_NUM_16;    // D0 <--> CS

void SetupSPI();
