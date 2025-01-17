#pragma once

#include <memory>

#include "BH1750.hpp"
#include "PicoW_I2C.h"

class AmbientLightSensor : private BH1750 {
public:
    explicit AmbientLightSensor(PicoW_I2C* i2c, BH1750::I2CDevAddr i2c_dev_addr);
    void StartContinuousMeasurement();
    void StopContinuousMeasurement();
    bool ReadLuxBlocking(float* lux);
    void ResetMeasurement();

private:

    enum MeasurementType : uint8_t {
        // clang-format off
        CONTINUOUS = 0b00010000,
        ONE_TIME   = 0b00100000,
        // clang-format on
    };

    enum MeasurementResolution : uint8_t {
        // clang-format off
        HIGH   = 0b00000000,
        HIGH_2 = 0b00000001,
        LOW    = 0b00000011,
        // clang-format on
    };

    void MediateMeasurementTime();
    TickType_t GetMeasurementTimeTicks() const;
    float Uint16ToLux(uint16_t u16) const;
    void AdjustMeasurementSettings(AmbientLightSensor::MeasurementType measurement_type);

    static constexpr uint8_t MEASUREMENT_TIME_TYPICAL_MS_HIGH_RES = 120;
    static constexpr uint8_t MEASUREMENT_TIME_TYPICAL_MS_HIGH_RES_2 = 120;
    static constexpr uint8_t MEASUREMENT_TIME_TYPICAL_MS_LOW_RES = 16;

    static constexpr auto MEASUREMENT_TIME_DEFAULT = static_cast<float>(BH1750::MEASUREMENT_TIME_DEFAULT);

    TickType_t m_measurement_started_at_ticks = 0;
    float m_previous_measurement = static_cast<float>(BH1750::RESET_VALUE);
    MeasurementResolution m_resolution = MeasurementResolution::HIGH;
};

void test_ALS();
