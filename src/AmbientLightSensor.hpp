#pragma once

#include <memory>

#include <FreeRTOS.h>
#include <task.h>

#include "BH1750.hpp"
#include "PicoW_I2C.h"

class AmbientLightSensor : private BH1750 {
public:
    explicit AmbientLightSensor(const char * name, uint32_t stack_depth, BaseType_t task_priority, PicoW_I2C* i2c, BH1750::I2CDevAddr i2c_dev_addr);

private:
    enum MeasurementType : uint8_t {
        // clang-format off
        CONTINUOUS = 0b00010000,
        ONE_TIME   = 0b00100000,
        // clang-format on
    };

    // Resolution nomenclature:
    // Datasheet = Translation
    //    HIGH_2 = HIGH
    //      HIGH = MEDIUM
    //       LOW = LOW
    enum MeasurementResolution : uint8_t {
        // clang-format off
        MEDIUM = 0b00000000,
        HIGH   = 0b00000001,
        LOW    = 0b00000011,
        // clang-format on
    };

    void Initialize();
    bool ReadLuxBlocking(float* lux);
    void AdjustMeasurementSettings(AmbientLightSensor::MeasurementType measurement_type);
    void WaitForMeasurement();
    float Uint16ToLux(uint16_t u16) const;
    void MediateMeasurementTime();
    void StartContinuousMeasurement();
    void StopContinuousMeasurement();
    void ResetMeasurement();
    void SetMeasurementTimeFactor(float factor);
    TickType_t GetMeasurementTimeTicks() const;
    void Task();

    static constexpr TickType_t MEASUREMENT_TIME_TYPICAL_TICKS_RES_MEDIUM = pdMS_TO_TICKS(MEASUREMENT_TIME_TYPICAL_RES_MEDIUM_MS);
    static constexpr TickType_t MEASUREMENT_TIME_TYPICAL_TICKS_RES_HIGH = pdMS_TO_TICKS(MEASUREMENT_TIME_TYPICAL_RES_HIGH_MS);
    static constexpr TickType_t MEASUREMENT_TIME_TYPICAL_TICKS_RES_LOW = pdMS_TO_TICKS(MEASUREMENT_TIME_TYPICAL_RES_LOW_MS);

    static constexpr auto MEASUREMENT_TIME_REFERENCE_DEFAULT_FLOAT = static_cast<float>(BH1750::MEASUREMENT_TIME_REFERENCE_DEFAULT_MS);

    TaskHandle_t m_task_handle = nullptr;
    TickType_t m_measurement_started_at_ticks = 0;
    AmbientLightSensor::MeasurementResolution m_resolution = HIGH;
    float m_previous_measurement = static_cast<float>(BH1750::RESET_VALUE);
    float m_measurement_time_reference_factor = MEASUREMENT_TIME_REFERENCE_DEFAULT_FLOAT / MEASUREMENT_TIME_REFERENCE_DEFAULT_FLOAT;
};

void test_ALS();
