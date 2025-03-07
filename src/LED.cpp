#include "LED.hpp"

#include <hardware/pwm.h>

LED::LED(const uint pin, const char* name)
    : m_pin(pin)
    , m_slice(pwm_gpio_to_slice_num(pin))
    , m_channel(pwm_gpio_to_channel(pin))
    , m_name(name)
{
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv_int(&config, DIVIDER);
    pwm_config_set_wrap(&config, WRAP);

    pwm_set_enabled(m_slice, false);
    pwm_init(m_slice, &config, false);
    pwm_set_chan_level(m_slice, m_channel, m_on ? m_level : 0);
    gpio_set_function(m_pin, GPIO_FUNC_PWM);

    pwm_set_enabled(pwm_gpio_to_slice_num(m_pin), true);
}

bool LED::Put(const bool state)
{
    m_on = state;
    return Set(m_level);
}

bool LED::Set(const uint16_t new_level)
{
    if (new_level > WRAP) {
        return false;
    }
    m_level = new_level;
    pwm_set_chan_level(m_slice, m_channel, m_on ? m_level : 0);
    return true;
}

bool LED::Adjust(const int increment)
{
    int new_level = m_level + increment;
    bool at_limit = false;
    if (new_level < 0) {
        new_level = 0;
        at_limit = true;
    } else if (WRAP < new_level) {
        new_level = WRAP;
        at_limit = true;
    }
    m_level = new_level;
    return Set(m_level) && !at_limit;
}

/// TODO: remove
#include <FreeRTOS.h>
#include <task.h>

void test_led_task(void* params)
{
    auto led = LED { 20, "led" };
    led.Put(true);
    bool direction = true;

    while (true) {
        if (direction) {
            if (!led.Adjust(+1)) {
                direction = !direction;
            }
        } else {
            if (!led.Set(led.Level() -1)) {
                direction = !direction;
            }
        }

        if (led.Level() % 11 == 0) {
            led.Toggle();
        }
        vTaskDelay(10);
    }
}

void test_led()
{
    xTaskCreate(test_led_task, "asd", 512, nullptr, tskIDLE_PRIORITY + 2, nullptr);
}
