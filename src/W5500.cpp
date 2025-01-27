#include "W5500.hpp"

#include <pico/cyw43_arch.h>

W5500::W5500(SPI* spi, SPI::CS pin_cs, RST pin_rst)
    : m_spi(spi, pin_cs)
    , m_pin_rst(static_cast<uint>(pin_rst))
{
    gpio_init(m_pin_rst);
    gpio_set_dir(m_pin_rst, GPIO_OUT);
    sleep_us(500);
    gpio_put(m_pin_rst, true);
    sleep_us(1000);
}

void W5500::Reset()
{
    gpio_put(m_pin_rst, false);
    sleep_us(500);
    gpio_put(m_pin_rst, true);
    sleep_us(1000);
}

bool W5500::ReadMode(Mode* value)
{
    static_assert(sizeof(*value) == (0x0000) - (0x0000) + 1);
    return Read(BSB_COMMON_REGISTER, 0x0000, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadGatewayIPAddress(IPAddress* value)
{
    static_assert(sizeof(*value) == (0x0004) - (0x0001) + 1);
    return Read(BSB_COMMON_REGISTER, 0x0001, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadSubnetMask(IPAddress* value)
{
    static_assert(sizeof(*value) == (0x0008) - (0x0005) + 1);
    return Read(BSB_COMMON_REGISTER, 0x0005, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadSourceHardwareAddress(HWAddress* value)
{
    static_assert(sizeof(*value) == (0x000E) - (0x0009) + 1);
    return Read(BSB_COMMON_REGISTER, 0x0009, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadSourceIPAddress(IPAddress* value)
{
    static_assert(sizeof(*value) == (0x0012) - (0x000F) + 1);
    return Read(BSB_COMMON_REGISTER, 0x000F, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadInterruptLowLevelTimer(uint16_t* value)
{
    static_assert(sizeof(*value) == (0x0014) - (0x0013) + 1);
    uint8_t bytes[sizeof(*value)] = {};
    if (!Read(BSB_COMMON_REGISTER, 0x0013, bytes, sizeof(*value)))
        return false;
    *value = bytes[0] << 8 | bytes[1];
    return true;
}

bool W5500::ReadInterrupt(Interrupt* value)
{
    static_assert(sizeof(*value) == (0x0015) - (0x0015) + 1);
    return Read(BSB_COMMON_REGISTER, 0x0015, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadInterruptMask(Interrupt* value)
{
    static_assert(sizeof(*value) == (0x0016) - (0x0016) + 1);
    return Read(BSB_COMMON_REGISTER, 0x0016, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadSocketInterrupt(uint8_t* value)
{
    static_assert(sizeof(*value) == (0x0017) - (0x0017) + 1);
    return Read(BSB_COMMON_REGISTER, 0x0017, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadSocketInterruptMask(uint8_t* value)
{
    static_assert(sizeof(*value) == (0x0018) - (0x0018) + 1);
    return Read(BSB_COMMON_REGISTER, 0x0018, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadRetryTimeValue(uint16_t* value)
{
    static_assert(sizeof(*value) == (0x001A) - (0x0019) + 1);
    uint8_t bytes[sizeof(*value)] = {};
    if (!Read(BSB_COMMON_REGISTER, 0x0019, bytes, sizeof(*value)))
        return false;
    *value = bytes[0] << 8 | bytes[1];
    return true;
}

bool W5500::ReadRetryCountRegister(uint8_t* value)
{
    static_assert(sizeof(*value) == (0x001B) - (0x001B) + 1);
    return Read(BSB_COMMON_REGISTER, 0x001B, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadPPPLinkControlProtocolRequestTimer(uint8_t* value)
{
    static_assert(sizeof(*value) == (0x001C) - (0x001C) + 1);
    return Read(BSB_COMMON_REGISTER, 0x001C, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadPPPLinkControlProtocolMagicNumber(uint8_t* value)
{
    static_assert(sizeof(*value) == (0x001D) - (0x001D) + 1);
    return Read(BSB_COMMON_REGISTER, 0x001D, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadPPPoEDestinationHardwareAddress(HWAddress* value)
{
    static_assert(sizeof(*value) == (0x0023) - (0x001E) + 1);
    return Read(BSB_COMMON_REGISTER, 0x001E, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadPPPoESessionId(uint16_t* value)
{
    static_assert(sizeof(*value) == (0x0025) - (0x0024) + 1);
    uint8_t bytes[sizeof(*value)] = {};
    if (!Read(BSB_COMMON_REGISTER, 0x0024, bytes, sizeof(*value)))
        return false;
    *value = bytes[0] << 8 | bytes[1];
    return true;
}

bool W5500::ReadPPPoEMaximumReceiveUnit(uint16_t* value)
{
    static_assert(sizeof(*value) == (0x0027) - (0x0026) + 1);
    uint8_t bytes[sizeof(*value)] = {};
    if (!Read(BSB_COMMON_REGISTER, 0x0026, bytes, sizeof(*value)))
        return false;
    *value = bytes[0] << 8 | bytes[1];
    return true;
}

bool W5500::ReadUnreachableIPAddress(IPAddress* value)
{
    static_assert(sizeof(*value) == (0x002B) - (0x0028) + 1);
    return Read(BSB_COMMON_REGISTER, 0x0028, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadUnreachablePort(Port* value)
{
    static_assert(sizeof(*value) == (0x002D) - (0x002C) + 1);
    return Read(BSB_COMMON_REGISTER, 0x002C, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadPHYConfiguration(PHYConfiguration* value)
{
    static_assert(sizeof(*value) == (0x002E) - (0x002E) + 1);
    return Read(BSB_COMMON_REGISTER, 0x002E, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadChipVersion(uint8_t* value)
{
    static_assert(sizeof(*value) == (0x0039) - (0x0039) + 1);
    return Read(BSB_COMMON_REGISTER, 0x0039, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::WriteMode(Mode value)
{
    static_assert(sizeof(value) == (0x0000) - (0x0000) + 1);
    return Write(BSB_COMMON_REGISTER, 0x0000, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WriteGatewayIPAddress(IPAddress value)
{
    static_assert(sizeof(value) == (0x0004) - (0x0001) + 1);
    return Write(BSB_COMMON_REGISTER, 0x0001, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WriteSubnetMask(IPAddress value)
{
    static_assert(sizeof(value) == (0x0008) - (0x0005) + 1);
    return Write(BSB_COMMON_REGISTER, 0x0005, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WriteSourceHardwareAddress(HWAddress value)
{
    static_assert(sizeof(value) == (0x000E) - (0x0009) + 1);
    return Write(BSB_COMMON_REGISTER, 0x0009, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WriteSourceIPAddress(IPAddress value)
{
    static_assert(sizeof(value) == (0x0012) - (0x000F) + 1);
    return Write(BSB_COMMON_REGISTER, 0x000F, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WriteInterruptLowLevelTimer(uint16_t value)
{
    static_assert(sizeof(value) == (0x0014) - (0x0013) + 1);
    value = value >> 8 | value << 8;
    return Write(BSB_COMMON_REGISTER, 0x0013, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WriteInterrupt(Interrupt value)
{
    static_assert(sizeof(value) == (0x0015) - (0x0015) + 1);
    return Write(BSB_COMMON_REGISTER, 0x0015, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WriteInterruptMask(Interrupt value)
{
    static_assert(sizeof(value) == (0x0016) - (0x0016) + 1);
    return Write(BSB_COMMON_REGISTER, 0x0016, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WriteSocketInterrupt(uint8_t value)
{
    static_assert(sizeof(value) == (0x0017) - (0x0017) + 1);
    return Write(BSB_COMMON_REGISTER, 0x0017, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WriteSocketInterruptMask(uint8_t value)
{
    static_assert(sizeof(value) == (0x0018) - (0x0018) + 1);
    return Write(BSB_COMMON_REGISTER, 0x0018, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WriteRetryTimeValue(uint16_t value)
{
    static_assert(sizeof(value) == (0x001A) - (0x0019) + 1);
    value = value >> 8 | value << 8;
    return Write(BSB_COMMON_REGISTER, 0x0019, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WriteRetryCountRegister(uint8_t value)
{
    static_assert(sizeof(value) == (0x001B) - (0x001B) + 1);
    return Write(BSB_COMMON_REGISTER, 0x001B, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WritePPPLinkControlProtocolRequestTimer(uint8_t value)
{
    static_assert(sizeof(value) == (0x001C) - (0x001C) + 1);
    return Write(BSB_COMMON_REGISTER, 0x001C, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WritePPPLinkControlProtocolMagicNumber(uint8_t value)
{
    static_assert(sizeof(value) == (0x001D) - (0x001D) + 1);
    return Write(BSB_COMMON_REGISTER, 0x001D, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WritePPPoEDestinationHardwareAddress(HWAddress value)
{
    static_assert(sizeof(value) == (0x0023) - (0x001E) + 1);
    return Write(BSB_COMMON_REGISTER, 0x001E, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WritePPPoESessionId(uint16_t value)
{
    static_assert(sizeof(value) == (0x0025) - (0x0024) + 1);
    value = value >> 8 | value << 8;
    return Write(BSB_COMMON_REGISTER, 0x0024, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WritePPPoEMaximumReceiveUnit(uint16_t value)
{
    static_assert(sizeof(value) == (0x0027) - (0x0026) + 1);
    value = value >> 8 | value << 8;
    return Write(BSB_COMMON_REGISTER, 0x0026, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WritePHYConfiguration(PHYConfiguration value)
{
    static_assert(sizeof(value) == (0x002E) - (0x002E) + 1);
    return Write(BSB_COMMON_REGISTER, 0x002E, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::ReadSnMode(uint n, SocketMode* value)
{
    static_assert(sizeof(*value) == (0x0000) - (0x0000) + 1);
    return Read(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x0000, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadSnCommand(uint n, SocketCommand* value)
{
    static_assert(sizeof(*value) == (0x0001) - (0x0001) + 1);
    return Read(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x0001, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadSnInterrupt(uint n, SocketInterrupt* value)
{
    static_assert(sizeof(*value) == (0x0002) - (0x0002) + 1);
    return Read(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x0002, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadSnStatus(uint n, SocketStatus* value)
{
    static_assert(sizeof(*value) == (0x0003) - (0x0003) + 1);
    return Read(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x0003, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadSnSourcePort(uint n, Port* value)
{
    static_assert(sizeof(*value) == (0x0005) - (0x0004) + 1);
    return Read(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x0004, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadSnDestinationHardwareAddress(uint n, HWAddress* value)
{
    static_assert(sizeof(*value) == (0x000B) - (0x0006) + 1);
    return Read(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x0006, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadSnDestinationIPAddress(uint n, IPAddress* value)
{
    static_assert(sizeof(*value) == (0x000F) - (0x000C) + 1);
    return Read(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x000C, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadSnDestinationPort(uint n, Port* value)
{
    static_assert(sizeof(*value) == (0x0011) - (0x0010) + 1);
    return Read(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x0010, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadSnMaximumSegmentSize(uint n, uint16_t* value)
{
    static_assert(sizeof(*value) == (0x0013) - (0x0012) + 1);
    uint8_t bytes[sizeof(*value)] = {};
    if (!Read(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x0012, bytes, sizeof(*value)))
        return false;
    *value = bytes[0] << 8 | bytes[1];
    return true;
}

bool W5500::ReadSnIPTypeOfService(uint n, uint8_t* value)
{
    static_assert(sizeof(*value) == (0x0015) - (0x0015) + 1);
    return Read(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x0015, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadSnTimeToLive(uint n, uint8_t* value)
{
    static_assert(sizeof(*value) == (0x0016) - (0x0016) + 1);
    return Read(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x0016, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadSnRXBufferSize(uint n, SocketBufferSize* value)
{
    static_assert(sizeof(*value) == (0x001E) - (0x001E) + 1);
    return Read(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x001E, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadSnTXBufferSize(uint n, SocketBufferSize* value)
{
    static_assert(sizeof(*value) == (0x001F) - (0x001F) + 1);
    return Read(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x001F, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadSnTXFreeSize(uint n, uint16_t* value)
{
    static_assert(sizeof(*value) == (0x0021) - (0x0020) + 1);
    uint8_t bytes[sizeof(*value)] = {};
    if (!Read(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x0020, bytes, sizeof(*value)))
        return false;
    *value = bytes[0] << 8 | bytes[1];
    return true;
}

bool W5500::ReadSnTXReadPointer(uint n, uint16_t* value)
{
    static_assert(sizeof(*value) == (0x0023) - (0x0022) + 1);
    uint8_t bytes[sizeof(*value)] = {};
    if (!Read(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x0022, bytes, sizeof(*value)))
        return false;
    *value = bytes[0] << 8 | bytes[1];
    return true;
}

bool W5500::ReadSnTXWritePointer(uint n, uint16_t* value)
{
    static_assert(sizeof(*value) == (0x0025) - (0x0024) + 1);
    uint8_t bytes[sizeof(*value)] = {};
    if (!Read(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x0024, bytes, sizeof(*value)))
        return false;
    *value = bytes[0] << 8 | bytes[1];
    return true;
}

bool W5500::ReadSnReceivedSize(uint n, uint16_t* value)
{
    static_assert(sizeof(*value) == (0x0027) - (0x0026) + 1);
    uint8_t bytes[sizeof(*value)] = {};
    if (!Read(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x0026, bytes, sizeof(*value)))
        return false;
    *value = bytes[0] << 8 | bytes[1];
    return true;
}

bool W5500::ReadSnRXReadDataPointer(uint n, uint16_t* value)
{
    static_assert(sizeof(*value) == (0x0029) - (0x0028) + 1);
    uint8_t bytes[sizeof(*value)] = {};
    if (!Read(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x0028, bytes, sizeof(*value)))
        return false;
    *value = bytes[0] << 8 | bytes[1];
    return true;
}

bool W5500::ReadSnRXWritePointer(uint n, uint16_t* value)
{
    static_assert(sizeof(*value) == (0x002B) - (0x002A) + 1);
    uint8_t bytes[sizeof(*value)] = {};
    if (!Read(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x002A, bytes, sizeof(*value)))
        return false;
    *value = bytes[0] << 8 | bytes[1];
    return true;
}

bool W5500::ReadSnInterruptMask(uint n, SocketInterrupt* value)
{
    static_assert(sizeof(*value) == (0x002C) - (0x002C) + 1);
    return Read(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x002C, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::ReadSnFragment(uint n, uint16_t* value)
{
    static_assert(sizeof(*value) == (0x002E) - (0x002D) + 1);
    uint8_t bytes[sizeof(*value)] = {};
    if (!Read(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x002D, bytes, sizeof(*value)))
        return false;
    *value = bytes[0] << 8 | bytes[1];
    return true;
}

bool W5500::ReadSnKeepAliveTime(uint n, uint8_t* value)
{
    static_assert(sizeof(*value) == (0x002F) - (0x002F) + 1);
    return Read(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x002F, reinterpret_cast<uint8_t*>(value), sizeof(*value));
}

bool W5500::WriteSnMode(uint n, SocketMode value)
{
    static_assert(sizeof(value) == (0x0000) - (0x0000) + 1);
    return Write(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x0000, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WriteSnCommand(uint n, SocketCommand value)
{
    static_assert(sizeof(value) == (0x0001) - (0x0001) + 1);
    return Write(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x0001, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WriteSnInterrupt(uint n, SocketInterrupt value)
{
    static_assert(sizeof(value) == (0x0002) - (0x0002) + 1);
    return Write(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x0002, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WriteSnSourcePort(uint n, Port value)
{
    static_assert(sizeof(value) == (0x0005) - (0x0004) + 1);
    return Write(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x0004, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WriteSnDestinationHardwareAddress(uint n, HWAddress value)
{
    static_assert(sizeof(value) == (0x000B) - (0x0006) + 1);
    return Write(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x0006, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WriteSnDestinationIPAddress(uint n, IPAddress value)
{
    static_assert(sizeof(value) == (0x000F) - (0x000C) + 1);
    return Write(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x000C, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WriteSnDestinationPort(uint n, Port value)
{
    static_assert(sizeof(value) == (0x0011) - (0x0010) + 1);
    return Write(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x0010, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WriteSnMaximumSegmentSize(uint n, uint16_t value)
{
    static_assert(sizeof(value) == (0x0013) - (0x0012) + 1);
    value = value << 8 | value >> 8;
    return Write(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x0012, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WriteSnIPTypeOfService(uint n, uint8_t value)
{
    static_assert(sizeof(value) == (0x0015) - (0x0015) + 1);
    return Write(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x0015, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WriteSnTimeToLive(uint n, uint8_t value)
{
    static_assert(sizeof(value) == (0x0016) - (0x0016) + 1);
    return Write(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x0016, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WriteSnRXBufferSize(uint n, SocketBufferSize value)
{
    static_assert(sizeof(value) == (0x001E) - (0x001E) + 1);
    return Write(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x001E, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WriteSnTXBufferSize(uint n, SocketBufferSize value)
{
    static_assert(sizeof(value) == (0x001F) - (0x001F) + 1);
    return Write(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x001F, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WriteSnTXWritePointer(uint n, uint16_t value)
{
    static_assert(sizeof(value) == (0x0025) - (0x0024) + 1);
    value = value << 8 | value >> 8;
    return Write(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x0024, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WriteSnRXReadDataPointer(uint n, uint16_t value)
{
    static_assert(sizeof(value) == (0x0029) - (0x0028) + 1);
    value = value << 8 | value >> 8;
    return Write(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x0028, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WriteSnInterruptMask(uint n, SocketInterrupt value)
{
    static_assert(sizeof(value) == (0x002C) - (0x002C) + 1);
    return Write(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x002C, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WriteSnFragment(uint n, uint16_t value)
{
    static_assert(sizeof(value) == (0x002E) - (0x002D) + 1);
    value = value << 8 | value >> 8;
    return Write(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x002D, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

bool W5500::WriteSnKeepAliveTime(uint n, uint8_t value)
{
    static_assert(sizeof(value) == (0x002F) - (0x002F) + 1);
    return Write(SnToBlock(BSB_SOCKET0_REGISTER, n), 0x002F, reinterpret_cast<uint8_t*>(&value), sizeof(value));
}

uint16_t W5500::ReadSnReceiveBuffer(uint n, uint8_t* buffer, uint16_t len)
{
    if (len == 0) {
        return 0;
    }
    uint16_t ptr;
    if (!ReadSnRXReadDataPointer(n, &ptr)) {
        printf("Failed to read W5500 S0_RX_RD\n");
        return 0;
    }
    if (!Read(SnToBlock(BSB_SOCKET0_RXBUFFER, n), ptr, buffer, len)) {
        return 0;
    }
    ptr += len;
    WriteSnRXReadDataPointer(n, ptr);
    return len;
}

uint16_t W5500::WriteSnTransmitBuffer(uint n, const uint8_t* buffer, uint16_t len)
{
    if (len == 0) {
        return 0;
    }
    uint16_t ptr;
    if (!ReadSnTXWritePointer(n, &ptr)) {
        printf("Failed to read W5500 Sn_TX_WR\n");
        return 0;
    }
    if (!Write(SnToBlock(BSB_SOCKET0_TXBUFFER, n), ptr, buffer, len)) {
        return 0;
    }
    ptr += len;
    WriteSnTXWritePointer(n, ptr);
    return len;
}

W5500::BlockSelectBits W5500::SnToBlock(BlockSelectBits base, uint n)
{
    assert(n < 8);
    return static_cast<BlockSelectBits>(base | (n << 5));
}

bool W5500::Read(BlockSelectBits block, uint16_t address, uint8_t* buffer, size_t length)
{
    const uint8_t preamble[3] = {
        static_cast<uint8_t>(address >> 8U),
        static_cast<uint8_t>(address),
        static_cast<uint8_t>(static_cast<uint>(block) | RWB_READ | OM_VARIABLE),
    };

    SPI::TransmitBuffer wbufs[2] = { { preamble, sizeof(preamble) }, { nullptr, length } };
    SPI::ReceiveBuffer rbufs[2] = { { nullptr, sizeof(preamble) }, { buffer, length } };
    uint count = m_spi.Transaction(wbufs, 2, rbufs, 2);
    return count == sizeof(preamble) + length;
}

bool W5500::Write(BlockSelectBits block, uint16_t address, const uint8_t* buffer, size_t length)
{
    const uint8_t preamble[3] = {
        static_cast<uint8_t>(address >> 8U),
        static_cast<uint8_t>(address),
        static_cast<uint8_t>(static_cast<uint>(block) | RWB_WRITE | OM_VARIABLE),
    };

    SPI::TransmitBuffer wbufs[2] = { { preamble, sizeof(preamble) }, { buffer, length } };
    SPI::ReceiveBuffer rbuf = { nullptr, sizeof(preamble) + length };
    uint count = m_spi.Transaction(wbufs, 2, &rbuf, 1);
    return count == sizeof(preamble) + length;
}

void w5500_test_task(void* param)
{
    if (cyw43_arch_init() != 0) {
        printf("cyw43_arch_init() failed\n");
        for (;;) {
            vTaskDelay(10000);
        }
    }

    SPI* spi = new SPI(SPI::RX0::PIN_16, SPI::TX0::PIN_19, SPI::SCK0::PIN_18, 10'000'000);
    W5500 w5500 = W5500(spi, SPI::CS(17), W5500::RST(20));
    if (uint8_t version; w5500.ReadChipVersion(&version)) {
        printf("Chip Version: %u\n", version);
    } else {
        printf("Failed to read chip version\n");
    }
    if (W5500::HWAddress address; w5500.ReadSourceHardwareAddress(&address)) {
        printf("HW Address: %02x:%02x:%02x:%02x:%02x:%02x\n", address.bytes[0], address.bytes[1], address.bytes[2], address.bytes[3], address.bytes[4], address.bytes[5]);
    } else {
        printf("Failed to read source hw address\n");
    }
    if (W5500::Mode mode; w5500.ReadMode(&mode)) {
        printf("Mode: 0x%x\n", mode);
    } else {
        printf("Failed to read mode\n");
    }
    if (uint16_t retry_time; w5500.ReadRetryTimeValue(&retry_time)) {
        printf("Retry Time-value: 0x%04x\n", retry_time);
    } else {
        printf("Failed to read retry time-value\n");
    }

    // Set HW Address
    // first octet
    //     bit 1 must be set for a locally administered address
    //     bit 0 must be unset for unicast address
    // IEEE 802c further reserved addresses but we don't really need to care about it
    W5500::HWAddress address = { .bytes = { 0xDE, 0xAD, 0xBE, 0xEF, 0x13, 0x37 } };
    if (w5500.WriteSourceHardwareAddress(address)) {
        printf("Wrote hardware address\n");
    } else {
        printf("Failed to write source hardware address\n");
    }

    if (W5500::HWAddress address; w5500.ReadSourceHardwareAddress(&address)) {
        printf("HW Address: %02x:%02x:%02x:%02x:%02x:%02x\n", address.bytes[0], address.bytes[1], address.bytes[2], address.bytes[3], address.bytes[4], address.bytes[5]);
    } else {
        printf("Failed to read source hw address\n");
    }

    if (W5500::PHYConfiguration cfg; w5500.ReadPHYConfiguration(&cfg)) {
        printf("PHYConfiguration 0x%02x\n", cfg);
    } else {
        printf("Failed to read PHYConfiguration\n");
    }

    for (;;) {
        vTaskDelay(1000);
    }
}

void w5500_test()
{
    xTaskCreate(w5500_test_task, "test", 256, nullptr, tskIDLE_PRIORITY + 2, nullptr);
}
