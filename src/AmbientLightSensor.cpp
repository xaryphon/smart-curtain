#include "AmbientLightSensor.hpp"

#include <cmath>

#include "Logger.hpp"
#include "config.h"

AmbientLightSensor::AmbientLightSensor(const AmbientLightSensor::Parameters& parameters)
    : BH1750(parameters.i2c, parameters.BH1750_i2c_address)
    , m_v_lux_latest_my(parameters.v_lux_latest_my)
    , m_v_lux_latest_other(parameters.v_lux_latest_other)
    , m_v_lux_target(parameters.v_lux_target)
    , m_v_motor_command(parameters.v_motor_command)
    , m_s_control_auto(parameters.s_control_auto)
{
    // Issue: Delaying a task for less than 100 ms before the RP2040 has ran some small amount of time
    //        reduces the delay(s) unreliable.
    // Solution: Sleeping for 1 us before calling vTaskDelay resolves this issue.
    // Theory: *Maybe* TaskDelay does some math internally that requires the hardware timer to be big enough to proceed
    //         and simply returns without delaying if hardware timer < 0 us at the moment.
    //         This is how it seems to behave.
    sleep_us(1);
    /// TODO: if this is the case, this should be done not just for ALS but for all tasks and thus by some dedicated operation.

    if (xTaskCreate(TASK_KONDOM(AmbientLightSensor, Task),
            parameters.name,
            DEFAULT_TASK_STACK_SIZE * 3,
            static_cast<void*>(this),
            tskIDLE_PRIORITY + 3,
            &m_task_handle)
        == pdTRUE) {
        Logger::Log("Created task [{}]", parameters.name);
    } else {
        Logger::Log("Error: Failed to create task [{}]", parameters.name);
    }
}

/* Resolution nomenclature:
 * Datasheet = Translation
 *    HIGH_2 = HIGH
 *      HIGH = MEDIUM
 *       LOW = LOW
 *
 * BH1750 Datasheets:
 * Min detectable lux:       0.11 lux (presumably with HIGH)
 * Max detectable lux: 100 000    lux (presumably with LOW)
 *
 * Mathematical maximums with default measurement time reference:
 * HIGH:   0b 1111 1111 1111 1111 / 1.2 * 0.5 =  27306,25 lux
 * MEDIUM: 0b 1111 1111 1111 1111 / 1.2       =  54612,5  lux
 * LOW:    0b 1111 1111 1111 1111 / 1.2 * 4   = 218450    lux
 *
 * Expected LOW max, according to datasheets:
 * ~ 100 000 lux / 4 / 1.2 = ~ 20833 = 0b 0101 0001 0110 0001 u16
 *
 * If this u16 is a physical limitation of the ADC or some other part of the measuring instrument:
 * Expected HIGH max:
 * 0b 0101 0001 0110 0001 / 1.2 * 0.5 =  8680,4166... lux
 * Expected MEDIUM max:
 * 0b 0101 0001 0110 0001 / 1.2       = 17360,833...  lux
 *
 * The measurement ought to be adjusted earlier rather than later,
 * as the three resolutions offer quite a bit of overlap.
 * Divide these expected maximums to half:
 * HIGH_BOUNDARY   =  8680,4166... / 2 = 4340,20833... lux
 * MEDIUM_BOUNDARY = 17360,833...  / 2 = 8680,4166...  lux
 *
 * (dark)   HIGH:       0 < prev_measurement < ~4340
 * (medium) MEDIUM: ~4340 < prev_measurement < ~8680
 * (bright) LOW:    ~8680 < prev_measurement
 */
bool AmbientLightSensor::AdjustMeasurementResolution()
{
    static const uint16_t EXPECTED_MAX_RAW_READ = 0b0101000101100001;
    static const auto ACCURACY_FACTOR = static_cast<float>(BH1750::ACCURACY_FACTOR);
    static const auto MODE_FACTOR_HIGH = static_cast<float>(BH1750::MODE_FACTOR_HIGH);
    static const auto MODE_FACTOR_MEDIUM = static_cast<float>(BH1750::MODE_FACTOR_MEDIUM);

    static const float MAX_HIGH = EXPECTED_MAX_RAW_READ / ACCURACY_FACTOR * MODE_FACTOR_HIGH / 2;
    static const float MAX_MEDIUM = EXPECTED_MAX_RAW_READ / ACCURACY_FACTOR * MODE_FACTOR_MEDIUM / 2;

    auto new_mode = BH1750::Mode::CONTINUOUS_LOW;
    if (m_lux_latest < MAX_HIGH) {
        new_mode = BH1750::Mode::CONTINUOUS_HIGH;
    } else if (m_lux_latest < MAX_MEDIUM) {
        new_mode = BH1750::Mode::CONTINUOUS_MEDIUM;
    }
    const BH1750::Mode prev_mode = BH1750::GetMode();
    if (new_mode == prev_mode) {
        return false;
    }
    if (!BH1750::SetMode(new_mode)) {
        return false;
    }
    m_measurement_mode = new_mode;
    m_measurement_time = GetMeasurementTimeTicks();
    if (prev_mode == BH1750::Mode::POWER_DOWN || prev_mode == BH1750::Mode::POWER_ON) {
        MediateMeasurementTime();
    }
    return true;
}

