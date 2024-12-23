#include "BH1750.hpp"

#include <cstdio>

#define CLIOUT printf

BH1750::BH1750(PicoW_I2C* i2c, BH1750::I2CDevAddr i2c_dev_addr)
    : m_i2c(i2c)
    , m_dev_addr(i2c_dev_addr)
{
}

void BH1750::SetMode(BH1750::Mode mode)
{
    if (GetMode() == mode) {
        CLIOUT("Warning: Already in mode %hhu\n", mode);
        return;
    }
    m_write_buffer[0] = mode;
    if (m_i2c->Write(m_dev_addr, m_write_buffer.data(), I2C_INSTRUCTION_BUF_LEN) != I2C_INSTRUCTION_BUF_LEN) {
        CLIOUT("Warning: I2C mode write failed.\n");
        return;
    }
    m_mode = mode;
    vTaskDelay(I2C_GRACE_PERIOD_TICKS);
}

BH1750::Mode BH1750::GetMode() const
{
    return m_mode;
}

bool BH1750::ReadMeasurementData(uint16_t *data)
{
    if (m_i2c->Read(m_dev_addr, m_read_buffer.data(), I2C_MEASUREMENT_BUF_LEN) != I2C_MEASUREMENT_BUF_LEN) {
        CLIOUT("Warning: I2C measurement read failed.\n");
        return false;
    }
    *data = static_cast<uint16_t>((m_read_buffer[0]) << 8U) | m_read_buffer[1];
    return true;
}

void BH1750::Reset()
{
    const Mode mode = GetMode();
    if (mode == Mode::POWER_DOWN) {
        SetMode(Mode::POWER_ON);
    }
    m_write_buffer[0] = Operation::RESET;
    if (m_i2c->Write(m_dev_addr, m_write_buffer.data(), I2C_INSTRUCTION_BUF_LEN) != I2C_INSTRUCTION_BUF_LEN) {
        CLIOUT("Warning: I2C reset write failed.\n");
        return;
    }
    vTaskDelay(I2C_GRACE_PERIOD_TICKS);
    if (mode == Mode::POWER_DOWN) {
        SetMode(Mode::POWER_DOWN);
    }
}

bool BH1750::SetMeasurementTimeMS(uint8_t measurement_time_ms)
{
    if (measurement_time_ms <= MEASUREMENT_TIME_MIN || MEASUREMENT_TIME_MAX <= measurement_time_ms) {
        CLIOUT("Error: Measurement time [%hhu] outside accepted range (%hhu - %hhu).\n",
               measurement_time_ms, MEASUREMENT_TIME_MIN, MEASUREMENT_TIME_MAX);
        return false;
    }
    m_write_buffer[0] = Operation::SET_MTREG_HIGH_BITS | static_cast<uint8_t>(measurement_time_ms & MEASUREMENT_TIME_HIGH_BITS);
    if (m_i2c->Write(m_dev_addr, m_write_buffer.data(), I2C_INSTRUCTION_BUF_LEN) != I2C_INSTRUCTION_BUF_LEN) {
        CLIOUT("Warning: I2C MTReg-high-bits write failed.\n");
        return false;
    }
    vTaskDelay(I2C_GRACE_PERIOD_TICKS);
    m_write_buffer[0] = Operation::SET_MTREG_LOW_BITS | static_cast<uint8_t>(measurement_time_ms & MEASUREMENT_TIME_LOW_BITS);
    if (m_i2c->Write(m_dev_addr, m_write_buffer.data(), I2C_INSTRUCTION_BUF_LEN) != I2C_INSTRUCTION_BUF_LEN) {
        CLIOUT("Warning: I2C MTReg-low-bits write failed.\n");
        return false;
    }
    vTaskDelay(I2C_GRACE_PERIOD_TICKS);
    m_measurement_time_ms = measurement_time_ms;
    return true;
}

uint8_t BH1750::GetMeasurementTimeMs() const
{
    return m_measurement_time_ms;
}
