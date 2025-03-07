#pragma once

#include <FreeRTOS.h>
#include "Semaphore.hpp"
#include <task.h>

#include "LED.hpp"

class Indicator : private LED {
public:
    enum class GPIO : uint {
        Pin16 = 16,
        Pin17 = 17,
    };

    struct ConstructorParameters {
        const char* task_name;
        GPIO pin;
    };

    explicit Indicator(const ConstructorParameters& constructor_parameters);

    void On();
    void Off();

private:
    void Task();

    static constexpr TickType_t LOOP_INTERVAL = 10;

    TaskHandle_t m_handle;
    TickType_t m_tick = 0;

    bool m_direction = true;
};

void test_indicator();
