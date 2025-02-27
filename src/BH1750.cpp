#include "BH1750.hpp"

#include "Logger.hpp"

BH1750::BH1750(const Parameters& parameters)
    : m_name(parameters.name)
    , m_i2c(parameters.i2c)
    , m_dev_addr(static_cast<uint>(parameters.i2c_dev_addr))
{
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
bool BH1750::AdjustMeasurementResolution()
{
    static constexpr uint16_t EXPECTED_MAX_RAW_READ = 0b0101000101100001;

    static constexpr float MAX_HIGH = EXPECTED_MAX_RAW_READ / ACCURACY_FACTOR * MODE_FACTOR_HIGH / 2;
    static constexpr float MAX_MEDIUM = EXPECTED_MAX_RAW_READ / ACCURACY_FACTOR * MODE_FACTOR_MEDIUM / 2;

    auto new_mode = CONTINUOUS_LOW;
    if (m_lux_latest < MAX_HIGH) {
        new_mode = CONTINUOUS_HIGH;
    } else if (m_lux_latest < MAX_MEDIUM) {
        new_mode = CONTINUOUS_MEDIUM;
    }
    if (new_mode == m_mode) {
        return false;
    }
    const Mode prev_mode = m_mode;
    if (!SetMode(new_mode)) {
        return false;
    }
    AdjustMeasurementTime();
    if (prev_mode == POWER_DOWN || prev_mode == POWER_ON) {
        MediateMeasurementTime();
    }
    return true;
}

bool BH1750::ReadLuxBlocking(float* lux)
{
    WaitForMeasurement();
    uint16_t measurement_data = RESET_VALUE;
    if (!ReadMeasurementData(&measurement_data)) {
        return false;
    }
    m_lux_latest = Uint16ToLux(measurement_data);
    *lux = m_lux_latest;
    return true;
}

void BH1750::PowerDown()
{
    SetMode(POWER_DOWN);
}

bool BH1750::Reset()
{
    const Mode mode = m_mode;
    if (mode == POWER_DOWN) {
        if (!SetMode(POWER_ON)) {
            Logger::Log("[{}] Warning: BH1750 command {} failed", m_name, ModeString(POWER_ON));
            return false;
        }
    }
    m_write_buffer[0] = RESET;
    if (m_i2c->Write(m_dev_addr, m_write_buffer.data(), I2C_INSTRUCTION_BUF_LEN, I2CTimeoutTicks()) != I2C_INSTRUCTION_BUF_LEN) {
        Logger::Log("[{}] Warning: BH1750command RESET failed", m_name);
        return false;
    }
    // Logger::Log("Measurement Reset");
    if (mode == POWER_DOWN) {
        if (!SetMode(POWER_DOWN)) {
            Logger::Log("[{}] Warning: BH1750 command {} failed", m_name, ModeString(POWER_DOWN));
            return false;
        }
    }
    return true;
}

bool BH1750::SetMeasurementTimeReference(uint8_t measurement_time_reference_ms)
{
    if (MEASUREMENT_TIME_REFERENCE_MIN_MS > measurement_time_reference_ms || measurement_time_reference_ms > MEASUREMENT_TIME_REFERENCE_MAX_MS) {
        Logger::Log("[{}] Error: Measurement time reference [{}] outside BH1750's measurement time reference range [{} - {}]",
            m_name, measurement_time_reference_ms, MEASUREMENT_TIME_REFERENCE_MIN_MS, MEASUREMENT_TIME_REFERENCE_MAX_MS);
        return false;
    }
    static constexpr uint8_t HIGH_BITS_SHIFT = 5;
    m_write_buffer[0] = SET_MTREG_HIGH_BITS;
    m_write_buffer[0] |= static_cast<uint8_t>(measurement_time_reference_ms >> HIGH_BITS_SHIFT);
    if (m_i2c->Write(m_dev_addr, m_write_buffer.data(), I2C_INSTRUCTION_BUF_LEN, I2CTimeoutTicks()) != I2C_INSTRUCTION_BUF_LEN) {
        Logger::Log("[{}] Warning: BH1750 MTReg-high-bits write failed", m_name);
        return false;
    }
    static constexpr uint8_t LOW_BITS = 0b00011111U;
    m_write_buffer[0] = SET_MTREG_LOW_BITS;
    m_write_buffer[0] |= static_cast<uint8_t>(measurement_time_reference_ms & LOW_BITS);
    if (m_i2c->Write(m_dev_addr, m_write_buffer.data(), I2C_INSTRUCTION_BUF_LEN, I2CTimeoutTicks()) != I2C_INSTRUCTION_BUF_LEN) {
        Logger::Log("[{}] Warning: BH1750 MTReg-low-bits write failed", m_name);
        return false;
    }
    // Logger::Log("Measurement Time Reference set to {} ms", measurement_time_reference_ms);
    m_measurement_time_reference_ms = measurement_time_reference_ms;
    m_measurement_time_reference_factor = static_cast<float>(measurement_time_reference_ms) / MEASUREMENT_TIME_REFERENCE_DEFAULT_MS;
    return true;
}

/// Private
bool BH1750::SetMode(const Mode mode)
{
    if (m_mode == mode) {
        return true;
    }
    m_write_buffer[0] = mode;
    if (m_i2c->Write(m_dev_addr, m_write_buffer.data(), I2C_INSTRUCTION_BUF_LEN, I2CTimeoutTicks()) != I2C_INSTRUCTION_BUF_LEN) {
        Logger::Log("[{}] Warning: I2C mode write failed", m_name);
        return false;
    }
    m_mode = mode;
    // Logger::Log("[Mode] {}", ModeString(m_mode));
    return true;
}

bool BH1750::ReadMeasurementData(uint16_t* data)
{
    if (m_i2c->Read(m_dev_addr, m_read_buffer.data(), I2C_MEASUREMENT_BUF_LEN, I2CTimeoutTicks()) != I2C_MEASUREMENT_BUF_LEN) {
        Logger::Log("[{}] Warning: BH1750 measurement read failed", m_name);
        return false;
    }
    *data = static_cast<uint16_t>((m_read_buffer[0]) << 8U) | m_read_buffer[1];
    if (m_mode == ONE_TIME_MEDIUM || m_mode == ONE_TIME_HIGH || m_mode == ONE_TIME_LOW) {
        m_mode = POWER_DOWN;
    }
    return true;
}

void BH1750::AdjustMeasurementTime()
{
    switch (m_mode) {
    case CONTINUOUS_MEDIUM:
        m_measurement_time = static_cast<TickType_t>(MEASUREMENT_TIME_TYPICAL_TICKS_RES_MEDIUM * m_measurement_time_reference_factor);
        break;
    case CONTINUOUS_HIGH:
        m_measurement_time = static_cast<TickType_t>(MEASUREMENT_TIME_TYPICAL_TICKS_RES_HIGH * m_measurement_time_reference_factor);
        break;
    case CONTINUOUS_LOW:
        m_measurement_time = static_cast<TickType_t>(MEASUREMENT_TIME_TYPICAL_TICKS_RES_LOW * m_measurement_time_reference_factor);
        break;
    default:
        Logger::Log("[{}] Error: Called AdjustMeasurementTime even though not measuring.", m_name);
        m_measurement_time = static_cast<TickType_t>(MEASUREMENT_TIME_TYPICAL_TICKS_RES_MEDIUM * m_measurement_time_reference_factor);
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
void BH1750::MediateMeasurementTime()
{
    m_measurement_started_at_ticks = xTaskGetTickCount();
    m_measurement_time_mediation = m_measurement_time / 2;
}

void BH1750::WaitForMeasurement()
{
    xTaskDelayUntil(&m_measurement_started_at_ticks, m_measurement_time + m_measurement_time_mediation);
    m_measurement_time_mediation = 0;
}

float BH1750::Uint16ToLux(const uint16_t u16) const
{
    auto lux = static_cast<float>(u16);
    if (GetMeasurementTimeReferenceMs() != MEASUREMENT_TIME_REFERENCE_DEFAULT_MS) {
        const float measurement_time_factor = MEASUREMENT_TIME_REFERENCE_DEFAULT_FLOAT / static_cast<float>(GetMeasurementTimeReferenceMs());
        lux *= measurement_time_factor;
    }
    float mode_factor = MODE_FACTOR_HIGH;
    if (m_mode == CONTINUOUS_MEDIUM) {
        mode_factor = MODE_FACTOR_MEDIUM;
    } else if (m_mode == CONTINUOUS_LOW) {
        mode_factor = MODE_FACTOR_LOW;
    }
    lux *= mode_factor;
    lux /= ACCURACY_FACTOR;
    return lux;
}

const char* BH1750::ModeString(const Mode mode)
{
    switch (mode) {
    case POWER_DOWN:
        return "POWER_DOWN";
    case POWER_ON:
        return "POWER_ON";
    case CONTINUOUS_MEDIUM:
        return "CONTINUOUS_MEDIUM";
    case CONTINUOUS_HIGH:
        return "CONTINUOUS_HIGH";
    case CONTINUOUS_LOW:
        return "CONTINUOUS_LOW";
    case ONE_TIME_MEDIUM:
        return "ONE_TIME_MEDIUM";
    case ONE_TIME_HIGH:
        return "ONE_TIME_HIGH";
    case ONE_TIME_LOW:
        return "ONE_TIME_LOW";
    default:
        return "UNKNOWN";
    }
}
