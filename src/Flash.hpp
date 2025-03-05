#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <boards/pico_w.h>
#include <hardware/flash.h>
#include <hardware/regs/addressmap.h>

class Flash {
    static constexpr uint8_t BYTE_EMPTY = 0xFF;
    static constexpr ptrdiff_t FLASH_BOTTOM = 0;
    static constexpr ptrdiff_t FLASH_TOP = FLASH_BOTTOM + PICO_FLASH_SIZE_BYTES;

    using LuxType = float;
    using ModeType = int8_t;
    using MotorTargetType = int8_t;
    using CRCType = uint16_t;

    static constexpr size_t LUX_LEN = sizeof(LuxType);
    static constexpr size_t LUX_TARGETS = 24 + 1; // 24 hours + static
    static constexpr size_t LUX_MAP_LEN = LUX_LEN * LUX_TARGETS;
    static constexpr size_t SYSTEM_MODE_LEN = sizeof(ModeType);
    static constexpr size_t MOTOR_TARGET_LEN = sizeof(MotorTargetType);
    static constexpr size_t CRC_LEN = sizeof(CRCType);

    static constexpr size_t SETTINGS_LEN = CRC_LEN + LUX_MAP_LEN + SYSTEM_MODE_LEN + MOTOR_TARGET_LEN;

    static constexpr ptrdiff_t SETTINGS_CAPACITY = PICO_FLASH_SIZE_BYTES / 4;
    static constexpr ptrdiff_t FLASH_SETTINGS_BOTTOM = FLASH_BOTTOM + PICO_FLASH_SIZE_BYTES - SETTINGS_CAPACITY;
    static constexpr ptrdiff_t FLASH_SETTINGS_TOP = FLASH_SETTINGS_BOTTOM + SETTINGS_CAPACITY;

    static_assert(SETTINGS_LEN * 2 < SETTINGS_CAPACITY);

    static constexpr size_t SETTINGS_MEMORY_SIZE = (SETTINGS_LEN + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE * FLASH_PAGE_SIZE;

    static constexpr ptrdiff_t FLASH_SETTINGS_A = SETTINGS_CAPACITY * 3;
    static constexpr ptrdiff_t FLASH_SETTINGS_B = FLASH_SETTINGS_A + SETTINGS_CAPACITY / 2;

    static const uint8_t* FlashPointer(const ptrdiff_t flash_address) { return reinterpret_cast<const uint8_t*>(XIP_BASE + flash_address); }

public:
    enum Lux : uint8_t {
        // clang-format off
        H00, H01, H02, H03, H04, H05, H06, H07, H08, H09, H10, H11,
        H12, H13, H14, H15, H16, H17, H18, H19, H20, H21, H22, H23,
        LUX_STATIC,
        // clang-format on
    };
    enum SystemMode : uint8_t {
        bAUTO = 0b01,
        bAUTO_HOURLY = 0b10,
    };

    struct Settings {
        LuxType lux_targets[LUX_TARGETS];
        ModeType sys_mode;
        MotorTargetType motor_target;
    };

    void EraseSettings();

protected:
    explicit Flash();

    struct ProgramParameters {
        const Settings settings;
        struct {
            bool a;
            bool b;
        } ok;
        std::array<uint8_t, SETTINGS_MEMORY_SIZE>* buffer;
    };

    void Program() const;
    [[nodiscard]] Settings& AccessSettings() const { return m_settings; }

private:
    static bool SettingsMemoryIsSafe();
    void UpdateItems();

    [[nodiscard]] static Settings FactorySettings();
    void WriteSettingsBlocking(const Settings& items);

    static void Erase(void* settings_flash_address);
    static void EraseAndProgram(void* program_parameters);
    bool UpdateFrom(ptrdiff_t settings_address);
    static uint16_t CRC16(const uint8_t* buf, size_t length);

    mutable Settings m_settings;
    bool m_safe = true;
    mutable std::array<uint8_t, SETTINGS_MEMORY_SIZE> m_buffer;
};
