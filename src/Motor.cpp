#include "Motor.h"

#include <FreeRTOS.h>
#include <hardware/gpio.h>
#include <task.h>

#include "Logger.hpp"
#include "config.h"

constexpr bool DIRECTION_CW = true;
constexpr bool DIRECTION_CCW = false;

constexpr uint STEP_HIGH_MS = 1;
constexpr uint STEP_LOW_MS = 1;

Motor::Motor(const Motor::Infrastructure& infra, const char* name, PinStep step, PinDirection direction, PinLimitCW limit_cw, PinLimitCCW limit_ccw)
    : m_pin_step(static_cast<uint>(step))
    , m_pin_direction(static_cast<uint>(direction))
    , m_pin_limit_cw(static_cast<uint>(limit_cw))
    , m_pin_limit_ccw(static_cast<uint>(limit_ccw))
    , m_s_move_now(infra.move_now)
    , m_s_measure_als_1(infra.measure_als_1)
    , m_s_continous_lux_als_1(infra.continous_lux_als_1)
    , m_s_measure_als_2(infra.measure_als_2)
    , m_s_continous_lux_als_2(infra.continous_lux_als_2)
    , m_v_latest_lux(infra.latest_lux)
    , m_v_target_lux(infra.target_lux)
    , m_v_belt_position(infra.belt_position)
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
            name, DEFAULT_TASK_STACK_SIZE * 2,
            static_cast<void*>(this),
            tskIDLE_PRIORITY + 3,
            &m_handle)
        == pdTRUE) {
        Logger::Log("Created task [{}]", name);
    } else {
        Logger::Log("Error: Failed to created task [{}]", name);
    }
}

/// OPEN
bool Motor::IsCWLimitSwitchPressed() const
{
    return !gpio_get(m_pin_limit_cw);
}

/// CLOSE
bool Motor::IsCCWLimitSwitchPressed() const
{
    return !gpio_get(m_pin_limit_ccw);
}

/// OPEN
bool Motor::StepCW() // NOLINT(readability-make-member-function-const)
{
    if (IsCWLimitSwitchPressed()) {
        Logger::Log("Right limit reached");
        return false;
    }
    xTaskDelayUntil(&m_step_finished, STEP_LOW_MS);
    gpio_put(m_pin_direction, DIRECTION_CW);
    gpio_put(m_pin_step, true);
    vTaskDelay(pdMS_TO_TICKS(STEP_HIGH_MS));
    gpio_put(m_pin_step, false);
    m_step_finished = xTaskGetTickCount();
    ++m_belt_position;
    return true;
}

/// CLOSE
bool Motor::StepCCW() // NOLINT(readability-make-member-function-const)
{
    if (IsCCWLimitSwitchPressed()) {
        Logger::Log("Left limit reached");
        return false;
    }
    xTaskDelayUntil(&m_step_finished, STEP_LOW_MS);
    gpio_put(m_pin_direction, DIRECTION_CCW);
    gpio_put(m_pin_step, true);
    vTaskDelay(pdMS_TO_TICKS(STEP_HIGH_MS));
    gpio_put(m_pin_step, false);
    m_step_finished = xTaskGetTickCount();
    --m_belt_position;
    return true;
}

bool Motor::Calibrate()
{
    Logger::Log("Calibrating...");
    while (StepCW()) {
        if (++m_belt_max > EXPECTED_MAX_STEPS_ONE_WAY) {
            return false;
        }
    }
    vTaskDelay(pdMS_TO_TICKS(DIRECTION_CHANGE_DELAY_TICKS));
    m_belt_max = 0;
    while (StepCCW()) {
        if (++m_belt_max > EXPECTED_MAX_STEPS_ONE_WAY) {
            return false;
        }
    }
    m_belt_position = m_belt_max;
    return true;
}

bool Motor::OnTarget()
{
    if (!m_v_latest_lux->Peek(&m_latest_lux, 0)) {
        Logger::Log("Error: Latest Lux unavailable");
        return true;
    }
    if (!m_v_target_lux->Peek(&m_target_lux, 0)) {
        Logger::Log("Error: Target Lux unavailable");
        return true;
    }
    m_current_lux_difference = (m_target_lux - m_latest_lux) / m_target_lux;
    if (IsCWLimitSwitchPressed() && m_current_lux_difference < 0) {
        return true;
    }
    if (IsCCWLimitSwitchPressed() && m_current_lux_difference > 0) {
        return true;
    }
    static const float MARGIN = 0.1;
    return std::abs(m_current_lux_difference) < MARGIN;
}

void Motor::AdjustCurtain()
{
    if (OnTarget()) {
        return;
    }
    m_s_continous_lux_als_1->Give();
    m_s_measure_als_1->Give();
    m_s_continous_lux_als_2->Give();
    m_s_measure_als_2->Give();
    while (!OnTarget()) {
        if (m_current_lux_difference > 0) {
            if (StepCW()) {
                m_v_belt_position->Overwrite(static_cast<uint>(m_belt_position / m_belt_max * 1000));
            } else {
                vTaskDelay(DIRECTION_CHANGE_DELAY_TICKS);
                break;
            }
        } else {
            if (StepCCW()) {
                m_v_belt_position->Overwrite(static_cast<uint>(m_belt_position / m_belt_max * 1000));
            } else {
                vTaskDelay(DIRECTION_CHANGE_DELAY_TICKS);
                break;
            }
        }
    }
    m_s_continous_lux_als_1->Give();
    m_s_continous_lux_als_2->Give();
}

void Motor::Task()
{
    Logger::Log("Initiated");
    if (Calibrate()) {
        Logger::Log("Calibrated beyond expected maximum [{}]. Suspending.", EXPECTED_MAX_STEPS_ONE_WAY);
        vTaskSuspend(m_handle);
    } else {
        Logger::Log("Calibrated. Length: {} steps", m_belt_max);
    }
    while (true) {
        m_s_move_now->Take(portMAX_DELAY);
        AdjustCurtain();
    }
}
