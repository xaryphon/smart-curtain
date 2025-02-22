#pragma once

#include <hardware/gpio.h>

class LED {
public:
    explicit LED(const uint& pin, const char* name);
    bool Put(const bool& state);
    bool Set(const uint16_t& new_level);
    bool Adjust(const int& increment);

    bool Toggle() { return Put(!m_on); }
    [[nodiscard]] bool State() const { return m_on; }
    [[nodiscard]] uint16_t Level() const {return m_level;}
    [[nodiscard]] const char* Name() const { return m_name; }

private:
    static constexpr uint16_t DIVIDER = 125;
    static constexpr uint16_t WRAP = 99;
    static constexpr uint16_t INIT_LEVEL = 2;

    uint m_pin;
    uint m_slice;
    uint m_channel;
    const char* m_name;
    uint16_t m_level = INIT_LEVEL;
    bool m_on = false;
};

void test_led();
