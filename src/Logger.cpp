#include "Logger.hpp"

#include "config.h"

RTOS::Queue<Logger::LogContent>* Logger::s_syslog;
RTOS::Counter* Logger::s_lost_logs;

Logger::Logger(const char* task_name, uint32_t stack_depth, UBaseType_t priority)
{
    if (xTaskCreate(
            TASK_KONDOM(Logger, Task),
            task_name,
            stack_depth,
            static_cast<void*>(this),
            tskIDLE_PRIORITY + priority,
            &m_task_handle)
        == pdPASS) {
        Logger::Log("Created task [{}]", task_name);
    } else {
        Logger::Log("Error: Failed to create task [{}]", task_name);
    }
    s_syslog = new RTOS::Queue<LogContent>(SYSLOG_QUEUE_LENGTH, "SysLog");
    Log("[Queue] '{}' created", s_syslog->Name());
    s_lost_logs = new RTOS::Counter(LOST_LOGS_MAX, 0, "LostLogs");
}

void Logger::LogMessage(const std::string& msg)
{
    auto log = LogContent {
        .timestamp = time_us_64(),
        .task_name = GetTaskName(),
        .msg = new std::string(msg)
    };
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        LogToQueue(log);
    } else {
        PrintLogAndDeleteMsg(log);
    }
}

void Logger::LogToQueue(Logger::LogContent log)
{
    if (!s_syslog->Append(log, 0)) {
        s_lost_logs->Give();
        delete log.msg;
    }
}

void Logger::PrintLogAndDeleteMsg(const Logger::LogContent& log_content)
{
    const std::string log = fmt::format("[{}] [{}] {}\n", FormatTime(log_content.timestamp), log_content.task_name, *log_content.msg);
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

/// TODO: with real time, perhaps date or day of the weak
std::string Logger::FormatTime(uint64_t time)
{
    enum : uint64_t {
        us_in_ms = 1000,
        ms_in_s = 1000,
        s_in_m = 60,
        ms_in_m = ms_in_s * s_in_m,
        m_in_h = 60,
        ms_in_h = ms_in_m * m_in_h,
        h_in_d = 24
    };

    time /= us_in_ms;
    return fmt::format("{:0>2}:{:0>2}:{:0>2}:{:0>3}",
        time / ms_in_h % h_in_d,
        time / ms_in_m % m_in_h,
        time / ms_in_s % s_in_m,
        time % ms_in_s);
}

void Logger::Task()
{
    Logger::Log("Initiated");
    while (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        while (s_syslog->Receive(&m_log, s_lost_logs->Count() > 0 ? 0 : portMAX_DELAY)) {
            PrintLogAndDeleteMsg(m_log);
        }
        if (s_lost_logs->Count() > 0) {
            Logger::Log("Warning: lost {} logs.", s_lost_logs->Count());
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
