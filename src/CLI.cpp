#include "CLI.hpp"

#include <pico/stdlib.h>

#include "Logger.hpp"
#include "config.h"

CLI::CLI(const Parameters& parameters)
    : m_v_lux_target(parameters.v_lux_target)
    , m_v_motor_command(parameters.v_motor_command)
    , m_s_control_auto(parameters.s_control_auto)
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
    }
    if (cmd == "target") {
        TargetCommand();
    }
    if (cmd == "motor") {
        MotorCommand();
    }
    if (cmd == "status") {
        StatusCommand();
    }
    if (cmd == "rtc") {
        RTCCommand();
    }
    if (cmd.empty()) {

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
    /// TODO command response formatting and grammar improvements
    Logger::Log("Available commands:\n"
                "help - show this\n"
                "status - show system status\n"
                "target X - set target lux level\n"
                "motor X - \n"
                "wifi SSID PWD - set wifi credentials");
}

void CLI::StatusCommand()
{
    /// TODO get status info from relevant task(s)
    Logger::Log("status: ok\ntarget: 69");
}

void CLI::TargetCommand()
{
    float target = 0;
    if (m_input >> target) {
        Logger::Log("Lux target set to {}", target);
        m_v_lux_target->Overwrite(target);
    } else {
        Logger::Log("target: missing target value");
    }
}

void CLI::MotorCommand()
{
    uint8_t motor_cmd = Motor::STOP;
    if (m_input >> motor_cmd) {
        Motor::Command current = Motor::STOP;
        if (!m_v_motor_command->Peek(&current, 0) && current != Motor::CALIBRATE) {
            m_s_control_auto->Take(0);
            m_v_motor_command->Overwrite(static_cast<Motor::Command>(motor_cmd));
            Logger::Log("[{}]", motor_cmd);
        }
        Logger::Log("[{}]", motor_cmd);
    }
}

void CLI::RTCCommand()
{
}
