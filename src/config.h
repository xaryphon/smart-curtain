#pragma once

#define DEFAULT_TASK_STACK_SIZE 256
#define EXAMPLE_TASK_PRIORITY 1

#define TASK_KONDOM(klass, func) [](void* param) -> void { static_cast<klass*>(param)->func(); }