TickType_t AmbientLightSensor::GetMeasurementTimeTicks() const
{
    switch (m_measurement_mode) {
    case BH1750::Mode::CONTINUOUS_MEDIUM:
        return static_cast<TickType_t>(MEASUREMENT_TIME_TYPICAL_TICKS_RES_MEDIUM * m_measurement_time_reference_factor);
    case BH1750::Mode::CONTINUOUS_HIGH:
        return static_cast<TickType_t>(MEASUREMENT_TIME_TYPICAL_TICKS_RES_HIGH * m_measurement_time_reference_factor);
    case BH1750::Mode::CONTINUOUS_LOW:
        return static_cast<TickType_t>(MEASUREMENT_TIME_TYPICAL_TICKS_RES_LOW * m_measurement_time_reference_factor);
    default:
        Logger::Log("Error: Called GetMeasurementTimeTicks even though not measuring.");
        return static_cast<TickType_t>(MEASUREMENT_TIME_TYPICAL_TICKS_RES_MEDIUM * m_measurement_time_reference_factor);
    }
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
    m_measurement_started_at_ticks = xTaskGetTickCount();
    m_measurement_time_mediation = m_measurement_time / 2;
}

bool AmbientLightSensor::ReadLuxBlocking()
{
    WaitForMeasurement();
    uint16_t measurement_data = BH1750::RESET_VALUE;
    if (!BH1750::ReadMeasurementData(&measurement_data)) {
        return false;
    }
    m_lux_latest = Uint16ToLux(measurement_data);
    return true;
}

void AmbientLightSensor::WaitForMeasurement()
{
    xTaskDelayUntil(&m_measurement_started_at_ticks, m_measurement_time + m_measurement_time_mediation);
    m_measurement_time_mediation = 0;
}

float AmbientLightSensor::Uint16ToLux(uint16_t u16) const
{
    auto lux = static_cast<float>(u16);
    if (BH1750::GetMeasurementTimeReferenceMs() != BH1750::MEASUREMENT_TIME_REFERENCE_DEFAULT_MS) {
        const float measurement_time_factor = MEASUREMENT_TIME_REFERENCE_DEFAULT_FLOAT / static_cast<float>(BH1750::GetMeasurementTimeReferenceMs());
        lux *= measurement_time_factor;
    }
    float mode_factor = MODE_FACTOR_HIGH;
    if (m_measurement_mode == BH1750::Mode::CONTINUOUS_MEDIUM) {
        mode_factor = MODE_FACTOR_MEDIUM;
    } else if (m_measurement_mode == BH1750::Mode::CONTINUOUS_LOW) {
        mode_factor = MODE_FACTOR_LOW;
    }
    lux *= mode_factor;
    lux /= ACCURACY_FACTOR;
    return lux;
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
    Logger::Log("Initiated");
    Initialize();
    while (true) {
        while (Measure() && ControlAuto() && !OnTarget()) { }
        StopMotorAdjusting();
        StopMeasuring();
        xTaskDelayUntil(&m_previous_measurement_tick, m_passive_measurement_period_ticks);
    }
}

void AmbientLightSensor::Initialize()
{
    BH1750::SetMeasurementTimeReference(BH1750::MEASUREMENT_TIME_REFERENCE_DEFAULT_MS);
    BH1750::SetMode(BH1750::Mode::POWER_DOWN);
    ResetMeasurement();
}

bool AmbientLightSensor::Measure()
{
    while (AdjustMeasurementResolution()) {
        if (!ReadLuxBlocking()) {
            // Remove misleading "sitting" measurement to imply sensor's failure state
            m_v_lux_latest_my->Receive(&m_lux_latest, 0);
            if (!OtherALSWorking()) {
                StopMotorAdjusting();
            }
            return false;
        }
        m_v_lux_latest_my->Overwrite(m_lux_latest);
        Logger::Log("Lux: {}: {}", BH1750::ModeString(BH1750::GetMode()), m_lux_latest);
    }
    return true;
}

bool AmbientLightSensor::OnTarget()
{
    float lux_target = NAN;
    if (MotorAdjusting()) {
        if (!m_v_lux_target->Peek(&lux_target, 0)) {
            return true;
        }
    } else if (m_commanding_motor) {
        return true;
    }
    float lux_current_average = m_lux_latest;
    float lux_latest_other = NAN;
    if (m_v_lux_latest_other->Peek(&lux_latest_other, 0)) {
        lux_current_average += lux_latest_other;
        lux_current_average /= 2;
    }
    const float lux_current_difference = (lux_target - lux_current_average) / lux_target;
    static const float MARGIN = 0.1;
    if (lux_current_difference < -MARGIN) {
        m_v_motor_command->Overwrite(Motor::CLOSE);
    } else if (lux_current_difference > MARGIN) {
        m_v_motor_command->Overwrite(Motor::OPEN);
    } else {
        StopMotorAdjusting();
        Logger::Log("On target [{}]: Measurement: {} ; Average: {}", lux_target, m_lux_latest, lux_current_average);
        return true;
    }
    m_commanding_motor = true;
    return false;
}

void AmbientLightSensor::StopMeasuring()
{
    if (!BH1750::SetMode(BH1750::Mode::POWER_DOWN)) {
        Logger::Log("Warning: Failed to stop continuous measurement mode");
    }
}

bool AmbientLightSensor::MotorAdjusting()
{
    Motor::Command command;
    if (!m_v_motor_command->Peek(&command, 0)) {
        return false;
    }
    return (command == Motor::OPEN || command == Motor::CLOSE);
}

void AmbientLightSensor::StopMotorAdjusting()
{
    m_commanding_motor = false;
    if (!ControlAuto()) {
        return;
    }
    Motor::Command command;
    if (m_v_motor_command->Receive(&command, 0) && command != Motor::OPEN && command != Motor::CLOSE) {
        m_v_motor_command->Append(command, 0);
    }
}
bool AmbientLightSensor::OtherALSWorking()
{
    float other_lux = NAN;
    return m_v_lux_latest_other->Peek(&other_lux, 0);
}
