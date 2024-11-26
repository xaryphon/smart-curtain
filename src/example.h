#pragma once
#include <memory>
#include <string>

#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include <task.h>

#include "config.h"

// semaphore
class Example {
public:
    static std::unique_ptr<Example> create()
    {
        SemaphoreHandle_t sem = xSemaphoreCreateBinary();
        if (sem == nullptr) {
            return nullptr;
        }
        return std::unique_ptr<Example>(new Example(sem));
    }
    SemaphoreHandle_t GetSem()
    {
        return m_sem;
    }

private:
    explicit Example(SemaphoreHandle_t sem)
        : m_sem(sem)
    {
    }
    SemaphoreHandle_t m_sem;
};

// task
#define TASK_KONDOM(klass, func) [](void* param) -> void { static_cast<klass*>(param)->func(); }

class Example2 {
public:
    explicit Example2(const char* name, SemaphoreHandle_t sem);
    [[noreturn]] void task();

private:
    int m_num;
    void FuncKyKong();
    SemaphoreHandle_t m_sem;
};
