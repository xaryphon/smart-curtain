#include "Storage.hpp"

#include <FreeRTOS.h>
#include <task.h>

#include "Logger.hpp"
#include "config.h"

RTOS::Semaphore* Storage::s_update_lux = nullptr;
RTOS::Semaphore* Storage::s_lux_target_auto = nullptr;

Storage::Storage(const Parameters& parameters)
    : m_write_access(RTOS::Mutex { "FlashWriteAccess" })
    , m_lux_target(parameters.lux_target)
    , m_http_notify(parameters.http_notify)
    , m_rtc(parameters.rtc)

{
    if (xTaskCreate(
            TASK_KONDOM(Storage, Task),
            parameters.task_name,
            TaskStackSize::STORAGE,
            this,
            TaskPriority::STORAGE,
            &m_handle)
        == pdPASS) {
        Logger::Log("Created task [{}]", parameters.task_name);
    } else {
        Logger::Log("Warning: Failed to create task [{}]", parameters.task_name);
    }
    s_lux_target_auto = parameters.lux_target_auto;
    s_update_lux = parameters.update_lux_target;
}

std::string Storage::StringifySettings(const Settings& settings)
{
    std::string settings_str = "Settings:";
    const bool on_auto = settings.sys_mode & Flash::bAUTO;
    settings_str += fmt::format("\nAuto: {}", on_auto ? "ON" : "OFF");
    if (on_auto) {
        const bool auto_hourly = settings.sys_mode & Flash::bAUTO_HOURLY;
        settings_str += fmt::format("\nAutoMode: {}", auto_hourly ? "HOURLY" : "STATIC");
    }
    settings_str += "\nLux Targets:";
    for (size_t hour = Flash::H00; hour <= Flash::H23; ++hour) {
        settings_str += fmt::format("\n - H{:0>2}: {:>6}", hour, settings.lux_targets[hour]);
    }
    settings_str += fmt::format("\n - Static: {:>6}", settings.lux_targets[Flash::LUX_STATIC]);
    settings_str += fmt::format("\nStep Target: {:>3} %", settings.motor_target);
    return settings_str;
}

void Storage::Task()
{
    Logger::Log("Initiated");
    ReadOnlyAccessLocked(portMAX_DELAY, [&](const Settings& settings) {
        if (settings.sys_mode & bAUTO_HOURLY) {
            s_lux_target_auto->Give();
        }
        Logger::Log("Flash {}", StringifySettings(settings));
    });
    m_rtc->SetAlarm(ALARM_TIME, AlarmEvent);
    while (true) {
        s_update_lux->Take(portMAX_DELAY);
        const datetime_t time = m_rtc->GetDatetime();
        const int8_t hour_A = time.hour;
        const int8_t hour_B = time.hour < 23 ? hour_A + 1 : 00;

        float target_A = 0;
        float target_B = 0;

        ReadOnlyAccessLocked(portMAX_DELAY, [&](const Settings& settings) {
            target_A = settings.lux_targets[hour_A];
            target_B = settings.lux_targets[hour_B];
        });

        const float hour_fraction = static_cast<float>(time.min) / 60.0;
        const float new_target = target_A * (1 - hour_fraction) + target_B * hour_fraction;

        m_lux_target->Overwrite(new_target);
        Logger::Log("Updated Lux target to {}", new_target);
        m_http_notify->Give();
    }
}

void Storage::AlarmEvent()
{
    BaseType_t hptw = pdFALSE;
    if (s_lux_target_auto->CountFromISR() == 1) {
        s_update_lux->GiveFromISR(&hptw);
    }
    portYIELD_FROM_ISR(hptw);
}

/// TODO: remove

void test_storage_task(void* params)
{
    //auto* update_lux_target = static_cast<TestStorageParams*>(params)->update_lux_target;
    //auto* lux_target_auto = static_cast<TestStorageParams*>(params)->lux_target_auto;
    auto* storage = static_cast<TestStorageParams*>(params)->storage;

    Flash::Settings new_settings {};

    for (size_t lux = Flash::H00; lux <= Flash::LUX_STATIC; ++lux) {
        new_settings.lux_targets[lux] = rand() % 1000;
    }
    new_settings.sys_mode = rand() % 4;
    new_settings.motor_target = rand() % 101;

    Logger::Log("random settings\n{}", storage->StringifySettings(new_settings));
    vTaskDelay(pdMS_TO_TICKS(1000));

    Flash::Settings flash_settings {};
    storage->ReadOnlyAccessLocked(portMAX_DELAY, [&](const Flash::Settings& settings) {
        flash_settings = settings;
    });

    Logger::Log("flash settings\n{}", storage->StringifySettings(flash_settings));
    vTaskDelay(pdMS_TO_TICKS(1000));

    storage->WriteAccessAndProgramLocked(portMAX_DELAY, [&](Flash::Settings& settings) {
        settings = new_settings;
    });

    storage->ReadOnlyAccessLocked(portMAX_DELAY, [&](const Flash::Settings& settings) {
        flash_settings = settings;
    });

    Logger::Log("programmed settings\n{}", storage->StringifySettings(flash_settings));
    vTaskDelay(pdMS_TO_TICKS(1000));

    storage->EraseSettings();

    vTaskDelay(portMAX_DELAY);
}

void test_storage(TestStorageParams* params)
{
    srand(time(nullptr));
    xTaskCreate(test_storage_task, "TestStorage", 1024, params, tskIDLE_PRIORITY + 3, nullptr);
}
