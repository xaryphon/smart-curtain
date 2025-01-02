#include "Motor.h"

#include <cstdio>

#include <FreeRTOS.h>
#include <hardware/gpio.h>
#include <task.h>

#include "config.h"

constexpr bool DIRECTION_CW = true;
constexpr bool DIRECTION_CCW = false;

constexpr uint STEP_HIGH_MS = 1;
constexpr uint STEP_LOW_MS = 1;

Motor::Motor(PinStep step, PinDirection direction, PinLimitCW limit_cw, PinLimitCCW limit_ccw)
    : m_pin_step(static_cast<uint>(step))
    , m_pin_direction(static_cast<uint>(direction))
    , m_pin_limit_cw(static_cast<uint>(limit_cw))
    , m_pin_limit_ccw(static_cast<uint>(limit_ccw))
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
}

bool Motor::IsCWLimitSwitchPressed() const
{
    return !gpio_get(m_pin_limit_cw);
}

bool Motor::IsCCWLimitSwitchPressed() const
{
    return !gpio_get(m_pin_limit_ccw);
}

bool Motor::StepCW() // NOLINT(readability-make-member-function-const)
{
    if (IsCWLimitSwitchPressed()) {
        return false;
    }
    gpio_put(m_pin_direction, DIRECTION_CW);
    gpio_put(m_pin_step, true);
    vTaskDelay(pdMS_TO_TICKS(STEP_HIGH_MS));
    gpio_put(m_pin_step, false);
    vTaskDelay(pdMS_TO_TICKS(STEP_LOW_MS));
    return true;
}

bool Motor::StepCCW() // NOLINT(readability-make-member-function-const)
{
    if (IsCCWLimitSwitchPressed()) {
        return false;
    }
    gpio_put(m_pin_direction, DIRECTION_CCW);
    gpio_put(m_pin_step, true);
    vTaskDelay(pdMS_TO_TICKS(STEP_HIGH_MS));
    gpio_put(m_pin_step, false);
    vTaskDelay(pdMS_TO_TICKS(STEP_LOW_MS));
    return true;
}

uint Motor::Calibrate()
{
    while (StepCW()) { }
    uint step_count = 0;
    while (StepCCW()) {
        step_count += 1;
    }
    return step_count;
}

constexpr auto PIN_STEP = Motor::PinStep(10);
constexpr auto PIN_DIRECTION = Motor::PinDirection(11);
constexpr auto PIN_LIMIT_SWITCH_CW = Motor::PinLimitCW(12);
constexpr auto PIN_LIMIT_SWITCH_CCW = Motor::PinLimitCCW(13);

// TODO: remove
static void test_motor_task([[maybe_unused]] void* param)
{
    Motor motor { PIN_STEP, PIN_DIRECTION, PIN_LIMIT_SWITCH_CW, PIN_LIMIT_SWITCH_CCW };

    uint steps = motor.Calibrate();
    printf("Calibration done: %u steps\n", steps); // NOLINT(hicpp-vararg,cppcoreguidelines-pro-type-vararg)

    while (true) {
        while (motor.StepCW()) { }
        while (motor.StepCCW()) { }
    }
}

// TODO: remove
void test_motor()
{
    xTaskCreate(test_motor_task, "Motor", DEFAULT_TASK_STACK_SIZE, nullptr, tskIDLE_PRIORITY + 3, nullptr);
}
