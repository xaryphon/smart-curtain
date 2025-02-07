#include "HttpConnection.hpp"

#include <cstring>

#include "Logger.hpp"

HttpConnection::HttpConnection(struct tcp_pcb* pcb)
    : m_pcb(pcb)
{
    tcp_arg(pcb, this);
    tcp_recv(pcb, [](void* arg, tcp_pcb* pcb, pbuf* p, err_t err) -> err_t {
        (void)pcb;
        auto* conn = static_cast<HttpConnection*>(arg);
        auto ret = conn->RecvCallback(p, err);
        if (p != nullptr) {
            pbuf_free(p);
        }
        if (conn->m_pcb == nullptr) {
            delete conn;
        }
        return ret;
    });
    tcp_sent(pcb, [](void* arg, tcp_pcb* pcb, u16_t len) -> err_t {
        (void)pcb;
        auto* conn = static_cast<HttpConnection*>(arg);
        auto ret = conn->SentCallback(len);
        if (conn->m_pcb == nullptr) {
            delete conn;
        }
        return ret;
    });
    tcp_err(pcb, [](void* arg, err_t err) -> void {
        auto* conn = static_cast<HttpConnection*>(arg);
        conn->m_pcb = nullptr;
        conn->ErrorCallback(err);
        delete conn;
    });
}

HttpConnection::~HttpConnection()
{
    Abort();
}

err_t HttpConnection::Abort()
{
    if (m_pcb != nullptr) {
        tcp_recv(m_pcb, nullptr);
        tcp_sent(m_pcb, nullptr);
        tcp_err(m_pcb, nullptr);
        tcp_abort(m_pcb);
        m_pcb = nullptr;
    }
    return ERR_ABRT;
}

void HttpConnection::Close()
{
    assert(m_pcb);
    tcp_recv(m_pcb, nullptr);
    tcp_sent(m_pcb, nullptr);
    tcp_err(m_pcb, nullptr);
    err_t success = tcp_close(m_pcb);
    assert(success == ERR_OK);
    m_tx_closed = m_rx_closed = true;
    m_pcb = nullptr;
}

void HttpConnection::ShutdownTransmit()
{
    assert(m_pcb);
    tcp_sent(m_pcb, nullptr);
    if (m_rx_closed) {
        tcp_err(m_pcb, nullptr);
    }
    err_t success = tcp_shutdown(m_pcb, 0, 1);
    assert(success == ERR_OK);
    m_tx_closed = true;
    if (m_rx_closed) {
        m_pcb = nullptr;
    }
}

void HttpConnection::ShutdownReceive()
{
    assert(m_pcb);
    tcp_recv(m_pcb, nullptr);
    if (m_tx_closed) {
        tcp_err(m_pcb, nullptr);
    }
    err_t success = tcp_shutdown(m_pcb, 1, 0);
    assert(success == ERR_OK);
    m_rx_closed = true;
    if (m_tx_closed) {
        m_pcb = nullptr;
    }
}

void HttpConnection::Shutdown()
{
    assert(m_pcb);
    tcp_recv(m_pcb, nullptr);
    tcp_sent(m_pcb, nullptr);
    tcp_err(m_pcb, nullptr);
    err_t success = tcp_shutdown(m_pcb, 1, 1);
    assert(success == ERR_OK);
    m_tx_closed = m_rx_closed = true;
    m_pcb = nullptr;
}

#define DUMP_WIDTH 16
void dump_pbuf(pbuf* p)
{
    printf("pbuf tot_len: %u {\n", p->tot_len);
    pbuf* pnext = p;
    do {
        p = pnext;
        printf("  %u {\n", p->len);
        for (size_t offset = DUMP_WIDTH; offset <= p->len; offset += DUMP_WIDTH) {
            uint8_t* d = static_cast<uint8_t*>(p->payload) + (offset - DUMP_WIDTH);
            printf("  ");
            for (size_t i = 0; i < DUMP_WIDTH; i++) {
                if (i % 4 == 0)
                    printf(" ");
                printf(" %02x", d[i]);
            }
            printf("  ");
            for (size_t b = 0; b < DUMP_WIDTH; b++) {
                printf("%c", isprint(d[b]) ? d[b] : '.');
            }
            printf("\n");
        }
        size_t excess = p->len % DUMP_WIDTH;
        if (excess > 0) {
            uint8_t* d = (uint8_t*)p->payload + (p->len - (p->len % DUMP_WIDTH));
            printf("  ");
            for (size_t i = 0; i < DUMP_WIDTH; i++) {
                if (i % 4 == 0)
                    printf(" ");
                if (i < excess) {
                    printf(" %02x", d[i]);
                } else {
                    printf("   ");
                }
            }
            printf("  ");
            for (size_t i = 0; i < excess; i++) {
                printf("%c", isprint(d[i]) ? d[i] : '.');
            }
            printf("\n");
        }
        printf("  }\n");
        pnext = p->next;
    } while (p->len != p->tot_len);
    printf("}\n");
}

