#include "AmbientLightSensor.hpp"

#include "Logger.hpp"
#include "config.h"

AmbientLightSensor::AmbientLightSensor(const char * task_name, uint32_t stack_depth, BaseType_t task_priority, PicoW_I2C* i2c, BH1750::I2CDevAddr i2c_dev_addr)
    : BH1750(i2c, i2c_dev_addr)
{
    // Issue: Delaying a task for less than 100 ms before the RP2040 has ran some small amount of time.
    // Solution: Sleeping for 1 us before calling vTaskDelay resolves this issue.
    // Theory: *Maybe* TaskDelay does some math internally that requires the hardware timer to be big enough to proceed
    //         and simply returns without delaying if hardware timer < 0 us at the moment.
    //         This is how it seems to behave.
    sleep_us(1);
    /// TODO: if this is the case, this should be done not just for ALS but for all tasks and thus by some dedicated operation.

    if (xTaskCreate(TASK_KONDOM(AmbientLightSensor, Task),
                task_name,
                stack_depth,
                static_cast<void *>(this),
                tskIDLE_PRIORITY + task_priority,
                &m_task_handle) == pdTRUE) {
        Logger::Log("Created task [{}]", task_name);
    } else {
        Logger::Log("Error: Failed to create task [{}]", task_name);
    }
}

void AmbientLightSensor::Initialize()
{
    BH1750::SetMeasurementTimeReference(BH1750::MEASUREMENT_TIME_REFERENCE_DEFAULT_MS);
    BH1750::SetMode(BH1750::Mode::POWER_DOWN);
    BH1750::Reset();
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
        Logger::Log("Warning: Failed to read indoor/outdoor sensor.");
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
        Logger::Log("Error: Mode command {} | {} unfit for measuring",
            static_cast<uint8_t>(measurement_type), static_cast<uint8_t>(m_resolution));
        return;
    }
    const BH1750::Mode prev_mode = BH1750::GetMode();
    if (!BH1750::SetMode(new_mode)) {
        Logger::Log("Warning: Failed to set ALS mode to {}", BH1750::MODE_STR[new_mode]);
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
    if (m_resolution == MeasurementResolution::HIGH) {
        mode_factor = MODE_FACTOR_HIGH;
    } else if (m_resolution == MeasurementResolution::LOW) {
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
    switch (m_resolution) {
    case MEDIUM:
        return static_cast<TickType_t>(MEASUREMENT_TIME_TYPICAL_TICKS_RES_MEDIUM * m_measurement_time_reference_factor);
    case HIGH:
        return static_cast<TickType_t>(MEASUREMENT_TIME_TYPICAL_TICKS_RES_HIGH * m_measurement_time_reference_factor);
    case LOW:
        return static_cast<TickType_t>(MEASUREMENT_TIME_TYPICAL_TICKS_RES_LOW * m_measurement_time_reference_factor);
    default:
        return static_cast<TickType_t>(MEASUREMENT_TIME_TYPICAL_TICKS_RES_MEDIUM * m_measurement_time_reference_factor);
    }
}

void AmbientLightSensor::StartContinuousMeasurement()
{
    AdjustMeasurementSettings(AmbientLightSensor::CONTINUOUS);
}

void AmbientLightSensor::StopContinuousMeasurement()
{
    if (!BH1750::SetMode(BH1750::Mode::POWER_DOWN)) {
        Logger::Log("Warning: Failed to stop continuous measurement mode");
    }
}

void AmbientLightSensor::ResetMeasurement()
{
    if (!BH1750::Reset()) {
        Logger::Log("Warning: Failed to reset measurement");
    }
}

void AmbientLightSensor::SetMeasurementTimeFactor(float factor)
{
    if (factor == m_measurement_time_reference_factor) {
        return;
    }
    if (factor < 0) {
        Logger::Log("Error: Cannot set measurement time reference to negative; factor: {}", factor);
        return;
    }
    const auto mt_ref_ms = static_cast<uint32_t>(MEASUREMENT_TIME_REFERENCE_DEFAULT_FLOAT * factor);
    if (!BH1750::SetMeasurementTimeReference(mt_ref_ms)) {
        return;
    }
    m_measurement_time_reference_factor = factor;
    Logger::Log("Measurement Time multiplied to x{}", m_measurement_time_reference_factor);
}

void AmbientLightSensor::Task()
{
    Initialize();

    uint m_i = 0;
    float measurement = 0;
    const int cont_reads = 10;
    float measurement_time_factor = 0.1;

    while (true) {

        uint64_t start = time_us_64();
        ReadLuxBlocking(&measurement);
        uint64_t stop = time_us_64();
        Logger::Log("One time read #{}: {} in {} us", ++m_i, measurement, stop - start);

        StartContinuousMeasurement();
        start = time_us_64();

        Logger::Log("{} continuous reads", cont_reads);
        for (int cont_read_i = 0; cont_read_i < cont_reads; ++cont_read_i) {
            if (ReadLuxBlocking(&measurement)) {
                stop = time_us_64();
                Logger::Log("Cont read #{}: {} in {} us", ++m_i, measurement, stop - start);
                start = stop;
            } else {
                Logger::Log("Warning: Failed to receive lux measurement");
            }
        }
        StopContinuousMeasurement();
        ResetMeasurement();
        SetMeasurementTimeFactor(measurement_time_factor);
        measurement_time_factor += 0.1;
        if (measurement_time_factor >= 4) {
            measurement_time_factor = 0.1;
        }

    }
}
