#pragma once

#include <utility>

#include "Primitive.hpp"

namespace RTOS {
namespace Implementation {
    class SemaphoreHandler : protected ::Implementation::RTOS::Primitive {
    public:
        ~SemaphoreHandler();
        SemaphoreHandler(SemaphoreHandler&& semaphore_handler) noexcept;

        SemaphoreHandler(const SemaphoreHandler& semaphore_handler) = delete;
        SemaphoreHandler& operator=(const SemaphoreHandler& semaphore_handler) = delete;
        SemaphoreHandler& operator=(SemaphoreHandler&& /*unused*/) = delete;

    protected:
        explicit SemaphoreHandler(SemaphoreHandle_t handle, const char* name);
        [[nodiscard]] SemaphoreHandle_t Handle() const { return m_handle; }

    private:
        SemaphoreHandle_t m_handle;
    };
} // namespace Implementation

class Semaphore : public Implementation::SemaphoreHandler {
public:
    explicit Semaphore(const char* name)
        : Implementation::SemaphoreHandler(xSemaphoreCreateBinary(), name)
    {
    }

    void Give() { xSemaphoreGive(Handle()); }
    bool Take(TickType_t wait_time_ticks) { return xSemaphoreTake(Handle(), wait_time_ticks) == pdTRUE; }
    [[nodiscard]] UBaseType_t Count() const { return uxSemaphoreGetCount(Handle()); }
    void Reset() { xQueueReset(Handle()); }

    void GiveFromISR(BaseType_t* higher_priority_task_woken) { xSemaphoreGiveFromISR(Handle(), higher_priority_task_woken); }
    bool TakeFromISR(BaseType_t* higher_priority_task_woken) { return xSemaphoreTakeFromISR(Handle(), higher_priority_task_woken) == pdTRUE; }
    [[nodiscard]] BaseType_t CountFromISR() const { return uxSemaphoreGetCountFromISR(Handle()); }

    void lock() { Take(portMAX_DELAY); } // for lock_guard
    void try_lock() { Take(0); } // for lock_guard
    void unlock() { Give(); } // for lock_guard

protected:
    explicit Semaphore(SemaphoreHandle_t handle, const char* name)
        : SemaphoreHandler(handle, name)
    {
    }
};

class Counter final : public Semaphore {
public:
    explicit Counter(UBaseType_t max_count, UBaseType_t initial_count, const char* name)
        : Semaphore(xSemaphoreCreateCounting(max_count, initial_count), name)
        , m_max_count(max_count)
    {
    }

    ~Counter() = default;
    Counter(Counter&& counter) noexcept = default;

    Counter(const Counter& counter) = delete;
    Counter& operator=(const Counter& counter) = delete;
    Counter& operator=(Counter&& counter) = delete;

    [[nodiscard]] UBaseType_t MaxCount() const { return m_max_count; }

private:
    UBaseType_t m_max_count;
};

class Mutex final : public Semaphore {
public:
    explicit Mutex(const char* name)
        : Semaphore(xSemaphoreCreateMutex(), name)
    {
    }

    UBaseType_t CountFromISR() = delete;
    void GiveFromISR() = delete;
    bool TakeFromISR() = delete;

    [[nodiscard]] TaskHandle_t GetHolder() const { return xSemaphoreGetMutexHolder(Handle()); };
    [[nodiscard]] TaskHandle_t GetHolderFromISR() const { return xSemaphoreGetMutexHolderFromISR(Handle()); };
};

class RecursiveMutex final : public Implementation::SemaphoreHandler {
public:
    explicit RecursiveMutex(const char* name)
        : SemaphoreHandler(xSemaphoreCreateRecursiveMutex(), name)
    {
    }

    ~RecursiveMutex() = default;
    RecursiveMutex(RecursiveMutex&& recursive_mutex) noexcept = default;

    RecursiveMutex(const RecursiveMutex& recursive_mutex) = delete;
    RecursiveMutex& operator=(const RecursiveMutex& recursive_mutex) = delete;
    RecursiveMutex& operator=(RecursiveMutex&& recursive_mutex) = delete;

    void Give() { xSemaphoreGiveRecursive(Handle()); }
    bool Take(TickType_t wait_time_ticks) { return xSemaphoreTakeRecursive(Handle(), wait_time_ticks) == pdFALSE; }
    [[nodiscard]] UBaseType_t Count() const = delete;

    void lock() { Take(portMAX_DELAY); } // for lock_guard
    void try_lock() { Take(0); } // for lock_guard
    void unlock() { Give(); } // for lock_guard
};
} // namespace RTOS

/// TODO: remove
void test_semaphore();
void test_counter();
void test_mutex();
void test_recursive_mutex();
void test_all_semaphore_types();
