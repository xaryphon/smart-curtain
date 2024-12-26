#include <FreeRTOS.h>
#include <hardware/timer.h>
#include <pico/stdlib.h>
#include <task.h>

#include "example.h"
#include "Logger.hpp"


extern "C" {
uint32_t read_runtime_ctr(void)
{
    return timer_hw->timerawl;
}
}

int main()
{
    assert(stdio_init_all());
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
