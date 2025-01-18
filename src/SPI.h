#pragma once

#include <FreeRTOS.h>
#include <hardware/spi.h>
#include <pico/stdlib.h>
#include <semphr.h>

class SPI {
public:
    enum class RX0 : uint {
        PIN_0 = 0,
        PIN_4 = 4,
        PIN_16 = 16,
    };
    enum class SCK0 : uint {
        PIN_2 = 2,
        PIN_6 = 6,
        PIN_18 = 18,
    };
    enum class TX0 : uint {
        PIN_3 = 3,
        PIN_7 = 7,
        PIN_19 = 19,
    };

    enum class RX1 : uint {
        PIN_8 = 8,
        PIN_12 = 12,
    };
    enum class SCK1 : uint {
        PIN_10 = 10,
        PIN_14 = 14,
    };
    enum class TX1 : uint {
        PIN_11 = 11,
        PIN_15 = 15,
    };

    enum class CS : uint;

    explicit SPI(RX0 pin_rx, TX0 pin_tx, SCK0 pin_sck, uint baud_rate);
    explicit SPI(RX1 pin_rx, TX1 pin_tx, SCK1 pin_sck, uint baud_rate);

    struct ReceiveBuffer {
        uint8_t* buffer;
        size_t length;
    };
    struct TransmitBuffer {
        const uint8_t* buffer;
        size_t length;
    };

    uint Transaction(CS pin_cs, TransmitBuffer* wbufs, size_t wbuf_count, ReceiveBuffer* rbufs, size_t rbuf_count);

    uint GetBaudRate() const { return m_baud_rate; }

private:
    explicit SPI(spi_inst_t* inst, uint pin_rx, uint pin_tx, uint pin_sck, uint baud_rate);

    void FillTxBuffer();
    void FillRxBuffer();

    static constexpr size_t FIFO_DEPTH = 8;

    spi_inst_t* m_inst;
    uint m_baud_rate;

    SemaphoreHandle_t m_mutex;

    TransmitBuffer* m_tx_buffers = nullptr;
    size_t m_tx_remaining_total = 0;
    TransmitBuffer m_tx_current = {};

    ReceiveBuffer* m_rx_buffers = nullptr;
    size_t m_rx_remaining_total = 0;
    ReceiveBuffer m_rx_current = {};
};

// TODO: remove
void spi_test();
