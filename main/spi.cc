#include "spi.h"

#include <string.h>

void SetupSPI() {
  spi_config_t config;
  memset(&config, 0, sizeof(config));
  config.mode = SPI_MASTER_MODE;
  config.clk_div = SPI_8MHz_DIV;  // SSD1331 min clock cycle: 150ns.
  spi_init(HSPI_HOST, &config);

  spi_interface_t interface;
  memset(&interface, 0, sizeof(interface));
  // Matches Arduino SPI_MODE3.
  interface.cpol = true;
  interface.cpha = true;
  interface.bit_tx_order = 0;  // MSB first.
  interface.bit_rx_order = 0;  // MSB first.
  interface.mosi_en = true;
  interface.cs_en = true;
  spi_set_interface(HSPI_HOST, &interface);
}
