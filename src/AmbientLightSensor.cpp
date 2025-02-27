#include "AmbientLightSensor.hpp"

#include <cmath>

#include "Logger.hpp"
#include "config.h"

AmbientLightSensor::AmbientLightSensor(const Parameters& parameters)
    : m_als1(parameters.als1)
    , m_als2(parameters.als2)
    , m_v_measurement_als_1(parameters.v_latest_measurement_als1)
    , m_v_measurement_als_2(parameters.v_latest_measurement_als2)
    , m_v_lux_target(parameters.v_lux_target)
    , m_v_motor_command(parameters.v_motor_command)
    , m_s_control_auto(parameters.s_control_auto)
{
    if (xTaskCreate(TASK_KONDOM(AmbientLightSensor, Task),
            parameters.task_name,
            TaskStackSize::ALS,
            this,
            TaskPriority::ALS,
            &m_task_handle)
        == pdTRUE) {
        Logger::Log("Created task [{}]", parameters.task_name);
    } else {
        Logger::Log("Error: Failed to create task [{}]", parameters.task_name);
    }
}

void AmbientLightSensor::Task()
{
    Logger::Log("Initiated");
    Initialize();
    while (true) {
        if (StartMeasuring()) {
            while (Measure() && ControlAuto() && !OnTarget()) { }
        }
        StopMotorAdjusting();
        StopMeasuring();
        xTaskDelayUntil(&m_previous_measurement_tick, m_passive_measurement_period_ticks);
    }
}

void AmbientLightSensor::Initialize()
{
    m_als1.SetMeasurementTimeReference(BH1750::MEASUREMENT_TIME_REFERENCE_DEFAULT_MS);
    m_als1.PowerDown();
    m_als1.Reset();

    m_als2.SetMeasurementTimeReference(BH1750::MEASUREMENT_TIME_REFERENCE_DEFAULT_MS);
    m_als2.PowerDown();
    m_als2.Reset();
}

bool AmbientLightSensor::StartMeasuring()
{
    const bool als1_working = m_als1.AdjustMeasurementResolution();
    const bool als2_working = m_als2.AdjustMeasurementResolution();
    return als1_working || als2_working;
}

bool AmbientLightSensor::Measure()
{
    bool als1_working = true;
    bool als2_working = true;
    do {
        als1_working = m_als1.ReadLuxBlocking(&m_measurement_als1.lux);
        m_measurement_als1.timestamp = time_us_64(); /// TODO: change to RT
        als2_working = m_als2.ReadLuxBlocking(&m_measurement_als2.lux);
        m_measurement_als2.timestamp = time_us_64(); /// TODO: change to RT
        if (!als1_working && !als2_working) {
            return false;
        }
    } while (StartMeasuring());
    if (als1_working) {
        m_v_measurement_als_1->Overwrite(m_measurement_als1);
        Logger::Log("[{}}: {} lux", m_als1.Name(), m_measurement_als1.lux);
        m_lux_average = m_measurement_als1.lux;
    }
    if (als2_working) {
        m_v_measurement_als_2->Overwrite(m_measurement_als2);
        Logger::Log("[{}]: {} lux", m_als2.Name(), m_measurement_als2.lux);
        if (als1_working) {
            m_lux_average += m_measurement_als2.lux;
            m_lux_average /= 2;
        } else {
            m_lux_average = m_measurement_als2.lux;
        }
    }
    return true;
}

bool AmbientLightSensor::OnTarget()
{
    float lux_target = NAN;
    if (!m_v_lux_target->Peek(&lux_target, 0)) {
        return true;
    }
    if (!MotorAdjusting() && m_commanding_motor) {
        // Implies motor has reached a limit
        return true;
    }
    const float lux_current_difference = (lux_target - m_lux_average) / lux_target;
    static constexpr float MARGIN = 0.1;
    if (lux_current_difference < -MARGIN) {
        m_v_motor_command->Overwrite(Motor::CLOSE);
    } else if (lux_current_difference > MARGIN) {
        m_v_motor_command->Overwrite(Motor::OPEN);
    } else {
        if (m_commanding_motor) {
            Logger::Log("Lux target reached: {} lux ~= Measured average: {} lux", lux_target, m_lux_average);
        }
        return true;
    }
    m_commanding_motor = true;
    return false;
}

void AmbientLightSensor::StopMeasuring()
{
    m_als1.PowerDown();
    m_als2.PowerDown();
}

bool AmbientLightSensor::MotorAdjusting() const
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
