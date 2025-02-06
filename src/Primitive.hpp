#pragma once

#include <cstdio>

#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>

namespace RTOS::Implementation {
class Primitive {
public:
    explicit Primitive() = default;

    static uint GetSemaphoreCount() { return s_semaphore_count; }
    static uint GetQueueCount() { return s_queue_count; }

protected:
    static void IncrementSemaphoreCount();
    static void DecrementSemaphoreCount();

private:
    static uint s_semaphore_count;
    static uint s_queue_count;
};
} // namespace RTOS::Implementation
