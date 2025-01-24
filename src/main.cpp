#include <FreeRTOS.h>
#include <hardware/timer.h>
#include <pico/stdlib.h>
#include <task.h>

#include "Logger.hpp"
#include "example.h"

extern "C" {
uint32_t read_runtime_ctr(void)
{
    return timer_hw->timerawl;
}
}

int main()
{
    const bool stdio_initialized = stdio_init_all();
    assert(stdio_initialized);
    Logger::Initialize();
    Logger::Log("Boot");

    auto example = Example::create();
    if (!example) {
        //handle error
        Logger::Log("Error: Failed to create [Example] task");
    }
    new Logger("Logger", DEFAULT_TASK_STACK_SIZE * 3, 1);

    Logger::Log("Initializing Scheduler...");
    vTaskStartScheduler();
}
