#include "AmbientLightSensor.hpp"

#include <cstdio>

#include "FreeRTOS.h"
#include "task.h"

#define DELAY_MS sleep_ms
#define GET_TIME_MS (get_absolute_time() / 1000)
#define DELAY_UNTIL_ABS(abs) sleep_until(abs)
#define CLIOUT printf

AmbientLightSensor::AmbientLightSensor(const std::shared_ptr<PicoW_I2C>& i2c, BH1750::I2C_dev_addr i2c_dev_addr)
    : BH1750(i2c, i2c_dev_addr)
{
}

void AmbientLightSensor::StartContinuousMeasurement()
{
    SetWaitTime();
    /// TODO: upgrade to evaluate appropriate resolution mode according to previous measurement
    auto const new_mode = CONTINUOUS_HIGH_RES;
    BH1750::SetMode(new_mode);
}

void AmbientLightSensor::StopContinuousMeasurement()
{
    BH1750::SetMode(POWER_DOWN);
}

float AmbientLightSensor::ReadLuxBlocking()
{
    float lux = BH1750::RESET_VALUE;
    switch (BH1750::GetMode()) {
    case POWER_DOWN:
    case POWER_ON:
        /// TODO: upgrade to evaluate appropriate resolution mode according to previous measurement
        SetWaitTime();
        SetMode(ONE_TIME_HIGH_RES);
        DELAY_UNTIL_ABS(m_measurement_ready_at_ms);
        lux = BH1750::ReadLuxData();
        break;
    case CONTINUOUS_HIGH_RES:
    case CONTINUOUS_HIGH_RES_2:
    case CONTINUOUS_LOW_RES:
    case ONE_TIME_HIGH_RES:
    case ONE_TIME_HIGH_RES_2:
    case ONE_TIME_LOW_RES:
        SetWaitTime();
        DELAY_UNTIL_ABS(m_measurement_ready_at_ms);
        lux = BH1750::ReadLuxData();
        break;
    default:
        CLIOUT("Error: Undefined BH1750 mode #%hhu.\n", BH1750::GetMode());
    }
    return lux;
}

void AmbientLightSensor::SetWaitTime()
{
    auto wait_time = static_cast<uint32_t>(BH1750::GetMeasurementTimeMs());
    // Measurement should be waited for for a longer time
    // if measurement starts from a stagnant state.
    if (BH1750::GetMode() == POWER_DOWN || BH1750::GetMode() == POWER_ON) {
        // measurement time =* 1.5
        wait_time *= 3;
        wait_time /= 2;
    }
    m_measurement_ready_at_ms = make_timeout_time_ms(wait_time);
}

void test_task(void* params)
{
    auto sda = PicoW_I2C::sda1_pin::SDA1_26;
    auto scl = PicoW_I2C::scl1_pin::SCL1_27;
    auto i2c_1 = std::make_shared<PicoW_I2C>(sda, scl, BH1750::BAUDRATE_MAX);
    auto ALS = AmbientLightSensor(i2c_1, BH1750::ADDR_LOW);
    uint m_i = 0;
    ALS.StartContinuousMeasurement();
    while (true) {
        float measurement = ALS.ReadLuxBlocking();
        printf("Measurement #%u: %f\n", ++m_i, measurement);
    }
}

void test_ALS()
{
    xTaskCreate(test_task,
        "ALS",
        DEFAULT_TASK_STACK_SIZE,
        nullptr,
        tskIDLE_PRIORITY + 3,
        nullptr);
}
