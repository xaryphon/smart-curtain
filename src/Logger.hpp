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
    static void Initialize();
    explicit Logger(const char* task_name, uint32_t stack_depth, UBaseType_t priority);

    template <typename... T>
    static void Log(fmt::format_string<T...> fmt, T&&... args)
    {
        LogMessage(fmt::vformat(fmt, fmt::make_format_args(args...)));
    }

private:
    struct LogContent {
    public:
        explicit LogContent(std::string log_msg);
        void PrintAndDelete();

    private:
        static const char* GetTaskName();
        static std::string FormatTime(uint64_t time_us);

        uint64_t timestamp;
        const char* task_name;
        std::string msg;
    };

    static void LogMessage(std::string msg);
    static void LogToQueue(LogContent* log);
    void Task();

    static constexpr UBaseType_t SYSLOG_QUEUE_LENGTH = 10;
    TaskHandle_t m_task_handle = nullptr;
    static QueueHandle_t m_syslog_q;
    static SemaphoreHandle_t m_mutex;
    static uint32_t m_lost_logs;
    LogContent* m_log = nullptr;
};

void Logger_stress_tester(const char* task_name);
