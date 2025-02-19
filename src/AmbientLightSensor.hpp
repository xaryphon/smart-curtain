#pragma once

#include <memory>

#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>

#include "BH1750.hpp"
#include "Motor.h"
#include "PicoW_I2C.h"
#include "Queue.hpp"
#include "Semaphore.hpp"

class AmbientLightSensor : private BH1750 {
public:
    struct Parameters {
        const char* name;

        PicoW_I2C* i2c;
        BH1750::I2CDevAddr BH1750_i2c_address;

        RTOS::Variable<float>* v_lux_latest_my;
        RTOS::Variable<float>* v_lux_latest_other;
        RTOS::Variable<float>* v_lux_target;
        RTOS::Variable<Motor::Command>* v_motor_command;
        RTOS::Semaphore* s_control_auto;
    };

    explicit AmbientLightSensor(const Parameters& parameters);

private:
    bool AdjustMeasurementResolution();
    [[nodiscard]] TickType_t GetMeasurementTimeTicks() const;
    void MediateMeasurementTime();
    void WaitForMeasurement();
    bool ReadLuxBlocking();
    [[nodiscard]] float Uint16ToLux(uint16_t u16) const;
    void ResetMeasurement();
    void SetMeasurementTimeFactor(float factor);
    void StopMeasuring();

    void Task();

    void Initialize();
    bool Measure();
    [[nodiscard]] bool OnTarget();
    [[nodiscard]] bool ControlAuto() { return m_s_control_auto->Count() == 0; }
    [[nodiscard]] bool OtherALSWorking();
    [[nodiscard]] bool MotorAdjusting();
    void StopMotorAdjusting();

    static constexpr TickType_t MEASUREMENT_TIME_TYPICAL_TICKS_RES_MEDIUM = pdMS_TO_TICKS(MEASUREMENT_TIME_TYPICAL_RES_MEDIUM_MS);
    static constexpr TickType_t MEASUREMENT_TIME_TYPICAL_TICKS_RES_HIGH = pdMS_TO_TICKS(MEASUREMENT_TIME_TYPICAL_RES_HIGH_MS);
    static constexpr TickType_t MEASUREMENT_TIME_TYPICAL_TICKS_RES_LOW = pdMS_TO_TICKS(MEASUREMENT_TIME_TYPICAL_RES_LOW_MS);

    static constexpr auto MEASUREMENT_TIME_REFERENCE_DEFAULT_FLOAT = static_cast<float>(BH1750::MEASUREMENT_TIME_REFERENCE_DEFAULT_MS);

    TaskHandle_t m_task_handle = nullptr;
    TickType_t m_previous_measurement_tick = 0;

    TickType_t m_passive_measurement_period_ticks = pdMS_TO_TICKS(10000);
    TickType_t m_measurement_started_at_ticks = 0;
    TickType_t m_measurement_time = MEASUREMENT_TIME_TYPICAL_TICKS_RES_MEDIUM;
    TickType_t m_measurement_time_mediation = 0;
    float m_measurement_time_reference_factor = MEASUREMENT_TIME_REFERENCE_DEFAULT_FLOAT / MEASUREMENT_TIME_REFERENCE_DEFAULT_FLOAT;

    BH1750::Mode m_measurement_mode = BH1750::GetMode();

    RTOS::Variable<float>* m_v_lux_latest_my;
    RTOS::Variable<float>* m_v_lux_latest_other;
    RTOS::Variable<float>* m_v_lux_target;
    RTOS::Variable<Motor::Command>* m_v_motor_command;
    RTOS::Semaphore* m_s_control_auto;

    float m_lux_latest = static_cast<float>(BH1750::RESET_VALUE);
    bool m_commanding_motor = false;
};
