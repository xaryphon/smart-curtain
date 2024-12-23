#include "AmbientLightSensor.hpp"

#include <cstdio>

#include <FreeRTOS.h>
#include <task.h>

#define CLIOUT printf

AmbientLightSensor::AmbientLightSensor(PicoW_I2C* i2c, BH1750::I2CDevAddr i2c_dev_addr)
    : BH1750(i2c, i2c_dev_addr)
{
}

/// TODO: upgrade to evaluate appropriate resolution mode according to previous measurement
void AmbientLightSensor::StartContinuousMeasurement()
{
    const BH1750::Mode current_mode = BH1750::GetMode();
    BH1750::SetMode(BH1750::Mode::CONTINUOUS_HIGH_RES);
    if (current_mode == BH1750::Mode::POWER_DOWN || current_mode == BH1750::Mode::POWER_ON) {
        m_measurement_ready_in_ticks = GetMediatedTimeTicks();
    }
}

void AmbientLightSensor::StopContinuousMeasurement()
{
    BH1750::SetMode(BH1750::Mode::POWER_DOWN);
}

/// TODO: upgrade to evaluate appropriate resolution mode according to previous measurement
bool AmbientLightSensor::ReadLuxBlocking(float* lux)
{
    const BH1750::Mode current_mode = BH1750::GetMode();
    if (current_mode == BH1750::Mode::POWER_DOWN || current_mode == BH1750::Mode::POWER_ON) {
        SetMode(BH1750::Mode::ONE_TIME_HIGH_RES);
        m_measurement_ready_in_ticks = GetMediatedTimeTicks();
    }
    if (!ReadMeasurementData(&m_measurement_data)) {
        CLIOUT("Warning: Failed to read indoor/outdoor sensor.");
        return false;
    }
    vTaskDelay(m_measurement_ready_in_ticks);
    m_measurement_ready_in_ticks = pdMS_TO_TICKS(BH1750::GetMeasurementTimeMs());
    *lux = Uint16ToLux(m_measurement_data);
    return true;
}

/** Measurement should be waited for for a longer time if measurement starts from a stagnant state.
 * This is to avoid reading too early, assuring that the module has finished writing the new value to the register.
 * The safest wait time is in the middle of the expected write time for this read and the expected write time for a potential next read,
 * as in: (x1 + x2) / 2 = 1.5 x set measurement time
 */
TickType_t AmbientLightSensor::GetMediatedTimeTicks() const
{
    return pdMS_TO_TICKS(static_cast<uint32_t>(BH1750::GetMeasurementTimeMs()) * 3 / 2);
}

float AmbientLightSensor::Uint16ToLux(uint16_t u16) const
{
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

/// TODO: remove
void test_task(void* params)
{
    auto sda = PicoW_I2C::SDA1Pin::SDA1_26;
    auto scl = PicoW_I2C::SCL1Pin::SCL1_27;
    auto i2c_1 = std::make_unique<PicoW_I2C>(sda, scl, BH1750::BAUDRATE_MAX);
    auto ALS = AmbientLightSensor(i2c_1.get(), BH1750::I2CDevAddr::ADDR_LOW);
    uint m_i = 0;
    float measurement = 0;
    ALS.StartContinuousMeasurement();
    while (true) {
        if (ALS.ReadLuxBlocking(&measurement)) {
            CLIOUT("Measurement #%u: %f\n", ++m_i, measurement);
        } else {
            CLIOUT("Warning: Failed to receive lux measurement.");
        }
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
