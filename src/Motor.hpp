#pragma once

#include "Queue.hpp"
#include "Semaphore.hpp"
#include "Storage.hpp"

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
    enum class PinStep : uint { };
    enum class PinDirection : uint { };
    enum class PinLimitCW : uint { };
    enum class PinLimitCCW : uint { };

    enum Command : uint8_t {
        OPEN_COMPLETELY = 0,
        /// Numbers here inbetween [0 - 100] represent a percentage:
        /// Command { 30 } means the curtain will be adjusted to ~30% closed / ~70 % open
        CLOSE_COMPLETELY = 100,
        OPEN = 101,
        CLOSE = 102,
        STOP = 103,
        CALIBRATE = 104,
    };

    struct Parameters {
        const char* name;

        PinStep step;
        PinDirection direction;
        PinLimitCW limit_cw;
        PinLimitCCW limit_ccw;

        RTOS::Variable<Command>* v_command;
        RTOS::Semaphore* s_control_auto;
        RTOS::Variable<uint8_t>* v_belt_position;

        Storage* storage;
    };

    explicit Motor(const Parameters& parameters);
    [[nodiscard]] static std::string CommandString(Motor::Command cmd);

    [[nodiscard]] int GetBeltPosition() const
    {
        return m_belt_position;
    }

    [[nodiscard]] int GetBeltMaximum() const
    {
        return m_belt_max;
    }

private:
    [[nodiscard]] bool IsCWLimitSwitchPressed() const;
    [[nodiscard]] bool IsCCWLimitSwitchPressed() const;

    bool StepCW();
    bool StepCCW();

    void Task();

    bool Calibrate();
    bool Open();
    bool Close();
    bool MoveTo();
    void ConcludeCommand();
    [[nodiscard]] uint8_t BeltPosition() const { return static_cast<uint8_t>(m_belt_position * 100 / m_belt_max); }
    void PermitAutomaticControl();

    static constexpr int OUT_OF_BOUNDS_CLOSE = 30'000;
    static constexpr int OUT_OF_BOUNDS_OPEN = -10'000;
    static constexpr TickType_t DIRECTION_CHANGE_DELAY_TICKS = pdMS_TO_TICKS(10);

    uint m_pin_step;
    uint m_pin_direction;
    uint m_pin_limit_cw;
    uint m_pin_limit_ccw;

    bool m_previous_direction;
    TickType_t m_step_finished = 0;

    TaskHandle_t m_handle = nullptr;

    RTOS::Variable<Command>* m_v_command;
    RTOS::Semaphore* m_s_control_auto;
    RTOS::Variable<uint8_t>* m_v_belt_position;

    Command m_command = CALIBRATE;
    int m_belt_position = 0;
    int m_belt_max = 0;
    Storage* m_storage;
};

/// TODO: Remove
struct test_params {
    RTOS::Variable<Motor::Command>* cmd;
    RTOS::Semaphore* control_auto;
    RTOS::Variable<float>* target;
};

void test_motor_commands(const test_params& params);
