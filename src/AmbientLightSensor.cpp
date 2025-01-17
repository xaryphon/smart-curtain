#include "AmbientLightSensor.hpp"

#include <cstdio>

#include <FreeRTOS.h>
#include <task.h>

#define CLIOUT printf

AmbientLightSensor::AmbientLightSensor(PicoW_I2C* i2c, BH1750::I2CDevAddr i2c_dev_addr)
    : BH1750(i2c, i2c_dev_addr)
{
}

bool AmbientLightSensor::ReadLuxBlocking(float* lux)
{
    switch (BH1750::GetMode()) {
    case POWER_DOWN:
    case POWER_ON:
    case ONE_TIME_MEDIUM:
    case ONE_TIME_HIGH:
    case ONE_TIME_LOW:
        AdjustMeasurementSettings(MeasurementType::ONE_TIME);
        break;
    case CONTINUOUS_MEDIUM:
    case CONTINUOUS_HIGH:
    case CONTINUOUS_LOW:
        AdjustMeasurementSettings(MeasurementType::CONTINUOUS);
        break;
    }
    WaitForMeasurement();
    uint16_t measurement_data = BH1750::RESET_VALUE;
    if (!BH1750::ReadMeasurementData(&measurement_data)) {
        CLIOUT("Warning: Failed to read indoor/outdoor sensor.\n");
        return false;
    }
    *lux = Uint16ToLux(measurement_data);
    m_previous_measurement = *lux;
    return true;
}

/* BH1750 Datasheets:
 * Min detectable lux:       0.11 lux (presumably with HIGH_RES_2)
 * Max detectable lux: 100 000    lux (presumably with LOW_RES)
 *
 * Mathematical maximums with default measurement time reference:
 * HIGH_RES_2: 0b 1111 1111 1111 1111 / 1.2 * 0.5 =  27306,25 lu
 * HIGH_RES  : 0b 1111 1111 1111 1111 / 1.2       =  54612,5  lux
 * LOW_RES   : 0b 1111 1111 1111 1111 / 1.2 * 4   = 218450    lux
 *
 * Expected LOW_RES max, according to datasheets:
 * ~ 100 000 lux / 4 / 1.2 = ~ 20833 = 0b 0101 0001 0110 0001 u16
 *
 * If this u16 is a physical limitation of the ADC or some other part of the measuring instrument:
 * Expected HIGH_RES_2 max:
 * 0b 0101 0001 0110 0001 / 1.2 * 0.5 =  8680,4166... lux
 * Expected HIGH_RES max:
 * 0b 0101 0001 0110 0001 / 1.2       = 17360,833...  lux
 *
 * The measurement ought to be adjusted earlier rather than later,
 * as the three resolutions offer quite a bit of overlap.
 * Divide these expected maximums to half:
 * HIGH_2_UPPER_LIMIT =  8680,4166... / 2 = 4340,20833... lux
 * HIGH_UPPER_LIMIT   = 17360,833...  / 2 = 8680,4166...  lux
 *
 * (dark)   HIGH_2:   0 < prev_measurement < ~4340
 * (medium) HIGH: ~4340 < prev_measurement < ~8680
 * (bright) LOW:  ~8680 < prev_measurement
 */
