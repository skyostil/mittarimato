#include "display.h"

#include <FreeRTOS.h>
#include <freertos/task.h>

#include "util.h"

SSD1331::SSD1331() {
  constexpr gpio_config_t kConfig = {
      .pin_bit_mask = (1 << kPinRES) | (1 << kPinDC) | (1 << kPinCS),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&kConfig);

  // Enable chip select.
  gpio_set_level(kPinCS, 0);

  // Reset.
  gpio_set_level(kPinRES, 1);
  os_delay_us(500);
  gpio_set_level(kPinRES, 0);
  os_delay_us(500);
  gpio_set_level(kPinRES, 1);
  os_delay_us(500);

  WriteCommand(CMD_DISPLAYOFF);  // 0xAE
  WriteCommand(CMD_SETREMAP);    // 0xA0
  if (kColorOrder == COLOR_ORDER_RGB) {
    WriteCommand((0x72 & 0b111111) | 0b01000000);  // RGB Color
  } else {
    WriteCommand((0x76 & 0b111111) | 0b01000000);  // BGR Color
  }
  WriteCommand(CMD_STARTLINE);  // 0xA1
  WriteCommand(0x0);
  WriteCommand(CMD_DISPLAYOFFSET);  // 0xA2
  WriteCommand(0x0);
  WriteCommand(CMD_NORMALDISPLAY);  // 0xA4
  WriteCommand(CMD_SETMULTIPLEX);   // 0xA8
  WriteCommand(0x3F);               // 0x3F 1/64 duty
  WriteCommand(CMD_SETMASTER);      // 0xAD
  WriteCommand(0x8E);
  WriteCommand(CMD_POWERMODE);  // 0xB0
  WriteCommand(0x0B);
  WriteCommand(CMD_PRECHARGE);  // 0xB1
  WriteCommand(0x31);
  WriteCommand(CMD_CLOCKDIV);  // 0xB3
  WriteCommand(0xF0);  // 7:4 = Oscillator Frequency, 3:0 = CLK Div Ratio
                       // (A[3:0]+1 = 1..16)
  WriteCommand(CMD_PRECHARGEA);  // 0x8A
  WriteCommand(0x64);
  WriteCommand(CMD_PRECHARGEB);  // 0x8B
  WriteCommand(0x78);
  WriteCommand(CMD_PRECHARGEA);  // 0x8C
  WriteCommand(0x64);
  WriteCommand(CMD_PRECHARGELEVEL);  // 0xBB
  WriteCommand(0x3A);
  WriteCommand(CMD_VCOMH);  // 0xBE
  WriteCommand(0x3E);
  WriteCommand(CMD_MASTERCURRENT);  // 0x87
  WriteCommand(0x06);
  WriteCommand(CMD_CONTRASTA);  // 0x81
  WriteCommand(0x91);
  WriteCommand(CMD_CONTRASTB);  // 0x82
  WriteCommand(0x50);
  WriteCommand(CMD_CONTRASTC);  // 0x83
  WriteCommand(0x7D);
  WriteCommand(CMD_DISPLAYON);  // Turn on the panel.

  // Fill(31, 63, 31);
  Clear();
#if 0
  if (!kRenderInBatches) {
    for (size_t y = 0; y < kHeight; y++) {
      for (size_t x = 0; x < kWidth; x++) {
        uint16_t r = ((1 << 5) - 1) * (((x % 16) < 8) ? 1 : 0);
        uint16_t g = ((1 << 6) - 1) * (((x % 32) < 16) ? 1 : 0);
        uint16_t b = ((1 << 5) - 1) * (((y % 16) < 8) ? 1 : 0);
        pixels_[(y * kWidth + x)] = r | (g << 5) | (b << 11);
      }
    }
  }

  Benchmark([&] { Render([](uint32_t*) {}); });
#endif
}

SSD1331::~SSD1331() = default;

void SSD1331::Clear() {
  WriteCommand(CMD_CLEAR);
  WriteCommand(0);
  WriteCommand(0);
  WriteCommand(kWidth - 1);
  WriteCommand(kHeight - 1);
}

void SSD1331::Fill(uint8_t r, uint8_t g, uint8_t b) {
  WriteCommand(CMD_FILL);
  WriteCommand(0x01);

  WriteCommand(CMD_DRAWRECT);
  WriteCommand(0);
  WriteCommand(0);
  WriteCommand(kWidth - 1);
  WriteCommand(kHeight - 1);

  // Outline color (6 bit range).
  WriteCommand(r);
  WriteCommand(g);
  WriteCommand(b);

  // Fill color.
  WriteCommand(r);
  WriteCommand(g);
  WriteCommand(b);
}
