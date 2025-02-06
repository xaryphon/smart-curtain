#pragma once

#include <utility>

#include "Primitive.hpp"

namespace RTOS {
namespace Implementation {
    class SemaphoreHandler : protected Primitive {
    public:
        ~SemaphoreHandler();
        SemaphoreHandler(SemaphoreHandler&& semaphore_handler) noexcept;

        SemaphoreHandler(const SemaphoreHandler& semaphore_handler) = delete;
        SemaphoreHandler& operator=(const SemaphoreHandler& semaphore_handler) = delete;
        SemaphoreHandler& operator=(SemaphoreHandler&& /*unused*/) = delete;

        [[nodiscard]] const char* Name() const { return m_name; }

    protected:
        explicit SemaphoreHandler(SemaphoreHandle_t handle, const char* type, const char* name);
        [[nodiscard]] SemaphoreHandle_t Handle() const { return m_handle; }

    private:
        SemaphoreHandle_t m_handle;
        const char* m_name;
    };
} // namespace Implementation

class Semaphore : public Implementation::SemaphoreHandler {
public:
    explicit Semaphore(const char* name)
        : Implementation::SemaphoreHandler(xSemaphoreCreateBinary(), TYPE, name)
    {
    }

    bool Give();
    bool Take(TickType_t wait_time_ticks) { return xSemaphoreTake(Handle(), wait_time_ticks) == pdTRUE; }
    [[nodiscard]] UBaseType_t Count() const { return uxSemaphoreGetCount(Handle()); }
    void Reset() { xQueueReset(Handle()); }

    bool GiveFromISR(BaseType_t* higher_priority_task_woken) { return xSemaphoreGiveFromISR(Handle(), higher_priority_task_woken) == pdTRUE; }
    bool TakeFromISR(BaseType_t* higher_priority_task_woken) { return xSemaphoreTakeFromISR(Handle(), higher_priority_task_woken) == pdTRUE; }
    [[nodiscard]] BaseType_t CountFromISR() const { return uxSemaphoreGetCountFromISR(Handle()); }

    void lock() { Take(portMAX_DELAY); } // for lock_guard
    void try_lock() { Take(0); } // for lock_guard
    void unlock() { Give(); } // for lock_guard

protected:
    explicit Semaphore(SemaphoreHandle_t handle, const char* type, const char* name)
        : SemaphoreHandler(handle, type, name)
    {
    }

private:
    static constexpr const char* TYPE = "Semaphore";
};

class Counter final : public Semaphore {
public:
    explicit Counter(UBaseType_t max_count, UBaseType_t initial_count, const char* name)
        : Semaphore(xSemaphoreCreateCounting(max_count, initial_count), TYPE, name)
        , m_max_count(max_count)
    {
    }

    [[nodiscard]] UBaseType_t MaxCount() const { return m_max_count; }

private:
    static constexpr const char* TYPE = "Counter";

    UBaseType_t m_max_count;
};

class Mutex final : public Semaphore {
public:
    explicit Mutex(const char* name)
        : Semaphore(xSemaphoreCreateMutex(), TYPE, name)
    {
    }

    UBaseType_t CountFromISR() = delete;
    bool GiveFromISR() = delete;
    bool TakeFromISR() = delete;

    [[nodiscard]] TaskHandle_t GetHolder() const { return xSemaphoreGetMutexHolder(Handle()); };
    [[nodiscard]] TaskHandle_t GetHolderFromISR() const { return xSemaphoreGetMutexHolderFromISR(Handle()); };

private:
    static constexpr const char* TYPE = "Mutex";
};

class RecursiveMutex final : public Implementation::SemaphoreHandler {
public:
    explicit RecursiveMutex(const char* name)
        : SemaphoreHandler(xSemaphoreCreateRecursiveMutex(), TYPE, name)
    {
    }

    bool Give();
    bool Take(TickType_t wait_time_ticks) { return xSemaphoreTakeRecursive(Handle(), wait_time_ticks) == pdFALSE; }
    [[nodiscard]] UBaseType_t Count() const = delete;

    void lock() { Take(portMAX_DELAY); } // for lock_guard
    void try_lock() { Take(0); } // for lock_guard
    void unlock() { Give(); } // for lock_guard
private:
    static constexpr const char* TYPE = "RecursiveMutex";
};
} // namespace RTOS

/// TODO: remove
void test_semaphore();
void test_counter();
void test_mutex();
void test_recursive_mutex();
void test_all_semaphore_types();
