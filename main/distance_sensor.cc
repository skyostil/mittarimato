#include "distance_sensor.h"

#include "i2c.h"
#include "third_party/VL53L1_register_map.h"

// Driver for the VL53L1X distance sensor. Based on
// https://github.com/pololu/vl53l1x-arduino.
class VL53L1X : public DistanceSensor {
  constexpr static uint8_t kI2CAddress = 0x29;
  constexpr static uint16_t kTargetRate = 0x0A00;

  struct __attribute__((packed)) RangeResults {
    uint8_t range_status;
    uint8_t report_status;
    uint8_t stream_count;
    uint16_t dss_actual_effective_spads_sd0;
    uint16_t peak_signal_count_rate_mcps_sd0;
    uint16_t ambient_count_rate_mcps_sd0;
    uint16_t sigma_sd0;
    uint16_t phase_sd0;
    uint16_t final_crosstalk_corrected_range_mm_sd0;
    uint16_t peak_signal_count_rate_crosstalk_corrected_mcps_sd0;
  };

 public:
  ~VL53L1X() override = default;

  static std::unique_ptr<VL53L1X> Create() {
    std::unique_ptr<VL53L1X> sensor(new VL53L1X());
    if (!sensor->Initialize())
      return nullptr;
    return sensor;
  }

  void Start(uint32_t period_ms) override {
    WriteReg32(VL53L1_SYSTEM__INTERMEASUREMENT_PERIOD,
               period_ms * osc_calibrate_val_);
    WriteReg8(VL53L1_SYSTEM__INTERRUPT_CLEAR,
              0x01);                             // sys_interrupt_clear_range
    WriteReg8(VL53L1_SYSTEM__MODE_START, 0x40);  // mode_range__timed
  }

  void Stop() override {
    WriteReg8(VL53L1_SYSTEM__MODE_START, 0x80);  // mode_range__abort
    calibrated_ = false;

    // "restore vhv configs"
    if (saved_vhv_init_) {
      WriteReg8(VL53L1_VHV_CONFIG__INIT, saved_vhv_init_);
    }
    if (saved_vhv_timeout_) {
      WriteReg8(VL53L1_VHV_CONFIG__TIMEOUT_MACROP_LOOP_BOUND,
                saved_vhv_timeout_);
    }

    // "remove phasecal override"
    WriteReg8(VL53L1_PHASECAL_CONFIG__OVERRIDE, 0x00);
  }

  void SetRange(Range range) override {
    switch (range) {
      case Range::kShort:
        // Timing config.
        WriteReg8(VL53L1_RANGE_CONFIG__VCSEL_PERIOD_A, 0x07);
        WriteReg8(VL53L1_RANGE_CONFIG__VCSEL_PERIOD_B, 0x05);
        WriteReg8(VL53L1_RANGE_CONFIG__VALID_PHASE_HIGH, 0x38);

        // Dynamic config.
        WriteReg8(VL53L1_SD_CONFIG__WOI_SD0, 0x07);
        WriteReg8(VL53L1_SD_CONFIG__WOI_SD1, 0x05);
        WriteReg8(VL53L1_SD_CONFIG__INITIAL_PHASE_SD0,
                  6);  // Tuning parm default.
        WriteReg8(VL53L1_SD_CONFIG__INITIAL_PHASE_SD1,
                  6);  // Tuning parm default.
        break;

      case Range::kMedium:
        // Timing config.
        WriteReg8(VL53L1_RANGE_CONFIG__VCSEL_PERIOD_A, 0x0B);
        WriteReg8(VL53L1_RANGE_CONFIG__VCSEL_PERIOD_B, 0x09);
        WriteReg8(VL53L1_RANGE_CONFIG__VALID_PHASE_HIGH, 0x78);

        // Dynamic config.
        WriteReg8(VL53L1_SD_CONFIG__WOI_SD0, 0x0B);
        WriteReg8(VL53L1_SD_CONFIG__WOI_SD1, 0x09);
        WriteReg8(VL53L1_SD_CONFIG__INITIAL_PHASE_SD0,
                  10);  // Tuning parm default
        WriteReg8(VL53L1_SD_CONFIG__INITIAL_PHASE_SD1,
                  10);  // Tuning parm default.
        break;

      case Range::kLong:
        // Timing config.
        WriteReg8(VL53L1_RANGE_CONFIG__VCSEL_PERIOD_A, 0x0F);
        WriteReg8(VL53L1_RANGE_CONFIG__VCSEL_PERIOD_B, 0x0D);
        WriteReg8(VL53L1_RANGE_CONFIG__VALID_PHASE_HIGH, 0xB8);

        // Dynamic config.
        WriteReg8(VL53L1_SD_CONFIG__WOI_SD0, 0x0F);
        WriteReg8(VL53L1_SD_CONFIG__WOI_SD1, 0x0D);
        WriteReg8(VL53L1_SD_CONFIG__INITIAL_PHASE_SD0,
                  14);  // Tuning parm default.
        WriteReg8(VL53L1_SD_CONFIG__INITIAL_PHASE_SD1,
                  14);  // Tuning parm default.
        break;
    }
  }

