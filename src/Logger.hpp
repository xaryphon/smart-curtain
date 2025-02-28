#pragma once

#include <cstdarg>
#include <cstring>
#include <memory>

#include <FreeRTOS.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <sys/unistd.h>
#include <task.h>

#include "Queue.hpp"
#include "RTC.hpp"
#include "Semaphore.hpp"

class Logger {
public:

    struct Initializers {
        RTC* rtc;
    };

    static void Initialize(const Initializers& initializers);

    struct Constructors {
        const char* task_name;
    };

    explicit Logger(const Constructors& constructors);

    template <typename... T>
    static void Log(fmt::format_string<T...> fmt, T&&... args)
    {
        LogMessage(fmt::vformat(fmt, fmt::make_format_args(args...)));
    }

    struct LogContent {
        datetime_t datetime;
        const char* task_name;
        std::string* msg;
    };

private:
    static void PrintLogAndDeleteMsg(const LogContent& log_content);
    static const char* GetTaskName();
    static std::string FormatTime(datetime_t dt);
    static void LogMessage(const std::string& msg);
    static void LogToQueue(LogContent log);
    void Task();

    static constexpr UBaseType_t SYSLOG_QUEUE_LENGTH = 20;
    static constexpr UBaseType_t LOST_LOGS_MAX = 100;

    static RTOS::Queue<LogContent>* s_syslog;
    static RTOS::Counter* s_lost_logs;
    static RTC* s_rtc;

    TaskHandle_t m_task_handle = nullptr;
    LogContent m_log = {};
};

/// TODO: remove
void Logger_stress_tester(const char* task_name);
