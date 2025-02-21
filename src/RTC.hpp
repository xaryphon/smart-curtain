#pragma once

#include <Semaphore.hpp>
#include <hardware/rtc.h>

class RTC {
public:
    explicit RTC();
    explicit RTC(const datetime_t& time);

    void Set(const datetime_t& time);
    void SetAlarm(const datetime_t& time, const rtc_callback_t& callback);

    [[nodiscard]] datetime_t GetDatetime();
    [[nodiscard]] static const char* DayOfWeekString(const datetime_t& time);
    [[nodiscard]] static const char* MonthString(const datetime_t& time);

private:
    [[nodiscard]] static datetime_t Default();

    static bool s_initialized;
    RTOS::Mutex m_access;
    bool m_alarmed = false;
};

/// TODO: remove
void test_rtc();
