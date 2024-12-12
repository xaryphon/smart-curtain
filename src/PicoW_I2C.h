//
// Created by Keijo Länsikunnas on 10.9.2024.
// Renovated by Smart Curtains.
//

#pragma once

#include "Fmutex.h"
#include "FreeRTOS.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "semphr.h"
#include "task.h"

class PicoW_I2C {
public:
    enum sda0_pin {
        SDA0_0 = 0,
        SDA0_4 = 4,
        SDA0_8 = 8,
        SDA0_12 = 12,
        SDA0_16 = 16,
        SDA0_20 = 20,
    };

    enum scl0_pin {
        SCL0_1 = 1,
        SCL0_5 = 5,
        SCL0_9 = 9,
        SCL0_13 = 13,
        SCL0_17 = 17,
        SCL0_21 = 21,
    };

    enum sda1_pin {
        SDA1_2 = 2,
        SDA1_6 = 6,
        SDA1_10 = 10,
        SDA1_14 = 14,
        SDA1_18 = 18,
        SDA1_26 = 26
    };

    enum scl1_pin {
        SCL1_3 = 3,
        SCL1_7 = 7,
        SCL1_11 = 11,
        SCL1_15 = 15,
        SCL1_19 = 19,
        SCL1_27 = 27
    };

    explicit PicoW_I2C(sda0_pin sda, scl0_pin scl, uint baudrate);
    explicit PicoW_I2C(sda1_pin sda, scl1_pin scl, uint baudrate);
    void Init();
    PicoW_I2C(const PicoW_I2C&) = delete;
    uint Write(uint8_t addr, const uint8_t* buffer, uint length);
    uint Read(uint8_t addr, uint8_t* buffer, uint length);
    uint Transaction(uint8_t addr, const uint8_t* wbuffer, uint wlength, uint8_t* rbuffer, uint rlength);

private:
    uint m_sda;
    uint m_scl;
    uint m_irqn;
    irq_handler_t m_irq_handler;
    i2c_inst* m_i2c;
    uint m_baudrate;
    TaskHandle_t m_task_to_notify { nullptr };
    Fmutex m_access;
    const uint8_t* m_wbuf { nullptr };
    uint m_wctr { 0 };
    uint8_t* m_rbuf { 0 };
    uint m_rctr { 0 };
    uint m_rcnt { 0 };

    void FillTxFifo();
    void FillRxFifo();
    void ISR();
    static void IRQ_I2C0();
    static void IRQ_I2C1();

    static PicoW_I2C* i2c0_instance;
    static PicoW_I2C* i2c1_instance;
};
