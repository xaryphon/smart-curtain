#include "HttpConnection.hpp"

#include <cmath>
#include <cstring>

#include <ArduinoJson.hpp>

#include "HttpServer.hpp"
#include "Logger.hpp"

#define HTTP_ENABLE_DEBUG 0

HttpConnection::HttpConnection(const ConstructionParameters& params)
    : m_server(params.server)
    , m_pcb(params.pcb)
{
    tcp_arg(m_pcb, this);
    tcp_recv(m_pcb, [](void* arg, tcp_pcb* pcb, pbuf* p, err_t err) -> err_t {
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
    tcp_sent(m_pcb, [](void* arg, tcp_pcb* pcb, u16_t len) -> err_t {
        (void)pcb;
        auto* conn = static_cast<HttpConnection*>(arg);
        auto ret = conn->SentCallback(len);
        if (conn->m_pcb == nullptr) {
            delete conn;
        }
        return ret;
    });
    tcp_err(m_pcb, [](void* arg, err_t err) -> void {
        auto* conn = static_cast<HttpConnection*>(arg);
        conn->m_pcb = nullptr;
        conn->ErrorCallback(err);
        delete conn;
    });
}

HttpConnection::~HttpConnection()
{
    m_server->m_subscribed.remove(this);
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
    bool keep_alive = false;
    if (method == "GET") {
        keep_alive = HandleGET(path);
    } else if (method == "POST") {
        HandlePOST(path, std::string_view(m_buffer.data() + m_headers_size, m_body_size));
    } else {
        RespondWith("405 Method Not Allowed", "");
    }

    memmove(m_buffer.data(), m_buffer.data() + request_size, m_buffer_offset - request_size);
    m_buffer_offset -= request_size;
    m_parse_last_len = 0;

    if (!keep_alive) {
        ShutdownReceive();
        ShutdownTransmit();
    }
    return true;
}

bool HttpConnection::HandleGET(std::string_view path)
{
    if (path == "/status") {
        std::string body = m_server->BuildBody(true, false);
        RespondWith("200 OK", body.c_str());
    } else if (path == "/settings") {
        std::string body = m_server->BuildBody(false, true);
        RespondWith("200 OK", body.c_str());
    } else if (path == "/status/full") {
        std::string body = m_server->BuildBody(true, true);
        RespondWith("200 OK", body.c_str());
    } else if (path == "/subscribe") {
        std::string msg = fmt::format(
            "HTTP/1.1 200 OK\r\n"
            "Connection: keep-alive\r\n"
            "Content-Type: text/event-stream\r\n"
            "\r\n"
            "data: {}" /* contains implicit newline */
            "\n",
            m_server->BuildBody(true, true));
        err_t err = tcp_write(m_pcb, msg.c_str(), msg.size(), TCP_WRITE_FLAG_COPY);
        assert(err == ERR_OK);
        m_server->m_subscribed.emplace_front(this);
        return true;
    } else {
        RespondWith("404 Not Found", R"({"message":"Page not found"})");
    }
    return false;
}

void HttpConnection::HandlePOST(std::string_view path, std::string_view body)
{
    enum class Mode {
        UNKNOWN,
        MANUAL,
        AUTO_STATIC,
        AUTO_HOURLY,
    };

    if (path == "/settings") {
        ArduinoJson::JsonDocument doc;
        ArduinoJson::deserializeJson(doc, body);

        Mode next_mode = Mode::UNKNOWN;
        int manual_target = -1;
        float static_target = NAN;
        std::array<float, 24> hourly_targets = { NAN };
        uint64_t new_time = UINT64_MAX;

        if (std::string_view wanted_mode = doc["wanted_mode"]; !wanted_mode.empty()) {
            if (wanted_mode == "manual") {
                next_mode = Mode::MANUAL;
            } else if (wanted_mode == "auto_static") {
                next_mode = Mode::AUTO_STATIC;
            } else if (wanted_mode == "auto_hourly") {
                next_mode = Mode::AUTO_HOURLY;
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
            if (ArduinoJson::JsonVariantConst target_ = manual["target"]; !target_.isNull()) {
                if (!target_.is<int>()) {
                    RespondWith("400 Bad Request", R"({"message": "Invalid manual.target"})");
                    return;
                }
                manual_target = target_;
                if (manual_target < 0) {
                    manual_target = 0;
                } else if (manual_target > 100) {
                    manual_target = 100;
                }
            }
        }
        if (ArduinoJson::JsonVariantConst auto_static = doc["auto_static"]) {
            if (!auto_static.is<ArduinoJson::JsonObjectConst>()) {
                RespondWith("400 Bad Request", R"({"message": "Invalid auto_static"})");
                return;
            }
            if (ArduinoJson::JsonVariantConst target_ = auto_static["target"]; !target_.isNull()) {
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
        if (ArduinoJson::JsonVariantConst time_high = doc["time_high"]; !time_high.isNull()) {
            if (!time_high.is<uint32_t>()) {
                RespondWith("400 Bad Request", R"({"message": "Invalid time_high"})");
                return;
            }
            ArduinoJson::JsonVariantConst time_low = doc["time_low"];
            if (time_low.isNull()) {
                RespondWith("400 Bad Request", R"({"message": "time_high is set, but time_low is missing"})");
            }
            if (!time_low.is<uint32_t>()) {
                RespondWith("400 Bad Request", R"({"message": "Invalid time_low"})");
                return;
            }
            uint32_t high = time_high;
            uint32_t low = time_low;
            new_time = static_cast<uint64_t>(high) << 32u | low;
            if (new_time == UINT64_MAX) {
                RespondWith("400 Bad Request", R"({"message": "Invalid time"})");
                return;
            }
        } else if (!doc["time_low"].isNull()) {
            RespondWith("400 Bad Request", R"({"message": "time_low is set, but time_high is missing"})");
            return;
        }

        if (new_time != UINT64_MAX) {
            datetime_t dt = { 0 };
            dt.sec = new_time % 60u;
            new_time /= 60;
            dt.min = new_time % 60u;
            new_time /= 60;
            dt.hour = new_time % 24u;
            new_time /= 24;
            dt.dotw = new_time % 7u;
            new_time /= 7;
            dt.day = new_time % 31u + 1;
            new_time /= 31;
            dt.month = new_time % 12u + 1;
            new_time /= 12;
            dt.year = new_time % 4096;
            new_time /= 4096;
            if (!m_server->m_params.rtc->Set(dt)) {
                RespondWith("400 Bad Request", R"({"message": "Could not set time, probably invalid date"})");
                return;
            }
        }
        m_server->m_params.storage->WriteAccessAndProgramLocked(portMAX_DELAY, [&](Flash::Settings& settings) {
            Mode current_mode = Mode::UNKNOWN;

            if (next_mode != Mode::UNKNOWN) {
                int mode = 0;
                if (next_mode == Mode::AUTO_STATIC) {
                    mode = Flash::bAUTO;
                    m_server->m_params.control_auto->Give();
                    m_server->m_params.auto_hourly->Take(0);
                    current_mode = Mode::AUTO_STATIC;
                } else if (next_mode == Mode::AUTO_HOURLY) {
                    mode = Flash::bAUTO | Flash::bAUTO_HOURLY;
                    m_server->m_params.control_auto->Give();
                    m_server->m_params.auto_hourly->Give();
                    current_mode = Mode::AUTO_HOURLY;
                } else {
                    m_server->m_params.control_auto->Take(0);
                    m_server->m_params.auto_hourly->Take(0);
                    current_mode = Mode::MANUAL;
                }
                settings.sys_mode = Flash::SystemMode(mode);
            }

            if (current_mode == Mode::UNKNOWN) {
                if (m_server->m_params.control_auto->Count() == 0) {
                    current_mode = Mode::MANUAL;
                } else if (m_server->m_params.auto_hourly->Count() == 0) {
                    current_mode = Mode::AUTO_STATIC;
                } else {
                    current_mode = Mode::AUTO_HOURLY;
                }
            }

            if (manual_target != -1) {
                settings.motor_target = static_cast<int8_t>(manual_target);
                if (current_mode == Mode::MANUAL) {
                    m_server->m_params.motor_command->Overwrite(Motor::Command(manual_target));
                }
            } else if (next_mode == Mode::MANUAL) {
                m_server->m_params.motor_command->Overwrite(Motor::Command(settings.motor_target));
            }

            if (!std::isnan(static_target)) {
                settings.lux_targets[Flash::LUX_STATIC] = static_target;
                if (current_mode != Mode::AUTO_HOURLY) {
                    m_server->m_params.lux_target->Overwrite(static_target);
                }
            }

            if (!std::isnan(hourly_targets[0])) {
                for (size_t i = 0; i < 24; i++) {
                    settings.lux_targets[Flash::H00 + i] = hourly_targets.at(i);
                }
            }
        });

        std::string res_body = m_server->BuildBody(false, true);
        RespondWith("200 OK", res_body.c_str());
    } else if (path == "/calibrate") {
        m_server->m_params.motor_command->Overwrite(Motor::Command::CALIBRATE);
        RespondWith("202 Accepted", R"({})");
    } else {
        RespondWith("404 Not Found", R"({"message":"Page not found"})");
    }
}
