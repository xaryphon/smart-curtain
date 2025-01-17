#include "AmbientLightSensor.hpp"

#include <cstdio>

#include <FreeRTOS.h>
#include <task.h>

#define CLIOUT printf

AmbientLightSensor::AmbientLightSensor(PicoW_I2C* i2c, BH1750::I2CDevAddr i2c_dev_addr)
    : BH1750(i2c, i2c_dev_addr)
{
}

void AmbientLightSensor::StartContinuousMeasurement()
{
    AdjustMeasurementSettings(AmbientLightSensor::CONTINUOUS);
}

void AmbientLightSensor::StopContinuousMeasurement()
{
    BH1750::SetMode(BH1750::Mode::POWER_DOWN);
}

bool AmbientLightSensor::ReadLuxBlocking(float* lux)
{
    switch (BH1750::GetMode()) {
    case POWER_DOWN:
    case POWER_ON:
    case ONE_TIME_HIGH_RES:
    case ONE_TIME_HIGH_RES_2:
    case ONE_TIME_LOW_RES:
        AdjustMeasurementSettings(MeasurementType::ONE_TIME);
        break;
    case CONTINUOUS_HIGH_RES:
    case CONTINUOUS_HIGH_RES_2:
    case CONTINUOUS_LOW_RES:
        AdjustMeasurementSettings(MeasurementType::CONTINUOUS);
        break;
    }
    uint16_t measurement_data = BH1750::RESET_VALUE;
    if (!BH1750::ReadMeasurementData(&measurement_data)) {
        CLIOUT("Warning: Failed to read indoor/outdoor sensor.\n");
        return false;
    }
    xTaskDelayUntil(&m_measurement_started_at_ticks, GetMeasurementTimeTicks());
    *lux = Uint16ToLux(measurement_data);
    m_previous_measurement = *lux;
    return true;
}

/* Measurement should be waited for for a longer time
 * if measurement starts from a stagnant state.
 * This is to avoid reading too early,
 * assuring that the module has finished writing the new value to the register.
 * The safest wait time is in the middle of the expected write time for this read
 * and the expected write time for a potential next read,
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
        const float measurement_time_factor = MEASUREMENT_TIME_DEFAULT / static_cast<float>(BH1750::GetMeasurementTimeMs());
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

/* BH1750 Datasheets:
 * Min detectable lux:       0.11 lux (presumably with HIGH_RES_2)
 * Max detectable lux: 100 000    lux (presumably with LOW_RES)
 *
 * Mathematical Maximums
 * HIGH_RES_2: 0b 1111 1111 1111 1111 / 1.2 * (69 / 120) * 0.5 =  15701,09375
 * HIGH_RES  : 0b 1111 1111 1111 1111 / 1.2 * (69 / 120)       =  31402,1875
 * LOW_RES   : 0b 1111 1111 1111 1111 / 1.2 * (69 / 16)  * 4   = 942065,625
 *
 * Expected LOW_RES max:
 * 100 000 lux / 4 / (69 / 16) * 1.2 = ~ 0b 0001 1011 0010 1100
 *
 * Thus, with that same u16,
 * Expected HIGH_RES max:
 * 0b 0001 1011 0010 1100 / 1.2 * (69 / 120)       = 3333,0833...
 * Expected HIGH_RES_2 max:
 * 0b 0001 1011 0010 1100 / 1.2 * (69 / 120) * 0.5 = 1666,54166...
 *
 * The measurement ought to be adjusted early enough, so I propose we divide these "maximums" to half:
 * HIGH_RES_2 (dark) for when
 *                 previous measurement < 1666,54166... / 2 ==  833,270833...
 * HIGH_RES (medium) for when
 * 833,270833... < previous measurement < 3333,0833...  / 2 == 1666,54166...
 * LOW_RES  (bright) for when
 * 1666,54166... < previous measurement
 */
