#include "Logger.hpp"

#include <hardware/timer.h>
#include <pico/stdio.h>

#include "config.h"

RTOS::Queue<Logger::LogContent>* Logger::s_syslog;
RTOS::Counter* Logger::s_lost_logs;
RTC* Logger::s_rtc;

void Logger::Initialize(const Initializers& initializers)
{
    const bool stdio_initialized = stdio_init_all();
    assert(stdio_initialized);
    s_rtc = initializers.rtc;
    write(1, "\n", 1);
}

Logger::Logger(const Constructors& constructors)
{
    if (xTaskCreate(
            TASK_KONDOM(Logger, Task),
            constructors.task_name,
            TaskStackSize::LOGGER,
            this,
            TaskPriority::LOGGER,
            &m_task_handle)
        == pdPASS) {
        Log("Created task [{}]", constructors.task_name);
    } else {
        Log("Error: Failed to create task [{}]", constructors.task_name);
    }
    s_syslog = new RTOS::Queue<LogContent>(SYSLOG_QUEUE_LENGTH, "SysLog");
    s_lost_logs = new RTOS::Counter(LOST_LOGS_MAX, 0, "LostLogs");
}

void Logger::LogMessage(std::string&& msg)
{
    auto log = LogContent {
        .datetime = s_rtc->GetDatetime(),
        .task_name = GetTaskName(),
        .msg = new std::string(std::move(msg))
    };
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        LogToQueue(log);
    } else {
        PrintLogAndDeleteMsg(log);
    }
}

void Logger::LogToQueue(const LogContent& log)
{
    if (!s_syslog->Append(log, 0)) {
        s_lost_logs->Give();
        delete log.msg;
    }
}

void Logger::PrintLogAndDeleteMsg(const LogContent& log_content)
{
    const std::string log = fmt::format("[{}] [{}] {}\n",
        FormatTime(log_content.datetime),
        log_content.task_name,
        *log_content.msg);
    write(1, log.c_str(), log.size());
    delete log_content.msg;
}

const char* Logger::GetTaskName()
{
    const char* taskName = "Unknown";
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
        if (currentTask != nullptr) {
            taskName = pcTaskGetName(currentTask);
        }
    } else {
        taskName = "Initializer";
    }
    return taskName;
}

std::string Logger::FormatTime(const datetime_t& dt)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        return fmt::format("{} {:0>2}.{:0>2}.{} {:0>2}:{:0>2}:{:0>2}",
            s_rtc->DayOfWeekString(dt), dt.day, dt.month, dt.year, dt.hour, dt.min, dt.sec);
    }
    enum : uint64_t {
        us_in_ms = 1000,
        ms_in_s = 1000,
        us_in_s = us_in_ms * ms_in_s,
        s_in_m = 60,
        us_in_m = us_in_s * s_in_m,
        m_in_h = 60,
        us_in_h = us_in_m * m_in_h,
        h_in_d = 24
    };
    const uint64_t us = time_us_64();
    return fmt::format("{:0>2}:{:0>2}:{:0>2}:{:0>3}:{:0>3}",
        us / us_in_h,
        us / us_in_m % m_in_h,
        us / us_in_s % s_in_m,
        us / us_in_ms % ms_in_s,
        us % us_in_ms);
}

void Logger::Task()
{
    Log("Initiated");
    while (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        while (s_syslog->Receive(&m_log, s_lost_logs->Count() > 0 ? 0 : portMAX_DELAY)) {
            PrintLogAndDeleteMsg(m_log);
        }
        if (s_lost_logs->Count() > 0) {
            Log("Warning: lost {} logs.", s_lost_logs->Count());
            s_lost_logs->Reset();
        }
    }
}

/// TODO: remove
// With current setup, pico tends to panic at around log number #8000 with one stresser as it runs out of memory.
void StressTask(void* params)
{
    (void)params;
    std::string msg { "a" };
    uint i = 1;
    while (true) {
        Logger::Log("#{}: {}", i++, msg);
        msg += "a";
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void Logger_stress_tester(const char* task_name)
{
    xTaskCreate(StressTask, task_name, DEFAULT_TASK_STACK_SIZE, nullptr, 2, nullptr);
}
