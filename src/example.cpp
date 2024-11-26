#include "example.h"

Example2::Example2(const char* name, SemaphoreHandle_t sem)
    :m_num(0)
    ,m_sem(sem)
{
    xTaskCreate(TASK_KONDOM(Example2, task), name, DEFAULT_TASK_STACK_SIZE, static_cast<void*>(this), EXAMPLE_TASK_PRIORITY, nullptr);
}

void Example2::FuncKyKong()
{
}

void Example2::task()
{
    while (true) {

    }


}
