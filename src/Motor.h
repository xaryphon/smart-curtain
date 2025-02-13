#pragma once

#include <FreeRTOS.h>
#include <pico/stdlib.h>
#include <task.h>

#include "Queue.hpp"
#include "Semaphore.hpp"

// Assuming pinout
//  1. Step
//  2. Direction
//  3. Limit switch CW
//  4. Limit switch CCW
//  5. NC
//  6. NC
//  7. GND
//  8. 3V3
//  9. NC
// 10. NC

class Motor {
public:
    struct Infrastructure {
        RTOS::Semaphore* move_now;
        RTOS::Semaphore* measure_als_1;
        RTOS::Semaphore* continous_lux_als_1;
        RTOS::Semaphore* measure_als_2;
        RTOS::Semaphore* continous_lux_als_2;
        RTOS::Variable<float>* latest_lux;
        RTOS::Variable<float>* target_lux;
        RTOS::Variable<uint16_t>* belt_position;
    };

    enum class PinStep : uint {};
    enum class PinDirection : uint {};
    enum class PinLimitCW : uint {};
    enum class PinLimitCCW : uint {};

    static constexpr auto PIN_STEP = Motor::PinStep(10);
    static constexpr auto PIN_DIRECTION = Motor::PinDirection(11);
    static constexpr auto PIN_LIMIT_SWITCH_CW = Motor::PinLimitCW(12);
    static constexpr auto PIN_LIMIT_SWITCH_CCW = Motor::PinLimitCCW(13);

    explicit Motor(const Infrastructure& infra, const char* name, PinStep step, PinDirection direction, PinLimitCW limit_cw, PinLimitCCW limit_ccw);

private:
    [[nodiscard]] bool IsCWLimitSwitchPressed() const;
    [[nodiscard]] bool IsCCWLimitSwitchPressed() const;

    bool StepCW();
    bool StepCCW();
    bool Calibrate();

    bool OnTarget();
    void AdjustCurtain();

    void Task();

    static constexpr uint EXPECTED_MAX_STEPS_ONE_WAY = 30'000;
    static constexpr TickType_t DIRECTION_CHANGE_DELAY_TICKS = pdMS_TO_TICKS(10);

    uint m_pin_step;
    uint m_pin_direction;
    uint m_pin_limit_cw;
    uint m_pin_limit_ccw;

    TickType_t m_step_finished = 0;

    TaskHandle_t m_handle = nullptr;

    RTOS::Semaphore* m_s_move_now;
    RTOS::Semaphore* m_s_measure_als_1;
    RTOS::Semaphore* m_s_continous_lux_als_1;
    RTOS::Semaphore* m_s_measure_als_2;
    RTOS::Semaphore* m_s_continous_lux_als_2;
    RTOS::Variable<float>* m_v_latest_lux;
    RTOS::Variable<float>* m_v_target_lux;
    RTOS::Variable<uint16_t>* m_v_belt_position;

    float m_latest_lux = 0;
    float m_target_lux = 0;
    float m_current_lux_difference = 0;
    uint m_belt_position = 0;
    uint m_belt_max = 0;
};

void test_motor();
