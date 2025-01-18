#include "SPIDevice.h"

SPIDevice::SPIDevice(SPI* spi, SPI::CS pin_cs)
    : m_spi(spi)
    , m_cs(pin_cs)
{
    // NOTE: We can't use the pico's builtin CS as it goes high after every byte
    // This isn't written anywhere in the pico-sdk documentation, might be in the RP2040 data sheet.
    // More info here: https://forums.raspberrypi.com/viewtopic.php?t=322617
    gpio_init(static_cast<uint>(pin_cs));
    gpio_set_dir(static_cast<uint>(pin_cs), GPIO_OUT);
    gpio_put(static_cast<uint>(pin_cs), true); // CS is active low
}
