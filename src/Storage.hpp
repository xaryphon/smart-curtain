#pragma once

#include <mutex>

#include "Flash.hpp"
#include "Queue.hpp"
#include "RTC.hpp"
#include "Semaphore.hpp"
#include "config.h"

class Storage final : public Flash {
public:
    struct Parameters {
        const char* task_name;

        RTOS::Semaphore* update_lux_target;
        RTOS::Semaphore* lux_target_auto;
        RTOS::Variable<float>* lux_target;

        RTC* rtc;
    };

    explicit Storage(const Parameters& parameters);

    template <typename Callback>
    bool ReadOnlyAccessLocked(const TickType_t wait_for_access, Callback&& callback) const
    {
        if (!m_write_access.Take(wait_for_access)) {
            return false;
        }
        const Settings& settings = AccessSettings();
        callback(settings);
        m_write_access.Give();
        return true;
    }

    template <typename Callback>
    bool WriteAccessAndProgramLocked(const TickType_t wait_for_access, Callback&& callback) const
    {
        if (!m_write_access.Take(wait_for_access)) {
            return false;
        }
        callback(AccessSettings());
        Program();
        m_write_access.Give();
        return true;
    }

    static std::string StringifySettings(const Settings& settings);

private:
    void Task();
    static void AlarmEvent();

    static constexpr datetime_t ALARM_TIME = {
        .year = -1,
        .month = -1,
        .day = -1,
        .dotw = -1,
        .hour = -1,
        .min = -1,
        .sec = 00, /// When seconds are 00 -- every minute
    };

    mutable RTOS::Mutex m_write_access;
    static RTOS::Semaphore* s_update_lux;
    static RTOS::Semaphore* s_lux_target_auto;
    RTOS::Variable<float>* m_lux_target;

    RTC* m_rtc;

    TaskHandle_t m_handle = nullptr;
};

/// TODO: remove

struct TestStorageParams {
    RTOS::Semaphore* update_lux_target;
    RTOS::Semaphore* lux_target_auto;

    Storage* storage;
};

void test_storage(TestStorageParams* params);

/* COPY TO MAIN TO TEST
 *
auto* p = new TestStorageParams {
    .update_lux_target = update_hourly_lux_target,
    .lux_target_auto = auto_hourly,
    .storage = storage,
};
test_storage(p);
 *
*/
