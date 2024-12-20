#include "BH1750.hpp"

#include <cstdio>

#define CLIOUT printf

BH1750::BH1750(const std::shared_ptr<PicoW_I2C>& i2c, BH1750::I2C_dev_addr i2c_dev_addr)
    : m_i2c(i2c)
    , m_dev_addr(i2c_dev_addr)
{
}

void BH1750::SetMode(BH1750::mode mode)
{
    if (GetMode() == mode) {
        CLIOUT("Warning: Already in mode %hhu\n", mode);
    }
    m_write_buffer[0] = mode;
    m_i2c->Write(m_dev_addr, m_write_buffer.data(), I2C_INSTRUCTION_BUF_LEN);
    m_mode = mode;
    vTaskDelay(I2C_GRACE_PERIOD_TICKS);
}

BH1750::mode BH1750::GetMode() const
{
    return m_mode;
}

uint16_t BH1750::ReadMeasurementData()
{
    if (m_i2c->Read(m_dev_addr, m_read_buffer.data(), I2C_MEASUREMENT_BUF_LEN) > 0) {
        return static_cast<uint16_t>((m_read_buffer[0]) << BYTE) | m_read_buffer[1];
    }
    return RESET_VALUE;
}

void BH1750::Reset()
{
    const mode mode = GetMode();
    if (mode == POWER_DOWN) {
        SetMode(POWER_ON);
    }
    m_write_buffer[0] = RESET;
    m_i2c->Write(m_dev_addr, m_write_buffer.data(), I2C_INSTRUCTION_BUF_LEN);
    vTaskDelay(I2C_GRACE_PERIOD_TICKS);
    if (mode == POWER_DOWN) {
        SetMode(POWER_DOWN);
    }
}

bool BH1750::SetMeasurementTimeMS(uint8_t measurement_time_ms)
{
    if (MEASUREMENT_TIME_MIN <= measurement_time_ms && measurement_time_ms <= MEASUREMENT_TIME_MAX) {
        m_write_buffer[0] = operation::SET_MTREG_HIGH_BITS | static_cast<uint8_t>(measurement_time_ms & MEASUREMENT_TIME_HIGH_BITS);
        m_i2c->Write(m_dev_addr, m_write_buffer.data(), I2C_INSTRUCTION_BUF_LEN);
        vTaskDelay(I2C_GRACE_PERIOD_TICKS);
        m_write_buffer[0] = operation::SET_MTREG_LOW_BITS | static_cast<uint8_t>(measurement_time_ms & MEASUREMENT_TIME_LOW_BITS);
        m_i2c->Write(m_dev_addr, m_write_buffer.data(), I2C_INSTRUCTION_BUF_LEN);
        vTaskDelay(I2C_GRACE_PERIOD_TICKS);
        m_measurement_time_ms = measurement_time_ms;
        return true;
    }
    CLIOUT("Error: Measurement time [%hhu] outside accepted range (%hhu - %hhu).\n",
        measurement_time_ms, MEASUREMENT_TIME_MIN, MEASUREMENT_TIME_MAX);
    return false;
}

uint8_t BH1750::GetMeasurementTimeMs() const
{
    return m_measurement_time_ms;
}
