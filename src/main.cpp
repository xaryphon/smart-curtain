#include <functional>

#include <FreeRTOS.h>
#include <pico/cyw43_arch.h>
#include <pico/stdlib.h>
#include <task.h>

#include "AmbientLightSensor.hpp"
#include "HttpServer.hpp"
#include "I2C.hpp"
#include "Logger.hpp"
#include "Motor.hpp"
#include "Primitive.hpp"
#include "SPI.hpp"
#include "Storage.hpp"
#include "W5500LWIP.hpp"
#include "config.h"

extern "C" {
uint32_t read_runtime_ctr(void)
{
    return timer_hw->timerawl;
}
}

static void late_main(std::function<void()>&& callback)
{
    auto* ptr = new std::function<void()>(std::move(callback));
    xTaskCreate([](void* param) {
        Logger::Log("Start of late main");
        auto* ptr = static_cast<std::function<void()>*>(param);
        (*ptr)();
        delete ptr;
        Logger::Log("End of late main");
        vTaskDelete(nullptr);
        for (;;) {
            vTaskDelay(portMAX_DELAY);
        }
    },
        "late_main", 1024, static_cast<void*>(ptr), tskIDLE_PRIORITY + 4, nullptr);
}

int main()
{
    auto* rtc = new RTC();
    Logger::Initialize({ .rtc = rtc });
    Logger::Log("Boot");

    /// Serial Interfaces
    const auto i2c_1 = std::make_unique<I2C>(I2C::SDA1::PIN_2, I2C::SCL1::PIN_3, BH1750::BAUDRATE_MAX);
    const auto i2c_0 = std::make_unique<I2C>(I2C::SDA0::PIN_4, I2C::SCL0::PIN_5, BH1750::BAUDRATE_MAX);
    SPI* spi_1 = new SPI(SPI::RX1::PIN_12, SPI::TX1::PIN_15, SPI::SCK1::PIN_10, 10'000'000);

    /// Semaphores
    auto* control_auto = new RTOS::Semaphore { "ControlAuto" };
    auto* http_notify = new RTOS::Semaphore { "HttpNotify" };
    auto* auto_hourly = new RTOS::Semaphore { "AutoHourly" };
    auto* update_hourly_lux_target = new RTOS::Semaphore { "UpdateLux" };

    /// Variables
    auto* latest_measurement_als1 = new RTOS::Variable<LuxMeasurement> { "ALS-1-LatestMeasurement" };
    auto* latest_measurement_als2 = new RTOS::Variable<LuxMeasurement> { "ALS-2-LatestMeasurement" };
    auto* belt_position = new RTOS::Variable<uint8_t> { "BeltPosition" };
    auto* lux_target = new RTOS::Variable<float> { "LuxTarget" };
    auto* motor_command = new RTOS::Variable<Motor::Command> { "MotorAction" };

    /// Tasks
    auto* storage = new Storage({
        .task_name = "Storage",

        .update_lux_target = update_hourly_lux_target,
        .lux_target_auto = auto_hourly,
        .lux_target = lux_target,
        .http_notify = http_notify,
        .rtc = rtc,
    });
    new Logger({ .task_name = "Logger" });
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
    auto* motor = new Motor({
        .name = "Motor",

        .step = Motor::PinStep { 21 },
        .direction = Motor::PinDirection { 20 },
        .limit_cw = Motor::PinLimitCW { 19 },
        .limit_ccw = Motor::PinLimitCCW { 18 },

        .v_command = motor_command,
        .s_control_auto = control_auto,
        .v_belt_position = belt_position,
        .storage = storage,
    });
    auto* http = new HttpServer({
        .port = 80,
        .motor = motor,
        .als1 = latest_measurement_als1,
        .als2 = latest_measurement_als2,
        .notify = http_notify,
        .motor_command = motor_command,
        .lux_target = lux_target,
        .control_auto = control_auto,
    });

    late_main([spi_1, http]() {
        if (cyw43_arch_init() != 0) {
            Logger::Log("cyw43_arch_init() failed");
            return;
        }
        new W5500LWIP(spi_1, SPI::CS(9), W5500::INT(8), W5500::RST(7));
        http->Listen();
    });

    Logger::Log("Semaphores: {}", RTOS::Implementation::Primitive::GetSemaphoreCount());
    Logger::Log("Queues: {}", RTOS::Implementation::Primitive::GetQueueCount());
    Logger::Log("Initializing Scheduler...");
    sleep_us(1); // Ensure RTOS delay functionality
    vTaskStartScheduler();
}
