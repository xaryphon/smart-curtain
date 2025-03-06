#include "Motor.hpp"

#include <hardware/gpio.h>
#include <task.h>

#include "Logger.hpp"
#include "config.h"

constexpr bool DIRECTION_CW = true;
constexpr bool DIRECTION_CCW = false;

constexpr uint STEP_HIGH_MS = 1;
constexpr uint STEP_LOW_MS = 1;

Motor::Motor(const Parameters& parameters)
    : m_pin_step(static_cast<uint>(parameters.step))
    , m_pin_direction(static_cast<uint>(parameters.direction))
    , m_pin_limit_cw(static_cast<uint>(parameters.limit_cw))
    , m_pin_limit_ccw(static_cast<uint>(parameters.limit_ccw))
    , m_previous_direction(DIRECTION_CW)
    , m_v_command(parameters.v_command)
    , m_s_control_auto(parameters.s_control_auto)
    , m_v_belt_position(parameters.v_belt_position)
    , m_storage(parameters.storage)
{
    gpio_init(m_pin_step);
    gpio_set_dir(m_pin_step, GPIO_OUT);

    gpio_init(m_pin_direction);
    gpio_set_dir(m_pin_direction, GPIO_OUT);

    gpio_init(m_pin_limit_cw);
    gpio_set_dir(m_pin_limit_cw, GPIO_IN);
    gpio_pull_up(m_pin_limit_cw);

    gpio_init(m_pin_limit_ccw);
    gpio_set_dir(m_pin_limit_ccw, GPIO_IN);
    gpio_pull_up(m_pin_limit_ccw);

    if (xTaskCreate(
            TASK_KONDOM(Motor, Task),
            parameters.name,
            TaskStackSize::MOTOR,
            this,
            TaskPriority::MOTOR,
            &m_handle)
        == pdTRUE) {
        Logger::Log("Created task [{}]", parameters.name);
    } else {
        Logger::Log("Error: Failed to created task [{}]", parameters.name);
    }
    m_s_control_auto->Take(0);
}

std::string Motor::CommandString(const Motor::Command cmd)
{
    switch (cmd) {
    case OPEN_COMPLETELY:
        return "OPEN_COMPLETELY";
    case CLOSE_COMPLETELY:
        return "CLOSE_COMPLETELY";
    case OPEN:
        return "OPEN";
    case CLOSE:
        return "CLOSE";
    case CALIBRATE:
        return "CALIBRATE";
    default:
        if (OPEN_COMPLETELY < cmd && cmd < CLOSE_COMPLETELY) {
            return fmt::format("{}%", static_cast<uint8_t>(cmd));
        }
    }
    return "UNKNOWN";
}

/// RIGHT
bool Motor::IsCWLimitSwitchPressed() const
{
    return !gpio_get(m_pin_limit_cw);
}

/// LEFT
bool Motor::IsCCWLimitSwitchPressed() const
{
    return !gpio_get(m_pin_limit_ccw);
}

/// RIGHT
bool Motor::StepCW() // NOLINT(readability-make-member-function-const)
{
    if (IsCWLimitSwitchPressed()) {
        return false;
    }
    gpio_put(m_pin_direction, DIRECTION_CW);
    if (m_previous_direction == DIRECTION_CCW) {
        vTaskDelay(1);
    }
    xTaskDelayUntil(&m_step_finished, STEP_LOW_MS);
    gpio_put(m_pin_step, true);
    vTaskDelay(pdMS_TO_TICKS(STEP_HIGH_MS));
    gpio_put(m_pin_step, false);
    m_step_finished = xTaskGetTickCount();
    m_previous_direction = DIRECTION_CW;
    return true;
}

