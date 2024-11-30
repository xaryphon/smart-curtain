#include "BH1750.hpp"
#include <cstdio>

BH1750::BH1750(const std::shared_ptr<PicoI2C> &i2c, BH1750::I2C_dev_addr i2c_dev_addr)
        : m_i2c(i2c), m_dev_addr(i2c_dev_addr) {}

bool BH1750::SetMode(BH1750::operation operation) {
    switch (operation) {
        case POWER_DOWN:
        case POWER_ON:
        case CONTINUOUS_HIGH_RES:
        case CONTINUOUS_HIGH_RES_2:
        case CONTINUOUS_LOW_RES:
        case ONE_TIME_HIGH_RES:
        case ONE_TIME_HIGH_RES_2:
        case ONE_TIME_LOW_RES:
            m_write_buffer[0] = operation;
            m_i2c->write(m_dev_addr, m_write_buffer.data(), I2C_INSTRUCTION_LEN);
            m_mode = operation;
            return true;
        case RESET_MEASUREMENT_DATA:
        case SET_MEASUREMENT_TIME_HIGH_BITS:
        case SET_MEASUREMENT_TIME_LOW_BITS:
            printf("Operation code %hhu does not set a mode.\n", operation);
            break;
        default:
            printf("Undefined operation code %hhu.\n", operation);
    }
    return false;
}

BH1750::operation BH1750::GetMode() {
    return m_mode;
}

bool BH1750::ResetDataRegistry() {
    if (m_mode != operation::POWER_DOWN) {
        m_write_buffer[0] = operation::RESET_MEASUREMENT_DATA;
        m_i2c->write(m_dev_addr, m_write_buffer.data(), I2C_INSTRUCTION_LEN);
        m_read_buffer.fill(0);
        return true;
    }
    printf("Cannot reset data while powered down.\n");
    return false;
}

bool BH1750::SetMeasurementTime(uint8_t measurement_time_ms) {
    if (MEASUREMENT_TIME_MIN <= measurement_time_ms && measurement_time_ms <= MEASUREMENT_TIME_MAX) {
        m_write_buffer[0] = operation::SET_MEASUREMENT_TIME_HIGH_BITS | (measurement_time_ms & MEASUREMENT_TIME_HIGH_BITS);
        m_i2c->write(m_dev_addr, m_write_buffer.data(), I2C_INSTRUCTION_LEN);
        m_write_buffer[0] = operation::SET_MEASUREMENT_TIME_LOW_BITS | (measurement_time_ms & MEASUREMENT_TIME_LOW_BITS);
        m_i2c->write(m_dev_addr, m_write_buffer.data(), I2C_INSTRUCTION_LEN);
        m_measurement_time_ms = measurement_time_ms;
        return true;
    }
    printf("Measurement time [%hhu] outside accepted range (%hhu - %hhu).\n",
           measurement_time_ms, MEASUREMENT_TIME_MIN, MEASUREMENT_TIME_MAX);
    return false;
}

float BH1750::ReadLuxData() {
    return ConvertUint16ToLux(ReadMeasurementData());
}

float BH1750::GetLuxData() {
    return ConvertUint16ToLux(GetMeasurementData());
}

uint16_t BH1750::ReadMeasurementData() {
    if (m_i2c->read(m_dev_addr, m_read_buffer.data(), I2C_MEASUREMENT_LEN) > 0) {
        m_read_buffer.fill(0);
        return static_cast<uint16_t>(m_read_buffer[0]) << BYTE | m_read_buffer[1];
    }
    return ERROR_VALUE;
}

uint16_t BH1750::GetMeasurementData() {
    return static_cast<uint16_t>(m_read_buffer[0]) << BYTE | m_read_buffer[1];
}

float BH1750::ConvertUint16ToLux(uint16_t u16) {
    if (u16 != ERROR_VALUE) {
        auto lux = static_cast<float>(u16);
        const float measurement_time_factor = static_cast<float>(MEASUREMENT_TIME_DEFAULT) / static_cast<float>(m_measurement_time_ms);
        lux *= measurement_time_factor;
        float mode_factor = MODE_FACTOR_HIGH;
        if (m_mode == CONTINUOUS_HIGH_RES_2 || m_mode == ONE_TIME_HIGH_RES_2) {
            mode_factor = MODE_FACTOR_HIGH_2;
        } else if (m_mode == CONTINUOUS_LOW_RES || m_mode == ONE_TIME_LOW_RES) {
            mode_factor = MODE_FACTOR_LOW;
        }
        lux *= mode_factor;
        lux /= ACCURACY_FACTOR;
        return lux;
    }
    return ERROR_VALUE;
}
