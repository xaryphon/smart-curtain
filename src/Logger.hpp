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
#include <sys/unistd.h>
#include <task.h>

#include "Queue.hpp"
#include "Semaphore.hpp"

class Logger {
public:
    explicit Logger(const char* task_name, uint32_t stack_depth, UBaseType_t priority);

    template <typename... T>
    static void Log(fmt::format_string<T...> fmt, T&&... args)
    {
        LogMessage(fmt::vformat(fmt, fmt::make_format_args(args...)));
    }

    struct LogContent {
        uint64_t timestamp;
        const char* task_name;
        std::string* msg;
    };

private:
    static void PrintLogAndDeleteMsg(const Logger::LogContent& log_content);
    static const char* GetTaskName();
    static std::string FormatTime(uint64_t time_us);
    static void LogMessage(const std::string& msg);
    static void LogToQueue(LogContent log);
    void Task();

    static constexpr UBaseType_t SYSLOG_QUEUE_LENGTH = 20;
    static constexpr UBaseType_t LOST_LOGS_MAX = 100;

    static RTOS::Queue<Logger::LogContent>* s_syslog;
    static RTOS::Counter* s_lost_logs;

    TaskHandle_t m_task_handle = nullptr;
    LogContent m_log = {};
};

/// TODO: remove
void Logger_stress_tester(const char* task_name);
