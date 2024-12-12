#pragma once

#include <memory>

#include "BH1750.hpp"
#include "PicoW_I2C.h"

class AmbientLightSensor : BH1750 {
public:
    explicit AmbientLightSensor(const std::shared_ptr<PicoW_I2C>& i2c, BH1750::I2C_dev_addr i2c_dev_addr);
    void StartContinuousMeasurement();
    void StopContinuousMeasurement();
    float ReadLuxBlocking();

private:
    void SetWaitTime();

    static constexpr uint8_t MEASUREMENT_TIME_TYPICAL_MS = 120;

    absolute_time_t m_measurement_ready_at_ms { 0 };
    float m_previous_measurement { BH1750::RESET_VALUE };
};

void test_ALS();
