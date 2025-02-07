#pragma once

#include <array>

#include <lwip/tcp.h>
#include <picohttpparser.h>

class HttpConnection {
public:
    explicit HttpConnection(struct tcp_pcb* pcb);
    ~HttpConnection();

    HttpConnection(const HttpConnection&) = delete;
    HttpConnection(HttpConnection&&) = delete;

    HttpConnection& operator=(const HttpConnection&) = delete;
    HttpConnection& operator=(HttpConnection&&) = delete;

private:
    err_t Abort();
    void Close();
    void ShutdownReceive();
    void ShutdownTransmit();
    void Shutdown();

    [[nodiscard]] err_t RecvCallback(pbuf* packet, err_t err);
    [[nodiscard]] err_t SentCallback(u16_t len);
    void ErrorCallback(err_t err);

    void SendResponse(const char* data);
    [[nodiscard]] size_t WriteToBuffer(const uint8_t* data, size_t len);

    [[nodiscard]] bool HandleRequest();

    tcp_pcb* m_pcb;
    bool m_tx_closed = false;
    bool m_rx_closed = false;
    std::array<char, 4096> m_buffer = {};
    size_t m_buffer_offset = 0;
    size_t m_parse_last_len = 0;
    const char* m_method = nullptr;
    size_t m_method_len = 0;
    const char* m_path = nullptr;
    size_t m_path_len = 0;
    int m_minor_version = 0;
    std::array<phr_header, 128> m_headers = {};
};