/// LEFT
bool Motor::StepCCW() // NOLINT(readability-make-member-function-const)
{
    if (IsCCWLimitSwitchPressed()) {
        return false;
    }
    gpio_put(m_pin_direction, DIRECTION_CCW);
    if (m_previous_direction == DIRECTION_CW) {
        vTaskDelay(1);
    }
    xTaskDelayUntil(&m_step_finished, STEP_LOW_MS);
    gpio_put(m_pin_step, true);
    vTaskDelay(pdMS_TO_TICKS(STEP_HIGH_MS));
    gpio_put(m_pin_step, false);
    m_step_finished = xTaskGetTickCount();
    m_previous_direction = DIRECTION_CCW;
    return true;
}

void Motor::Task()
{
    Logger::Log("Initiated");
    Calibrate();
    if (m_belt_max == 0) {
        m_s_control_auto->Take(0);
    } else {
        PermitAutomaticControl();
    }
    while (true) {
        m_v_command->Peek(&m_command, portMAX_DELAY);
        if (m_belt_max == 0 && m_command != CALIBRATE) {
            Logger::Log("Warning: Uncalibrated. Retracting command [{}]. Switching to manual.", static_cast<uint8_t>(m_command));
            m_s_control_auto->Take(0);
            ConcludeCommand();
            continue;
        }
        switch (m_command) {
        case OPEN_COMPLETELY:
        case OPEN:
            if (!Open()) {
                m_belt_max = 0;
            }
            break;
        case CLOSE_COMPLETELY:
        case CLOSE:
            if (!Close()) {
                m_belt_max = 0;
            }
            break;
        case STOP:
            ConcludeCommand();
            break;
        case CALIBRATE:
            if (!Calibrate()) {
                m_belt_max = 0;
            }
            ConcludeCommand();
            if (m_belt_max != 0) {
                PermitAutomaticControl();
            }
            break;
        default:
            if (OPEN_COMPLETELY < m_command && m_command < CLOSE_COMPLETELY) {
                if (!MoveTo()) {
                    m_belt_max = 0;
                }
            } else {
                Logger::Log("Error: Unknown action command: {}", static_cast<uint8_t>(m_command));
                m_v_command->Receive(&m_command, 0);
            }
        }
    }
}

bool Motor::Calibrate()
{
    Logger::Log("Calibrating...");
    m_belt_position = 0;
    while (StepCW()) {
        if (++m_belt_position > OUT_OF_BOUNDS_CLOSE) {
            Logger::Log("Warning: CALIBRATE: CW LimitSW not found");
            return false;
        }
    }
    m_belt_position = 0;
    while (StepCCW()) {
        if (++m_belt_position > OUT_OF_BOUNDS_CLOSE) {
            Logger::Log("Warning: CALIBRATE: CCW LimitSW not found");
            return false;
        }
    }
    m_belt_max = m_belt_position;
    m_v_belt_position->Overwrite(BeltPosition());
    Logger::Log("Calibrated. Max steps: {}", m_belt_max);
    return true;
}

bool Motor::Open()
{
    if (!StepCW()) {
        ConcludeCommand();
    } else if (m_belt_position < OUT_OF_BOUNDS_OPEN) {
        Logger::Log("Warning: OPEN out of bounds: {} / {}", m_belt_position, m_belt_max);
        return false;
    } else {
        --m_belt_position;
        m_v_belt_position->Overwrite(BeltPosition());
    }
    return true;
}

bool Motor::Close()
{
    if (!StepCCW()) {
        ConcludeCommand();
    } else if (m_belt_position > OUT_OF_BOUNDS_CLOSE) {
        Logger::Log("Warning: CLOSE out of bounds: {} / {}", m_belt_position, m_belt_max);
        return false;
    } else {
        ++m_belt_position;
        m_v_belt_position->Overwrite(BeltPosition());
    }
    return true;
}

bool Motor::MoveTo()
{
    const auto current_position = BeltPosition();
    if (current_position < m_command) {
        return Close();
    }
    if (current_position > m_command) {
        return Open();
    }
    Logger::Log("Position reached: ~{} % ; {} / {} ", current_position, m_belt_position, m_belt_max);
    ConcludeCommand();
    return true;
}

