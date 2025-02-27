//
// Created by Keijo Länsikunnas on 10.9.2024.
// Renovated by Smart Curtains.
//

#include "I2C.hpp"

#include <cstdio>
#include <mutex>

// #define DEBUG_PRINT_TASK
#ifdef DEBUG_PRINT_TASK
#include "Logger.hpp"
#endif

I2C* I2C::i2c0_instance = nullptr;
I2C* I2C::i2c1_instance = nullptr;

void I2C::IRQ_I2C0()
{
    if (i2c0_instance != nullptr) {
        i2c0_instance->ISR();
    } else {
        irq_set_enabled(I2C0_IRQ, false);
    }
}

void I2C::IRQ_I2C1()
{
    if (i2c1_instance != nullptr) {
        i2c1_instance->ISR();
    } else {
        irq_set_enabled(I2C1_IRQ, false); // disable interrupt if we don't have instance
    }
}

I2C::I2C(const SDA0 sda0, const SCL0 scl0, const uint baudrate)
    : m_sda(static_cast<uint>(sda0))
    , m_scl(static_cast<uint>(scl0))
    , m_baudrate(baudrate)
    , m_irqn(I2C0_IRQ)
    , m_irq_handler(IRQ_I2C0)
    , m_i2c(i2c0)
    , m_access("I2C0-Access")
{
    if (i2c0_instance != nullptr) {
        abort();
    }
    i2c0_instance = this;
    Init();
}

I2C::I2C(const SDA1 sda1, const SCL1 scl1, const uint baudrate)
    : m_sda(static_cast<uint>(sda1))
    , m_scl(static_cast<uint>(scl1))
    , m_baudrate(baudrate)
    , m_irqn(I2C1_IRQ)
    , m_irq_handler(IRQ_I2C1)
    , m_i2c(i2c1)
    , m_access("I2C1-Access")
{
    if (i2c1_instance != nullptr) {
        abort();
    }
    i2c1_instance = this;
    Init();
}

void I2C::Init() // NOLINT(readability-make-member-function-const)
{
    gpio_init(m_scl);
    gpio_pull_up(m_scl);
    gpio_init(m_sda);
    gpio_pull_up(m_sda);
    irq_set_enabled(m_irqn, false);
    irq_set_exclusive_handler(m_irqn, m_irq_handler);
    i2c_init(m_i2c, m_baudrate);
    gpio_set_function(m_sda, GPIO_FUNC_I2C);
    gpio_set_function(m_scl, GPIO_FUNC_I2C);
    // Set FIFO watermarks
    m_i2c->hw->tx_tl = 0; // TX_FIFO watermark to 0
    m_i2c->hw->rx_tl = 14; // RX_FIFO watermark to 15 (manual gives impression that level is one higher than reg value)
}

void I2C::FillTxFifo()
{
#ifdef DEBUG_PRINT_TASK
    int fill { 0 };
#endif
    while (m_wctr > 0 && i2c_get_write_available(m_i2c) > 0) {
        const bool last = m_wctr == 1;
        const bool stop = m_rctr == 0;
        m_i2c->hw->data_cmd =
            // There may be a restart needed instead of (stop)-start
            bool_to_bit(m_i2c->restart_on_next) << I2C_IC_DATA_CMD_RESTART_LSB |
            // stop is needed if this is last write and there is no read after this
            bool_to_bit(last && stop) << I2C_IC_DATA_CMD_STOP_LSB | *m_wbuf++;
        // clear restart after first write
        if (m_i2c->restart_on_next) {
            m_i2c->restart_on_next = false;
        }
        --m_wctr;

        if (last && !stop) {
            m_i2c->restart_on_next = true;
        }
#ifdef DEBUG_PRINT_TASK
        ++fill;
#endif
    }
#ifdef DEBUG_PRINT_TASK
    Logger::Log("tx_fill: {}", fill);
#endif
}

void I2C::FillRxFifo()
{
#ifdef DEBUG_PRINT_TASK
    int fill { 0 };
#endif

    while (m_rctr > 0 && i2c_get_write_available(m_i2c) > 0) {
        bool const last = m_rctr == 1;
        m_i2c->hw->data_cmd =
            // There may be a restart needed instead of (stop)-start
            bool_to_bit(m_i2c->restart_on_next) << I2C_IC_DATA_CMD_RESTART_LSB |
            // Read is always at the last transaction so stop is issued after last command
            bool_to_bit(last) << I2C_IC_DATA_CMD_STOP_LSB | I2C_IC_DATA_CMD_CMD_BITS; // -> 1 for read;
        // clear restart-bit after first command
        if (m_i2c->restart_on_next) {
            m_i2c->restart_on_next = false;
        }
        --m_rctr;
#ifdef DEBUG_PRINT_TASK
        ++fill;
#endif
    }

#ifdef DEBUG_PRINT_TASK
    Logger::Log("rx_fill: {}", fill);
#endif
}

