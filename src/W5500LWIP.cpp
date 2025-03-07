#include "W5500LWIP.hpp"

#include <array>
#include <cstring>

#include <lwip/dhcp.h>
#include <lwip/tcpip.h>
#include <netif/etharp.h>

#include "Logger.hpp"
#include "config.h"

static SemaphoreHandle_t g_W5500_Semaphore;
static uint g_W5500_interrupt_pin;

// TODO: Maybe create an interrupt handler class
static void W5500_InterruptHandler(uint pin, uint32_t event_mask)
{
    assert(pin == g_W5500_interrupt_pin);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(g_W5500_Semaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

W5500LWIP::W5500LWIP(SPI* spi, SPI::CS pin_cs, W5500::INT pin_int, W5500::RST pin_rst)
    : W5500(spi, pin_cs, pin_rst)
    , m_netif()
{
    g_W5500_interrupt_pin = static_cast<uint>(pin_int);
    g_W5500_Semaphore = xSemaphoreCreateCounting(20, 0);
    vQueueAddToRegistry(g_W5500_Semaphore, "W5500_INT");
    gpio_init(static_cast<uint>(pin_int));
    gpio_set_irq_enabled(static_cast<uint>(pin_int), GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_callback(W5500_InterruptHandler);

    constexpr auto init = [](struct netif* netif) -> err_t {
        return static_cast<decltype(this)>(netif->state)->NetInit();
    };
    netif_add_noaddr(&m_netif, this, init, tcpip_input);
    netif_set_up(&m_netif);

    netif_set_status_callback(&m_netif, [](struct netif* netif) -> void {
        uint32_t ip_ = netif->ip_addr.addr;
        std::array<uint8_t, 4> ip = {};
        static_assert(sizeof(uint32_t) == ip.size());
        memcpy(ip.data(), &ip_, sizeof(uint32_t));
        Logger::Log("W5500 {} {}.{}.{}.{}", netif_is_link_up(netif) ? "UP" : "DOWN", ip[0], ip[1], ip[2], ip[3]);
    });
    dhcp_start(&m_netif);
}

bool W5500LWIP::IsLinkUp()
{
    return netif_is_link_up(&m_netif);
}

err_t W5500LWIP::NetInit()
{
    Reset();

    uint8_t version;
    if (!ReadChipVersion(&version)) {
        printf("Failed to read W5500 version\n");
        return ERR_IF;
    }
    if (version != 4) {
        printf("W5500 version mismatch\n");
        return ERR_IF;
    }
    WriteMode(MR_Reset);
    WriteSnMode(0, SocketMode(Sn_MR_P_MACRAW | Sn_MR_MACRAW_IPv6PacketBlocking | Sn_MR_MACRAW_MulticastBlocking));
    WriteSnRXBufferSize(0, Sn_BUFFER_SIZE_16KB);
    WriteSnTXBufferSize(0, Sn_BUFFER_SIZE_16KB);
    for (uint i = 1; i < 8; i++) {
        WriteSnRXBufferSize(i, Sn_BUFFER_SIZE_0KB);
        WriteSnTXBufferSize(i, Sn_BUFFER_SIZE_0KB);
    }
    WriteInterruptLowLevelTimer(1);
    WriteSocketInterruptMask(1);
    uint8_t phycfgr = PHYCFGR_ConfigurePHYOperationMode | PHYCFGR_OPMDC_100_Full_NoNeg;
    WritePHYConfiguration(PHYConfiguration(phycfgr));
    phycfgr |= PHYCFGR_Reset;
    WritePHYConfiguration(PHYConfiguration(phycfgr));
    WriteSnInterruptMask(0, SocketInterrupt::Sn_IR_RECV);
    WriteSnCommand(0, SocketCommand::Sn_CR_OPEN);

    W5500::HWAddress address = { .bytes = { 0xDE, 0xAD, 0xBE, 0xEF, 0x13, 0x37 } };
    if (!WriteSourceHardwareAddress(address)) {
        printf("Failed to set HW address for W5500\n");
        return ERR_IF;
    }
    m_netif.hwaddr_len = ETHARP_HWADDR_LEN;
    static_assert(ETHARP_HWADDR_LEN == sizeof(address));
    memcpy(m_netif.hwaddr, address.bytes, sizeof(address));
    m_netif.name[0] = 'e';
    m_netif.name[1] = '0';
    m_netif.output = etharp_output;
    constexpr auto linkoutput = [](struct netif* netif, struct pbuf* packet) -> err_t {
        return static_cast<decltype(this)>(netif->state)->LinkOutput(packet);
    };
    m_netif.linkoutput = linkoutput;
    m_netif.mtu = 1500;
    m_netif.flags |= NETIF_FLAG_ETHARP | NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHERNET;
    xTaskCreate(TASK_KONDOM(W5500LWIP, TaskEntry), "W5500", TaskStackSize::W5500LWIP, this, TaskPriority::W5500LWIP, nullptr);

    return ERR_OK;
}

err_t W5500LWIP::LinkOutput(struct pbuf* pbuf)
{
    SocketStatus status;
    do {
        if (!ReadSnStatus(0, &status)) {
            printf("Failed to read W5500 S0_SR\n");
            return ERR_IF;
        }
        // Timeout?
    } while (status != Sn_SR_SOCK_MACRAW);

    if (!TransmitFragment(pbuf)) {
        printf("Failed to transmit fragment\n");
        return ERR_IF;
    }

    return ERR_OK;
}

void W5500LWIP::TaskEntry()
{
    struct pbuf* pbuf = nullptr;
    for (;;) {
        if (xSemaphoreTake(g_W5500_Semaphore, pdMS_TO_TICKS(500)) != pdTRUE) {
            ClearInterrupts();
            CheckLinkState();
            continue;
        }
        if (!netif_is_link_up(&m_netif)) {
            CheckLinkState();
        }

        ClearInterrupts();
        if (ReceiveFragment(&pbuf)) {
            if (m_netif.input(pbuf, &m_netif) != ERR_OK) {
                pbuf_free(pbuf);
            }
            pbuf = nullptr;
        }
    }
}

void W5500LWIP::ClearInterrupts()
{
    WriteSnInterrupt(0, Sn_IR_ALL);
    WriteSocketInterrupt(0);
}

void W5500LWIP::CheckLinkState()
{
    PHYConfiguration phycfgr;
    if (!ReadPHYConfiguration(&phycfgr)) {
        printf("Failed to read W5500 PHYCFGR\n");
        phycfgr = PHYConfiguration(0);
    }
    bool status = phycfgr & PHYCFGR_LinkStatus;
    if (status && !netif_is_link_up(&m_netif)) {
        netif_set_link_up(&m_netif);
    } else if (!status && netif_is_link_up(&m_netif)) {
        netif_set_link_down(&m_netif);
    }
}

bool W5500LWIP::ReceiveFragment(struct pbuf** pbuf)
{
    assert(*pbuf == nullptr);

    uint16_t data_pointer;
    if (!ReadSnRXReadDataPointer(0, &data_pointer)) {
        printf("Failed to read W5500 S0_RX_RD\n");
        return false;
    }

    uint16_t length;
    if (!ReadSnReceivedSize(0, &length)) {
        printf("Failed to read W5500 S0_RX_RSR\n");
        return false;
    }

    if (length < 4) {
        if (length == 0)
            return false;
        // according to w5500-lwip-freertos:
        // This could be indicative of a crashed (brown-outed?) controller
        printf("W5500 S0_RX_RSR returned less than 4 (got %d, this might be bad maybe idk)\n", length);
        return false;
    }

    uint8_t header[2];
    uint16_t read = ReadSnReceiveBuffer(0, header, 2);
    if (read != 2) {
        printf("Failed to read W5500 S0 Receive Buffer\n");
        return false;
    }
    if (!WriteSnCommand(0, Sn_CR_RECV)) {
        printf("Failed to write W5500 S0_CR\n");
        return false;
    }
    uint16_t framelen = header[0] << 8 | header[1];

    // Not sure if applicable for us but works around https://savannah.nongnu.org/bugs/index.php?50040
    if (framelen > 32000) {
        ReadSnReceiveBuffer(0, nullptr, framelen);
        if (!WriteSnCommand(0, Sn_CR_RECV)) {
            printf("Failed to write W5500 S0_CR\n");
            return false;
        }
        return true;
    }

    framelen -= 2;

    *pbuf = pbuf_alloc(PBUF_RAW, framelen, PBUF_RAM);
    if (*pbuf == nullptr) {
        printf("Failed to alloc W5500 pbuf\n");
        return false;
    }

    if (ReadSnReceiveBuffer(0, static_cast<uint8_t*>((*pbuf)->payload), framelen) != framelen) {
        printf("Failed to read W5500 S0 Receive Buffer\n");
        return false;
    }

    if (!WriteSnCommand(0, Sn_CR_RECV)) {
        printf("Failed to write W5500 S0_CR\n");
        return false;
    }

    return true;
}

bool W5500LWIP::TransmitFragment(struct pbuf* pbuf)
{
    uint transferred = 0;
    for (;;) {
        uint16_t free_size;
        if (!ReadFreeSize(&free_size)) {
            printf("Failed to read TX_FSR\n");
            return false;
        }
        if (free_size == 0) {
            continue;
        }

        uint n = pbuf->len - transferred;
        if (n > free_size) {
            n = free_size;
        }
        if (!WriteSnTransmitBuffer(0, static_cast<const uint8_t*>(pbuf->payload) + transferred, n)) {
            printf("Failed to write to transmit buffer\n");
            return false;
        }

        if (free_size == n && !WriteSnCommand(0, Sn_CR_SEND)) {
            printf("Failed to write W5500 S0_CR\n");
            return false;
        }

        transferred += n;
        if (transferred == pbuf->tot_len) {
            if (free_size != n && !WriteSnCommand(0, Sn_CR_SEND)) {
                printf("Failed to write W5500 S0_CR\n");
                return false;
            }
            break;
        }
        if (transferred == pbuf->len) {
            pbuf = pbuf->next;
            transferred = 0;
        }
    }

    return true;
}

bool W5500LWIP::ReadFreeSize(uint16_t* free_size)
{
    uint16_t last_read = 0;
    uint16_t new_read = 0;
    if (!ReadSnTXFreeSize(0, &new_read)) {
        printf("Failed to read TX_FSR\n");
        return false;
    }
    do {
        last_read = new_read;
        if (!ReadSnTXFreeSize(0, &new_read)) {
            printf("Failed to read TX_FSR\n");
            return false;
        }
    } while (last_read != new_read);
    *free_size = new_read;
    return true;
}

#include <lwip/autoip.h>
#include <lwip/dhcp.h>
#include <pico/cyw43_arch.h>

void w5500_lwip_test_task(void* param)
{
    if (cyw43_arch_init() != 0) {
        printf("cyw43_arch_init() failed\n");
        for (;;) {
            vTaskDelay(10000);
        }
    }

    SPI* spi = new SPI(SPI::RX0::PIN_16, SPI::TX0::PIN_19, SPI::SCK0::PIN_18, 10'000'000);
    W5500LWIP* w5500 = new W5500LWIP(spi, SPI::CS(17), W5500::INT(15), W5500::RST(20));

    // dhcp_start(w5500->GetNetif());
    autoip_start(w5500->GetNetif());

    bool was_up = false;
    uint32_t last_ip = 0;
    for (;;) {
        bool is_up = w5500->IsLinkUp();
        if (is_up != was_up) {
            printf("Link status changed to %s\n", w5500->IsLinkUp() ? "up" : "down");
            was_up = is_up;
        }
        uint32_t ip = w5500->GetNetif()->ip_addr.addr;
        if (ip != last_ip) {
            uint8_t bytes[4];
            memcpy(bytes, &ip, 4);
            printf("Got IP address %u.%u.%u.%u\n", bytes[0], bytes[1], bytes[2], bytes[3]);
            last_ip = ip;
        }
        vTaskDelay(1000);
    }
}

void w5500_lwip_test()
{
    xTaskCreate(w5500_lwip_test_task, "test", 256, nullptr, tskIDLE_PRIORITY + 2, nullptr);
}
