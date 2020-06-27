#include "distance_sensor.h"

#include "i2c.h"
#include "third_party/VL53L1_register_map.h"

class VL53L1X: public DistanceSensor {
  constexpr static uint8_t kI2CAddress = 0x29;
  
 public:
  VL53L1X() = default;
  ~VL53L1X() override = default;

  void Start() override {
  }

  void Stop() override {
  }

  int GetDistanceMM() override {
    return 0;
  }

 private:
  uint8_t ReadReg8(uint16_t reg) {
    return 0;
  }

  uint16_t ReadReg16(uint16_t reg) {
    return 0;
  }

  uint32_t ReadReg32(uint16_t reg) {
    return 0;
  }

  void WriteReg8(uint16_t reg, uint8_t value) {
    auto cmd = CreateCommand(I2C_MASTER_WRITE);
    i2c_master_write_byte(cmd, (reg >> 8) & 0xff, false);
    i2c_master_write_byte(cmd, reg & 0xff, false);
    i2c_master_write_byte(cmd, value, false);
    SendCommand(cmd);
  }

  void WriteReg16(uint16_t reg, uint16_t value) {
    auto cmd = CreateCommand(I2C_MASTER_WRITE);
    i2c_master_write_byte(cmd, (reg >> 8) & 0xff, false);
    i2c_master_write_byte(cmd, reg & 0xff, false);
    i2c_master_write_byte(cmd, (value >> 8) & 0xff, false);
    i2c_master_write_byte(cmd, value & 0xff, false);
    SendCommand(cmd);
  }

  void WriteReg32(uint16_t reg, uint16_t value) {
    auto cmd = CreateCommand(I2C_MASTER_WRITE);
    i2c_master_write_byte(cmd, (reg >> 8) & 0xff, false);
    i2c_master_write_byte(cmd, reg & 0xff, false);
    i2c_master_write_byte(cmd, (value >> 24) & 0xff, false);
    i2c_master_write_byte(cmd, (value >> 16) & 0xff, false);
    i2c_master_write_byte(cmd, (value >> 8) & 0xff, false);
    i2c_master_write_byte(cmd, value & 0xff, false);
    SendCommand(cmd);
  }

  static i2c_cmd_handle_t CreateCommand(int flags) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (kI2CAddress << 1) | flags, false);
    return cmd;
  }

  static void SendCommand(i2c_cmd_handle_t cmd) {
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(kI2CPort, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
  }
};

// static
std::unique_ptr<DistanceSensor> DistanceSensor::Create() {
  return std::unique_ptr<DistanceSensor>(new VL53L1X());
}