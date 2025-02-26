#pragma once

#include <FreeRTOS.h>

#define DEFAULT_TASK_STACK_SIZE 256
#define EXAMPLE_TASK_PRIORITY 1

namespace TaskPriority {
    using Type = BaseType_t;
    enum : Type {
        W5500LWIP = 24,
    };
} // namespace TaskPriority

namespace TaskStackSize {
    using Type = configSTACK_DEPTH_TYPE;
    enum : Type {
        W5500LWIP = 512,
    };
} // namespace TaskStackSize

#define TASK_KONDOM(klass, func) [](void* param) -> void { static_cast<klass*>(param)->func(); }