void AmbientLightSensor::AdjustMeasurementSettings(AmbientLightSensor::MeasurementType measurement_type)
{
    static const uint16_t EXPECTED_MAX_RAW_READ = 0b0001101100101100;
    static const auto ACCURACY_FACTOR = static_cast<float>(BH1750::ACCURACY_FACTOR);
    static const auto MT_TYP_HIGH_2 = static_cast<float>(MEASUREMENT_TIME_TYPICAL_MS_HIGH_RES_2);
    static const auto MT_TYP_HIGH = static_cast<float>(MEASUREMENT_TIME_TYPICAL_MS_HIGH_RES);
    static const auto MODE_FACTOR_HIGH_2 = static_cast<float>(BH1750::MODE_FACTOR_HIGH_2);
    static const auto MODE_FACTOR_HIGH = static_cast<float>(BH1750::MODE_FACTOR_HIGH);

    static const float MAX_HIGH_2 = EXPECTED_MAX_RAW_READ / ACCURACY_FACTOR * (MEASUREMENT_TIME_DEFAULT / MT_TYP_HIGH_2) * MODE_FACTOR_HIGH_2 / 2;
    static const float MAX_HIGH = EXPECTED_MAX_RAW_READ / ACCURACY_FACTOR * (MEASUREMENT_TIME_DEFAULT / MT_TYP_HIGH) * MODE_FACTOR_HIGH / 2;

    MeasurementResolution new_resolution = MeasurementResolution::LOW;
    uint8_t new_measurement_time = MEASUREMENT_TIME_TYPICAL_MS_LOW_RES;
    if (m_previous_measurement < MAX_HIGH_2) {
        new_resolution = MeasurementResolution::HIGH_2;
        new_measurement_time = MEASUREMENT_TIME_TYPICAL_MS_HIGH_RES_2;
    } else if (m_previous_measurement < MAX_HIGH) {
        new_resolution = MeasurementResolution::HIGH;
        new_measurement_time = MEASUREMENT_TIME_TYPICAL_MS_HIGH_RES;
    }
    const auto new_mode = static_cast<BH1750::Mode>(measurement_type | new_resolution);
    switch (new_mode) {
    case BH1750::Mode::CONTINUOUS_HIGH_RES:
    case BH1750::Mode::CONTINUOUS_HIGH_RES_2:
    case BH1750::Mode::CONTINUOUS_LOW_RES:
    case BH1750::Mode::ONE_TIME_HIGH_RES:
    case BH1750::Mode::ONE_TIME_HIGH_RES_2:
    case BH1750::Mode::ONE_TIME_LOW_RES:
        break;
    default:
        CLIOUT("Error: Mode command %hhu | %hhu unfit for measuring\n", measurement_type, new_resolution);
        return;
    }
    const BH1750::Mode prev_mode = BH1750::GetMode();
    if (!BH1750::SetMode(new_mode)) {
        CLIOUT("Warning: Failed to set ALS mode to %hhu\n", new_mode);
        return;
    }
    if (!BH1750::SetMeasurementTimeMS(new_measurement_time)) {
        CLIOUT("Warning: Failed to adjust measurement time to %hhu\n", new_measurement_time);
        return;
    }
    if (prev_mode == BH1750::Mode::POWER_DOWN ||
        prev_mode == BH1750::Mode::POWER_ON) {
        MediateMeasurementTime();
    }
}

void AmbientLightSensor::ResetMeasurement()
{
    BH1750::Reset();
}

/// TODO: remove
void test_task(void* params)
{
    (void)params; // apparently one way to shut up lint
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
        CLIOUT("One time read #%u: %f in %llu us\n", ++m_i, measurement, stop - start);

        ALS.StartContinuousMeasurement();
        start = time_us_64();

        CLIOUT("%d continuous reads\n", cont_reads);
        for (int cont_read_i = 0; cont_read_i < cont_reads; ++cont_read_i) {
            if (ALS.ReadLuxBlocking(&measurement)) {
                stop = time_us_64();
                CLIOUT("Cont read #%u: %f in %llu us\n", ++m_i, measurement, stop - start);
                start = stop;
            } else {
                CLIOUT("Warning: Failed to receive lux measurement.\n");
            }
        }
        ALS.StopContinuousMeasurement();
        ALS.ResetMeasurement();
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
