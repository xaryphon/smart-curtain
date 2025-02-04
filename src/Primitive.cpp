#include "Primitive.hpp"

#include "FreeRTOSConfig.h"

namespace RTOS::Implementation {
uint Primitive::s_semaphore_count = 0;
uint Primitive::s_queue_count = 0;

void Primitive::IncrementSemaphoreCount()
{
    ++s_semaphore_count;
    assert(s_semaphore_count + s_queue_count <= configQUEUE_REGISTRY_SIZE);
}

void Primitive::DecrementSemaphoreCount()
{
    assert(s_semaphore_count > 0);
    --s_semaphore_count;
}
} // namespace RTOS::Implementation
