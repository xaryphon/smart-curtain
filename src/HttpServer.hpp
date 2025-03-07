#pragma once

#include <forward_list>

#include <lwip/tcp.h>
#include <picohttpparser.h>

#include "AmbientLightSensor.hpp"
#include "Motor.hpp"
#include "Storage.hpp"

class HttpConnection;
class HttpServer {
public:
    struct ConstructionParameters {
        uint16_t port;
        const Motor* motor;
        RTOS::Variable<LuxMeasurement>* als1;
        RTOS::Variable<LuxMeasurement>* als2;
        RTOS::Semaphore* notify;
        RTOS::Variable<Motor::Command>* motor_command;
        RTOS::Variable<float>* lux_target;
        RTOS::Semaphore* control_auto;
        RTOS::Semaphore* auto_hourly;
        Storage* storage;
        RTC* rtc;
    };

    explicit HttpServer(const ConstructionParameters&);
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer(HttpServer&&) = delete;

    HttpServer& operator=(const HttpServer&) = delete;
    HttpServer& operator=(HttpServer&&) = delete;

    bool Listen();
    std::string BuildBody(bool include_status, bool include_settings);

private:
    err_t AcceptCallback(struct tcp_pcb* newpcb, err_t err);
    void TaskEntry();

    struct tcp_pcb* m_pcb;
    const ConstructionParameters m_params;
    std::forward_list<HttpConnection*> m_subscribed;
    friend HttpConnection;
};

void test_http_server();
