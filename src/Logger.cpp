#include "Logger.hpp"

#include <hardware/timer.h>
#include <pico/stdio.h>

#include "config.h"

RTOS::Queue<Logger::LogContent>* Logger::s_syslog;
RTOS::Counter* Logger::s_lost_logs;
RTC* Logger::s_rtc;
RTOS::Variable<Logger::LogTimeDetails>* s_log_time_details;

void Logger::Initialize(const Initializers& initializers)
{
    const bool stdio_initialized = stdio_init_all();
    assert(stdio_initialized);
    s_rtc = initializers.rtc;
    s_log_time_details = initializers.log_time_details;
    constexpr auto initial_settings = static_cast<LogTimeDetails>(S_FRACTIONS | DATE);
    s_log_time_details->Overwrite(initial_settings);
    write(1, "\n", 1);
}

Logger::Logger(const Constructors& constructors)
{
    if (xTaskCreate(
            TASK_KONDOM(Logger, Task),
            constructors.task_name,
            DEFAULT_TASK_STACK_SIZE * 3,
            this,
            tskIDLE_PRIORITY + 1,
            &m_task_handle)
        == pdPASS) {
        Log("Created task [{}]", constructors.task_name);
    } else {
        Log("Error: Failed to create task [{}]", constructors.task_name);
    }
    s_syslog = new RTOS::Queue<LogContent>(SYSLOG_QUEUE_LENGTH, "SysLog");
    s_lost_logs = new RTOS::Counter(LOST_LOGS_MAX, 0, "LostLogs");
}

void Logger::LogMessage(const std::string& msg)
{
    const uint64_t sys_time = time_us_64();
    auto log = LogContent {
        .datetime = s_rtc->GetDatetime(),
        .ms = static_cast<uint16_t>(sys_time / 1000 % 1000),
        .us = static_cast<uint16_t>(sys_time % 1000),
        .task_name = GetTaskName(),
        .msg = new std::string(msg)
    };
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        LogToQueue(log);
    } else {
        PrintLogAndDeleteMsg(log);
    }
}

void Logger::LogToQueue(LogContent log)
{
    if (!s_syslog->Append(log, 0)) {
        s_lost_logs->Give();
        delete log.msg;
    }
}

void Logger::PrintLogAndDeleteMsg(const LogContent& log_content)
{
    const std::string log = fmt::format("[{}] [{}] {}\n",
        FormatTime(log_content.datetime, log_content.ms, log_content.us),
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

std::string Logger::FormatTime(datetime_t dt, uint16_t ms, uint16_t us)
{
    std::string str = "";
    LogTimeDetails settings = NONE;
    if (s_log_time_details->Peek(&settings, 0) && settings & DATE) {
        str = fmt::format("{} {:0>2}.{:0>2}.{} ", s_rtc->DayOfWeekString(dt), dt.day, dt.month, dt.year);
    }
    str += fmt::format("{:0>2}:{:0>2}:{:0>2}", dt.hour, dt.min, dt.sec);
    if (settings & S_FRACTIONS) {
        str += fmt::format(":{:0>3}:{:0>3}", ms, us);
    }
    return str;
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
