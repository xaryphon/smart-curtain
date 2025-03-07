#include "Indicator.hpp"

#include "Logger.hpp"
#include "config.h"

Indicator::Indicator(const ConstructorParameters& constructor_parameters)
    : LED(static_cast<uint>(constructor_parameters.pin), constructor_parameters.task_name)
{
    if (xTaskCreate(
            TASK_KONDOM(Indicator, Task),
            constructor_parameters.task_name,
            TaskStackSize::INDICATOR,
            this,
            TaskPriority::INDICATOR,
            &m_handle)
        == pdPASS) {
        Logger::Log("Created task [{}]", constructor_parameters.task_name);
    } else {
        Logger::Log("Error: Failed to created task [{}]", constructor_parameters.task_name);
    }
    Put(true);
}

void Indicator::On()
{
    Set(LED::WRAP);
    Put(true);
    vTaskResume(m_handle);
}

void Indicator::Off()
{
    Put(false);
    vTaskSuspend(m_handle);
}

void Indicator::Task()
{
    Logger::Log("Initiated");
    Off();
    while (true) {
        if (m_direction) {
            if (!Adjust(+2)) {
                m_direction = !m_direction;
            }
        } else {
            if (!Adjust(-2)) {
                m_direction = !m_direction;
            }
        }
        vTaskDelayUntil(&m_tick, LOOP_INTERVAL);
    }
}

/// TODO: remove

void test_indicator_task(void* params)
{
    auto* indicator = static_cast<Indicator*>(params);
    Logger::Log("Initiated");
    vTaskDelay(1000);
    indicator->On();
    while (true) {
        vTaskDelay(portMAX_DELAY);
    }
}

void test_indicator()
{
    auto* indicator = new Indicator({
        .task_name = "Indicator",
        .pin = Indicator::GPIO::Pin17,
    });
    if (xTaskCreate(test_indicator_task, "test_indicator", 1024, indicator, 2, nullptr) == pdPASS) {
        Logger::Log("Created task test_indicator");
    } else {
        Logger::Log("Failed to create task test_indicator");
    }
}
