#include "Primitive.hpp"

#include "FreeRTOSConfig.h"


namespace Implementation::RTOS {
    uint Primitive::m_semaphore_count = 0;
    uint Primitive::m_queue_count = 0;

    void Primitive::IncrementSemaphoreCount()
    {
        ++m_semaphore_count;
        assert(m_semaphore_count + m_queue_count <= configQUEUE_REGISTRY_SIZE);
    }

    void Primitive::DecrementSemaphoreCount()
    {
        assert(m_semaphore_count > 0);
        --m_semaphore_count;
    }
} // namespace Implementation::RTOS

