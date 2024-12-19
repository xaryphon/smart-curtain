#pragma once

#include <cstdarg>
#include <cstring>
#include <memory>
#include <utility>

#include <FreeRTOS.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <hardware/timer.h>
#include <queue.h>
#include <task.h>

class Logger {
private:

public:
    explicit Logger(const char* task_name, uint32_t stack_depth, UBaseType_t priority);
    template <typename... T>
    static void Log(const char * fmt, T&&... args);
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
    static uint32_t m_lost_logs;
    log_content * m_log{};
};

void test_Logger();
