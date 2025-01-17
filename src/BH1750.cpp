#include "BH1750.hpp"

#include <cstdio>

#define CLIOUT printf

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
        CLIOUT("Warning: I2C mode write failed.\n");
        return false;
    }
    m_mode = mode;
    CLIOUT("[Mode] %s\n", MODE_STR[mode]);
    return true;
}

BH1750::Mode BH1750::GetMode() const
{
    return m_mode;
}

bool BH1750::ReadMeasurementData(uint16_t *data)
{
    if (m_i2c->Read(m_dev_addr, m_read_buffer.data(), I2C_MEASUREMENT_BUF_LEN) != I2C_MEASUREMENT_BUF_LEN) {
        CLIOUT("Warning: BH1750 measurement read failed.\n");
        return false;
    }
    *data = static_cast<uint16_t>((m_read_buffer[0]) << 8U) | m_read_buffer[1];
    if (m_mode == Mode::ONE_TIME_MEDIUM ||
        m_mode == Mode::ONE_TIME_HIGH ||
        m_mode == Mode::ONE_TIME_LOW) {
        m_mode = Mode::POWER_DOWN;
    }
    return true;
}

bool BH1750::Reset()
{
    const Mode mode = GetMode();
    if (mode == Mode::POWER_DOWN) {
        if (!SetMode(Mode::POWER_ON)) {
            CLIOUT("Warning: BH1750 POWER_ON command failed.\n");
            return false;
        }
    }
    m_write_buffer[0] = Operation::RESET;
    if (m_i2c->Write(m_dev_addr, m_write_buffer.data(), I2C_INSTRUCTION_BUF_LEN) != I2C_INSTRUCTION_BUF_LEN) {
        CLIOUT("Warning: BH1750 RESET command failed.\n");
        return false;
    }
    CLIOUT("Measurement Reset\n");
    if (mode == Mode::POWER_DOWN) {
        if (!SetMode(Mode::POWER_DOWN)) {
            CLIOUT("Warning: BH1750 POWER_DOWN command failed.\n");
            return false;
        }
    }
    return true;
}

bool BH1750::SetMeasurementTimeReference(uint8_t measurement_time_reference_ms)
{
    if (measurement_time_reference_ms == m_measurement_time_ms) {
        return true;
    }
    if (MEASUREMENT_TIME_REFERENCE_MIN_MS > measurement_time_reference_ms || measurement_time_reference_ms > MEASUREMENT_TIME_REFERENCE_MAX_MS) {
        CLIOUT("Error: Measurement time reference [%hhu] outside BH1750's measurement time reference range [%hhu - %hhu]\n",
            measurement_time_reference_ms, MEASUREMENT_TIME_REFERENCE_MIN_MS, MEASUREMENT_TIME_REFERENCE_MAX_MS);
        return false;
    }
    m_write_buffer[0] = Operation::SET_MTREG_HIGH_BITS | static_cast<uint8_t>(measurement_time_reference_ms & MEASUREMENT_TIME_HIGH_BITS);
    if (m_i2c->Write(m_dev_addr, m_write_buffer.data(), I2C_INSTRUCTION_BUF_LEN) != I2C_INSTRUCTION_BUF_LEN) {
        CLIOUT("Warning: BH1750 MTReg-high-bits write failed.\n");
        return false;
    }
    m_write_buffer[0] = Operation::SET_MTREG_LOW_BITS | static_cast<uint8_t>(measurement_time_reference_ms & MEASUREMENT_TIME_LOW_BITS);
    if (m_i2c->Write(m_dev_addr, m_write_buffer.data(), I2C_INSTRUCTION_BUF_LEN) != I2C_INSTRUCTION_BUF_LEN) {
        CLIOUT("Warning: BH1750 MTReg-low-bits write failed.\n");
        return false;
    }
    CLIOUT("[MeasurementTimeReference->] %hhu ms\n", measurement_time_reference_ms);
    m_measurement_time_ms = measurement_time_reference_ms;
    return true;
}

uint8_t BH1750::GetMeasurementTimeReferenceMs() const
{
    return m_measurement_time_ms;
}
