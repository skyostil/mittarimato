#include "display.h"

#include "spi.h"

#include <FreeRTOS.h>
#include <driver/gpio.h>
#include <freertos/task.h>
#include <string.h>

// Driver for the SSD1331 RGB OLED panel. Based on Adafruit_SSD1331.
class SSD1331 : public Display {
  constexpr static gpio_num_t kPinRES = GPIO_NUM_12;  // D6 <--> RES
  constexpr static gpio_num_t kPinCS = GPIO_NUM_16;   // D0 <--> CS
  constexpr static gpio_num_t kPinDC = GPIO_NUM_15;   // D8 <--> D/C
  constexpr static size_t kWidth = 96;
  constexpr static size_t kHeight = 64;
  constexpr static size_t kBitsPerPixel = 16;

  enum Command {
    CMD_DRAWLINE = 0x21,
    CMD_DRAWRECT = 0x22,
    CMD_CLEAR = 0x25,
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

  enum ColorOrder {
    COLOR_ORDER_RGB,
    COLOR_ORDER_BGR,
  };

  constexpr static ColorOrder kColorOrder = COLOR_ORDER_RGB;

 public:
  SSD1331() {
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
      WriteCommand(0x72);  // RGB Color
    } else {
      WriteCommand(0x76);  // BGR Color
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
    WriteCommand(0x91 | 0xff);
    WriteCommand(CMD_CONTRASTB);  // 0x82
    WriteCommand(0x50 | 0xff);
    WriteCommand(CMD_CONTRASTC);  // 0x83
    WriteCommand(0x7D | 0xff);
    WriteCommand(CMD_DISPLAYON);  // Turn on the panel.

    // Fill(31, 63, 31);
    Clear();
#if 0
    for (size_t y = 0; y < kHeight; y++) {
      for (size_t x = 0; x < kWidth; x++) {
        uint16_t r = ((1 << 5) - 1) * (((x % 16) < 8) ? 1 : 0);
        uint16_t g = ((1 << 6) - 1) * (((x % 32) < 16) ? 1 : 0);
        uint16_t b = ((1 << 5) - 1) * (((y % 16) < 8) ? 1 : 0);
        pixels_[(y * kWidth + x)] = r | (g << 5) | (b << 11);
      }
    }

    auto start = xTaskGetTickCount();
    int frames = 100;
    for (int i = 0; i < frames; i++)
      Present();
    auto diff = static_cast<float>(xTaskGetTickCount() - start) / frames;
    printf("%.2f ticks, %.2f ms\n", diff, (1000 / xPortGetTickRateHz()) * diff);
#endif
  }

  ~SSD1331() = default;

  size_t Width() override { return kWidth; }
  size_t Height() override { return kHeight; }

  void Clear() {
    WriteCommand(CMD_CLEAR);
    WriteCommand(0);
    WriteCommand(0);
    WriteCommand(kWidth - 1);
    WriteCommand(kHeight - 1);
  }

  void Fill(uint8_t r, uint8_t g, uint8_t b) {
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

  constexpr static size_t kChunkSizeBytes = 64;
  size_t RenderBatchSize() override {
    return kChunkSizeBytes / (kBitsPerPixel / 8);
  }

  void Render(const Renderer& renderer) {
    WriteCommand(CMD_SETCOLUMN);
    WriteCommand(0);
    WriteCommand(kWidth - 1);
    WriteCommand(CMD_SETROW);
    WriteCommand(0);
    WriteCommand(kHeight - 1);

    uint32_t* pixels = reinterpret_cast<uint32_t*>(&pixels_[0]);
    uint32_t* pixels_end =
        reinterpret_cast<uint32_t*>(&pixels_[pixels_.size()]);
    static_assert((kWidth * kHeight * kBitsPerPixel / 8) % kChunkSizeBytes == 0,
                  "Partial chunks not supported");

    while (pixels < pixels_end) {
      renderer(pixels);
      WriteData(pixels, kChunkSizeBytes);
      pixels += kChunkSizeBytes / sizeof(uint32_t);
    }
  }

 private:
  void WriteCommand(uint16_t cmd) {
    gpio_set_level(kPinDC, 0);
    gpio_set_level(kPinCS, 0);
    spi_trans_t trans;
    memset(&trans, 0, sizeof(trans));
    trans.cmd = &cmd;
    trans.bits.cmd = 8;
    spi_trans(HSPI_HOST, &trans);
  }

  void WriteData(const uint32_t* data, size_t bytes) {
    gpio_set_level(kPinDC, 1);
    gpio_set_level(kPinCS, 0);
    spi_trans_t trans;
    memset(&trans, 0, sizeof(trans));
    trans.mosi = const_cast<uint32_t*>(data);
    trans.bits.mosi = bytes * 8;
    spi_trans(HSPI_HOST, &trans);
  }

  std::array<uint16_t, kWidth * kHeight * kBitsPerPixel / 16> pixels_;
};

// static
std::unique_ptr<Display> Display::Create() {
  return std::unique_ptr<Display>(new SSD1331());
}