#include "Queue.hpp"

/// TODO: remove -- whole file can be deleted
#include <FreeRTOS.h>
#include <task.h>
struct TestItem {
    uint asd;
};

struct TestParams {
    RTOS::Queue<TestItem>* queue;
    RTOS::Variable<TestItem>* variable;
};

void queue_task(void* params)
{
    const TickType_t delay = 10;
    auto* queue = static_cast<TestParams*>(params)->queue;

    Logger::Log("[Q_TEST] name: {}", queue->Name());
    vTaskDelay(delay);
    TestItem app { 2 };
    if (!queue->Append(app, pdMS_TO_TICKS(30))) {
        Logger::Log("[Q_TEST] failed to append");
        vTaskDelay(delay);
    }
    TestItem rec;
    if (!queue->Receive(&rec, pdMS_TO_TICKS(123))) {
        Logger::Log("[Q_TEST] failed to receive");
        vTaskDelay(delay);
    } else if (rec.asd != app.asd) {
        Logger::Log("[Q_TEST] rec: {} != app: {}", rec.asd, app.asd);
        vTaskDelay(delay);
    }
    for (uint append = 0; append < queue->Length() + 1; ++append) {
        app.asd = append;
        if (!queue->Append(app, pdMS_TO_TICKS(30)) && append < queue->Length()) {
            Logger::Log("[Q_TEST] append # {} failed", append + 1);
            vTaskDelay(delay);
        }
    }
    if (queue->ItemCount() != queue->Length()) {
        Logger::Log("[Q_TEST] items: {} != length: {}", queue->ItemCount(), queue->Length());
        vTaskDelay(delay);
    }
    if (queue->Append(app, pdMS_TO_TICKS(30))) {
        Logger::Log("[Q_TEST] append should have failed");
        vTaskDelay(delay);
    }
    if (queue->ItemCount() != queue->Length()) {
        Logger::Log("[Q_TEST] items: {} != length: {}", queue->ItemCount(), queue->Length());
        vTaskDelay(delay);
    }
    if (!queue->Peek(&rec, pdMS_TO_TICKS(123))) {
        Logger::Log("[Q_TEST] failed to peek");
        vTaskDelay(delay);
    }
    TestItem rec2;
    if (!queue->Receive(&rec2, pdMS_TO_TICKS(123))) {
        Logger::Log("[Q_TEST] failed to receive");
        vTaskDelay(delay);
    } else if (rec.asd != rec2.asd) {
        Logger::Log("[Q_TEST] rec: {} != rec2: {}", rec.asd, rec2.asd);
        vTaskDelay(delay);
    }
    if (!queue->Prepend(app, pdMS_TO_TICKS(2))) {
        Logger::Log("[Q_TEST] prepend failed");
        vTaskDelay(delay);
    } else if (!queue->Peek(&rec, pdMS_TO_TICKS(123))) {
        Logger::Log("[Q_TEST] peek failed");
        vTaskDelay(delay);
    } else if (rec.asd != app.asd) {
        Logger::Log("[Q_TEST] rec: {} != rec2: {}", rec.asd, rec2.asd);
        vTaskDelay(delay);
    }
    for (uint receive = 0; receive < queue->Length() + 1; ++receive) {
        if (!queue->Receive(&rec, 0) && receive < queue->Length()) {
            Logger::Log("[Q_TEST] receive # {} failed", receive + 1);
            vTaskDelay(delay);
        }
    }
    if (queue->ItemCount() != 0) {
        Logger::Log("[Q_TEST] items: {} ; should be 0", queue->ItemCount());
        vTaskDelay(delay);
    }

    auto* variable = static_cast<TestParams*>(params)->variable;

    Logger::Log("[V_TEST] name: {}", variable->Name());
    vTaskDelay(delay);
    if (!variable->Append(app, pdMS_TO_TICKS(30))) {
        Logger::Log("[V_TEST] failed to append");
        vTaskDelay(delay);
    }
    if (!variable->Peek(&rec, pdMS_TO_TICKS(123))) {
        Logger::Log("[V_TEST] failed to peek");
        vTaskDelay(delay);
    }
    variable->Overwrite(app);
    if (!variable->Receive(&rec2, pdMS_TO_TICKS(123))) {
        Logger::Log("[V_TEST] failed to receive");
        vTaskDelay(delay);
    } else if (rec.asd != rec2.asd) {
        Logger::Log("[V_TEST] rec: {} != rec2: {}", rec.asd, rec2.asd);
        vTaskDelay(delay);
    }
    if (variable->ItemCount() != 0) {
        Logger::Log("[V_TEST] items != 0", variable->ItemCount());
        vTaskDelay(delay);
    }
    if (!variable->Prepend(app, pdMS_TO_TICKS(2))) {
        Logger::Log("[V_TEST] prepend failed");
        vTaskDelay(delay);
    } else if (!variable->Peek(&rec, pdMS_TO_TICKS(123))) {
        Logger::Log("[V_TEST] peek failed");
        vTaskDelay(delay);
    } else if (rec.asd != app.asd) {
        Logger::Log("[V_TEST] rec: {} != rec2: {}", rec.asd, rec2.asd);
        vTaskDelay(delay);
    }
    if (variable->ItemCount() != variable->Length()) {
        Logger::Log("[V_TEST] items: {} ; should be 0", variable->ItemCount());
        vTaskDelay(delay);
    }

    vTaskDelay(portMAX_DELAY);
}

void test_rtos_queue()
{
    auto* params = new TestParams {
        .queue = new RTOS::Queue<TestItem>(4, "Test_Q"),
        .variable = new RTOS::Variable<TestItem>("Test_V")
    };

    xTaskCreate(queue_task, "QueueTest", 1000, static_cast<void*>(params), tskIDLE_PRIORITY + 2, nullptr);
}
