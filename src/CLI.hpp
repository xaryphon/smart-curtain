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
    void DatetimeCommand();
    void DateCommand();
    void YearCommand();
    void MonthCommand();
    void DayCommand();
    void DOTWCommand();
    void TimeCommand();
    void HourCommand();
    void MinCommand();
    void SecCommand();

    RTOS::Variable<float>* m_v_lux_target;
    RTOS::Variable<Motor::Command>* m_v_motor_command;
    RTOS::Semaphore* m_s_control_auto;
    Storage* m_storage;
    RTC* m_rtc;

    std::stringstream m_input;
    Motor::Command MotorStringToTarget(const std::string& str);
};
