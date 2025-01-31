#pragma once

#include <cstdio>

#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>


namespace Implementation::RTOS {
    class Primitive {
    public:
        explicit Primitive() = default;

        static uint GetSemaphoreCount() { return m_semaphore_count; }
        static uint GetQueueCount() { return m_queue_count; }

    protected:
        static void IncrementSemaphoreCount();
        static void DecrementSemaphoreCount();

    private:
        static uint m_semaphore_count;
        static uint m_queue_count;
    };
} // namespace Implementation::RTOS

