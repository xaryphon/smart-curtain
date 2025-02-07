#include "HttpServer.hpp"

#include <FreeRTOS.h>
#include <lwip/autoip.h>
#include <lwip/dhcp.h>
#include <task.h>

#include "HttpConnection.hpp"
#include "Logger.hpp"

HttpServer::HttpServer(uint16_t port)
    : m_port(port)
    , m_pcb(nullptr)
{
}

HttpServer::~HttpServer()
{
    if (m_pcb != nullptr) {
        tcp_close(m_pcb);
    }
}

bool HttpServer::Listen()
{
    assert(m_pcb == nullptr);
    m_pcb = tcp_new();
    if (m_pcb == nullptr) {
        Logger::Log("ERROR: tcp_new() returned null");
        return false;
    }

    err_t err = tcp_bind(m_pcb, IP_ADDR_ANY, m_port);
    if (err != ERR_OK) {
        Logger::Log("ERROR: tcp_bind() failed");
        tcp_close(m_pcb);
        m_pcb = nullptr;
        return false;
    }

    tcp_pcb* pcb = tcp_listen(m_pcb);
    if (pcb == nullptr) {
        Logger::Log("ERROR: tcp_listen() failed");
        tcp_close(m_pcb);
        m_pcb = nullptr;
        return false;
    }
    m_pcb = pcb;

    tcp_arg(m_pcb, this);
    tcp_accept(m_pcb, [](void* arg, struct tcp_pcb* newpcb, err_t err) -> err_t {
        return static_cast<HttpServer*>(arg)->AcceptCallback(newpcb, err);
    });

    return true;
}

err_t HttpServer::AcceptCallback(struct tcp_pcb* newpcb, err_t err)
{
    if (err != ERR_OK) {
        tcp_abort(newpcb);
        return ERR_ABRT;
    }

    Logger::Log("Accepted client");
    new HttpConnection(newpcb);

    return ERR_OK;
}

#include <pico/cyw43_arch.h>

#include "W5500LWIP.hpp"

void test_http_server_task(void* param)
{
    if (cyw43_arch_init() != 0) {
        Logger::Log("cyw43_arch_init() failed");
        for (;;) {
            vTaskDelay(10000);
        }
    }

    SPI* spi = new SPI(SPI::RX1::PIN_12, SPI::TX1::PIN_15, SPI::SCK1::PIN_10, 10'000'000);
    W5500LWIP* w5500 = new W5500LWIP(spi, SPI::CS(9), W5500::INT(8), W5500::RST(7));

    // dhcp_start(w5500->GetNetif());
    autoip_start(w5500->GetNetif());

    HttpServer* server = new HttpServer(8080);
    server->Listen();

    bool was_up = false;
    uint32_t last_ip = 0;
    for (;;) {
        bool is_up = w5500->IsLinkUp();
        if (is_up != was_up) {
            Logger::Log("Link status changed to {}", w5500->IsLinkUp() ? "up" : "down");
            was_up = is_up;
        }
        uint32_t ip = w5500->GetNetif()->ip_addr.addr;
        if (ip != last_ip) {
            uint8_t bytes[4];
            memcpy(bytes, &ip, 4);
            Logger::Log("Got IP address {}.{}.{}.{}", bytes[0], bytes[1], bytes[2], bytes[3]);
            last_ip = ip;
        }
        vTaskDelay(1000);
    }
}

void test_http_server()
{
    xTaskCreate(test_http_server_task, "test", 512, nullptr, tskIDLE_PRIORITY + 2, nullptr);
}
