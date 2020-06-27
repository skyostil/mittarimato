#include "spi.h"

#include <string.h>

void SetupSPI() {
  spi_config_t config;
  memset(&config, 0, sizeof(config));
  config.mode = SPI_MASTER_MODE;
  config.clk_div = SPI_80MHz_DIV;
  spi_init(HSPI_HOST, &config);

  spi_interface_t interface;
  memset(&interface, 0, sizeof(interface));
  interface.mosi_en = true;
  interface.cs_en = true;
  spi_set_interface(HSPI_HOST, &interface);
}
