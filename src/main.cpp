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

    /// Serial Interfaces
    auto i2c_1_sda = PicoW_I2C::SDA1Pin::SDA1_2;
    auto i2c_1_scl = PicoW_I2C::SCL1Pin::SCL1_3;
    auto i2c_1 = std::make_unique<PicoW_I2C>(i2c_1_sda, i2c_1_scl, BH1750::BAUDRATE_MAX);
    auto i2c_0_sda = PicoW_I2C::SDA0Pin::SDA0_4;
    auto i2c_0_scl = PicoW_I2C::SCL0Pin::SCL0_5;
    auto i2c_0 = std::make_unique<PicoW_I2C>(i2c_0_sda, i2c_0_scl, BH1750::BAUDRATE_MAX);

    auto example = Example::create();
    if (!example) {
        // handle error
        Logger::Log("Error: Failed to create [Example] task");
    }

    /// Semaphores
    auto* control_auto = new RTOS::Semaphore { "ControlAuto" };

    /// Variables
    auto* als_1_lux_latest = new RTOS::Variable<float> { "ALS-1-LuxLatest" };
    auto* als_2_lux_latest = new RTOS::Variable<float> { "ALS-2-LuxLatest" };
    auto* belt_position = new RTOS::Variable<uint8_t> { "BeltPosition" };
    auto* lux_target = new RTOS::Variable<float> { "LuxTarget" };
    auto* motor_command = new RTOS::Variable<Motor::Command> { "MotorAction" };

    /// Tasks
    new Logger("Logger", DEFAULT_TASK_STACK_SIZE * 3, 1);
    new AmbientLightSensor({
        .name = "ALS-1",

        .i2c = i2c_1.get(),
        .BH1750_i2c_address = BH1750::I2CDevAddr::ADDR_LOW,

        .v_lux_latest_my = als_1_lux_latest,
        .v_lux_latest_other = als_2_lux_latest,
        .v_lux_target = lux_target,
        .v_motor_command = motor_command,
        .s_control_auto = control_auto,
    });
    new AmbientLightSensor({
        .name = "ALS-2",

        .i2c = i2c_0.get(),
        .BH1750_i2c_address = BH1750::I2CDevAddr::ADDR_LOW,

        .v_lux_latest_my = als_2_lux_latest,
        .v_lux_latest_other = als_1_lux_latest,
        .v_lux_target = lux_target,
        .v_motor_command = motor_command,
        .s_control_auto = control_auto,
    });
    new Motor({
        .name = "Motor",

        .step = Motor::PinStep { 16 },
        .direction = Motor::PinDirection { 17 },
        .limit_cw = Motor::PinLimitCW { 18 },
        .limit_ccw = Motor::PinLimitCCW { 19 },

        .v_command = motor_command,
        .s_control_auto = control_auto,
        .v_belt_position = belt_position,
    });

    Logger::Log("Semaphores: {}", RTOS::Implementation::Primitive::GetSemaphoreCount());
    Logger::Log("Queues: {}", RTOS::Implementation::Primitive::GetQueueCount());
    Logger::Log("Initializing Scheduler...");
    vTaskStartScheduler();
}
