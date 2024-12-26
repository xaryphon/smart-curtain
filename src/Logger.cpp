#include "Logger.hpp"

#include <utility>

#include "config.h"

/// TODO: classify queue
QueueHandle_t Logger::m_syslog_q;
/// TODO: classify mutex
SemaphoreHandle_t Logger::m_mutex;
uint32_t Logger::m_lost_logs = 0;

void Logger::Initialize()
{
    m_syslog_q = xQueueCreate(SYSLOG_QUEUE_LENGTH, sizeof(log_content*));
    m_mutex = xSemaphoreCreateMutex();
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

void Logger::LogToQueue(std::string str)
{
    xSemaphoreTake(m_mutex, portMAX_DELAY);

    auto* log = new Logger::log_content {
        /// TODO: real time
        .timestamp = time_us_64(),
        .task_name = GetTaskName(),
        .msg = std::move(str)
    };

    if (xQueueSendToBack(m_syslog_q, &log, 0) != pdTRUE) {
        delete log;
        ++Logger::m_lost_logs;
    }
    xSemaphoreGive(m_mutex);
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

void Logger::Task()
{
    Logger::Log("Initiated");
    fmt::print("\n"); // jump over buffer garbage
    while (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        while (xQueueReceive(
                   m_syslog_q,
                   static_cast<void*>(&m_log),
                   m_lost_logs > 0 ? 0 : portMAX_DELAY)
            == pdTRUE) {

            fmt::print("[{}] [{}] {}\n",
                FormatTime(m_log->timestamp),
                m_log->task_name,
                m_log->msg);

            delete m_log;
        }
        if (m_lost_logs > 0) {
            Logger::Log("Warning: lost {} logs.", m_lost_logs);
            xSemaphoreTake(m_mutex, portMAX_DELAY);
            Logger::m_lost_logs = 0;
            xSemaphoreGive(m_mutex);
        }
    }
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

#include <pico/stdlib.h>

#include "config.h"
#include "example.h"

void test_Logger()
{
    // Give time to open USB serial port // for testing.
    sleep_ms(5000);
    Logger::Log("Boot");

    // Stack size could be investigated further.
    // Even with plain C const char * 's, non-variadic arguments and no explicit memory allocation
    // it's still practically the same.
    new Logger("Logger", DEFAULT_TASK_STACK_SIZE * 3, 1);

    const char* desc = "extra log #";

    for (uint8_t i = 0; i < 10; ++i) {
        Logger::Log("{}{}", desc, i);
    }

    Logger::Log("Initializing Scheduler...");
}