void AmbientLightSensor::AdjustMeasurementSettings(AmbientLightSensor::MeasurementType measurement_type)
{
    static const uint16_t EXPECTED_MAX_RAW_READ = 0b0101000101100001;
    static const auto ACCURACY_FACTOR = static_cast<float>(BH1750::ACCURACY_FACTOR);
    static const auto MODE_FACTOR_HIGH = static_cast<float>(BH1750::MODE_FACTOR_HIGH);
    static const auto MODE_FACTOR_MEDIUM = static_cast<float>(BH1750::MODE_FACTOR_MEDIUM);

    static const float MAX_HIGH = EXPECTED_MAX_RAW_READ / ACCURACY_FACTOR * MODE_FACTOR_HIGH / 2;
    static const float MAX_MEDIUM = EXPECTED_MAX_RAW_READ / ACCURACY_FACTOR * MODE_FACTOR_MEDIUM / 2;

    m_resolution = MeasurementResolution::LOW;
    if (m_previous_measurement < MAX_HIGH) {
        m_resolution = MeasurementResolution::HIGH;
    } else if (m_previous_measurement < MAX_MEDIUM) {
        m_resolution = MeasurementResolution::MEDIUM;
    }
    const auto new_mode = static_cast<BH1750::Mode>(measurement_type | m_resolution);
    switch (new_mode) {
    case BH1750::Mode::CONTINUOUS_MEDIUM:
    case BH1750::Mode::CONTINUOUS_HIGH:
    case BH1750::Mode::CONTINUOUS_LOW:
    case BH1750::Mode::ONE_TIME_MEDIUM:
    case BH1750::Mode::ONE_TIME_HIGH:
    case BH1750::Mode::ONE_TIME_LOW:
        break;
    default:
        CLIOUT("Error: Mode command %hhu | %hhu unfit for measuring\n", measurement_type, m_resolution);
        return;
    }
    const BH1750::Mode prev_mode = BH1750::GetMode();
    if (!BH1750::SetMode(new_mode)) {
        CLIOUT("Warning: Failed to set ALS mode to %hhu\n", new_mode);
        return;
    }
    if (prev_mode == BH1750::Mode::POWER_DOWN || prev_mode == BH1750::Mode::POWER_ON) {
        MediateMeasurementTime();
    }
}

void AmbientLightSensor::WaitForMeasurement()
{
    const TickType_t now = xTaskGetTickCount();
    if (m_measurement_started_at_ticks > now) {
        vTaskDelay(m_measurement_started_at_ticks - xTaskGetTickCount());
    }
    xTaskDelayUntil(&m_measurement_started_at_ticks, GetMeasurementTimeTicks());
}

float AmbientLightSensor::Uint16ToLux(uint16_t u16) const
{
    auto lux = static_cast<float>(u16);
    if (BH1750::GetMeasurementTimeReferenceMs() != BH1750::MEASUREMENT_TIME_REFERENCE_DEFAULT_MS) {
        const float measurement_time_factor = MEASUREMENT_TIME_REFERENCE_DEFAULT_FLOAT / static_cast<float>(BH1750::GetMeasurementTimeReferenceMs());
        lux *= measurement_time_factor;
    }
    float mode_factor = MODE_FACTOR_MEDIUM;
    const Mode mode = BH1750::GetMode();
    if (mode == BH1750::Mode::CONTINUOUS_HIGH || mode == BH1750::Mode::ONE_TIME_HIGH) {
        mode_factor = MODE_FACTOR_HIGH;
    } else if (mode == BH1750::Mode::CONTINUOUS_LOW || mode == BH1750::Mode::ONE_TIME_LOW) {
        mode_factor = MODE_FACTOR_LOW;
    }
    lux *= mode_factor;
    lux /= ACCURACY_FACTOR;
    return lux;
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
    m_measurement_started_at_ticks = xTaskGetTickCount() + GetMeasurementTimeTicks() / 2;
}

TickType_t AmbientLightSensor::GetMeasurementTimeTicks() const
{
    const float measurement_time_factor = MEASUREMENT_TIME_REFERENCE_DEFAULT_FLOAT / static_cast<float>(BH1750::GetMeasurementTimeReferenceMs());
    switch (m_resolution) {
    case MEDIUM:
        return static_cast<TickType_t>(MEASUREMENT_TIME_TYPICAL_TICKS_RES_MEDIUM * measurement_time_factor);
    case HIGH:
        return static_cast<TickType_t>(MEASUREMENT_TIME_TYPICAL_TICKS_RES_HIGH * measurement_time_factor);
    case LOW:
        return static_cast<TickType_t>(MEASUREMENT_TIME_TYPICAL_TICKS_RES_LOW * measurement_time_factor);
    default:
        return static_cast<TickType_t>(MEASUREMENT_TIME_TYPICAL_TICKS_RES_MEDIUM * measurement_time_factor);
    }
}

void AmbientLightSensor::StartContinuousMeasurement()
{
    AdjustMeasurementSettings(AmbientLightSensor::CONTINUOUS);
}

void AmbientLightSensor::StopContinuousMeasurement()
{
    if (!BH1750::SetMode(BH1750::Mode::POWER_DOWN)) {
        CLIOUT("Warning: Failed to stop continuous measurement mode\n");
    }
}

void AmbientLightSensor::ResetMeasurement()
{
    if (!BH1750::Reset()) {
        CLIOUT("Warning: Failed to reset measurement\n");
    }
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
