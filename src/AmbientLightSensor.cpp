#include "AmbientLightSensor.hpp"

#include <cstdio>

#include <FreeRTOS.h>
#include <task.h>

#define CLIOUT printf

AmbientLightSensor::AmbientLightSensor(const std::shared_ptr<PicoW_I2C>& i2c, BH1750::I2CDevAddr i2c_dev_addr)
    : BH1750(i2c, i2c_dev_addr)
{
}

/// TODO: upgrade to evaluate appropriate resolution mode according to previous measurement
void AmbientLightSensor::StartContinuousMeasurement()
{
    SetWaitTime();
    BH1750::SetMode(BH1750::Mode::CONTINUOUS_HIGH_RES);
}

void AmbientLightSensor::StopContinuousMeasurement()
{
    BH1750::SetMode(BH1750::Mode::POWER_DOWN);
}

/// TODO: upgrade to evaluate appropriate resolution mode according to previous measurement
float AmbientLightSensor::ReadLuxBlocking()
{
    switch (BH1750::GetMode()) {
    case BH1750::Mode::POWER_DOWN:
    case BH1750::Mode::POWER_ON:
        SetWaitTime();
        SetMode(BH1750::Mode::ONE_TIME_HIGH_RES);
        break;
    case BH1750::Mode::CONTINUOUS_HIGH_RES:
    case BH1750::Mode::CONTINUOUS_HIGH_RES_2:
    case BH1750::Mode::CONTINUOUS_LOW_RES:
    case BH1750::Mode::ONE_TIME_HIGH_RES:
    case BH1750::Mode::ONE_TIME_HIGH_RES_2:
    case BH1750::Mode::ONE_TIME_LOW_RES:
        SetWaitTime();
        break;
    default:
        CLIOUT("Error: Undefined BH1750 mode #%hhu.\n", BH1750::GetMode());
        return static_cast<float>(RESET_VALUE);
    }
    vTaskDelay(m_measurement_ready_in_ticks);
    return Uint16ToLux(BH1750::ReadMeasurementData());
}

void AmbientLightSensor::SetWaitTime()
{
    auto wait_time = static_cast<uint32_t>(BH1750::GetMeasurementTimeMs());
    if (BH1750::GetMode() == BH1750::Mode::POWER_DOWN || BH1750::GetMode() == BH1750::Mode::POWER_ON) {
        /** Measurement should be waited for for a longer time if measurement starts from a stagnant state.
         * This is to avoid reading too early, assuring that the module has finished writing the new value to the register.
         * The safest wait_time is in the middle of the write for this read and the write for a potential next read,
         * as in: (x1 + x2) / 2 = 1.5 == 3:2
         */
        wait_time *= 3;
        wait_time /= 2;
    }
    m_measurement_ready_in_ticks = pdMS_TO_TICKS(wait_time);
}

float AmbientLightSensor::Uint16ToLux(uint16_t u16) const
{
    if (u16 != BH1750::RESET_VALUE) {
        auto lux = static_cast<float>(u16);
        if (BH1750::GetMeasurementTimeMs() != BH1750::MEASUREMENT_TIME_DEFAULT) {
            const float measurement_time_factor = static_cast<float>(BH1750::MEASUREMENT_TIME_DEFAULT) / static_cast<float>(BH1750::GetMeasurementTimeMs());
            lux *= measurement_time_factor;
        }
        float mode_factor = MODE_FACTOR_HIGH;
        const Mode mode = BH1750::GetMode();
        if (mode == BH1750::Mode::CONTINUOUS_HIGH_RES_2 || mode == BH1750::Mode::ONE_TIME_HIGH_RES_2) {
            mode_factor = MODE_FACTOR_HIGH_2;
        } else if (mode == BH1750::Mode::CONTINUOUS_LOW_RES || mode == BH1750::Mode::ONE_TIME_LOW_RES) {
            mode_factor = MODE_FACTOR_LOW;
        }
        lux *= mode_factor;
        lux /= ACCURACY_FACTOR;
        return lux;
    }
    return BH1750::RESET_VALUE;
}

/// TODO: remove
void test_task(void* params)
{
    auto sda = PicoW_I2C::SDA1Pin::SDA1_26;
    auto scl = PicoW_I2C::SCL1Pin::SCL1_27;
    auto i2c_1 = std::make_shared<PicoW_I2C>(sda, scl, BH1750::BAUDRATE_MAX);
    auto ALS = AmbientLightSensor(i2c_1, BH1750::I2CDevAddr::ADDR_LOW);
    uint m_i = 0;
    ALS.StartContinuousMeasurement();
    while (true) {
        float measurement = ALS.ReadLuxBlocking();
        CLIOUT("Measurement #%u: %f\n", ++m_i, measurement);
    }
}

/// TODO: remove
void test_ALS()
{
    xTaskCreate(test_task,
        "ALS",
        DEFAULT_TASK_STACK_SIZE,
        nullptr,
        tskIDLE_PRIORITY + 3,
        nullptr);
}
