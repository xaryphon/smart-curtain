#pragma once

#include <array>

#include "I2C.hpp"

class BH1750 {
public:
    enum class I2CDevAddr : uint8_t {
        LOW = 0x23,
        HIGH = 0x5C,
    };

    static constexpr uint BAUDRATE_MAX = 400000;

    struct Parameters {
        const char* name;
        I2C* i2c;
        I2CDevAddr i2c_dev_addr;
    };

    explicit BH1750(const Parameters& parameters);

    bool AdjustMeasurementResolution();
    bool ReadLuxBlocking(float* lux);
    void PowerDown();
    void ResetMeasurement();
    bool Reset();
    bool SetMeasurementTimeReference(uint8_t measurement_time_reference_ms);

    [[nodiscard]] const char* Name() const { return m_name; }
    [[nodiscard]] const char* ModeString() const { return ModeString(m_mode); }

    static constexpr uint8_t MEASUREMENT_TIME_REFERENCE_DEFAULT_MS = 69; // nice

private:
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

    bool SetMode(const Mode& mode);
    bool ReadMeasurementData(uint16_t* data);
    void AdjustMeasurementTime();
    void MediateMeasurementTime();
    void WaitForMeasurement();
    [[nodiscard]] float Uint16ToLux(const uint16_t& u16) const;
    static const char* ModeString(const Mode& mode);
    [[nodiscard]] TickType_t GetMeasurementTimeReferenceMs() const { return pdMS_TO_TICKS(m_measurement_time_reference_ms); };

    // I2C should time out faster than how long a measurement is expected to take,
    // in order to avoid operational delays -- i.e. while motor is expecting updates.
    [[nodiscard]] TickType_t I2CTimeoutTicks() const { return m_measurement_time / 2 + 1; }

    static constexpr uint8_t MEASUREMENT_TIME_REFERENCE_MIN_MS = 31;
    static constexpr uint8_t MEASUREMENT_TIME_REFERENCE_MAX_MS = 254;
    static constexpr uint8_t MEASUREMENT_TIME_TYPICAL_RES_MEDIUM_MS = 120;
    static constexpr uint8_t MEASUREMENT_TIME_TYPICAL_RES_HIGH_MS = 120;
    static constexpr uint8_t MEASUREMENT_TIME_TYPICAL_RES_LOW_MS = 16;

    static constexpr float MODE_FACTOR_MEDIUM = 1;
    static constexpr float MODE_FACTOR_HIGH = 0.5;
    static constexpr float MODE_FACTOR_LOW = 4;
    static constexpr float ACCURACY_FACTOR = 1.2;

    static constexpr uint16_t RESET_VALUE = 0;

    static constexpr size_t I2C_INSTRUCTION_BUF_LEN = sizeof(uint8_t);
    static constexpr size_t I2C_MEASUREMENT_BUF_LEN = sizeof(uint16_t);

    static constexpr TickType_t MEASUREMENT_TIME_TYPICAL_TICKS_RES_MEDIUM = pdMS_TO_TICKS(MEASUREMENT_TIME_TYPICAL_RES_MEDIUM_MS);
    static constexpr TickType_t MEASUREMENT_TIME_TYPICAL_TICKS_RES_HIGH = pdMS_TO_TICKS(MEASUREMENT_TIME_TYPICAL_RES_HIGH_MS);
    static constexpr TickType_t MEASUREMENT_TIME_TYPICAL_TICKS_RES_LOW = pdMS_TO_TICKS(MEASUREMENT_TIME_TYPICAL_RES_LOW_MS);

    static constexpr auto MEASUREMENT_TIME_REFERENCE_DEFAULT_FLOAT = static_cast<float>(BH1750::MEASUREMENT_TIME_REFERENCE_DEFAULT_MS);

    const char* m_name;

    I2C* m_i2c;
    uint m_dev_addr;
    std::array<uint8_t, I2C_INSTRUCTION_BUF_LEN> m_write_buffer = {};
    std::array<uint8_t, I2C_MEASUREMENT_BUF_LEN> m_read_buffer = {};
    Mode m_mode = POWER_DOWN;
    uint8_t m_measurement_time_reference_ms = MEASUREMENT_TIME_REFERENCE_DEFAULT_MS;

    TickType_t m_measurement_started_at_ticks = 0;

    TickType_t m_measurement_time = MEASUREMENT_TIME_TYPICAL_TICKS_RES_MEDIUM;
    TickType_t m_measurement_time_mediation = 0;
    float m_measurement_time_reference_factor = MEASUREMENT_TIME_REFERENCE_DEFAULT_FLOAT / MEASUREMENT_TIME_REFERENCE_DEFAULT_FLOAT;

    float m_lux_latest = RESET_VALUE;
};