err_t HttpConnection::RecvCallback(pbuf* p, err_t err)
{
    if (p == nullptr) {
        Logger::Log("Closed connection");
        Close();
        return ERR_OK;
    }

    // dump_pbuf(p);

    while (true) {
        const uint8_t* payload = static_cast<const uint8_t*>(p->payload);
        size_t len = p->len;
        while (len > 0) {
            size_t wrote = WriteToBuffer(payload, len);
            len -= wrote;
            payload += wrote;
            if (wrote < p->len) {
                if (!HandleRequest()) {
                    return Abort();
                }
            }
        }
        if (p->len == p->tot_len) {
            break;
        }
        p = p->next;
    }
    if (!HandleRequest()) {
        return Abort();
    }

    ShutdownReceive();
    ShutdownTransmit();
    return ERR_OK;
}

err_t HttpConnection::SentCallback(u16_t len)
{
    return ERR_OK;
}

void HttpConnection::ErrorCallback(err_t err)
{
    Logger::Log("A tcp connection closed with error: {}", err);
}

void HttpConnection::SendResponse(const char* data)
{
    err_t err = tcp_write(m_pcb, data, strlen(data), TCP_WRITE_FLAG_COPY);
    assert(err == ERR_OK);
    err = tcp_output(m_pcb);
    assert(err == ERR_OK);
}

size_t HttpConnection::WriteToBuffer(const uint8_t* data, size_t len)
{
    size_t remaining = m_buffer.size() - m_buffer_offset;
    if (len > remaining) {
        len = remaining;
    }

    memcpy(m_buffer.data() + m_buffer_offset, data, len);
    m_buffer_offset += len;

    return len;
}

bool HttpConnection::HandleRequest()
{
    if (m_buffer_offset == 0) {
        return true;
    }

    size_t num_headers = m_headers.size();
    int ret = phr_parse_request(m_buffer.data(), m_buffer_offset, &m_method, &m_method_len, &m_path, &m_path_len, &m_minor_version, m_headers.data(), &num_headers, m_parse_last_len);
    m_parse_last_len = m_buffer_offset;

    if (ret == -1) {
        Logger::Log("HTTP: Invalid request");
        return false;
    }
    if (ret == -2) {
        if (m_buffer_offset == m_buffer.size()) {
            Logger::Log("HTTP: Request too big");
            return false;
        }
        return true;
    }
    assert(ret > 0);

#if 1
    std::string_view method = { m_method, m_method_len };
    std::string_view path = { m_path, m_path_len };
    Logger::Log("Request len:{}", ret);
    Logger::Log("method:{}", method);
    Logger::Log("path:{}", path);
    Logger::Log("version:1.{}", m_minor_version);
    Logger::Log("headers:");
    for (size_t i = 0; i < num_headers; i++) {
        phr_header* header = &m_headers[i];
        std::string_view name = { header->name, header->name_len };
        std::string_view value = { header->value, header->value_len };
        Logger::Log("  {}: {}", name, value);
    }
#endif

    memmove(m_buffer.data(), m_buffer.data() + m_buffer_offset, m_buffer_offset);
    m_buffer_offset -= ret;
    m_parse_last_len = 0;

    const char* response = "HTTP/1.1 200 Ok\r\n"
                           "Content-Type: text/html\r\n"
                           "Content-Length: 48\r\n"
                           "\r\n"
                           "<html><body><h1>Hello, World!</h1></body></html>";

    SendResponse(response);
    return true;
}
