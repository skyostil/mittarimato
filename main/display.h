#pragma once

#include <driver/gpio.h>
#include <string.h>
#include <memory>

#include "spi.h"

#ifndef ICACHE_RAM_ATTR
// TODO
#define ICACHE_RAM_ATTR
#endif

// Driver for the SSD1331 RGB OLED panel. Based on Adafruit_SSD1331.
class SSD1331 {
  constexpr static gpio_num_t kPinRES = GPIO_NUM_12;  // D6 <--> RES
  constexpr static gpio_num_t kPinCS = GPIO_NUM_16;   // D0 <--> CS
  constexpr static gpio_num_t kPinDC = GPIO_NUM_15;   // D8 <--> D/C
  constexpr static size_t kChunkSizeBytes = 64;

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
  constexpr static uint8_t kWidth = 96;
  constexpr static uint8_t kHeight = 64;
  constexpr static uint8_t kBitsPerPixel = 16;

  // Whether to render in small batches to parallelize with the DMA update or
  // all at once.
  constexpr static bool kRenderInBatches = true;

  // Number of pixels the renderer should produce per batch.
  constexpr static auto kRenderBatchPixels =
      kRenderInBatches ? (kChunkSizeBytes / (kBitsPerPixel / 8))
                       : (kWidth * kHeight);

  SSD1331();
  ~SSD1331();

  void Clear();
  void Fill(uint8_t r, uint8_t g, uint8_t b);

  template <typename Renderer>
  inline void ICACHE_RAM_ATTR Render(const Renderer& renderer) {
    WriteCommand(CMD_SETCOLUMN);
    WriteCommand(0);
    WriteCommand(kWidth - 1);
    WriteCommand(CMD_SETROW);
    WriteCommand(0);
    WriteCommand(kHeight - 1);

    uint32_t* pixels = reinterpret_cast<uint32_t*>(&pixels_[0]);
    static_assert((kWidth * kHeight * kBitsPerPixel / 8) % kChunkSizeBytes == 0,
                  "Partial chunks not supported");

    if (kRenderInBatches) {
      // Render the screen in small batches to parallelize with the screen
      // update DMA.
      size_t batch_count = kWidth * kHeight / kRenderBatchPixels;
      while (batch_count--) {
        renderer(pixels);
        WriteData(pixels, kChunkSizeBytes);
      }
    } else {
      // Render the entire screen up front and then scan out.
      renderer(pixels);
      const uint32_t* pixels_end =
          reinterpret_cast<const uint32_t*>(&pixels_[pixels_.size()]);
      while (pixels < pixels_end) {
        WriteData(pixels, kChunkSizeBytes);
        pixels += kChunkSizeBytes / sizeof(uint32_t);
      }
    }
  }

 private:
  void ICACHE_RAM_ATTR WriteCommand(uint16_t cmd) {
    gpio_set_level(kPinDC, 0);
    gpio_set_level(kPinCS, 0);
    spi_trans_t trans;
    memset(&trans, 0, sizeof(trans));
    trans.cmd = &cmd;
    trans.bits.cmd = 8;
    spi_trans(HSPI_HOST, &trans);
  }

  void ICACHE_RAM_ATTR WriteData(const uint32_t* data, size_t bytes) {
    gpio_set_level(kPinDC, 1);
    gpio_set_level(kPinCS, 0);
    spi_trans_t trans;
    memset(&trans, 0, sizeof(trans));
    trans.mosi = const_cast<uint32_t*>(data);
    trans.bits.mosi = bytes * 8;
    spi_trans(HSPI_HOST, &trans);
  }

  std::array<uint16_t,
             (kRenderInBatches ? kRenderBatchPixels : (kWidth * kHeight)) *
                 kBitsPerPixel / 16>
      pixels_ __attribute__((aligned));
};

using Display = SSD1331;