#include "RTC.hpp"

#include <mutex>

#include <hardware/timer.h>

#include "Logger.hpp"

bool RTC::s_initialized = false;

RTC::RTC()
    : m_access("RTC-Access")
{
    assert(!s_initialized);
    rtc_init();
    s_initialized = true;
}

bool RTC::Set(const datetime_t& time)
{
    std::lock_guard exclusive(m_access);
    datetime_t t = time;
    if (!rtc_set_datetime(&t)) {
        Logger::Log("[RTC] Error: Set failed.");
        return false;
    }
    return true;
}

void RTC::SetAlarm(const datetime_t& time, const rtc_callback_t& callback)
{
    std::lock_guard exclusive(m_access);
    if (m_alarmed) {
        Logger::Log("[RTC] Overwriting alarm");
    }
    datetime_t t = time;
    rtc_set_alarm(&t, callback);
    rtc_enable_alarm();
    m_alarmed = true;
}

datetime_t RTC::GetDatetime()
{
    std::lock_guard exclusive(m_access);
    datetime_t time;
    if (s_initialized && m_is_set && rtc_get_datetime(&time)) {
        return time;
    }
    return Default();
}

const char* RTC::DayOfWeekString(const datetime_t& time)
{
    switch (time.dotw) {
    case 0:
        return "Sun";
    case 1:
        return "Mon";
    case 2:
        return "Tue";
    case 3:
        return "Wed";
    case 4:
        return "Thu";
    case 5:
        return "Fri";
    case 6:
        return "Sat";
    default:
        return "Unk";
    }
}

datetime_t RTC::Default()
{
    enum : uint64_t {
        us_in_s = 1000'000,
        s_in_m = 60,
        m_in_h = 60,
        s_in_h = s_in_m * m_in_h,
        h_in_d = 24,
        s_in_d = s_in_h * h_in_d,
        d_in_w = 7,
        s_in_w = s_in_d * d_in_w,
        d_in_mon = 30,
        s_in_mon = s_in_d * d_in_mon,
        d_in_y = 365,
        s_in_y = s_in_d * d_in_y,
        mon_in_y = 12,

        default_year = 2025,
        default_month = 2,
        default_day = 28,
        default_weekday = 5,
        default_hour = 23,
        default_minute = 59,
        default_sec = 59,
    };

    const uint64_t sys_time_s = time_us_64() / us_in_s;

    return {
        .year = static_cast<int16_t>(default_year + (sys_time_s / s_in_y)),
        .month = static_cast<int8_t>((default_month + sys_time_s / s_in_mon) % mon_in_y),
        .day = static_cast<int8_t>((default_day + sys_time_s / s_in_d) % d_in_mon),
        .dotw = static_cast<int8_t>((default_weekday + sys_time_s / s_in_d) % d_in_w),
        .hour = static_cast<int8_t>((default_hour + sys_time_s / s_in_h) % h_in_d),
        .min = static_cast<int8_t>((default_minute + sys_time_s / s_in_m) % m_in_h),
        .sec = static_cast<int8_t>((default_sec + sys_time_s) % s_in_m),
    };
}

/// TODO: remove
#include <FreeRTOS.h>
#include <task.h>

struct test_params {
    RTC* rtc;
    RTOS::Semaphore* sema;
};

static RTOS::Semaphore* sema = nullptr;

static void alarm()
{
    BaseType_t hptw = pdFALSE;
    sema->GiveFromISR(&hptw);
    portYIELD_FROM_ISR(hptw);
}

void test_task(void* params)
{
    auto* rtc = static_cast<test_params*>(params)->rtc;
    sema = static_cast<test_params*>(params)->sema;
    rtc->SetAlarm({
                      .year = -1,
                      .month = -1,
                      .day = -1,
                      .dotw = -1,
                      .hour = -1,
                      .min = -1,
                      .sec = 00, // every minute
                  },
        alarm);
    while (true) {
        sema->Take(portMAX_DELAY);
        datetime_t time = rtc->GetDatetime();
        Logger::Log("[{} {:>2}.{:0>2}.{:0>4} {:0>2}:{:0>2}:{:0>2}]",
            rtc->DayOfWeekString(time), time.day, time.month, time.year, time.hour, time.min, time.sec);
        rtc->Set({
            .year = 2025,
            .month = 3,
            .day = 10,
            .dotw = 1,
            .hour = 10,
            .min = 37,
            .sec = 22,
        });
    }
}

void test_rtc()
{
    auto* params = new test_params {
        .rtc = new RTC(),
        .sema = new RTOS::Semaphore { "alarm" },
    };
    xTaskCreate(test_task, "test_rtc", 512, params, 2, nullptr);
}
