#pragma once

#include <lwip/tcp.h>
#include <picohttpparser.h>

class HttpServer {
public:
    explicit HttpServer(uint16_t port);
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer(HttpServer&&) = delete;

    HttpServer& operator=(const HttpServer&) = delete;
    HttpServer& operator=(HttpServer&&) = delete;

    bool Listen();

private:
    err_t AcceptCallback(struct tcp_pcb* newpcb, err_t err);

    uint16_t m_port;
    struct tcp_pcb* m_pcb;
};

void test_http_server();