  void SetMeasurementTimingBudget(uint32_t budget_us) {
    // vhv = LOWPOWER_AUTO_VHV_LOOP_DURATION_US + LOWPOWERAUTO_VHV_LOOP_BOUND
    //       (tuning parm default) * LOWPOWER_AUTO_VHV_LOOP_DURATION_US
    //     = 245 + 3 * 245 = 980
    // TimingGuard = LOWPOWER_AUTO_OVERHEAD_BEFORE_A_RANGING +
    //               LOWPOWER_AUTO_OVERHEAD_BETWEEN_A_B_RANGING + vhv
    //             = 1448 + 2100 + 980 = 4528
    constexpr uint32_t kMinMeasurementBudgetUs = 4528;
    constexpr uint32_t kMaxMeasurementBudgetUs = 1100000;

    budget_us = std::max(kMinMeasurementBudgetUs, budget_us);
    budget_us -= kMinMeasurementBudgetUs;
    budget_us = std::min(kMaxMeasurementBudgetUs, budget_us);

    uint32_t range_config_timeout_us = budget_us / 2;

    // "Update Macro Period for Range A VCSEL Period"
    uint32_t macro_period_us =
        CalcMacroPeriod(ReadReg8(VL53L1_RANGE_CONFIG__VCSEL_PERIOD_A));

    // "Update Phase timeout - uses Timing A"
    // Timeout of 1000 is tuning parm default
    // (TIMED_PHASECAL_CONFIG_TIMEOUT_US_DEFAULT) via
    // VL53L1_get_preset_mode_timing_cfg().
    uint32_t phasecal_timeout_mclks =
        TimeoutMicrosecondsToMclks(1000, macro_period_us);
    phasecal_timeout_mclks = std::min(0xffu, phasecal_timeout_mclks);
    WriteReg8(VL53L1_PHASECAL_CONFIG__TIMEOUT_MACROP, phasecal_timeout_mclks);

    // "Update MM Timing A timeout"
    // Timeout of 1 is tuning parm default
    // (LOWPOWERAUTO_MM_CONFIG_TIMEOUT_US_DEFAULT) via
    // VL53L1_get_preset_mode_timing_cfg(). With the API, the register actually
    // ends up with a slightly different value because it gets assigned,
    // retrieved, recalculated with a different macro period, and reassigned,
    // but it probably doesn't matter because it seems like the MM ("mode
    // mitigation"?) sequence steps are disabled in low power auto mode anyway.
    WriteReg16(VL53L1_MM_CONFIG__TIMEOUT_MACROP_A,
               EncodeTimeout(TimeoutMicrosecondsToMclks(1, macro_period_us)));

    // "Update Range Timing A timeout"
    WriteReg16(VL53L1_RANGE_CONFIG__TIMEOUT_MACROP_A,
               EncodeTimeout(TimeoutMicrosecondsToMclks(range_config_timeout_us,
                                                        macro_period_us)));

    // "Update Macro Period for Range B VCSEL Period"
    macro_period_us =
        CalcMacroPeriod(ReadReg8(VL53L1_RANGE_CONFIG__VCSEL_PERIOD_B));

    // "Update MM Timing B timeout"
    // (See earlier comment about MM Timing A timeout.)
    WriteReg16(VL53L1_MM_CONFIG__TIMEOUT_MACROP_B,
               EncodeTimeout(TimeoutMicrosecondsToMclks(1, macro_period_us)));

    // "Update Range Timing B timeout"
    WriteReg16(VL53L1_RANGE_CONFIG__TIMEOUT_MACROP_B,
               EncodeTimeout(TimeoutMicrosecondsToMclks(range_config_timeout_us,
                                                        macro_period_us)));
#if 0
    uint32_t eff_macro_period_us =
        CalcMacroPeriod(ReadReg8(VL53L1_RANGE_CONFIG__VCSEL_PERIOD_A));
    uint32_t eff_range_config_timeout_us = TimeoutMclksToMicroseconds(
        DecodeTimeout(ReadReg16(VL53L1_RANGE_CONFIG__TIMEOUT_MACROP_A)),
        eff_macro_period_us);
    uint32_t eff_timeout = 2 * eff_range_config_timeout_us + kMinMeasurementBudgetUs);
#endif
  }

  static uint32_t DecodeTimeout(uint16_t reg_val) {
    return (static_cast<uint32_t>(reg_val & 0xFF) << (reg_val >> 8)) + 1;
  }

  bool GetDistanceMM(uint32_t& distance_mm) override {
    if (ReadReg8(VL53L1_GPIO__TIO_HV_STATUS) & 0x01)
      return false;

    RangeResults range_results;
    ReadResults(range_results);

#if 0
    printf("VL53L1X: ---- Range results ----\n");
    printf("VL53L1X: range_status: %d\n", range_results.range_status);
    printf("VL53L1X: report_status: %d\n", range_results.report_status);
    printf("VL53L1X: stream_count: %d\n", range_results.stream_count);
    printf("VL53L1X: dss_actual_effective_spads_sd0: %d\n",
           range_results.dss_actual_effective_spads_sd0);
    printf("VL53L1X: peak_signal_count_rate_mcps_sd0: %d\n",
           range_results.peak_signal_count_rate_mcps_sd0);
    printf("VL53L1X: ambient_count_rate_mcps_sd0: %d\n",
           range_results.ambient_count_rate_mcps_sd0);
    printf("VL53L1X: sigma_sd0: %d\n", range_results.sigma_sd0);
    printf("VL53L1X: phase_sd0: %d\n", range_results.phase_sd0);
    printf("VL53L1X: final_crosstalk_corrected_range_mm_sd0: %d\n",
           range_results.final_crosstalk_corrected_range_mm_sd0);
    printf("VL53L1X: peak_signal_count_rate_crosstalk_corrected_mcps_sd0: %d\n",
           range_results.peak_signal_count_rate_crosstalk_corrected_mcps_sd0);
#endif

    if (!calibrated_) {
      SetupManualCalibration();
      calibrated_ = true;
    }
    UpdateDSS(range_results);
    WriteReg8(VL53L1_SYSTEM__INTERRUPT_CLEAR,
              0x01);  // sys_interrupt_clear_range

    constexpr uint8_t kRangeComplete = 9;
    if (range_results.range_status != kRangeComplete ||
        !range_results.stream_count) {
      return false;
    }

    distance_mm = range_results.final_crosstalk_corrected_range_mm_sd0;
    // "apply correction gain"
    // gain factor of 2011 is tuning parm default
    // (VL53L1_TUNINGPARM_LITE_RANGING_GAIN_FACTOR_DEFAULT) Basically, this
    // appears to scale the result by 2011/2048, or about 98% (with the 1024
    // added for proper rounding).
    distance_mm = (distance_mm * 2011 + 0x0400) / 0x0800;
    return true;
  }

 private:
  VL53L1X() = default;

  bool Initialize() {
    // Do a software reset.
    WriteReg8(VL53L1_SOFT_RESET, 0x00);
    os_delay_us(100);
    WriteReg8(VL53L1_SOFT_RESET, 0x01);

    auto model = ReadReg16(VL53L1_IDENTIFICATION__MODEL_ID);
    if (model != 0xeacc) {
      printf("VL53L1X: Unexpected model: %x\n", model);
      return false;
    }

    constexpr int kTimeout = 100;
    for (int i = 0; i < kTimeout; i++) {
      if (ReadReg8(VL53L1_FIRMWARE__SYSTEM_STATUS) & 0x01)
        break;
      if (i == kTimeout) {
        printf("VL53L1X: Boot timeout\n");
        return false;
      }
      os_delay_us(100);
    }

    // Switch to 2V8 mode.
    WriteReg8(VL53L1_PAD_I2C_HV__EXTSUP_CONFIG,
              ReadReg8(VL53L1_PAD_I2C_HV__EXTSUP_CONFIG) | 0x01);

    fast_osc_frequency_ = ReadReg16(VL53L1_OSC_MEASURED__FAST_OSC__FREQUENCY);
    osc_calibrate_val_ = ReadReg16(VL53L1_RESULT__OSC_CALIBRATE_VAL);

    // Static config (applied at the beginning of a measurement).
    WriteReg16(VL53L1_DSS_CONFIG__TARGET_TOTAL_RATE_MCPS, kTargetRate);
    WriteReg8(VL53L1_GPIO__TIO_HV_STATUS, 0x02);
    WriteReg8(VL53L1_SIGMA_ESTIMATOR__EFFECTIVE_PULSE_WIDTH_NS,
              8);  // Tuning parm default.
    WriteReg8(VL53L1_SIGMA_ESTIMATOR__EFFECTIVE_AMBIENT_WIDTH_NS,
              16);  // Tuning parm default.
    WriteReg8(VL53L1_ALGO__CROSSTALK_COMPENSATION_VALID_HEIGHT_MM, 0x01);
    WriteReg8(VL53L1_ALGO__RANGE_IGNORE_VALID_HEIGHT_MM, 0xFF);
    WriteReg8(VL53L1_ALGO__RANGE_MIN_CLIP, 0);  // Tuning parm default.
    WriteReg8(VL53L1_ALGO__CONSISTENCY_CHECK__TOLERANCE,
              2);  // Tuning parm default.

    // General config.
    WriteReg16(VL53L1_SYSTEM__THRESH_RATE_HIGH, 0x0000);
    WriteReg16(VL53L1_SYSTEM__THRESH_RATE_LOW, 0x0000);
    WriteReg8(VL53L1_DSS_CONFIG__APERTURE_ATTENUATION, 0x38);

    // Timing config.
    WriteReg16(VL53L1_RANGE_CONFIG__SIGMA_THRESH, 360);  // tuning parm default
    WriteReg16(VL53L1_RANGE_CONFIG__MIN_COUNT_RATE_RTN_LIMIT_MCPS,
               192);  // tuning parm default

    // Dynamic config.
    WriteReg8(VL53L1_SYSTEM__GROUPED_PARAMETER_HOLD_0, 0x01);
    WriteReg8(VL53L1_SYSTEM__GROUPED_PARAMETER_HOLD_1, 0x01);
    WriteReg8(VL53L1_SD_CONFIG__QUANTIFIER, 2);  // tuning parm default

    WriteReg8(VL53L1_SYSTEM__GROUPED_PARAMETER_HOLD, 0x00);
    WriteReg8(VL53L1_SYSTEM__SEED_CONFIG, 1);  // tuning parm default

    // from VL53L1_config_low_power_auto_mode
    WriteReg8(VL53L1_SYSTEM__SEQUENCE_CONFIG,
              0x8B);  // VHV, PHASECAL, DSS1, RANGE
    WriteReg16(VL53L1_DSS_CONFIG__MANUAL_EFFECTIVE_SPADS_SELECT, 200 << 8);
    WriteReg8(VL53L1_DSS_CONFIG__ROI_MODE_CONTROL,
              2);  // REQUESTED_EFFFECTIVE_SPADS

    SetRange(Range::kMedium);
    SetMeasurementTimingBudget(50000);

    WriteReg16(VL53L1_ALGO__PART_TO_PART_RANGE_OFFSET_MM,
               ReadReg16(VL53L1_MM_CONFIG__OUTER_OFFSET_MM) * 4);
    return true;
  }

  void ReadResults(RangeResults& range_results) {
    static_assert(sizeof(RangeResults) == 17,
                  "Results structure not packed correctly");
    uint16_t reg = VL53L1_RESULT__RANGE_STATUS;
    auto cmd = CreateCommand(I2C_MASTER_WRITE);
    i2c_master_write_byte(cmd, (reg >> 8) & 0xff, true);
    i2c_master_write_byte(cmd, reg & 0xff, true);
    SendCommand(cmd);

    cmd = CreateCommand(I2C_MASTER_READ);
    i2c_master_read(cmd, reinterpret_cast<uint8_t*>(&range_results),
                    sizeof(range_results), I2C_MASTER_LAST_NACK);
    SendCommand(cmd);

    // The data is returned in big endian, so convert to little endian.
    range_results.dss_actual_effective_spads_sd0 =
        __builtin_bswap16(range_results.dss_actual_effective_spads_sd0);
    range_results.peak_signal_count_rate_mcps_sd0 =
        __builtin_bswap16(range_results.peak_signal_count_rate_mcps_sd0);
    range_results.ambient_count_rate_mcps_sd0 =
        __builtin_bswap16(range_results.ambient_count_rate_mcps_sd0);
    range_results.sigma_sd0 = __builtin_bswap16(range_results.sigma_sd0);
    range_results.phase_sd0 = __builtin_bswap16(range_results.phase_sd0);
    range_results.final_crosstalk_corrected_range_mm_sd0 =
        __builtin_bswap16(range_results.final_crosstalk_corrected_range_mm_sd0);
    range_results.peak_signal_count_rate_crosstalk_corrected_mcps_sd0 =
        __builtin_bswap16(
            range_results.peak_signal_count_rate_crosstalk_corrected_mcps_sd0);
  }

  // Perform Dynamic SPAD Selection calculation/update.
  void UpdateDSS(const RangeResults& range_results) {
    uint16_t spad_count = range_results.dss_actual_effective_spads_sd0;

    if (spad_count) {
      // "Calc total rate per spad"
      uint32_t total_rate_per_spad =
          (uint32_t)range_results
              .peak_signal_count_rate_crosstalk_corrected_mcps_sd0 +
          range_results.ambient_count_rate_mcps_sd0;

      // "clip to 16 bits"
      total_rate_per_spad = std::min(0xffffu, total_rate_per_spad);

      // "shift up to take advantage of 32 bits"
      total_rate_per_spad <<= 16;
      total_rate_per_spad /= spad_count;

      if (total_rate_per_spad) {
        // "get the target rate and shift up by 16"
        uint32_t required_spads =
            (static_cast<uint32_t>(kTargetRate) << 16) / total_rate_per_spad;

        // "clip to 16 bit"
        required_spads = std::min(0xffffu, required_spads);

        // "override DSS config"
        WriteReg16(VL53L1_DSS_CONFIG__MANUAL_EFFECTIVE_SPADS_SELECT,
                   required_spads);
        // DSS_CONFIG__ROI_MODE_CONTROL should already be set to
        // REQUESTED_EFFFECTIVE_SPADS.
        return;
      }
    }

    // If we reached this point, it means something above would have resulted in
    // a divide by zero. "We want to gracefully set a spad target, not just exit
    // with an error"

    // "set target to mid point"
    WriteReg16(VL53L1_DSS_CONFIG__MANUAL_EFFECTIVE_SPADS_SELECT, 0x8000);
  }

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

  // Convert sequence step timeout from macro periods to microseconds with given
  // macro period in microseconds (12.12 format)
  static uint32_t TimeoutMclksToMicroseconds(uint32_t timeout_mclks,
                                             uint32_t macro_period_us) {
    return ((uint64_t)timeout_mclks * macro_period_us + 0x800) >> 12;
  }

  // Convert sequence step timeout from microseconds to macro periods with given
  // macro period in microseconds (12.12 format)
  static uint32_t TimeoutMicrosecondsToMclks(uint32_t timeout_us,
                                             uint32_t macro_period_us) {
    return (((uint32_t)timeout_us << 12) + (macro_period_us >> 1)) /
           macro_period_us;
  }

  // Calculate macro period in microseconds (12.12 format) with given VCSEL
  // period.
  uint32_t CalcMacroPeriod(uint8_t vcsel_period) {
    // fast osc frequency in 4.12 format; PLL period in 0.24 format
    uint32_t pll_period_us = ((uint32_t)0x01 << 30) / fast_osc_frequency_;
    uint8_t vcsel_period_pclks = (vcsel_period + 1) << 1;

    // VL53L1_MACRO_PERIOD_VCSEL_PERIODS = 2304
    uint32_t macro_period_us = (uint32_t)2304 * pll_period_us;
    macro_period_us >>= 6;
    macro_period_us *= vcsel_period_pclks;
    macro_period_us >>= 6;

    return macro_period_us;
  }

  static uint16_t EncodeTimeout(uint32_t timeout_mclks) {
    // Encoded format: "(LSByte * 2^MSByte) + 1"
    uint32_t ls_byte = 0;
    uint16_t ms_byte = 0;

    if (timeout_mclks > 0) {
      ls_byte = timeout_mclks - 1;

      while ((ls_byte & 0xFFFFFF00) > 0) {
        ls_byte >>= 1;
        ms_byte++;
      }

      return (ms_byte << 8) | (ls_byte & 0xFF);
    } else {
      return 0;
    }
  }

  void SetupManualCalibration() {
    // "save original vhv configs"
    saved_vhv_init_ = ReadReg8(VL53L1_VHV_CONFIG__INIT);
    saved_vhv_timeout_ = ReadReg8(VL53L1_VHV_CONFIG__TIMEOUT_MACROP_LOOP_BOUND);

    // "disable VHV init"
    WriteReg8(VL53L1_VHV_CONFIG__INIT, saved_vhv_init_ & 0x7F);

    // "set loop bound to tuning param"
    WriteReg8(VL53L1_VHV_CONFIG__TIMEOUT_MACROP_LOOP_BOUND,
              (saved_vhv_timeout_ & 0x03) +
                  (3 << 2));  // tuning parm default
                              // (LOWPOWERAUTO_VHV_LOOP_BOUND_DEFAULT)

    // "override phasecal"
    WriteReg8(VL53L1_PHASECAL_CONFIG__OVERRIDE, 0x01);
    WriteReg8(VL53L1_CAL_CONFIG__VCSEL_START,
              ReadReg8(VL53L1_PHASECAL_RESULT__VCSEL_START));
  }

  uint16_t fast_osc_frequency_ = 0;
  uint16_t osc_calibrate_val_ = 0;

  bool calibrated_ = false;
  uint8_t saved_vhv_init_ = 0;
  uint8_t saved_vhv_timeout_ = 0;
};

// static
std::unique_ptr<DistanceSensor> DistanceSensor::Create() {
  return VL53L1X::Create();
}
