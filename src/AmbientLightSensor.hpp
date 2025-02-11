#pragma once

#include <memory>

#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>

#include "BH1750.hpp"
#include "PicoW_I2C.h"
#include "Queue.hpp"
#include "Semaphore.hpp"

class AmbientLightSensor : private BH1750 {
public:
    struct Infrastructure {
        RTOS::Semaphore* measure_now;
        RTOS::Semaphore* measure_continuously;
        RTOS::Variable<float>* latest_lux;
    };

    explicit AmbientLightSensor(
        const char* name,
        uint32_t stack_depth,
        BaseType_t task_priority,
        PicoW_I2C* i2c,
        BH1750::I2CDevAddr i2c_dev_addr,
        const AmbientLightSensor::Infrastructure& infra);

private:
    void Initialize();
    bool Measure();
    void MeasureOnce();
    void MeasureContinuously();
    bool ReadLuxBlocking();
    bool AdjustMeasurementResolution();
    void WaitForMeasurement();
    [[nodiscard]] float Uint16ToLux(uint16_t u16) const;
    void MediateMeasurementTime();
    void StopContinuousMeasurement();
    void ResetMeasurement();
    void SetMeasurementTimeFactor(float factor);
    [[nodiscard]] TickType_t GetMeasurementTimeTicks() const;
    void Task();
    static void PassiveMeasurementEvent(TimerHandle_t timer_handle) { static_cast<RTOS::Semaphore*>(pvTimerGetTimerID(timer_handle))->Give(); };

    static constexpr TickType_t MEASUREMENT_TIME_TYPICAL_TICKS_RES_MEDIUM = pdMS_TO_TICKS(MEASUREMENT_TIME_TYPICAL_RES_MEDIUM_MS);
    static constexpr TickType_t MEASUREMENT_TIME_TYPICAL_TICKS_RES_HIGH = pdMS_TO_TICKS(MEASUREMENT_TIME_TYPICAL_RES_HIGH_MS);
    static constexpr TickType_t MEASUREMENT_TIME_TYPICAL_TICKS_RES_LOW = pdMS_TO_TICKS(MEASUREMENT_TIME_TYPICAL_RES_LOW_MS);

    static constexpr auto MEASUREMENT_TIME_REFERENCE_DEFAULT_FLOAT = static_cast<float>(BH1750::MEASUREMENT_TIME_REFERENCE_DEFAULT_MS);

    TaskHandle_t m_task_handle = nullptr;
    RTOS::Semaphore* m_measure_now;
    RTOS::Semaphore* m_measure_continuously;
    RTOS::Variable<float>* m_latest_lux;
    TimerHandle_t m_timer_passive_measurement;
    TickType_t m_passive_measurement_period_ticks = pdMS_TO_TICKS(10000);
    TickType_t m_measurement_started_at_ticks = 0;
    TickType_t m_measurement_time = MEASUREMENT_TIME_TYPICAL_TICKS_RES_MEDIUM;
    TickType_t m_measurement_time_mediation = 0;
    BH1750::Mode m_measurement_mode = BH1750::GetMode();
    float m_previous_measurement = static_cast<float>(BH1750::RESET_VALUE);
    float m_measurement_time_reference_factor = MEASUREMENT_TIME_REFERENCE_DEFAULT_FLOAT / MEASUREMENT_TIME_REFERENCE_DEFAULT_FLOAT;
};

void test_ALS();
