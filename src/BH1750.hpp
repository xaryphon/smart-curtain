#pragma once

#include <array>
#include <map>
#include <memory>

#include "PicoW_I2C.h"
#include "config.h"

class BH1750 {
public:
    enum I2CDevAddr : uint8_t {
        ADDR_LOW = 0x23,
        ADDR_HIGH = 0x5C,
    };

    static constexpr uint BAUDRATE_MAX = 400000;
    static constexpr uint16_t RESET_VALUE = 0;

protected:
    enum Operation : uint8_t {
        // clang-format off
        RESET               = 0b00000111,
        SET_MTREG_HIGH_BITS = 0b01000000,
        SET_MTREG_LOW_BITS  = 0b01100000,
        // clang-format on
    };

    // Resolution nomenclature:
    // Datasheet = Translation
    //    HIGH_2 = HIGH
    //      HIGH = MEDIUM
    //       LOW = LOW
    enum Mode : uint8_t {
        // clang-format off
        POWER_DOWN        = 0b00000000,
        POWER_ON          = 0b00000001,
        CONTINUOUS_MEDIUM = 0b00010000,
        CONTINUOUS_HIGH   = 0b00010001,
        CONTINUOUS_LOW    = 0b00010011,
        ONE_TIME_MEDIUM   = 0b00100000,
        ONE_TIME_HIGH     = 0b00100001,
        ONE_TIME_LOW      = 0b00100011,
        // clang-format on
    };

    explicit BH1750(PicoW_I2C* picoI2C, BH1750::I2CDevAddr i2c_dev_addr);
    bool SetMode(BH1750::Mode mode);
    BH1750::Mode GetMode() const;
    bool ReadMeasurementData(uint16_t* data);
    bool Reset();
    bool SetMeasurementTimeReference(uint8_t measurement_time_reference_ms);
    uint8_t GetMeasurementTimeReferenceMs() const;

    static constexpr uint8_t MEASUREMENT_TIME_REFERENCE_MIN_MS = 31;
    static constexpr uint8_t MEASUREMENT_TIME_REFERENCE_DEFAULT_MS = 69; // nice
    static constexpr uint8_t MEASUREMENT_TIME_REFERENCE_MAX_MS = 254;

    static const uint8_t MEASUREMENT_TIME_TYPICAL_RES_MEDIUM_MS = 120;
    static const uint8_t MEASUREMENT_TIME_TYPICAL_RES_HIGH_MS = 120;
    static const uint8_t MEASUREMENT_TIME_TYPICAL_RES_LOW_MS = 16;

    static constexpr float MODE_FACTOR_MEDIUM = 1;
    static constexpr float MODE_FACTOR_HIGH = 0.5;
    static constexpr float MODE_FACTOR_LOW = 4;
    static constexpr float ACCURACY_FACTOR = 1.2;

private:

    std::map<BH1750::Mode, const char *> MODE_STR = {
        {Mode::POWER_DOWN, "POWER_DOWN"},
        {Mode::POWER_ON, "POWER_ON"},
        {Mode::CONTINUOUS_HIGH, "CONTINUOUS_HIGH"},
        {Mode::CONTINUOUS_MEDIUM, "CONTINUOUS_MEDIUM"},
        {Mode::CONTINUOUS_LOW, "CONTINUOUS_LOW"},
        {Mode::ONE_TIME_HIGH, "ONE_TIME_HIGH"},
        {Mode::ONE_TIME_MEDIUM, "ONE_TIME_MEDIUM"},
        {Mode::ONE_TIME_LOW, "ONE_TIME_LOW"},
    };

    static constexpr size_t I2C_INSTRUCTION_BUF_LEN = sizeof(uint8_t);
    static constexpr size_t I2C_MEASUREMENT_BUF_LEN = sizeof(uint16_t);
    static constexpr uint8_t MEASUREMENT_TIME_HIGH_BITS = 0b11100000U;
    static constexpr uint8_t MEASUREMENT_TIME_LOW_BITS = 0b00011111U;

    PicoW_I2C* m_i2c;
    BH1750::I2CDevAddr m_dev_addr;
    std::array<uint8_t, I2C_INSTRUCTION_BUF_LEN> m_write_buffer = {};
    std::array<uint8_t, I2C_MEASUREMENT_BUF_LEN> m_read_buffer = {};
    BH1750::Mode m_mode = POWER_DOWN;
    uint8_t m_measurement_time_ms = MEASUREMENT_TIME_REFERENCE_DEFAULT_MS;
};
