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
            DEFAULT_TASK_STACK_SIZE * 2,
            this,
            tskIDLE_PRIORITY + 3,
            &m_handle)
        == pdTRUE) {
        Logger::Log("Created task [{}]", parameters.name);
    } else {
        Logger::Log("Error: Failed to created task [{}]", parameters.name);
    }
}

/// LEFT
bool Motor::IsCWLimitSwitchPressed() const
{
    return !gpio_get(m_pin_limit_cw);
}

/// RIGHT
bool Motor::IsCCWLimitSwitchPressed() const
{
    return !gpio_get(m_pin_limit_ccw);
}

/// LEFT
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

/// RIGHT
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
    m_s_control_auto->Take(0);
    m_calibrated = Calibrate();
    if (!m_calibrated) {
        m_s_control_auto->Take(0);
    } else {
        m_s_control_auto->Give();
    }
    while (true) {
        m_v_command->Peek(&m_command, portMAX_DELAY);
        if (!m_calibrated && m_command != CALIBRATE) {
            Logger::Log("Warning: Uncalibrated. Retracting command [{}]. Switching to manual.", static_cast<uint8_t>(m_command));
            m_s_control_auto->Take(0);
            m_v_command->Receive(&m_command, 0);
            continue;
        }
        switch (m_command) {
        case OPEN_COMPLETELY:
        case OPEN:
            m_calibrated = Open();
            break;
        case CLOSE_COMPLETELY:
        case CLOSE:
            m_calibrated = Close();
            break;
        case CALIBRATE:
            m_calibrated = Calibrate();
            ConcludeCommand();
            if (!m_calibrated) {
                m_s_control_auto->Take(0);
            } else {
                m_s_control_auto->Give();
            }
            break;
        default:
            if (OPEN_COMPLETELY < m_command && m_command < CLOSE_COMPLETELY) {
                m_calibrated = MoveTo();
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
    while (StepCW()) {
        if (++m_belt_position > OUT_OF_BOUNDS_OPEN) {
            Logger::Log("Warning: CALIBRATE: CW LimitSW not found");
            return false;
        }
    }
    m_belt_position = 0;
    while (StepCCW()) {
        if (++m_belt_position > OUT_OF_BOUNDS_OPEN) {
            Logger::Log("Warning: CALIBRATE: CCW LimitSW not found");
            return false;
        }
    }
    m_belt_max = m_belt_position;
    m_v_belt_position->Overwrite(static_cast<uint8_t>(m_belt_position / m_belt_max * 100));
    Logger::Log("Calibrated. Max steps: {}", m_belt_max);
    return true;
}

bool Motor::Open()
{
    if (!StepCW()) {
        ConcludeCommand();
    } else if (m_belt_position > OUT_OF_BOUNDS_OPEN) {
        Logger::Log("Warning: OPEN out of bounds: {} / {}", m_belt_position, m_belt_max);
        return false;
    } else {
        --m_belt_position;
        m_v_belt_position->Overwrite(static_cast<uint8_t>(m_belt_position / m_belt_max * 100));
    }
    return true;
}

bool Motor::Close()
{
    if (!StepCCW()) {
        ConcludeCommand();
    } else if (m_belt_position < OUT_OF_BOUNDS_CLOSE) {
        Logger::Log("Warning: CLOSE out of bounds: {} / {}", m_belt_position, m_belt_max);
        return false;
    } else {
        ++m_belt_position;
        m_v_belt_position->Overwrite(static_cast<uint8_t>(m_belt_position / m_belt_max * 100));
    }
    return true;
}

bool Motor::MoveTo()
{
    const auto current_position = static_cast<uint8_t>(m_belt_position / m_belt_max * 100);
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
