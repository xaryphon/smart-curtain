#pragma once

#include <cstdarg>
#include <cstring>
#include <memory>
#include <utility>

#include <FreeRTOS.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <hardware/timer.h>
#include <queue.h>
#include <semphr.h>
#include <task.h>

class Logger {
public:
    explicit Logger(const char* task_name, uint32_t stack_depth, UBaseType_t priority);

    template <typename... T>
    static void Log(fmt::format_string<T...> fmt, T&&... args)
    {
        if (xSemaphoreTake(m_mutex, LOG_BLOCK_TIME_TICKS) == pdFALSE) {
            ++Logger::m_lost_logs;
            return;
        }

        auto* log = new Logger::log_content {
            /// TODO: real time
            .timestamp = time_us_64(),
            .task_name = GetTaskName(),
            .msg = fmt::vformat(fmt, fmt::make_format_args(args...))
        };

        if (xQueueSendToBack(m_syslog_q, &log, 0) != pdTRUE) {
            delete log;
            ++Logger::m_lost_logs;
        }
        assert(xSemaphoreGive(m_mutex));
    }

    struct log_content {
        uint64_t timestamp { 0 };
        const char* task_name { nullptr };
        std::string msg;
    };

private:
    static const char* GetTaskName();
    void Task();
    static std::string FormatTime(uint64_t time_us);

    TaskHandle_t m_task_handle { nullptr };
    static QueueHandle_t m_syslog_q;
    static SemaphoreHandle_t m_mutex;
    static constexpr TickType_t LOG_BLOCK_TIME_TICKS = pdMS_TO_TICKS(100);
    static uint32_t m_lost_logs;
    log_content* m_log {};
};

void test_Logger();
