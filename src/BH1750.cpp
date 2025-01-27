#include "BH1750.hpp"

#include "Logger.hpp"

BH1750::BH1750(PicoW_I2C* i2c, BH1750::I2CDevAddr i2c_dev_addr)
    : m_i2c(i2c)
    , m_dev_addr(i2c_dev_addr)
{
}

bool BH1750::SetMode(BH1750::Mode mode)
{
    if (m_mode == mode) {
        return true;
    }
    m_write_buffer[0] = mode;
    if (m_i2c->Write(m_dev_addr, m_write_buffer.data(), I2C_INSTRUCTION_BUF_LEN) != I2C_INSTRUCTION_BUF_LEN) {
        Logger::Log("Warning: I2C mode write failed");
        return false;
    }
    m_mode = mode;
    Logger::Log("[Mode] {}", ModeString(m_mode));
    return true;
}

BH1750::Mode BH1750::GetMode() const
{
    return m_mode;
}

bool BH1750::ReadMeasurementData(uint16_t* data)
{
    if (m_i2c->Read(m_dev_addr, m_read_buffer.data(), I2C_MEASUREMENT_BUF_LEN) != I2C_MEASUREMENT_BUF_LEN) {
        Logger::Log("Warning: BH1750 measurement read failed");
        return false;
    }
    *data = static_cast<uint16_t>((m_read_buffer[0]) << 8U) | m_read_buffer[1];
    if (m_mode == Mode::ONE_TIME_MEDIUM || m_mode == Mode::ONE_TIME_HIGH || m_mode == Mode::ONE_TIME_LOW) {
        m_mode = Mode::POWER_DOWN;
    }
    return true;
}

bool BH1750::Reset()
{
    const Mode mode = GetMode();
    if (mode == Mode::POWER_DOWN) {
        if (!SetMode(Mode::POWER_ON)) {
            Logger::Log("Warning: BH1750 command {} failed", ModeString(Mode::POWER_ON));
            return false;
        }
    }
    m_write_buffer[0] = Operation::RESET;
    if (m_i2c->Write(m_dev_addr, m_write_buffer.data(), I2C_INSTRUCTION_BUF_LEN) != I2C_INSTRUCTION_BUF_LEN) {
        Logger::Log("Warning: BH1750 command RESET failed");
        return false;
    }
    Logger::Log("Measurement Reset");
    if (mode == Mode::POWER_DOWN) {
        if (!SetMode(Mode::POWER_DOWN)) {
            Logger::Log("Warning: BH1750 command {} failed", ModeString(Mode::POWER_DOWN));
            return false;
        }
    }
    return true;
}

bool BH1750::SetMeasurementTimeReference(uint8_t measurement_time_reference_ms)
{
    if (MEASUREMENT_TIME_REFERENCE_MIN_MS > measurement_time_reference_ms || measurement_time_reference_ms > MEASUREMENT_TIME_REFERENCE_MAX_MS) {
        Logger::Log("Error: Measurement time reference [{}] outside BH1750's measurement time reference range [{} - {}]",
            measurement_time_reference_ms, MEASUREMENT_TIME_REFERENCE_MIN_MS, MEASUREMENT_TIME_REFERENCE_MAX_MS);
        return false;
    }
    static constexpr uint8_t HIGH_BITS_SHIFT = 5;
    m_write_buffer[0] = Operation::SET_MTREG_HIGH_BITS;
    m_write_buffer[0] |= static_cast<uint8_t>(measurement_time_reference_ms >> HIGH_BITS_SHIFT);
    if (m_i2c->Write(m_dev_addr, m_write_buffer.data(), I2C_INSTRUCTION_BUF_LEN) != I2C_INSTRUCTION_BUF_LEN) {
        Logger::Log("Warning: BH1750 MTReg-high-bits write failed");
        return false;
    }
    static constexpr uint8_t LOW_BITS = 0b00011111U;
    m_write_buffer[0] = Operation::SET_MTREG_LOW_BITS;
    m_write_buffer[0] |= static_cast<uint8_t>(measurement_time_reference_ms & LOW_BITS);
    if (m_i2c->Write(m_dev_addr, m_write_buffer.data(), I2C_INSTRUCTION_BUF_LEN) != I2C_INSTRUCTION_BUF_LEN) {
        Logger::Log("Warning: BH1750 MTReg-low-bits write failed");
        return false;
    }
    Logger::Log("Measurement Time Reference set to {} ms", measurement_time_reference_ms);
    m_measurement_time_reference_ms = measurement_time_reference_ms;
    return true;
}

uint8_t BH1750::GetMeasurementTimeReferenceMs() const
{
    return m_measurement_time_reference_ms;
}

const char* BH1750::ModeString(BH1750::Mode mode)
{
    switch (mode) {
    case Mode::POWER_DOWN:
        return "POWER_DOWN";
    case Mode::POWER_ON:
        return "POWER_ON";
    case Mode::CONTINUOUS_MEDIUM:
        return "CONTINUOUS_MEDIUM";
    case Mode::CONTINUOUS_HIGH:
        return "CONTINUOUS_HIGH";
    case Mode::CONTINUOUS_LOW:
        return "CONTINUOUS_LOW";
    case Mode::ONE_TIME_MEDIUM:
        return "ONE_TIME_MEDIUM";
    case Mode::ONE_TIME_HIGH:
        return "ONE_TIME_HIGH";
    case Mode::ONE_TIME_LOW:
        return "ONE_TIME_LOW";
    default:
        return "UNKNOWN";
    }
}
