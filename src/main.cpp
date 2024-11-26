#include <cstdio>

#include <FreeRTOS.h>
#include <hardware/timer.h>
#include <pico/stdlib.h>
#include <task.h>

#include "example.h"

extern "C" {
uint32_t read_runtime_ctr(void)
{
    return timer_hw->timerawl;
}
}

int main()
{
    stdio_init_all();
    printf("\nBoot\n");

    auto example = Example::create();
    if (!example) {
        //handle error
    }

    vTaskStartScheduler();

    while (true) { }
}
