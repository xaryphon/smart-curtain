#include "Semaphore.hpp"

#include <utility>

#include "Logger.hpp"

namespace RTOS::Implementation {
// Destructor
SemaphoreHandler::~SemaphoreHandler()
{
    if (m_handle != nullptr) {
        vSemaphoreDelete(m_handle);
        Primitive::DecrementSemaphoreCount();
    }
}

// Move Constructor
SemaphoreHandler::SemaphoreHandler(SemaphoreHandler&& semaphore_handler) noexcept
    : m_handle(std::exchange(semaphore_handler.m_handle, nullptr))
    , m_name(semaphore_handler.m_name)
{
}

// Explicit Derivation Constructor
SemaphoreHandler::SemaphoreHandler(SemaphoreHandle_t handle, const char* type, const char* name)
    : m_handle(handle)
    , m_name(name)
{
    assert(m_handle);
    Primitive::IncrementSemaphoreCount();
    Logger::Log("[{}] '{}' created", type, m_name);
}
} // namespace RTOS::Implementation

bool RTOS::Semaphore::Give()
{
    if (xSemaphoreGive(Handle()) == pdFALSE) {
        Logger::Log("Error: {}.Give failed", Name());
        return false;
    }
    return true;
}

bool RTOS::RecursiveMutex::Give()
{
    if (xSemaphoreGiveRecursive(Handle()) != pdTRUE) {
        Logger::Log("Error: {}.Give failed", Name());
        return false;
    }
    return true;
}

/// TODO: remove
#include <mutex>

void example_semaphores()
{
    /// Allowed:
    RTOS::Semaphore semaphore1 { "asd" };
    auto semaphore2 = std::move(semaphore1);

    /// Disallowed:
    // semaphore1 = semaphore2;
    // auto semaphore4 = semaphore1;
    // semaphore2 = std::move(semaphore1);
}

void test_semaphore()
{
    RTOS::Semaphore semaphore_1 { "semaphore" };
    Logger::Log("semaphore name: {}", semaphore_1.Name());
    Logger::Log("{} initial .Count: {}", semaphore_1.Name(), semaphore_1.Count());
    semaphore_1.Give();
    if (semaphore_1.Count() != 1) {
        Logger::Log("semaphore.Give failed");
    }
    if (!semaphore_1.Take(0)) {
        Logger::Log("semaphore.Give didn't give");
    }
    if (semaphore_1.Count() != 0) {
        Logger::Log("semaphore.Take doesn't work");
    }
    auto semaphore_2 = std::move(semaphore_1);
    // now semaphore_1 calls will fail
    semaphore_2.Give();
    {
        std::lock_guard<RTOS::Semaphore> const exclusive(semaphore_2);
        if (semaphore_2.Count() != 0) {
            Logger::Log("semaphore_1 lock_guard take failed");
        }
    }
    if (semaphore_2.Count() != 1) {
        Logger::Log("semaphore_1 lock_guard didn't give back");
    }
    Logger::Log("Semaphores: {}",
        RTOS::Implementation::Primitive::GetSemaphoreCount());
}

void test_counter()
{
    const UBaseType_t counter_max = 4;
    const UBaseType_t counter_init = 2;
    RTOS::Counter counter { counter_max, counter_init, "counter" };
    Logger::Log("semaphore name: {}", counter.Name());
    Logger::Log("counter initial .Count: {}", counter.Count());
    Logger::Log("counter.MaxCount: {}", counter.MaxCount());
    if (counter.Count() != counter_init) {
        Logger::Log("counter init incorrect");
    }
    counter.Take(0);
    if (counter.Count() != counter_init - 1) {
        Logger::Log("counter.Take didn't take");
    }
    while (counter.Take(0)) { }
    if (counter.Count() != 0) {
        Logger::Log("counter.Take didn't take all");
    }
    while (counter.Count() < counter.MaxCount()) {
        counter.Give();
    }
    auto transferred_counter = std::move(counter);
    Logger::Log("Counters: {}",
        RTOS::Implementation::Primitive::GetSemaphoreCount());
}

void test_mutex()
{
    RTOS::Mutex mutex { "mutex" };
    Logger::Log("semaphore name: {}", mutex.Name());
    Logger::Log("mutex initial Count: {}", mutex.Count());
    if (mutex.GetHolder() == nullptr) {
        Logger::Log("mutex no initial holder");
    }
    if (!mutex.Take(0)) {
        Logger::Log("mutex Take failed");
    }
    if (mutex.GetHolder() == nullptr) {
        Logger::Log("mutex has no holder even though taken");
    }
    auto transferred_mutex = std::move(mutex);
    Logger::Log("Mutexes: {}",
        RTOS::Implementation::Primitive::GetSemaphoreCount());
}

void test_recursive_mutex()
{
    RTOS::RecursiveMutex recursive_mutex { "recursive mutex" };
    Logger::Log("semaphore name: {}", recursive_mutex.Name());
    const uint attempts = 3;
    for (uint attempt = 0; attempt < attempts; ++attempt) {
        recursive_mutex.Give();
    }
    for (uint attempt = 0; attempt < attempts; ++attempt) {
        if (!recursive_mutex.Take(0)) {
            Logger::Log("recursive_mutex.Take #{} failed", attempt + 1);
        }
    }
    auto transferred_recursive_mutex = std::move(recursive_mutex);
    Logger::Log("Recursive mutexes: {}",
        RTOS::Implementation::Primitive::GetSemaphoreCount());
}

void test_all_semaphore_types()
{
    test_semaphore();
    test_counter();
    test_mutex();
    test_recursive_mutex();

    RTOS::Semaphore sema1 { "asd" };
    RTOS::Semaphore sema2 { "asd" };
    RTOS::Semaphore sema3 { "asd" };
    RTOS::Semaphore sema4 { "asd" };
    RTOS::Semaphore sema5 { "asd" };
    RTOS::Semaphore sema6 { "asd" };
    RTOS::Semaphore sema7 { "asd" };
    RTOS::Semaphore sema8 { "asd" };

    Logger::Log("Semaphores: {}",
        RTOS::Implementation::Primitive::GetSemaphoreCount());
    Logger::Log("Queues: {}",
        RTOS::Implementation::Primitive::GetQueueCount());
    Logger::Log("Total queues/semaphores registered: {} / {}",
        RTOS::Implementation::Primitive::GetQueueCount() + RTOS::Implementation::Primitive::GetSemaphoreCount(),
        configQUEUE_REGISTRY_SIZE);
}
