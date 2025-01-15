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
        MediateMeasurementTime();
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
        MediateMeasurementTime();
    }
    uint16_t measurement_data = BH1750::RESET_VALUE;
    if (!BH1750::ReadMeasurementData(&measurement_data)) {
        CLIOUT("Warning: Failed to read indoor/outdoor sensor.\n");
        return false;
    }
    xTaskDelayUntil(&m_measurement_started_at_ticks, GetMeasurementTimeTicks());
    *lux = Uint16ToLux(measurement_data);
    return true;
}

/** Measurement should be waited for for a longer time if measurement starts from a stagnant state.
 * This is to avoid reading too early, assuring that the module has finished writing the new value to the register.
 * The safest wait time is in the middle of the expected write time for this read and the expected write time for a potential next read,
 * as in: (x1 + x2) / 2 = 1.5 x set measurement time
 */
void AmbientLightSensor::MediateMeasurementTime()
{
    vTaskDelay(GetMeasurementTimeTicks() / 2);
    m_measurement_started_at_ticks = xTaskGetTickCount();
}

TickType_t AmbientLightSensor::GetMeasurementTimeTicks() const
{
    return pdMS_TO_TICKS(BH1750::GetMeasurementTimeMs());
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
    const int cont_reads = 10;
    while (true) {

        uint64_t start = time_us_64();
        ALS.ReadLuxBlocking(&measurement);
        uint64_t stop = time_us_64();
        CLIOUT("One time read #%u: %f in %llu us ; should be ~ %d ms\n", ++m_i, measurement, stop - start, 69 + 69 / 2);

        ALS.StartContinuousMeasurement();
        start = time_us_64();

        CLIOUT("%d continuous reads\n", cont_reads);
        for (int cont_read_i = 0; cont_read_i < cont_reads; ++cont_read_i) {
            if (ALS.ReadLuxBlocking(&measurement)) {
                stop = time_us_64();
                CLIOUT("Cont read #%u: %f in %llu us ; should be ~ %d ms\n", ++m_i, measurement, stop - start, 69);
                start = stop;
            } else {
                CLIOUT("Warning: Failed to receive lux measurement.\n");
            }
        }

        ALS.StopContinuousMeasurement();
    }
}

/// TODO: remove
void test_ALS()
{
    // Issue: Delaying a task for less than 100 ms before the RP2040 has ran some small amount of time.
    // Solution: Sleeping for 1 us before calling vTaskDelay resolves this issue.
    // Theory: *Maybe* TaskDelay does some math internally that requires the hardware timer to be big enough to proceed
    //         and simply returns without delaying if hardware timer < 0 us at the moment.
    //         This is how it seems to behave.
    sleep_us(1);
    /// TODO: if this is the case, this should be done not just for ALS but for all tasks and thus by some dedicated operation.

    xTaskCreate(test_task,
        "ALS",
        DEFAULT_TASK_STACK_SIZE,
        nullptr,
        tskIDLE_PRIORITY + 3,
        nullptr);
}
