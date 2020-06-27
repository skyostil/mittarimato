#include "distance_sensor.h"

#include "i2c.h"
#include "third_party/VL53L1_register_map.h"

class VL53L1X : public DistanceSensor {
  constexpr static uint8_t kI2CAddress = 0x29;

 public:
  VL53L1X() = default;
  ~VL53L1X() override = default;

  void Start() override {
    auto model = ReadReg16(VL53L1_IDENTIFICATION__MODEL_ID);
    if (model != 0xeacc) {
      printf("Unexpected VL53L1X model: %x", model);
      abort();
    }

    // Software reset.
    WriteReg8(VL53L1_SOFT_RESET, 0x00);
    os_delay_us(100);
    WriteReg8(VL53L1_SOFT_RESET, 0x01);
  }

  void Stop() override {}

  int GetDistanceMM() override { return 0; }

 private:
  uint8_t ReadReg8(uint16_t reg) {
    auto cmd = CreateCommand(I2C_MASTER_WRITE);
    i2c_master_write_byte(cmd, (reg >> 8) & 0xff, false);
    i2c_master_write_byte(cmd, reg & 0xff, false);
    SendCommand(cmd);

    uint8_t value;
    cmd = CreateCommand(I2C_MASTER_READ);
    i2c_master_read_byte(cmd, &value, I2C_MASTER_LAST_NACK);
    SendCommand(cmd);
    return value;
  }

  uint16_t ReadReg16(uint16_t reg) {
    auto cmd = CreateCommand(I2C_MASTER_WRITE);
    i2c_master_write_byte(cmd, (reg >> 8) & 0xff, false);
    i2c_master_write_byte(cmd, reg & 0xff, false);
    SendCommand(cmd);

    uint8_t value_lo;
    uint8_t value_hi;
    cmd = CreateCommand(I2C_MASTER_READ);
    i2c_master_read_byte(cmd, &value_hi, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &value_lo, I2C_MASTER_LAST_NACK);
    SendCommand(cmd);
    return (value_hi << 8) | value_lo;
  }

  uint32_t ReadReg32(uint16_t reg) {
    auto cmd = CreateCommand(I2C_MASTER_WRITE);
    i2c_master_write_byte(cmd, (reg >> 8) & 0xff, false);
    i2c_master_write_byte(cmd, reg & 0xff, false);
    SendCommand(cmd);

    uint8_t values[4];
    cmd = CreateCommand(I2C_MASTER_READ);
    i2c_master_read_byte(cmd, &values[0], I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &values[1], I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &values[2], I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &values[3], I2C_MASTER_LAST_NACK);
    SendCommand(cmd);
    return (values[0] << 24) | (values[1] << 16) | (values[2] << 8) | values[3];
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