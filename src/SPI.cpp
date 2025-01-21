#include "SPI.h"

#include <cstdio>

#include <FreeRTOS.h>
#include <task.h>

#include "config.h"

SPI* SPI::isr0 = nullptr;
SPI* SPI::isr1 = nullptr;

void SPI::ISR0()
{
    if (isr0 != nullptr) {
        isr0->ISR();
    } else {
        irq_set_enabled(SPI0_IRQ, false);
    }
}

void SPI::ISR1()
{
    if (isr1 != nullptr) {
        isr1->ISR();
    } else {
        irq_set_enabled(SPI1_IRQ, false);
    }
}

SPI::SPI(RX0 pin_rx, TX0 pin_tx, SCK0 pin_sck, uint baud_rate)
    : SPI(spi0, static_cast<uint>(pin_rx), static_cast<uint>(pin_tx), static_cast<uint>(pin_sck), static_cast<uint>(baud_rate), SPI0_IRQ, ISR0)
{
    assert(isr0 == nullptr);
    isr0 = this;
}

SPI::SPI(RX1 pin_rx, TX1 pin_tx, SCK1 pin_sck, uint baud_rate)
    : SPI(spi1, static_cast<uint>(pin_rx), static_cast<uint>(pin_tx), static_cast<uint>(pin_sck), static_cast<uint>(baud_rate), SPI1_IRQ, ISR1)
{
    assert(isr1 == nullptr);
    isr1 = this;
}

SPI::SPI(spi_inst_t* inst, uint pin_rx, uint pin_tx, uint pin_sck, uint baud_rate, uint irqn, irq_handler_t irq_handler)
    : m_inst(inst)
    , m_baud_rate(spi_init(inst, baud_rate))
    , m_irqn(irqn)
    , m_mutex(xSemaphoreCreateMutex())
    , m_notify_semaphore(xSemaphoreCreateBinary())
{
    gpio_set_function(pin_rx, GPIO_FUNC_SPI);
    gpio_set_function(pin_tx, GPIO_FUNC_SPI);
    gpio_set_function(pin_sck, GPIO_FUNC_SPI);
    irq_set_enabled(m_irqn, false);
    irq_set_exclusive_handler(m_irqn, irq_handler);
}

uint SPI::Transaction(CS pin_cs, TransmitBuffer* wbufs, size_t wbuf_count, ReceiveBuffer* rbufs, size_t rbuf_count)
{
    assert(wbufs);
    assert(rbufs);
    assert(wbuf_count > 0);
    assert(rbuf_count > 0);
    size_t wlen = 0;
    for (size_t i = 0; i < wbuf_count; i++) {
        assert(wbufs[i].length > 0);
        wlen += wbufs[i].length;
    }
    size_t rlen = 0;
    for (size_t i = 0; i < rbuf_count; i++) {
        assert(rbufs[i].length > 0);
        rlen += rbufs[i].length;
    }
    assert(wlen == rlen);

    xSemaphoreTake(m_mutex, portMAX_DELAY);

    m_tx_buffers = wbufs + 1;
    m_tx_remaining_total = wlen;
    m_tx_current = wbufs[0];

    m_rx_buffers = rbufs + 1;
    m_rx_remaining_total = rlen;
    m_rx_current = rbufs[0];

    spi_get_hw(m_inst)->imsc = SPI_SSPIMSC_TXIM_BITS | SPI_SSPIMSC_RXIM_BITS;

    gpio_put(static_cast<uint>(pin_cs), false);
    irq_set_enabled(m_irqn, true);

    xSemaphoreTake(m_notify_semaphore, portMAX_DELAY);
    while (m_rx_remaining_total > 0) {
        FillRxBuffer();
    }

    irq_set_enabled(m_irqn, false);
    gpio_put(static_cast<uint>(pin_cs), true);

    assert(m_tx_remaining_total == 0);
    assert(m_rx_remaining_total == 0);

    xSemaphoreGive(m_mutex);

    return wlen;
}

void SPI::FillTxBuffer()
{
    while (m_tx_remaining_total > 0 && m_rx_remaining_total - m_tx_remaining_total < FIFO_DEPTH && spi_is_writable(m_inst)) {
        uint8_t byte = 0;
        if (m_tx_current.buffer != nullptr) {
            byte = *m_tx_current.buffer++;
        }
        spi_get_hw(m_inst)->dr = byte;
        --m_tx_remaining_total;
        if (--m_tx_current.length == 0) {
            m_tx_current = *m_tx_buffers++;
        }
    }
}

void SPI::FillRxBuffer()
{
    while (spi_is_readable(m_inst)) {
        assert(m_rx_remaining_total > 0);
        uint8_t byte = spi_get_hw(m_inst)->dr;
        if (m_rx_current.buffer != nullptr) {
            *m_rx_current.buffer++ = byte;
        }
        --m_rx_remaining_total;
        if (--m_rx_current.length == 0) {
            m_rx_current = *m_rx_buffers++;
        }
    }
}

void SPI::ISR()
{
    FillRxBuffer();
    FillTxBuffer();
    if (m_tx_remaining_total == 0) {
        spi_get_hw(m_inst)->imsc = 0;
        irq_set_enabled(m_irqn, false);
        xSemaphoreGiveFromISR(m_notify_semaphore, nullptr);
    }
}

// TODO: remove
#include "SPIDevice.h"

void spi_test_print_version(SPIDevice& dev)
{
    uint8_t tx_buf[] = {
        0x00, 0x39, // address
        0x00, // block
    };
    uint8_t rx_buf[1] = {};

    SPI::TransmitBuffer wbufs[2] = { { tx_buf, sizeof(tx_buf) }, { nullptr, sizeof(rx_buf) } };
    SPI::ReceiveBuffer rbufs[2] = { { nullptr, sizeof(tx_buf) }, { rx_buf, sizeof(rx_buf) } };
    dev.Transaction(wbufs, 2, rbufs, 2);
    printf("version: %02x\n", rx_buf[0]);
}

void spi_test_print_hw_address(SPIDevice& dev)
{
    uint8_t tx_buf[] = {
        0x00, 0x09, // address
        0x00, // block
    };
    uint8_t rx_buf[6] = {};

    SPI::TransmitBuffer wbufs[2] = { { tx_buf, sizeof(tx_buf) }, { nullptr, sizeof(rx_buf) } };
    SPI::ReceiveBuffer rbufs[2] = { { nullptr, sizeof(tx_buf) }, { rx_buf, sizeof(rx_buf) } };
    dev.Transaction(wbufs, 2, rbufs, 2);
    printf("hw address: %02x:%02x:%02x:%02x:%02x:%02x\n", rx_buf[0], rx_buf[1], rx_buf[2], rx_buf[3], rx_buf[4], rx_buf[5]);
}

void spi_test_task(void* param)
{
    SPI* spi = new SPI(SPI::RX0::PIN_16, SPI::TX0::PIN_19, SPI::SCK0::PIN_18, 10'000'000);
    SPIDevice dev { spi, SPI::CS(17) };

    spi_test_print_version(dev);
    spi_test_print_hw_address(dev);

    while (true) {
        vTaskDelay(1000);
    }
}

void spi_test()
{
    xTaskCreate(spi_test_task, "SPI", DEFAULT_TASK_STACK_SIZE, nullptr, tskIDLE_PRIORITY + 3, nullptr);
}
