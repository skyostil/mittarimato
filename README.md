# mittarimato

A simple little real-time distance display for ESP8266.

## Hardware

Distance sensor: **STMicroelectronics VL53L1X**. Connected to I2C port 0,
address 0x29.

| ESP8266 pin       | VL53L1X pin|
|-------------------|------------|
| VCC (3v3)         | VCC        |
| GND (GND)         | GND        |
| SDA (GPIO 4)      | SDA        |
| SCL (GPIO 5)      | SCL        |

Display: **Waveshare 0.95inch RGB OLED (SSD1331)**. Connected over HSPI.

| ESP8266 pin       | Display pin |
|-------------------|-------------|
| VCC (3v3)         | VCC         |
| GND (GND)         | GND         |
| SCLK (D5, GPIO 14)| SCL         |
| MOSI (D7, GPIO 13)| SDA/DIN     |
| RST  (D6, GPIO 12)| RES         |
| DC   (D8, GPIO 15)| D/C         |
| CS   (D0, GPIO 16)| CS          |