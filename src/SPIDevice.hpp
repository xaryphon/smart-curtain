#pragma once

#include "SPI.hpp"

class SPIDevice {
public:
    SPIDevice(SPI* spi, SPI::CS pin_cs);

    uint Transaction(SPI::TransmitBuffer* wbufs, size_t wbuf_count, SPI::ReceiveBuffer* rbufs, size_t rbuf_count)
    {
        return m_spi->Transaction(m_cs, wbufs, wbuf_count, rbufs, rbuf_count);
    }

private:
    SPI* m_spi;
    SPI::CS m_cs;
};
