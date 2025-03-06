#include "Flash.hpp"

#include <hardware/flash.h>
#include <pico/flash.h>

#include "Logger.hpp"

Flash::Flash()
    : m_settings(FactorySettings())
{
    if (!SettingsMemoryIsSafe()) {
        Logger::Log("[Flash] Disabling flash");
        m_safe = false;
        return;
    }
    UpdateItems();
}

bool Flash::UpdateFrom(ptrdiff_t settings_address)
{
    if (!m_safe) {
        Logger::Log("[Flash] Flash disabled. Cannot UpdateFrom(0x{:X})", settings_address);
        return false;
    }
    if (settings_address != FLASH_SETTINGS_A && settings_address != FLASH_SETTINGS_B) {
        Logger::Log("[Flash] Error: Invalid settings address [0x{:X}]", settings_address);
        return false;
    }
    auto settings_pointer = FlashPointer(settings_address);
    if (CRC16(settings_pointer, SETTINGS_LEN) != 0) {
        Logger::Log("[Flash] Warning: CRC {} failed", settings_address == FLASH_SETTINGS_A ? "A" : "B");
        return false;
    }
    std::memcpy(&m_settings.lux_targets, settings_pointer, LUX_MAP_LEN);
    settings_pointer += LUX_MAP_LEN;
    std::memcpy(&m_settings.sys_mode, settings_pointer, SYSTEM_MODE_LEN);
    settings_pointer += SYSTEM_MODE_LEN;
    std::memcpy(&m_settings.motor_target, settings_pointer, MOTOR_TARGET_LEN);
    settings_pointer += MOTOR_TARGET_LEN;

    Logger::Log("[Flash] Updated from flash {}", settings_address == FLASH_SETTINGS_A ? "A" : "B");
    return true;
}

void Flash::EraseSettings()
{
    flash_safe_execute(Erase, nullptr, UINT32_MAX);
}

bool Flash::SettingsMemoryIsSafe()
{
    static constexpr ptrdiff_t FLASH_MIDDLE = FLASH_BOTTOM + PICO_FLASH_SIZE_BYTES / 2;

    uint8_t byte = 0;
    int program_percentage = 0;

    static constexpr ptrdiff_t start = FLASH_SETTINGS_BOTTOM - 1;
    for (ptrdiff_t flash_address = start; flash_address > FLASH_BOTTOM; --flash_address) {
        byte = *FlashPointer(flash_address);
        if (byte != BYTE_EMPTY) {
            program_percentage = flash_address * 100 / PICO_FLASH_SIZE_BYTES;
            if (flash_address == start) {
                Logger::Log("[Flash] Error: Settings compromised; program memory reaches to ~{} % ; address [0x{:X}]",
                    program_percentage, flash_address);
                return false;
            }
            if (flash_address > FLASH_MIDDLE) {
                Logger::Log("[Flash] Warning: Settings safety range breached [0x{:X} - 0x{:X}]. Program memory reaches to ~{} %",
                    FLASH_MIDDLE, FLASH_SETTINGS_BOTTOM, program_percentage);
                return true;
            }
            break;
        }
    }
    Logger::Log("[Flash] Settings memory safe. Program reaches to ~{} %", program_percentage);
    return true;
}

void Flash::UpdateItems()
{
    if (UpdateFrom(FLASH_SETTINGS_A)) {
        return;
    }
    if (UpdateFrom(FLASH_SETTINGS_B)) {
        return;
    }
    Logger::Log("[Flash] Failed to read settings from memory. Reverting to Factory Settings");
}

Flash::Settings Flash::FactorySettings()
{
    return {
        .lux_targets = {
            [H00] = 0,
            [H01] = 0,
            [H02] = 0,
            [H03] = 0,
            [H04] = 0,
            [H05] = 0,
            [H06] = 0,
            [H07] = 400,
            [H08] = 400,
            [H09] = 0,
            [H10] = 0,
            [H11] = 0,
            [H12] = 0,
            [H13] = 0,
            [H14] = 0,
            [H15] = 0,
            [H16] = 400,
            [H17] = 400,
            [H18] = 350,
            [H19] = 300,
            [H20] = 200,
            [H21] = 100,
            [H22] = 0,
            [H23] = 0,
            [LUX_STATIC] = 200,
        },
        .sys_mode = bAUTO /* | bAUTO_HOURLY */,
        .motor_target = 0,
    };
}

void Flash::Program() const
{
    if (!m_safe) {
        Logger::Log("[Flash] Flash disabled. Cannot Program()");
        return;
    }
    ProgramParameters program_parameters {
        .settings = m_settings,
        .ok = { true, true },
        .buffer = &m_buffer,
    };
    flash_safe_execute(EraseAndProgram, &program_parameters, UINT32_MAX);
    if (!program_parameters.ok.a) {
        Logger::Log("[Flash] Error: CRC for settings A failed after write");
    }
    if (!program_parameters.ok.b) {
        Logger::Log("[Flash] Error: CRC for settings B failed after write");
    }
}

void Flash::Erase(void* params)
{
    (void)params;
    flash_range_erase(FLASH_SETTINGS_A, SETTINGS_MEMORY_SIZE);
    flash_range_erase(FLASH_SETTINGS_B, SETTINGS_MEMORY_SIZE);
}

void Flash::EraseAndProgram(void* program_parameters)
{
    const auto settings = static_cast<ProgramParameters*>(program_parameters)->settings;
    auto* ok = &static_cast<ProgramParameters*>(program_parameters)->ok;
    auto* buffer = static_cast<ProgramParameters*>(program_parameters)->buffer;

    buffer->fill(BYTE_EMPTY);

    ptrdiff_t index = 0;
    std::memcpy(buffer->data() + index, settings.lux_targets, LUX_MAP_LEN);
    index += LUX_MAP_LEN;
    std::memcpy(buffer->data() + index, &settings.sys_mode, SYSTEM_MODE_LEN);
    index += SYSTEM_MODE_LEN;
    std::memcpy(buffer->data() + index, &settings.motor_target, MOTOR_TARGET_LEN);
    index += MOTOR_TARGET_LEN;

    uint16_t crc = CRC16(buffer->data(), SETTINGS_LEN - CRC_LEN);
    crc = crc >> 8 | crc << 8;
    std::memcpy(buffer->data() + index, &crc, CRC_LEN);

    flash_range_erase(FLASH_SETTINGS_A, SETTINGS_MEMORY_SIZE);
    flash_range_program(FLASH_SETTINGS_A, buffer->data(), SETTINGS_MEMORY_SIZE);
    ok->a = CRC16(FlashPointer(FLASH_SETTINGS_A), SETTINGS_LEN) == 0;

    flash_range_erase(FLASH_SETTINGS_B, SETTINGS_MEMORY_SIZE);
    flash_range_program(FLASH_SETTINGS_B, buffer->data(), SETTINGS_MEMORY_SIZE);
    ok->b = CRC16(FlashPointer(FLASH_SETTINGS_B), SETTINGS_LEN) == 0;
}

uint16_t Flash::CRC16(const uint8_t* buf, size_t length)
{
    uint16_t crc = 0xFFFF;
    while (length--) {
        uint8_t x = crc >> 8 ^ *buf++;
        x ^= x >> 4;
        crc = (crc << 8) ^ (static_cast<uint16_t>(x << 5) ^ static_cast<uint16_t>(x));
    }
    return crc;
}
