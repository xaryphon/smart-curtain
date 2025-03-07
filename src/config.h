#pragma once

#include <FreeRTOS.h>

#define DEFAULT_TASK_STACK_SIZE 256
#define EXAMPLE_TASK_PRIORITY 1

namespace TaskPriority {
using Type = BaseType_t;
enum : Type {
    HTTP_SUB = 1,
    INDICATOR = 1,
    LOGGER = 1,
    STORAGE = 2,
    ALS = 3,
    MOTOR = 3,
    CLI = 5,
    W5500LWIP = 24,
};
} // namespace TaskPriority

namespace TaskStackSize {
using Type = configSTACK_DEPTH_TYPE;
enum : Type {
    HTTP_SUB = 1024,
    INDICATOR = 512,
    LOGGER = 1024,
    STORAGE = 1024,
    ALS = 1024,
    MOTOR = 512,
    CLI = 1024,
    W5500LWIP = 1536,
};
} // namespace TaskStackSize

#define TASK_KONDOM(klass, func) [](void* param) -> void { static_cast<klass*>(param)->func(); }
