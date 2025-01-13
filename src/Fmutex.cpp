#include "Fmutex.h"

Fmutex::Fmutex()
    : m_mutex(xSemaphoreCreateMutex())
{
}

Fmutex::~Fmutex()
{
    vSemaphoreDelete(m_mutex);
}

void Fmutex::lock()
{
    xSemaphoreTake(m_mutex, portMAX_DELAY);
}

void Fmutex::unlock()
{
    xSemaphoreGive(m_mutex);
}
