#include <task.h>

#include "AmbientLightSensor.hpp"
#include "I2C.hpp"
#include "Logger.hpp"
#include "Motor.hpp"
#include "Primitive.hpp"
#include "config.h"

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
    const auto i2c_1 = std::make_unique<I2C>(I2C::SDA1::PIN_2, I2C::SCL1::PIN_3, BH1750::BAUDRATE_MAX);
    const auto i2c_0 = std::make_unique<I2C>(I2C::SDA0::PIN_4, I2C::SCL0::PIN_5, BH1750::BAUDRATE_MAX);

    /// Semaphores
    auto* control_auto = new RTOS::Semaphore { "ControlAuto" };

    /// Variables
    auto* latest_measurement_als1 = new RTOS::Variable<LuxMeasurement> { "ALS-1-LatestMeasurement" };
    auto* latest_measurement_als2 = new RTOS::Variable<LuxMeasurement> { "ALS-2-LatestMeasurement" };
    auto* belt_position = new RTOS::Variable<uint8_t> { "BeltPosition" };
    auto* lux_target = new RTOS::Variable<float> { "LuxTarget" };
    auto* motor_command = new RTOS::Variable<Motor::Command> { "MotorAction" };

    /// Tasks
    new Logger("Logger", DEFAULT_TASK_STACK_SIZE * 3, 1);
    new AmbientLightSensor({
        .task_name = "ALS",

        .als1 = { "ALS-1", i2c_0.get(), BH1750::I2CDevAddr::LOW },
        .als2 = { "ALS-2", i2c_1.get(), BH1750::I2CDevAddr::LOW },

        .v_latest_measurement_als1 = latest_measurement_als1,
        .v_latest_measurement_als2 = latest_measurement_als2,

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
    sleep_us(1); // Ensure RTOS delay functionality
    vTaskStartScheduler();
}
