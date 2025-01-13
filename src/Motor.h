#pragma once

#include <pico/stdlib.h>

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
    enum class PinStep : uint {};
    enum class PinDirection : uint {};
    enum class PinLimitCW : uint {};
    enum class PinLimitCCW : uint {};

    explicit Motor(PinStep step, PinDirection direction, PinLimitCW limit_cw, PinLimitCCW limit_ccw);

    bool IsCWLimitSwitchPressed() const;
    bool IsCCWLimitSwitchPressed() const;

    bool StepCW();
    bool StepCCW();

    uint Calibrate();

private:
    uint m_pin_step;
    uint m_pin_direction;
    uint m_pin_limit_cw;
    uint m_pin_limit_ccw;
};

void test_motor();
