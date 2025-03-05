#pragma once

#include <iostream>
#include <sstream>
#include <string>

#include <FreeRTOS.h>
#include <task.h>

#include "AmbientLightSensor.hpp"
#include "Motor.hpp"
#include "Queue.hpp"
#include "Semaphore.hpp"

class CLI {
public:
    struct Parameters {
        const char* task_name;

        RTOS::Variable<LuxMeasurement>* v_latest_measurement_als1;
        RTOS::Variable<LuxMeasurement>* v_latest_measurement_als2;
        RTOS::Variable<float>* v_lux_target;
        RTOS::Variable<Motor::Command>* v_motor_command;
        RTOS::Semaphore* s_control_auto;
    };

    explicit CLI(const Parameters& parameters);
    void Task();
    bool ReadInput();
    void ProcessCommand();

private:
    TaskHandle_t m_task_handle = nullptr;

    void HelpCommand();
    void StatusCommand();
    void TargetCommand();
    void MotorCommand();
    void RTCCommand();

    RTOS::Variable<float>* m_v_lux_target;
    RTOS::Variable<Motor::Command>* m_v_motor_command;
    RTOS::Semaphore* m_s_control_auto;

    std::stringstream m_input;
};