void Motor::ConcludeCommand()
{
    const Command command = m_command;
    // remove the command to signal pertinent tasks that motor has concluded the command
    m_v_command->Receive(&m_command, 0);
    if (command != m_command) {
        // if command changed during command execution
        // put it back
        m_v_command->Append(m_command, 0);
    }
}

void Motor::PermitAutomaticControl()
{
    m_storage->ReadOnlyAccessLocked(portMAX_DELAY, [&](const Flash::Settings& settings) {
        if (settings.sys_mode & Flash::bAUTO) {
            m_s_control_auto->Give();
        } else {
            m_v_command->Overwrite(Command { settings.motor_target });
        }
    });
}

/// TODO: remove

void test_motor_commands_task(void* params)
{
    auto* cmd = static_cast<test_params*>(params)->cmd;
    auto* control_auto = static_cast<test_params*>(params)->control_auto;
    auto* target = static_cast<test_params*>(params)->target;

    Logger::Log("Waiting for initial {} to finish", Motor::CommandString(Motor::Command::CALIBRATE));
    control_auto->Take(portMAX_DELAY);
    Logger::Log("Finished initial {}", Motor::CommandString(Motor::Command::CALIBRATE));
    vTaskDelay(pdMS_TO_TICKS(10000));

    auto new_command = Motor::Command { 50 };
    cmd->Append(new_command, portMAX_DELAY);
    Logger::Log("Commanded to {}", Motor::CommandString(new_command));

    vTaskDelay(pdMS_TO_TICKS(15000));
    auto prev_command = new_command;
    new_command = Motor::CLOSE_COMPLETELY;
    cmd->Overwrite(new_command);
    Logger::Log("Overwrote {} command to {}", Motor::CommandString(prev_command), Motor::CommandString(new_command));
    vTaskDelay(pdMS_TO_TICKS(10000));

    new_command = Motor::Command { 75 };
    cmd->Append(new_command, portMAX_DELAY);
    Logger::Log("Commanded to {}", Motor::CommandString(new_command));

    Logger::Log("Waiting for {} to finish...", Motor::CommandString(new_command));
    cmd->Append(Motor::STOP, portMAX_DELAY);
    Logger::Log("Finished {}", Motor::CommandString(new_command));
    vTaskDelay(pdMS_TO_TICKS(10000));

    new_command = Motor::Command::OPEN_COMPLETELY;
    cmd->Append(new_command, portMAX_DELAY);
    Logger::Log("Commanded to {}", Motor::CommandString(new_command));

    Logger::Log("Waiting for {} to finish...", Motor::CommandString(new_command));
    cmd->Append(Motor::STOP, portMAX_DELAY);
    Logger::Log("Finished {}", Motor::CommandString(new_command));
    vTaskDelay(pdMS_TO_TICKS(10000));

    new_command = Motor::Command::CALIBRATE;
    cmd->Append(new_command, portMAX_DELAY);
    Logger::Log("Commanded to {}", Motor::CommandString(new_command));

    Logger::Log("Waiting for {} to finish...", Motor::CommandString(new_command));
    cmd->Append(Motor::STOP, portMAX_DELAY);
    Logger::Log("Finished {}", Motor::CommandString(new_command));
    vTaskDelay(pdMS_TO_TICKS(10000));

    control_auto->Give();
    Logger::Log("Manual -> Auto");

    float new_target = 200;
    target->Overwrite(new_target);
    Logger::Log("Setting lux target to: {}", new_target);

    vTaskDelay(pdMS_TO_TICKS(15000));
    new_target = 100;
    target->Overwrite(new_target);
    Logger::Log("Setting lux target to: {}", new_target);

    Logger::Log("Done");
    vTaskDelay(portMAX_DELAY);
}

void test_motor_commands(const test_params& params)
{
    auto* p = new test_params(params);
    xTaskCreate(test_motor_commands_task, "TestMotor", DEFAULT_TASK_STACK_SIZE * 4, p, tskIDLE_PRIORITY + 4, nullptr);
}
