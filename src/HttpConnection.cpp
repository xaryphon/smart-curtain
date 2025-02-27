#include "HttpConnection.hpp"

#include <cmath>
#include <cstring>

#include <ArduinoJson.hpp>

#include "Logger.hpp"

#define HTTP_ENABLE_DEBUG 0

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
#if HTTP_ENABLE_DEBUG
    Logger::Log("HTTP: Closed");
#endif
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

#if HTTP_ENABLE_DEBUG
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
#endif

err_t HttpConnection::RecvCallback(pbuf* p, err_t err)
{
    if (p == nullptr) {
        Logger::Log("Closed connection");
        Close();
        return ERR_OK;
    }

#if HTTP_ENABLE_DEBUG
    dump_pbuf(p);
#endif

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

    return ERR_OK;
}

err_t HttpConnection::SentCallback(u16_t len)
{
    return ERR_OK;
}

void HttpConnection::ErrorCallback(err_t err)
{
    Logger::Log("TCP connection closed with error: {}", err);
}

void HttpConnection::RespondWith(const char* status, const char* body)
{
    err_t err = ERR_OK;
    if (body == nullptr) {
        std::string head = fmt::format(
            "HTTP/1.1 {}\r\n"
            "Connection: close\r\n"
            "\r\n",
            status);
        err = tcp_write(m_pcb, head.c_str(), head.size(), TCP_WRITE_FLAG_COPY);
        assert(err == ERR_OK);
    } else {
        size_t body_len = strlen(body);
        std::string head = fmt::format(
            "HTTP/1.1 {}\r\n"
            "Connection: close\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: {}\r\n"
            "\r\n",
            status, body_len);
        err = tcp_write(m_pcb, head.c_str(), head.size(), TCP_WRITE_FLAG_COPY);
        assert(err == ERR_OK);
        err = tcp_write(m_pcb, body, body_len, TCP_WRITE_FLAG_COPY);
        assert(err == ERR_OK);
    }
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

static bool ParseSizeTFromStringView(std::string_view input_string, size_t* output_number)
{
    constexpr size_t MAXIMUM_ACCEPTABLE_NUMBER = 65536;
    size_t output_accumulator = 0;
    for (char input_character : input_string) {
        if (input_character < '0' || input_character > '9') {
            return false;
        }
        output_accumulator = output_accumulator * 10 + (input_character - '0');
        if (output_accumulator > MAXIMUM_ACCEPTABLE_NUMBER) {
            return false;
        }
    }
    *output_number = output_accumulator;
    return true;
}

bool HttpConnection::HandleRequest()
{
    if (m_buffer_offset == 0) {
        return true;
    }

    if (m_body_size == 0) {
        size_t num_headers = m_headers.size();
        int ret = phr_parse_request(m_buffer.data(), m_buffer_offset, &m_method, &m_method_len, &m_path, &m_path_len, &m_minor_version, m_headers.data(), &num_headers, m_parse_last_len);
        m_parse_last_len = m_buffer_offset;

        if (ret == -1) {
            Logger::Log("HTTP: Invalid request");
            return false;
        }
        if (ret == -2) {
            if (m_buffer_offset == m_buffer.size()) {
                // FIXME: Send a 4XX instead
                Logger::Log("HTTP: Request too big");
                return false;
            }
            return true;
        }
        assert(ret > 0);
        m_headers_size = ret;

#if HTTP_ENABLE_DEBUG
        std::string_view method = { m_method, m_method_len };
        std::string_view path = { m_path, m_path_len };
        Logger::Log("Request len:{}", ret);
        Logger::Log("method:{}", method);
        Logger::Log("path:{}", path);
        Logger::Log("version:1.{}", m_minor_version);
        Logger::Log("headers:");
#endif
        for (size_t i = 0; i < num_headers; i++) {
            phr_header* header = &m_headers[i];
            std::string_view name = { header->name, header->name_len };
            std::string_view value = { header->value, header->value_len };
#if HTTP_ENABLE_DEBUG
            Logger::Log("  {}: {}", name, value);
#endif
            if (name == "Content-Length") {
                size_t content_length = 0;
                if (!ParseSizeTFromStringView(value, &content_length)) {
                    // FIXME: Send a 4XX instead
                    Logger::Log("HTTP: Received invalid Content-Length");
                    return false;
                }
                m_body_size = content_length;
            }
        }
    }

    size_t request_size = m_headers_size + m_body_size;
    if (request_size > m_buffer.size()) {
        // FIXME: Send a 4XX instead
        Logger::Log("HTTP: Request too big");
        return false;
    }
    if (request_size > m_buffer_offset) {
        // wait for more data
        return true;
    }

#if HTTP_ENABLE_DEBUG
    Logger::Log("received body: {} ({} + {} <= {} -> {})", m_body_size, m_headers_size, m_body_size, m_buffer_offset, request_size <= m_buffer_offset);
#endif

    std::string_view method = { m_method, m_method_len };
    std::string_view path = { m_path, m_path_len };
    if (method == "GET") {
        HandleGET(path);
    } else if (method == "POST") {
        HandlePOST(path, std::string_view(m_buffer.data() + m_headers_size, m_body_size));
    } else {
        RespondWith("405 Method Not Allowed", "");
    }

    memmove(m_buffer.data(), m_buffer.data() + request_size, m_buffer_offset - request_size);
    m_buffer_offset -= request_size;
    m_parse_last_len = 0;

    ShutdownReceive();
    ShutdownTransmit();
    return true;
}

std::string HttpConnection::BuildBody(bool include_status, bool include_settings)
{
    std::string body = "{";
    auto ins = std::back_inserter(body);
    if (include_status) {
        if (body.size() != 1) {
            body += ',';
        }
        fmt::format_to(ins, R"("mode":"{}",)", "manual");
        fmt::format_to(ins, R"("motor":{{)");
        fmt::format_to(ins, R"("target":{},)", 1.0f);
        fmt::format_to(ins, R"("current":{},)", 0.5f);
        fmt::format_to(ins, R"("target_raw":{},)", 8000);
        fmt::format_to(ins, R"("current_raw":{},)", 4000);
        fmt::format_to(ins, R"("length_raw":{})", 8000);
        fmt::format_to(ins, "}},");
        fmt::format_to(ins, R"("lux":{{)");
        fmt::format_to(ins, R"("target":{},)", 100000.0f);
        fmt::format_to(ins, R"("current":{},)", 1000.0f);
        fmt::format_to(ins, R"("current_raw":[{},{}])", 900.0f, 1100.0f);
        fmt::format_to(ins, "}}");
    }
    if (include_settings) {
        if (body.size() != 1) {
            body += ',';
        }
        fmt::format_to(ins, R"("wanted_mode":"{}",)", "manual");
        fmt::format_to(ins, R"("manual":{{)");
        fmt::format_to(ins, R"("target":{},)", 0.0f);
        fmt::format_to(ins, R"("target_raw":{})", 0);
        fmt::format_to(ins, "}},");
        fmt::format_to(ins, R"("auto_static":{{"target":{}}},)", 0.0f);
        fmt::format_to(ins, R"("auto_hourly":{{"targets":[)");
        for (int i = 0; i < 24; i++) {
            if (i != 0) {
                body += ',';
            }
            fmt::format_to(ins, R"({})", static_cast<float>(1 << i) / 10.0f);
        }
        fmt::format_to(ins, "]}}");
    }
    body.append("}\n");
    return body;
}

void HttpConnection::HandleGET(std::string_view path)
{
    if (path == "/status") {
        std::string body = BuildBody(true, false);
        RespondWith("200 OK", body.c_str());
    } else if (path == "/settings") {
        std::string body = BuildBody(false, true);
        RespondWith("200 OK", body.c_str());
    } else if (path == "/status/full") {
        std::string body = BuildBody(true, true);
        RespondWith("200 OK", body.c_str());
    } else {
        RespondWith("404 Not Found", R"({"message":"Page not found"})");
    }
}

void HttpConnection::HandlePOST(std::string_view path, std::string_view body)
{
    if (path == "/settings") {
        ArduinoJson::JsonDocument doc;
        ArduinoJson::deserializeJson(doc, body);

        uint8_t next_mode = 0;
        float manual_target = NAN;
        int manual_target_raw = -1;
        float static_target = NAN;
        std::array<float, 24> hourly_targets = { NAN };

        if (std::string_view wanted_mode = doc["wanted_mode"]; !wanted_mode.empty()) {
            if (wanted_mode == "manual") {
                next_mode = 1;
            } else if (wanted_mode == "auto_static") {
                next_mode = 2;
            } else if (wanted_mode == "auto_hourly") {
                next_mode = 3;
            } else {
                RespondWith("400 Bad Request", R"({"message": "Invalid wanted_mode"})");
                return;
            }
        }
        if (ArduinoJson::JsonVariantConst manual = doc["manual"]) {
            if (!manual.is<ArduinoJson::JsonObjectConst>()) {
                RespondWith("400 Bad Request", R"({"message": "Invalid manual"})");
                return;
            }
            if (ArduinoJson::JsonVariantConst target_ = manual["target"]) {
                if (!target_.is<float>()) {
                    RespondWith("400 Bad Request", R"({"message": "Invalid manual.target"})");
                    return;
                }
                manual_target = target_;
            }
            if (ArduinoJson::JsonVariantConst target_ = manual["target_raw"]) {
                if (!target_.is<int>()) {
                    RespondWith("400 Bad Request", R"({"message": "Invalid manual.target_raw"})");
                    return;
                }
                manual_target_raw = target_;
            }
        }
        if (ArduinoJson::JsonVariantConst auto_static = doc["auto_static"]) {
            if (!auto_static.is<ArduinoJson::JsonObjectConst>()) {
                RespondWith("400 Bad Request", R"({"message": "Invalid auto_static"})");
                return;
            }
            if (ArduinoJson::JsonVariantConst target_ = auto_static["target"]) {
                if (!target_.is<float>()) {
                    RespondWith("400 Bad Request", R"({"message": "Invalid auto_static"})");
                    return;
                }
                static_target = target_;
            }
        }
        if (ArduinoJson::JsonVariantConst auto_hourly = doc["auto_hourly"]) {
            if (!auto_hourly.is<ArduinoJson::JsonObjectConst>()) {
                RespondWith("400 Bad Request", R"({"message": "Invalid auto_hourly"})");
                return;
            }
            if (ArduinoJson::JsonVariantConst targets_ = auto_hourly["targets"]) {
                if (ArduinoJson::JsonArrayConst targets = targets_) {
                    if (targets.size() != 24) {
                        RespondWith("400 Bad Request", R"({"message": "Invalid auto_hourly.targets"})");
                        return;
                    }
                    for (size_t i = 0; i < 24; i++) {
                        float target = targets[i] | NAN;
                        if (std::isnan(target)) {
                            RespondWith("400 Bad Request", R"({"message": "Invalid auto_hourly.targets"})");
                            return;
                        }
                        hourly_targets[i] = target;
                    }
                } else {
                    RespondWith("400 Bad Request", R"({"message": "Invalid auto_hourly.targets"})");
                    return;
                }
            }
        }
        if (!std::isnan(manual_target) && manual_target_raw >= 0) {
            RespondWith("400 Bad Request", R"({"message": "manual.target and manual.target_raw are mutually exclusive"})");
            return;
        }

        if (next_mode != 0) {
            Logger::Log("Setting mode to {}", next_mode);
        }
        if (!std::isnan(manual_target)) {
            Logger::Log("Setting manual_target to {}%", manual_target * 100.0f);
        }
        if (manual_target_raw >= 0) {
            Logger::Log("Setting manual_target to {}", manual_target_raw);
        }
        if (!std::isnan(static_target)) {
            Logger::Log("Setting static_target to {}", static_target);
        }
        if (!std::isnan(hourly_targets[0])) {
            Logger::Log("Setting hourly_targets something");
        }

        std::string res_body = BuildBody(false, true);
        RespondWith("200 OK", res_body.c_str());
    } else if (path == "/calibrate") {
        Logger::Log("Start calibration");
        RespondWith("202 Accepted", R"({})");
    } else {
        RespondWith("404 Not Found", R"({"message":"Page not found"})");
    }
}