uint I2C::Write(const uint8_t addr, const uint8_t* buffer, const uint length, const TickType_t timeout_ticks)
{
    return Transaction(addr, buffer, length, nullptr, 0, timeout_ticks);
}

uint I2C::Read(const uint8_t addr, uint8_t* buffer, const uint length, const TickType_t timeout_ticks)
{
    return Transaction(addr, nullptr, 0, buffer, length, timeout_ticks);
}

uint I2C::Transaction(const uint8_t addr, const uint8_t* wbuffer, const uint wlength, uint8_t* rbuffer, const uint rlength, const TickType_t timeout_ticks)
{
    assert((wbuffer && wlength > 0) || (rbuffer && rlength > 0));
    std::lock_guard const exclusive(m_access);
    m_task_to_notify = xTaskGetCurrentTaskHandle();

    m_i2c->hw->enable = 0;
    m_i2c->hw->tar = addr;
    m_i2c->hw->enable = 1;
    m_i2c->hw->intr_mask = I2C_IC_INTR_MASK_M_STOP_DET_BITS | I2C_IC_INTR_MASK_M_TX_EMPTY_BITS | I2C_IC_INTR_MASK_M_RX_FULL_BITS;
    m_i2c->restart_on_next = false;
    // setup transfer
    m_wbuf = wbuffer;
    m_wctr = wlength;
    m_rbuf = rbuffer;
    m_rctr = rlength; // for writing read commands
    m_rcnt = rlength; // for counting received bytes

    // write is done first if we have a combined transaction
    if (m_wctr > 0) {
        FillTxFifo();
    } else {
        FillRxFifo();
    }

    uint count = wlength + rlength;
    // enable interrupts
    irq_set_enabled(m_irqn, true);
    // wait for stop interrupt
    if (ulTaskNotifyTake(pdTRUE, timeout_ticks) == 0) {
        // timed out
        count = 0;
    } else {
        count -= m_rcnt + m_wctr;
    }
    irq_set_enabled(m_irqn, false);

    // if count !=  sum of lengths transaction failed
    return count;
}

void I2C::ISR()
{
    BaseType_t hpw = pdFALSE;
#ifdef DEBUG_PRINT_TASK
    /*
    DebugTask::debug("%d %d %d %d",
        !!(i2c->hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_STOP_DET_BITS),
        !!(i2c->hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_RX_FULL_BITS),
        !!(i2c->hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_TX_EMPTY_BITS),
        !!(i2c->hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_RX_OVER_BITS));
    */
#endif
    // See if we have active read and data available.
    // Read is paced writes to command register in master mode
    // so we don't need RX_FULL interrupt.
    // We just empty the rxfifo before additional writes to cmd register
    //  Testing (i2c->hw->status & I2C_IC_STATUS_RFNE_BITS) works also
#ifdef DEBUG_PRINT_TASK
    int fill { 0 };
#endif
    while (m_rcnt > 0 && m_i2c->hw->rxflr > 0) {
        *m_rbuf++ = static_cast<uint8_t>(m_i2c->hw->data_cmd);
        --m_rcnt;
#ifdef DEBUG_PRINT_TASK
        ++fill;
#endif
    }
#ifdef DEBUG_PRINT_TASK
    // DebugTask::debug("read: %d, %d", fill, rcnt);
#endif

    if ((m_i2c->hw->intr_stat & I2C_IC_INTR_MASK_M_TX_EMPTY_BITS) != 0U) {
        // write commands go first
        if (m_wctr > 0) {
            FillTxFifo();
        } else if (m_rctr > 0) {
            FillRxFifo();
        }
        if (m_wctr == 0 && m_rctr == 0) {
            // Disable TX_EMPTY interrupt when we are done with write and read commands
            // keep receive and stop interrupts active to catch received bytes and stop
            m_i2c->hw->intr_mask = I2C_IC_INTR_MASK_M_STOP_DET_BITS | I2C_IC_INTR_MASK_M_RX_FULL_BITS;
        }
    }

    // notify if we are done - hw should also issue a stop if transaction is aborted
    if ((m_i2c->hw->intr_stat & I2C_IC_INTR_MASK_M_STOP_DET_BITS) != 0U) {
        m_i2c->hw->intr_mask = 0; // mask all interrupts
        (void)m_i2c->hw->clr_stop_det;
        xTaskNotifyFromISR(m_task_to_notify, 1, eSetValueWithOverwrite, &hpw);
    }
    portYIELD_FROM_ISR(hpw);
}
