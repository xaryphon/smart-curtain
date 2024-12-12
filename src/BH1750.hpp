#pragma once

#include <array>
#include <memory>

#include "PicoW_I2C.h"
#include "config.h"

class BH1750 {
public:
    enum I2C_dev_addr {
        ADDR_LOW = 0x23,
        ADDR_HIGH = 0x5C
    };

    static constexpr uint BAUDRATE_MAX = 400000;
    static constexpr uint16_t RESET_VALUE = 0;

protected:
    enum operation {
        // clang-format off
        RESET               = 0b00000111,
        SET_MTREG_HIGH_BITS = 0b01000000,
        SET_MTREG_LOW_BITS  = 0b01100000
        // clang-format on
    };

    enum mode {
        // clang-format off
        POWER_DOWN            = 0b00000000,
        POWER_ON              = 0b00000001,
        CONTINUOUS_HIGH_RES   = 0b00010000,
        CONTINUOUS_HIGH_RES_2 = 0b00010001,
        CONTINUOUS_LOW_RES    = 0b00010011,
        ONE_TIME_HIGH_RES     = 0b00100000,
        ONE_TIME_HIGH_RES_2   = 0b00100001,
        ONE_TIME_LOW_RES      = 0b00100011
        // clang-format on
    };

    explicit BH1750(const std::shared_ptr<PicoW_I2C>& picoI2C, BH1750::I2C_dev_addr i2c_dev_addr);
    void SetMode(BH1750::mode mode);
    BH1750::mode GetMode() const;
    void Reset();
    bool SetMeasurementTimeMS(uint8_t measurement_time_ms);
    float ReadLuxData();
    uint8_t GetMeasurementTimeMs() const;

private:
    uint16_t ReadMeasurementData();
    float ConvertUint16ToLux(uint16_t u16) const;

    static constexpr uint8_t I2C_INSTRUCTION_BUF_LEN = 1;
    static constexpr uint8_t I2C_MEASUREMENT_BUF_LEN = 2;
    static constexpr uint8_t MEASUREMENT_TIME_MIN = 31;
    static constexpr uint8_t MEASUREMENT_TIME_DEFAULT = 69; // nice
    static constexpr uint8_t MEASUREMENT_TIME_MAX = 254;
    static constexpr uint8_t MEASUREMENT_TIME_HIGH_BITS = 0b11100000U;
    static constexpr uint8_t MEASUREMENT_TIME_LOW_BITS = 0b00011111U;
    static constexpr uint8_t BYTE = 8;

    static constexpr float ACCURACY_FACTOR = 1.2;
    static constexpr float MODE_FACTOR_HIGH = 1;
    static constexpr float MODE_FACTOR_HIGH_2 = 0.5;
    static constexpr float MODE_FACTOR_LOW = 4;

    std::shared_ptr<PicoW_I2C> m_i2c;
    BH1750::I2C_dev_addr m_dev_addr;
    std::array<uint8_t, I2C_INSTRUCTION_BUF_LEN> m_write_buffer;
    std::array<uint8_t, I2C_MEASUREMENT_BUF_LEN> m_read_buffer;
    BH1750::mode m_mode { POWER_DOWN };
    uint8_t m_measurement_time_ms { MEASUREMENT_TIME_DEFAULT };
};
