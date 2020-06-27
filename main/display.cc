#include "display.h"

#include "spi.h"

// Driver for the SSD1331 RGB OLED panel. Based on Adafruit_SSD1331.
class SSD1331 : public Display {
  constexpr static gpio_num_t kPinRES = GPIO_NUM_12;  // D6 <--> RES
  constexpr static gpio_num_t kPinCS = GPIO_NUM_16;   // D0 <--> CS

  enum Command {
    CMD_DRAWLINE = 0x21,
    CMD_DRAWRECT = 0x22,
    CMD_FILL = 0x26,
    CMD_SETCOLUMN = 0x15,
    CMD_SETROW = 0x75,
    CMD_CONTRASTA = 0x81,
    CMD_CONTRASTB = 0x82,
    CMD_CONTRASTC = 0x83,
    CMD_MASTERCURRENT = 0x87,
    CMD_SETREMAP = 0xA0,
    CMD_STARTLINE = 0xA1,
    CMD_DISPLAYOFFSET = 0xA2,
    CMD_NORMALDISPLAY = 0xA4,
    CMD_DISPLAYALLON = 0xA5,
    CMD_DISPLAYALLOFF = 0xA6,
    CMD_INVERTDISPLAY = 0xA7,
    CMD_SETMULTIPLEX = 0xA8,
    CMD_SETMASTER = 0xAD,
    CMD_DISPLAYOFF = 0xAE,
    CMD_DISPLAYON = 0xAF,
    CMD_POWERMODE = 0xB0,
    CMD_PRECHARGE = 0xB1,
    CMD_CLOCKDIV = 0xB3,
    CMD_PRECHARGEA = 0x8A,
    CMD_PRECHARGEB = 0x8B,
    CMD_PRECHARGEC = 0x8C,
    CMD_PRECHARGELEVEL = 0xBB,
    CMD_VCOMH = 0xBE,
  };

 public:
  SSD1331() {
    constexpr gpio_config_t kConfig = {
        .pin_bit_mask = (1 << kPinRES) | (1 << kPinCS),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&kConfig);
  }

  ~SSD1331() = default;

  size_t Width() override { return 96; }
  size_t Height() override { return 64; }

 private:
  void WriteCommand(uint8_t cmd) {}
};

// static
std::unique_ptr<Display> Display::Create() {
  return std::unique_ptr<Display>(new SSD1331());
}