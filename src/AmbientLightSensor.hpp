#pragma once

#include <FreeRTOS.h>
#include <task.h>

#include "BH1750.hpp"
#include "Motor.hpp"
#include "Queue.hpp"
#include "Semaphore.hpp"

struct LuxMeasurement {
    uint64_t timestamp; /// TODO: RT
    float lux;
};

class AmbientLightSensor {
public:
    struct Parameters {
        const char* task_name;

        BH1750::Parameters als1;
        BH1750::Parameters als2;

        RTOS::Variable<LuxMeasurement>* v_latest_measurement_als1;
        RTOS::Variable<LuxMeasurement>* v_latest_measurement_als2;
        RTOS::Variable<float>* v_lux_target;
        RTOS::Variable<Motor::Command>* v_motor_command;
        RTOS::Semaphore* s_control_auto;
        RTOS::Semaphore* s_http_notify;
    };

    explicit AmbientLightSensor(const Parameters& parameters);

private:
    void Task();

    void Initialize();
    bool StartMeasuring();
    bool Measure();
    [[nodiscard]] bool OnTarget();
    void StopMeasuring();
    [[nodiscard]] bool MotorAdjusting() const;
    void StopMotorAdjusting();
    [[nodiscard]] bool ControlAuto() const { return m_s_control_auto->Count() == 1; }

    BH1750 m_als1;
    BH1750 m_als2;

    RTOS::Variable<LuxMeasurement>* m_v_measurement_als_1;
    RTOS::Variable<LuxMeasurement>* m_v_measurement_als_2;
    RTOS::Variable<float>* m_v_lux_target;
    RTOS::Variable<Motor::Command>* m_v_motor_command;
    RTOS::Semaphore* m_s_control_auto;
    RTOS::Semaphore* m_s_http_notify;

    TaskHandle_t m_task_handle = nullptr;
    TickType_t m_passive_measurement_period_ticks = pdMS_TO_TICKS(10000);
    TickType_t m_previous_measurement_tick = 0;
    LuxMeasurement m_measurement_als1 = { 0, 0 };
    LuxMeasurement m_measurement_als2 = { 0, 0 };
    float m_lux_average = 0;
    bool m_commanding_motor = false;
};
