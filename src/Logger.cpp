#include "Logger.hpp"

#include <utility>

#include <sys/unistd.h>

#include "config.h"

/// TODO: classify queue
QueueHandle_t Logger::m_syslog_q;
/// TODO: classify mutex
SemaphoreHandle_t Logger::m_mutex;
uint32_t Logger::m_lost_logs = 0;

void Logger::Initialize()
{
    m_syslog_q = xQueueCreate(SYSLOG_QUEUE_LENGTH, sizeof(LogContent*));
    m_mutex = xSemaphoreCreateMutex();
    write(1, "\n", 1);
}

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
}

void Logger::LogMessage(std::string msg)
{
    auto* log = new LogContent(std::move(msg));
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        xSemaphoreTake(m_mutex, portMAX_DELAY);
        LogToQueue(log);
        xSemaphoreGive(m_mutex);
    } else {
        log->PrintAndDelete();
    }
}

void Logger::LogToQueue(Logger::LogContent* log)
{
    if (xQueueSendToBack(m_syslog_q, &log, 0) != pdTRUE) {
        delete log;
        ++Logger::m_lost_logs;
    }
}

Logger::LogContent::LogContent(std::string log_msg)
    : timestamp(time_us_64())
    , task_name(GetTaskName())
    , msg(std::move(log_msg))
{
}

void Logger::LogContent::PrintAndDelete()
{
    const std::string log = fmt::format("[{}] [{}] {}\n",
        FormatTime(timestamp),
        task_name,
        msg);
    write(1, log.c_str(), log.size());
    delete this;
}

const char* Logger::LogContent::GetTaskName()
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
std::string Logger::LogContent::FormatTime(uint64_t time)
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
        while (xQueueReceive(
                   m_syslog_q,
                   static_cast<void*>(&m_log),
                   m_lost_logs > 0 ? 0 : portMAX_DELAY)
            == pdTRUE) {
            m_log->PrintAndDelete();
        }
        if (m_lost_logs > 0) {
            Logger::Log("Warning: lost {} logs.", m_lost_logs);
            xSemaphoreTake(m_mutex, portMAX_DELAY);
            Logger::m_lost_logs = 0;
            xSemaphoreGive(m_mutex);
        }
    }
}

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
