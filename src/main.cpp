#include <FreeRTOS.h>
#include <hardware/timer.h>
#include <pico/stdlib.h>
#include <task.h>

#include "AmbientLightSensor.hpp"
#include "Logger.hpp"
#include "Primitive.hpp"
#include "example.h"

extern "C" {
uint32_t read_runtime_ctr(void)
{
    return timer_hw->timerawl;
}
}

int main()
{
    const bool stdio_initialized = stdio_init_all();
    assert(stdio_initialized);
    Logger::Initialize();
    Logger::Log("Boot");

    auto i2c_1_sda_26 = PicoW_I2C::SDA1Pin::SDA1_26;
    auto i2c_1_scl_27 = PicoW_I2C::SCL1Pin::SCL1_27;
    auto i2c_1 = std::make_unique<PicoW_I2C>(i2c_1_sda_26, i2c_1_scl_27, BH1750::BAUDRATE_MAX);

    auto example = Example::create();
    if (!example) {
        // handle error
        Logger::Log("Error: Failed to create [Example] task");
    }

    auto* als_1_measure_now = new RTOS::Semaphore { "ALS-1-MeasureNow" };
    auto* als_1_measure_continuously = new RTOS::Semaphore { "ALS-1-MeasureContinuously" };
    auto* als_1_latest_lux = new RTOS::Variable<float> { "ALS-1-LatestLux" };

    const AmbientLightSensor::Infrastructure als_1_infra {
        .measure_now = als_1_measure_now,
        .measure_continuously = als_1_measure_continuously,
        .latest_lux = als_1_latest_lux,
    };

    new Logger("Logger", DEFAULT_TASK_STACK_SIZE * 3, 1);
    new AmbientLightSensor("ALS-1", DEFAULT_TASK_STACK_SIZE * 2, 3, i2c_1.get(), BH1750::I2CDevAddr::ADDR_LOW, als_1_infra);

    Logger::Log("Semaphores: {}", RTOS::Implementation::Primitive::GetSemaphoreCount());
    Logger::Log("Queues: {}", RTOS::Implementation::Primitive::GetQueueCount());
    Logger::Log("Initializing Scheduler...");
    vTaskStartScheduler();
}
