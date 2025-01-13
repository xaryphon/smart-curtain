#pragma once

#include <mutex>

#include <FreeRTOS.h>
#include <semphr.h>

class Fmutex {
public:
    Fmutex();
    ~Fmutex();
    void lock();
    void unlock();

private:
    SemaphoreHandle_t m_mutex;
};
