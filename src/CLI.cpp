#include "CLI.hpp"

#include <cfloat>

#include <hardware/rtc.h>
#include <pico/stdlib.h>

#include "Logger.hpp"
#include "config.h"

CLI::CLI(const Parameters& parameters)
    : m_v_lux_target(parameters.v_lux_target)
    , m_v_motor_command(parameters.v_motor_command)
    , m_s_control_auto(parameters.s_control_auto)
    , m_s_http_notify(parameters.s_http_notify)
    , m_storage(parameters.storage)
    , m_rtc(parameters.rtc)
{
    if (xTaskCreate(TASK_KONDOM(CLI, Task),
            parameters.task_name,
            TaskStackSize::CLI,
            this,
            TaskPriority::CLI,
            &m_task_handle)
        == pdTRUE) {
        Logger::Log("Created task [{}]", parameters.task_name);
    } else {
        Logger::Log("Error: Failed to create task [{}]", parameters.task_name);
    }
}

bool CLI::ReadInput()
{
    int input_character = 0;
    while (((input_character = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT)) {
        if ((input_character == '\r' || input_character == '\n')) {
            return true;
        }
        m_input << static_cast<char>(input_character);
    }
    return false;
}

void CLI::ProcessCommand()
{
    std::string cmd;
    Logger::Log("[{}]", m_input.str());
    m_input >> cmd;
    if (cmd == "help") {
        HelpCommand();
    } else if (cmd == "target") {
        TargetCommand();
    } else if (cmd == "motor") {
        MotorCommand();
    } else if (cmd == "status") {
        StatusCommand();
    } else if (cmd == "rtc") {
        RTCCommand();
    } else if (cmd == "date") {
        DateCommand();
    } else if (cmd == "year") {
        YearCommand();
    } else if (cmd == "month") {
        MonthCommand();
    } else if (cmd == "day") {
        DayCommand();
    } else if (cmd == "dotw") {
        DOTWCommand();
    } else if (cmd == "time") {
        TimeCommand();
    } else if (cmd == "hour") {
        HourCommand();
    } else if (cmd == "min") {
        MinCommand();
    } else if (cmd == "sec") {
        SecCommand();
    } else if (cmd.empty()) {
        // avoid confusing log printing if nothing is inputted and/or terminal sends both CR/LF
    } else {
        Logger::Log("Invalid command, use 'help' to see available commands");
    }
    m_input.clear(std::ios_base::goodbit);
    m_input.str(std::string());
}

void CLI::Task()
{
    Logger::Log("Initiated");
    while (true) {
        if (ReadInput()) {
            ProcessCommand();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void CLI::HelpCommand()
{
    /// TODO command response formatting and grammar improvements, extended help pages for all commands
    std::string cmd;
    if (!(m_input >> cmd)) {
        Logger::Log("Available commands:\n"
                    "help - show this\n"
                    "status - show system status\n"
                    "target X - set target lux level\n"
                    "motor X - control motor\n"
                    "date");
    } else if (cmd == "help") {
        Logger::Log("help - show available commands");
    } else if (cmd == "status") {
        Logger::Log("status - shows system status, including the currently configured settings");
    } else if (cmd == "target") {
        Logger::Log("target - set target lux level\n"
                    "Lux target has a minimum value of 0\n"
                    "By default, will set the static target\n"
                    "Set the hourly targets by including the desired hour after the target\n"
                    "only applies to automatic mode\n"
                    "Example: 'target 500' will set the static target to 500 lux\n"
                    "         'target 700 12' will se the target for hour 12 to 700");
    } else {
        Logger::Log("Unknown command, see 'help' for all commands");
    }
}

void CLI::StatusCommand()
{
    /// TODO get status info from relevant task(s), simple/advanced status display etc.
    Logger::Log("status: ok\ntarget: 69");
    Flash::Settings flash_settings {};
    m_storage->ReadOnlyAccessLocked(portMAX_DELAY, [&](const Flash::Settings& settings) {
        flash_settings = settings;
    });
    Logger::Log("{}", Storage::StringifySettings(flash_settings));
}

void CLI::TargetCommand()
{
    float target = 0;
    int hour = 0;
    if (m_input >> target && m_input >> hour) {
        // lol dont care magic numbers ahoy
        if (target >= 0 && target <= FLT_MAX && hour >= 0 && hour <= 23) {
            Logger::Log("Lux target for hour {} set to {}", hour, target);
            m_v_lux_target->Overwrite(target);
            m_storage->WriteAccessAndProgramLocked(portMAX_DELAY, [&](Flash::Settings& settings) {
                settings.lux_targets[hour] = target;
            });
        } else {
            Logger::Log("Values out of range, check input");
        }
    } else if (target >= 0 && target <= FLT_MAX) {
        m_v_lux_target->Overwrite(target);
        m_storage->WriteAccessAndProgramLocked(portMAX_DELAY, [&](Flash::Settings& settings) {
            settings.lux_targets[Flash::Lux::LUX_STATIC] = target;
        });
        Logger::Log("Static lux target set to {}", target);
    } else {
        Logger::Log("target: missing target value");
    }
}

void CLI::MotorCommand()
{
    std::string motor_cmd;
    Motor::Command current = Motor::STOP;
    m_v_motor_command->Peek(&current, 0);
    if (current != Motor::CALIBRATE) {
        if (m_input >> motor_cmd) {
            if (motor_cmd == "manual") {
                m_s_control_auto->Take(0);
                m_storage->WriteAccessAndProgramLocked(portMAX_DELAY, [&](Flash::Settings& settings) {
                    settings.sys_mode = 0;
                });
            } else if (motor_cmd == "auto") {
                m_s_control_auto->Give();
                m_storage->WriteAccessAndProgramLocked(portMAX_DELAY, [&](Flash::Settings& settings) {
                    settings.sys_mode = Flash::bAUTO;
                });
            } else if (motor_cmd == "hourly") {
                m_s_control_auto->Give();
                m_storage->WriteAccessAndProgramLocked(portMAX_DELAY, [&](Flash::Settings& settings) {
                    settings.sys_mode = Flash::bAUTO_HOURLY | Flash::bAUTO;
                });
            } else if (motor_cmd == "calibrate") {
                m_v_motor_command->Overwrite(Motor::CALIBRATE);
            } else {
                m_s_control_auto->Take(0);
                Motor::Command const new_target = MotorStringToTarget(motor_cmd);
                m_v_motor_command->Overwrite(new_target);
                m_storage->WriteAccessAndProgramLocked(portMAX_DELAY, [&](Flash::Settings& settings) {
                    settings.motor_target = new_target;
                });
            }
        } else {
            Logger::Log("motor - missing motor command, see 'help motor' for additional info");
        }
    } else {
        Logger::Log("Motor currently calibrating, command ignored");
    }
}

Motor::Command CLI::MotorStringToTarget(const std::string& str)
{
    if (str == "open") {
        return Motor::OPEN_COMPLETELY;
    }
    if (str == "close") {
        return Motor::CLOSE_COMPLETELY;
    }
    char dummy[] = { 0 };
    int num;
    if (((sscanf(str.c_str(), "%d%c", &num, dummy) == 1 && dummy[0] == '\0')) && (Motor::CLOSE_COMPLETELY >= num && Motor::OPEN_COMPLETELY <= num)) {
        printf("\n%d\n", num);
        return static_cast<Motor::Command>(num);
    }
    printf("\n%d\n", num);
    return static_cast<Motor::Command>(-1);
}

void CLI::RTCCommand()
{
    int year;
    int month;
    int day;
    int dotw;
    int hour;
    int min;
    int sec;
    m_input >> year, m_input >> month, m_input >> day, m_input >> dotw, m_input >> hour, m_input >> min, m_input >> sec;
    datetime_t datetime = m_rtc->GetDatetime();
    datetime.year = static_cast<int16_t>(year),
    datetime.month = static_cast<int8_t>(month),
    datetime.day = static_cast<int8_t>(day),
    datetime.dotw = static_cast<int8_t>(dotw),
    datetime.hour = static_cast<int8_t>(hour);
    datetime.min = static_cast<int8_t>(min);
    datetime.sec = static_cast<int8_t>(sec);
    m_rtc->Set(datetime);
}

void CLI::DateCommand()
{
    int year;
    int month;
    int day;
    int dotw;
    m_input >> year, m_input >> month, m_input >> day, m_input >> dotw;
    datetime_t datetime = m_rtc->GetDatetime();
    datetime.year = static_cast<int16_t>(year),
    datetime.month = static_cast<int8_t>(month),
    datetime.day = static_cast<int8_t>(day),
    datetime.dotw = static_cast<int8_t>(dotw),
    m_rtc->Set(datetime);
}

void CLI::YearCommand()
{
    int year;
    m_input >> year;
    datetime_t datetime = m_rtc->GetDatetime();
    datetime.year = static_cast<int16_t>(year),
    m_rtc->Set(datetime);
}
void CLI::MonthCommand()
{
    int month;
    m_input >> month;
    datetime_t datetime = m_rtc->GetDatetime();
    datetime.month = static_cast<int8_t>(month),
    m_rtc->Set(datetime);
}
void CLI::DayCommand()
{
    int day;
    m_input >> day;
    datetime_t datetime = m_rtc->GetDatetime();
    datetime.day = static_cast<int8_t>(day),
    m_rtc->Set(datetime);
}
void CLI::DOTWCommand()
{
    int dotw;
    m_input >> dotw;
    datetime_t datetime = m_rtc->GetDatetime();
    datetime.dotw = static_cast<int8_t>(dotw),
    m_rtc->Set(datetime);
}

void CLI::TimeCommand()
{
    int hour;
    int min;
    int sec;
    m_input >> hour, m_input >> min, m_input >> sec;
    datetime_t datetime = m_rtc->GetDatetime();
    datetime.hour = static_cast<int8_t>(hour);
    datetime.min = static_cast<int8_t>(min);
    datetime.sec = static_cast<int8_t>(sec);
    m_rtc->Set(datetime);
}

void CLI::HourCommand()
{
    int hour;
    m_input >> hour;
    datetime_t datetime = m_rtc->GetDatetime();
    datetime.hour = static_cast<int8_t>(hour);
    m_rtc->Set(datetime);
}
void CLI::MinCommand()
{
    int min;
    m_input >> min;
    datetime_t datetime = m_rtc->GetDatetime();
    datetime.min = static_cast<int8_t>(min);
    m_rtc->Set(datetime);
}
void CLI::SecCommand()
{
    int sec;
     m_input >> sec;
    datetime_t datetime = m_rtc->GetDatetime();
    datetime.sec = static_cast<int8_t>(sec);
    m_rtc->Set(datetime);
}
