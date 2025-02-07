#pragma once

#include <string>

#include "Logger.hpp"
#include "Primitive.hpp"

namespace RTOS {

template <typename ItemType>
class Queue : private Implementation::Primitive {
public:
    explicit Queue(UBaseType_t length, const char* name)
        : m_handle(xQueueCreate(length, sizeof(ItemType)))
        , m_name(name)
        , m_queue_length(length)
    {
        Logger::Log("[{}] '{}' created", TYPE, Name());
        Implementation::Primitive::IncrementQueueCount();
    }

    ~Queue()
    {
        if (m_handle != nullptr) {
            vQueueDelete(m_handle);
            Implementation::Primitive::DecrementQueueCount();
        }
    }

    // Move Constructor
    Queue(Queue&& queue) noexcept
        : m_handle(std::exchange(queue->m_handle, nullptr))
        , m_name(queue.m_name)
        , m_queue_length(queue.m_queue_length)
    {
    }

    Queue(const Queue& queue) = delete;
    Queue& operator=(const Queue& queue) = delete;
    Queue& operator=(Queue&& /*unused*/) = delete;

    bool Append(ItemType item, TickType_t wait_time_ticks) { return xQueueSendToBack(m_handle, &item, wait_time_ticks) == pdTRUE; }
    bool AppendFromISR(ItemType item, BaseType_t* higher_priority_task_woken) { return xQueueSendToBackFromISR(m_handle, &item, higher_priority_task_woken) == pdTRUE; }
    bool Peek(ItemType* item, TickType_t wait_time_ticks) { return xQueuePeek(m_handle, item, wait_time_ticks) == pdTRUE; }
    bool PeekFromISR(ItemType* item, BaseType_t* higher_priority_task_woken) { return xQueuePeekFromISR(m_handle, item, higher_priority_task_woken); }
    bool Prepend(ItemType item, TickType_t wait_time_ticks) { return xQueueSendToFront(m_handle, &item, wait_time_ticks); }
    bool PrependFromISR(ItemType item, BaseType_t* higher_priority_task_woken) { return xQueueSendToFrontFromISR(m_handle, &item, higher_priority_task_woken) == pdTRUE; }
    bool Receive(ItemType* item, TickType_t wait_time_ticks) { return xQueueReceive(m_handle, item, wait_time_ticks) == pdTRUE; }
    bool ReceiveFromISR(ItemType* item, BaseType_t* higher_priority_task_woken) { return xQueueReceiveFromISR(m_handle, item, higher_priority_task_woken); }

    [[nodiscard]] UBaseType_t ItemCount() const { return uxSemaphoreGetCount(m_handle); }
    [[nodiscard]] UBaseType_t Length() const { return m_queue_length; }
    [[nodiscard]] const char* Name() const { return m_name; }
    void Reset() { xQueueReset(m_handle); }

protected:
    explicit Queue(const char* variable_name)
        : m_handle(xQueueCreate(1, sizeof(ItemType)))
        , m_name(variable_name)
        , m_queue_length(1)
    {
        Implementation::Primitive::IncrementQueueCount();
    }

    [[nodiscard]] QueueHandle_t Handle() const { return m_handle; };

private:
    static constexpr const char* TYPE = "Queue";

    QueueHandle_t m_handle;
    const char* m_name;
    const UBaseType_t m_queue_length;
};

template <typename ItemType>
class Variable final : public Queue<ItemType> {
public:
    using Base = Queue<ItemType>;

    explicit Variable(const char* name)
        : Base(name)
    {
        Logger::Log("[{}] '{}' created", TYPE, Base::Name());
    }

    void Overwrite(ItemType item) { xQueueOverwrite(this->Handle(), &item); }
    void OverwriteFromISR(ItemType item, BaseType_t* higher_priority_task_woken) { xQueueOverwriteFromISR(this->Handle(), &item, higher_priority_task_woken); }

private:
    static constexpr const char* TYPE = "Variable";
};
} // namespace RTOS

/// TODO: remove
void test_rtos_queue();
