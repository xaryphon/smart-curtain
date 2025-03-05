#include "HttpServer.hpp"

#include <FreeRTOS.h>
#include <lwip/autoip.h>
#include <lwip/dhcp.h>
#include <task.h>

#include "HttpConnection.hpp"
#include "Logger.hpp"

HttpServer::HttpServer(const ConstructionParameters& params)
    : m_pcb(nullptr)
    , m_params(params)
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

    err_t err = tcp_bind(m_pcb, IP_ADDR_ANY, m_params.port);
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

std::string HttpServer::BuildBody(bool include_status, bool include_settings)
{
    std::string body = "{";
    auto ins = std::back_inserter(body);
    if (include_status) {
        if (body.size() != 1) {
            body += ',';
        }
        Motor::Command motor_command;
        if (!m_params.motor_command->Peek(&motor_command, pdMS_TO_TICKS(100))) {
            motor_command = Motor::Command::STOP;
        }
        const char* mode = nullptr;
        if (motor_command == Motor::Command::CALIBRATE) {
            mode = "calibrating";
        } else if (m_params.control_auto->Count() == 0) {
            mode = "manual";
        } else {
            mode = "auto_static";
            // TODO: Check static vs hourly
        }
        fmt::format_to(ins, R"("mode":"{}",)", mode);
        fmt::format_to(ins, R"("motor":{{)");
        int motor_pos = m_params.motor->GetBeltPosition();
        int motor_max = m_params.motor->GetBeltMaximum();
        if (motor_max == 0) {
            motor_max = -1;
        }
        int motor_percent = motor_pos * 100 / motor_max;
        int motor_target = -1;
        if (motor_command == Motor::Command::CALIBRATE) {
            motor_pos = 0;
            motor_max = 0;
            motor_percent = 0;
            motor_target = 0;
        } else if (motor_command == Motor::Command::STOP) {
            motor_target = motor_percent;
        } else if (motor_command == Motor::Command::OPEN_COMPLETELY) {
            motor_target = 100;
        } else if (motor_command == Motor::Command::CLOSE_COMPLETELY) {
            motor_target = 0;
        } else {
            motor_target = 100 - motor_command;
        }
        assert(motor_target >= 0 && motor_target <= 100);
        fmt::format_to(ins, R"("target":{},)", motor_target);
        fmt::format_to(ins, R"("current":{},)", motor_percent);
        fmt::format_to(ins, R"("current_raw":{},)", motor_pos);
        fmt::format_to(ins, R"("length_raw":{})", motor_max);
        fmt::format_to(ins, "}},");
        fmt::format_to(ins, R"("lux":{{)");
        float lux_target = 0.0f;
        if (!m_params.lux_target->Peek(&lux_target, pdMS_TO_TICKS(100))) {
            lux_target = 0.0f;
        }
        fmt::format_to(ins, R"("target":{},)", lux_target);
        float lux_avg = -1.f;
        LuxMeasurement lux_als1 = {};
        LuxMeasurement lux_als2 = {};
        if (!m_params.als1->Peek(&lux_als1, 0)) {
            lux_als1.lux = -1.0f;
        }
        if (!m_params.als2->Peek(&lux_als2, 0)) {
            lux_als2.lux = -1.0f;
        }
        if (lux_als1.lux >= 0.0f) {
            if (lux_als2.lux >= 0.0f) {
                lux_avg = (lux_als1.lux + lux_als2.lux) / 2.0f;
            } else {
                lux_avg = lux_als1.lux;
            }
        } else {
            if (lux_als2.lux >= 0.0f) {
                lux_avg = lux_als2.lux;
            } else {
                lux_avg = -1.0f;
            }
        }
        fmt::format_to(ins, R"("current":{},)", lux_avg);
        fmt::format_to(ins, R"("current_raw":[{},{}])", lux_als1.lux, lux_als2.lux);
        fmt::format_to(ins, "}}");
    }
    if (include_settings) {
        if (body.size() != 1) {
            body += ',';
        }
        // TODO: Get values from storage
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

err_t HttpServer::AcceptCallback(struct tcp_pcb* newpcb, err_t err)
{
    if (err != ERR_OK) {
        tcp_abort(newpcb);
        return ERR_ABRT;
    }

    Logger::Log("Accepted client");
    new HttpConnection(HttpConnection::ConstructionParameters {
        .pcb = newpcb,
        .server = this,
    });

    return ERR_OK;
}
