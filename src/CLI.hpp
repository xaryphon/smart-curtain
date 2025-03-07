#pragma once

#include <iostream>
#include <sstream>
#include <string>

#include <FreeRTOS.h>
#include <hardware/rtc.h>
#include <task.h>

#include "AmbientLightSensor.hpp"
#include "Motor.hpp"
#include "Queue.hpp"
#include "RTC.hpp"
#include "Semaphore.hpp"
#include "Storage.hpp"

class CLI {
public:
    struct Parameters {
        const char* task_name;

        RTOS::Variable<LuxMeasurement>* v_latest_measurement_als1;
        RTOS::Variable<LuxMeasurement>* v_latest_measurement_als2;
        RTOS::Variable<float>* v_lux_target;
        RTOS::Variable<Motor::Command>* v_motor_command;
        RTOS::Semaphore* s_control_auto;
        RTOS::Semaphore* s_http_notify;
        Storage* storage;
        RTC* rtc;
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
    void DateCommand();
    void TimeCommand();

    RTOS::Variable<float>* m_v_lux_target;
    RTOS::Variable<Motor::Command>* m_v_motor_command;
    RTOS::Semaphore* m_s_control_auto;
    RTOS::Semaphore* m_s_http_notify;
    Storage* m_storage;
    RTC* m_rtc;
    datetime_t m_datetime;

    std::stringstream m_input;
    Motor::Command MotorStringToTarget(const std::string& str);
};
