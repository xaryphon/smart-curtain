//
// Created by Keijo Länsikunnas on 10.9.2024.
// Renovated by Smart Curtains.
//

#pragma once

#include <FreeRTOS.h>
#include <hardware/i2c.h>
#include <pico/stdlib.h>
#include <task.h>

#include "Semaphore.hpp"

class I2C {
public:
    enum class SDA0 : uint {
        PIN_0 = 0,
        PIN_4 = 4,
        PIN_8 = 8,
        PIN_12 = 12,
        PIN_16 = 16,
        PIN_20 = 20,
    };

    enum class SCL0 : uint {
        PIN_1 = 1,
        PIN_5 = 5,
        PIN_9 = 9,
        PIN_13 = 13,
        PIN_17 = 17,
        PIN_21 = 21,
    };

    enum class SDA1 : uint {
        PIN_2 = 2,
        PIN_6 = 6,
        PIN_10 = 10,
        PIN_14 = 14,
        PIN_18 = 18,
        PIN_26 = 26,
    };

    enum class SCL1 : uint {
        PIN_3 = 3,
        PIN_7 = 7,
        PIN_11 = 11,
        PIN_15 = 15,
        PIN_19 = 19,
        PIN_27 = 27,
    };

    explicit I2C(SDA1 sda, SCL1 scl, uint baudrate);
    explicit I2C(SDA0 sda, SCL0 scl, uint baudrate);
    I2C(const I2C&) = delete;
    I2C(I2C&&) = delete;
    I2C& operator=(const I2C&) = delete;
    I2C& operator=(I2C&&) = delete;

    void Init();
    uint Write(uint8_t addr, const uint8_t* buffer, uint length, TickType_t timeout_ticks);
    uint Read(uint8_t addr, uint8_t* buffer, uint length, TickType_t timeout_ticks);
    uint Transaction(uint8_t addr, const uint8_t* wbuffer, uint wlength, uint8_t* rbuffer, uint rlength, TickType_t timeout_ticks);

private:
    uint m_sda;
    uint m_scl;
    uint m_baudrate;
    uint m_irqn;
    irq_handler_t m_irq_handler;
    i2c_inst* m_i2c;

    const uint8_t* m_wbuf = nullptr;
    uint m_wctr = 0;
    uint8_t* m_rbuf = nullptr;
    uint m_rctr = 0;
    uint m_rcnt = 0;

    TaskHandle_t m_task_to_notify = nullptr;
    RTOS::Mutex m_access;

    void FillTxFifo();
    void FillRxFifo();
    void ISR();
    static void IRQ_I2C0();
    static void IRQ_I2C1();

    static I2C* i2c0_instance;
    static I2C* i2c1_instance;
};
