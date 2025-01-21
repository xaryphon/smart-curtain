#pragma once

#include <lwip/netif.h>

#include "W5500.h"

class W5500LWIP : private W5500 {
public:
    explicit W5500LWIP(SPI* spi, SPI::CS pin_cs, INT pin_int, RST pin_rst);

    bool IsLinkUp();
    struct netif* GetNetif() { return &m_netif; }

private:
    err_t NetInit();
    err_t LinkOutput(struct pbuf* pbuf);
    void TaskEntry();
    void ClearInterrupts();
    void CheckLinkState();
    bool ReceiveFragment(struct pbuf** pbuf);
    bool TransmitFragment(struct pbuf* pbuf);
    bool ReadFreeSize(uint16_t* free_size);

    struct netif m_netif;
};

void w5500_lwip_test();
