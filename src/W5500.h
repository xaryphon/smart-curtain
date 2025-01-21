#pragma once

#include <lwip/netif.h>

#include "SPIDevice.h"

class W5500 {
public:
    enum class INT : uint;
    enum class RST : uint;
    explicit W5500(SPI* spi, SPI::CS pin_cs, RST pin_rst);

    void Reset();

    struct HWAddress {
        uint8_t bytes[6];
    };

    struct IPAddress {
        uint8_t bytes[4];
    };

    struct Port {
        uint8_t bytes[2];
    };

    enum Mode : uint8_t {
        // clang-format off
        //          MR_Reserved      = 0x01,
        /* FARP  */ MR_ForceARP      = 0x02,
        //          MR_Reserved      = 0x04,
        /* PPPoE */ MR_PPPoEMode     = 0x08,
        /* PB    */ MR_PingBlockMode = 0x10,
        /* WOL   */ MR_WakeOnLAN     = 0x20,
        //          MR_Reserved      = 0x40,
        /* RST   */ MR_Reset         = 0x80,
        // clang-format on
    };

    enum Interrupt : uint8_t {
        // clang-format off
        //             IR_Reserved               = 0x01,
        //             IR_Reserved               = 0x02,
        //             IR_Reserved               = 0x04,
        //             IR_Reserved               = 0x08,
        /* MP       */ IR_MagicPacket            = 0x10,
        /* PPPoE    */ IR_PPPoEConnectionClose   = 0x20,
        /* UNREACH  */ IR_DestinationUnreachable = 0x40,
        /* CONFLICT */ IR_IPConflict             = 0x80,
        // clang-format on
    };

    enum PHYConfiguration : uint8_t {
        // clang-format off
        /* LNK   */ PHYCFGR_LinkStatus                     = 0x01,
        /* SPD   */ PHYCFGR_SpeedStatus                    = 0x02,
        /* DPX   */ PHYCFGR_DuplexStatus                   = 0x04,
        /* OPMDC */ PHYCFGR_OperationModeConfigurationMask = 0x38,
        /*       */ PHYCFGR_OPMDC_10_Half_NoNeg            = 0x00,
        /*       */ PHYCFGR_OPMDC_10_Full_NoNeg            = 0x08,
        /*       */ PHYCFGR_OPMDC_100_Half_NoNeg           = 0x10,
        /*       */ PHYCFGR_OPMDC_100_Full_NoNeg           = 0x18,
        /*       */ PHYCFGR_OPMDC_100_Full_AutoNeg         = 0x20,
        /*       */ PHYCFGR_OPMDC_Unused                   = 0x28,
        /*       */ PHYCFGR_OPMDC_PowerDown                = 0x30,
        /*       */ PHYCFGR_OPMDC_AllCap_AutoNeg           = 0x38,
        /* OPMD  */ PHYCFGR_ConfigurePHYOperationMode      = 0x40,
        /* RST   */ PHYCFGR_Reset                          = 0x80,
        // clang-format on
    };

    enum SocketMode : uint8_t {
        // clang-format off
        /* P0,P1,P2,P3  */ Sn_MR_Protocol_Mask             = 0x0f,
        /*              */ Sn_MR_P_Closed                  = 0x00,
        /*              */ Sn_MR_P_TCP                     = 0x01,
        /*              */ Sn_MR_P_UDP                     = 0x02,
        /*              */ Sn_MR_P_MACRAW                  = 0x04,
        /* UCASTB/MIP6B */ Sn_MR_UDP_UnicastBlocking       = 0x10,
        /*              */ Sn_MR_MACRAW_IPv6PacketBlocking = 0x10,
        /* ND/MC/MMB    */ Sn_MR_TCP_UseNoDelayedACK       = 0x20,
        /*              */ Sn_MR_UDP_MulticastIGMPVersion  = 0x20,
        /*              */ Sn_MR_MACRAW_MulticastBlocking  = 0x20,
        /* BCASTB       */ Sn_MR_BroadcastBlocking         = 0x40,
        /* MULTI/MFEN   */ Sn_MR_UDP_Multicast             = 0x80,
        /*              */ Sn_MR_MACRAW_MACFilterEnable    = 0x80,
        // clang-format on
    };

    enum SocketCommand : uint8_t {
        // clang-format off
        Sn_CR_OPEN      = 0x01,
        Sn_CR_LISTEN    = 0x02,
        Sn_CR_CONNECT   = 0x04,
        Sn_CR_DISCON    = 0x08,
        Sn_CR_CLOSE     = 0x10,
        Sn_CR_SEND      = 0x20,
        Sn_CR_SEND_MAC  = 0x21,
        Sn_CR_SEND_KEEP = 0x22,
        Sn_CR_RECV      = 0x40,
        // clang-format on
    };

    enum SocketInterrupt : uint8_t {
        // clang-format off
        Sn_IR_CON     = 0x01,
        Sn_IR_DISCON  = 0x02,
        Sn_IR_RECV    = 0x04,
        Sn_IR_TIMEOUT = 0x08,
        Sn_IR_SEND_OK = 0x10,
        Sn_IR_ALL     = 0x1f,
        // clang-format on
    };

    enum SocketStatus : uint8_t {
        // clang-format off
        Sn_SR_SOCK_CLOSED      = 0x00,
        Sn_SR_SOCK_INIT        = 0x13,
        Sn_SR_SOCK_LISTEN      = 0x14,
        Sn_SR_SOCK_ESTABLISHED = 0x17,
        Sn_SR_SOCK_CLOSE_WAIT  = 0x1C,
        Sn_SR_SOCK_UDP         = 0x22,
        Sn_SR_SOCK_MACRAW      = 0x42,
        Sn_SR_SOCK_SYNSENT     = 0x15,
        Sn_SR_SOCK_SYNRECV     = 0x16,
        Sn_SR_SOCK_FIN_WAIT    = 0x18,
        Sn_SR_SOCK_CLOSING     = 0x1A,
        Sn_SR_SOCK_TIME_WAIT   = 0x1B,
        Sn_SR_SOCK_LAST_ACK    = 0x1D,
        // clang-format on
    };

    enum SocketBufferSize : uint8_t {
        //clang-format off
        Sn_BUFFER_SIZE_0KB = 0,
        Sn_BUFFER_SIZE_1KB = 1,
        Sn_BUFFER_SIZE_2KB = 2,
        Sn_BUFFER_SIZE_4KB = 4,
        Sn_BUFFER_SIZE_8KB = 8,
        Sn_BUFFER_SIZE_16KB = 16,
        //clang-format on
    };

    /* MR       */ bool ReadMode(Mode* value);
    /* GAR      */ bool ReadGatewayIPAddress(IPAddress* value);
    /* SUBR     */ bool ReadSubnetMask(IPAddress* value);
    /* SHAR     */ bool ReadSourceHardwareAddress(HWAddress* value);
    /* SIPR     */ bool ReadSourceIPAddress(IPAddress* value);
    /* INTLEVEL */ bool ReadInterruptLowLevelTimer(uint16_t* value);
    /* IR       */ bool ReadInterrupt(Interrupt* value);
    /* IMR      */ bool ReadInterruptMask(Interrupt* value);
    /* SIR      */ bool ReadSocketInterrupt(uint8_t* value);
    /* SIMR     */ bool ReadSocketInterruptMask(uint8_t* value);
    /* RTR      */ bool ReadRetryTimeValue(uint16_t* value);
    /* RCR      */ bool ReadRetryCountRegister(uint8_t* value);
    /* PTIMER   */ bool ReadPPPLinkControlProtocolRequestTimer(uint8_t* value);
    /* PMAGIC   */ bool ReadPPPLinkControlProtocolMagicNumber(uint8_t* value);
    /* PHAR     */ bool ReadPPPoEDestinationHardwareAddress(HWAddress* value);
    /* PSID     */ bool ReadPPPoESessionId(uint16_t* value);
    /* PMRU     */ bool ReadPPPoEMaximumReceiveUnit(uint16_t* value);
    /* UIPR     */ bool ReadUnreachableIPAddress(IPAddress* value);
    /* UPORTR   */ bool ReadUnreachablePort(Port* value);
    /* PHYCFGR  */ bool ReadPHYConfiguration(PHYConfiguration* value);
    /* VERSIONR */ bool ReadChipVersion(uint8_t* value);

    /* MR       */ bool WriteMode(Mode value);
    /* GAR      */ bool WriteGatewayIPAddress(IPAddress value);
    /* SUBR     */ bool WriteSubnetMask(IPAddress value);
    /* SHAR     */ bool WriteSourceHardwareAddress(HWAddress value);
    /* SIPR     */ bool WriteSourceIPAddress(IPAddress value);
    /* INTLEVEL */ bool WriteInterruptLowLevelTimer(uint16_t value);
    /* IR       */ bool WriteInterrupt(Interrupt value);
    /* IMR      */ bool WriteInterruptMask(Interrupt value);
    /* SIR      */ bool WriteSocketInterrupt(uint8_t value);
    /* SIMR     */ bool WriteSocketInterruptMask(uint8_t value);
    /* RTR      */ bool WriteRetryTimeValue(uint16_t value);
    /* RCR      */ bool WriteRetryCountRegister(uint8_t value);
    /* PTIMER   */ bool WritePPPLinkControlProtocolRequestTimer(uint8_t value);
    /* PMAGIC   */ bool WritePPPLinkControlProtocolMagicNumber(uint8_t value);
    /* PHAR     */ bool WritePPPoEDestinationHardwareAddress(HWAddress value);
    /* PSID     */ bool WritePPPoESessionId(uint16_t value);
    /* PMRU     */ bool WritePPPoEMaximumReceiveUnit(uint16_t value);
    /* PHYCFGR  */ bool WritePHYConfiguration(PHYConfiguration value);

    /* Sn_MR         */ bool ReadSnMode(uint n, SocketMode* value);
    /* Sn_CR         */ bool ReadSnCommand(uint n, SocketCommand* value);
    /* Sn_IR         */ bool ReadSnInterrupt(uint n, SocketInterrupt* value);
    /* Sn_SR         */ bool ReadSnStatus(uint n, SocketStatus* value);
    /* Sn_PORT       */ bool ReadSnSourcePort(uint n, Port* value);
    /* Sn_DHAR       */ bool ReadSnDestinationHardwareAddress(uint n, HWAddress* value);
    /* Sn_DIPR       */ bool ReadSnDestinationIPAddress(uint n, IPAddress* value);
    /* Sn_DPORT      */ bool ReadSnDestinationPort(uint n, Port* value);
    /* Sn_MSSR       */ bool ReadSnMaximumSegmentSize(uint n, uint16_t* value);
    /* Sn_TOS        */ bool ReadSnIPTypeOfService(uint n, uint8_t* value);
    /* Sn_TTL        */ bool ReadSnTimeToLive(uint n, uint8_t* value);
    /* Sn_RXBUF_SIZE */ bool ReadSnRXBufferSize(uint n, SocketBufferSize* value);
    /* Sn_TXBUF_SIZE */ bool ReadSnTXBufferSize(uint n, SocketBufferSize* value);
    /* Sn_TX_FSR     */ bool ReadSnTXFreeSize(uint n, uint16_t* value);
    /* Sn_TX_RD      */ bool ReadSnTXReadPointer(uint n, uint16_t* value);
    /* Sn_TX_WR      */ bool ReadSnTXWritePointer(uint n, uint16_t* value);
    /* Sn_RX_RSR     */ bool ReadSnReceivedSize(uint n, uint16_t* value);
    /* Sn_RX_RD      */ bool ReadSnRXReadDataPointer(uint n, uint16_t* value);
    /* Sn_RX_WR      */ bool ReadSnRXWritePointer(uint n, uint16_t* value);
    /* Sn_IMR        */ bool ReadSnInterruptMask(uint n, SocketInterrupt* value);
    /* Sn_FRAG       */ bool ReadSnFragment(uint n, uint16_t* value);
    /* Sn_KPALVTR    */ bool ReadSnKeepAliveTime(uint n, uint8_t* value);

    /* Sn_MR         */ bool WriteSnMode(uint n, SocketMode value);
    /* Sn_CR         */ bool WriteSnCommand(uint n, SocketCommand value);
    /* Sn_IR         */ bool WriteSnInterrupt(uint n, SocketInterrupt value);
    /* Sn_PORT       */ bool WriteSnSourcePort(uint n, Port value);
    /* Sn_DHAR       */ bool WriteSnDestinationHardwareAddress(uint n, HWAddress value);
    /* Sn_DIPR       */ bool WriteSnDestinationIPAddress(uint n, IPAddress value);
    /* Sn_DPORT      */ bool WriteSnDestinationPort(uint n, Port value);
    /* Sn_MSSR       */ bool WriteSnMaximumSegmentSize(uint n, uint16_t value);
    /* Sn_TOS        */ bool WriteSnIPTypeOfService(uint n, uint8_t value);
    /* Sn_TTL        */ bool WriteSnTimeToLive(uint n, uint8_t value);
    /* Sn_RXBUF_SIZE */ bool WriteSnRXBufferSize(uint n, SocketBufferSize value);
    /* Sn_TXBUF_SIZE */ bool WriteSnTXBufferSize(uint n, SocketBufferSize value);
    /* Sn_TX_WR      */ bool WriteSnTXWritePointer(uint n, uint16_t value);
    /* Sn_RX_RD      */ bool WriteSnRXReadDataPointer(uint n, uint16_t value);
    /* Sn_IMR        */ bool WriteSnInterruptMask(uint n, SocketInterrupt value);
    /* Sn_FRAG       */ bool WriteSnFragment(uint n, uint16_t value);
    /* Sn_KPALVTR    */ bool WriteSnKeepAliveTime(uint n, uint8_t value);

    uint16_t ReadSnReceiveBuffer(uint n, uint8_t* buffer, uint16_t len);
    uint16_t WriteSnTransmitBuffer(uint n, const uint8_t* buffer, uint16_t len);

private:
    enum BlockSelectBits : uint8_t {
        // clang-format off
        BSB_COMMON_REGISTER  = 0b00000000,
        BSB_SOCKET0_REGISTER = 0b00001000,
        BSB_SOCKET0_TXBUFFER = 0b00010000,
        BSB_SOCKET0_RXBUFFER = 0b00011000,
        //          RESERVED = 0b00100000,
        BSB_SOCKET1_REGISTER = 0b00101000,
        BSB_SOCKET1_TXBUFFER = 0b00110000,
        BSB_SOCKET1_RXBUFFER = 0b00111000,
        //          RESERVED = 0b01000000,
        BSB_SOCKET2_REGISTER = 0b01001000,
        BSB_SOCKET2_TXBUFFER = 0b01010000,
        BSB_SOCKET2_RXBUFFER = 0b01011000,
        //          RESERVED = 0b01100000,
        BSB_SOCKET3_REGISTER = 0b01101000,
        BSB_SOCKET3_TXBUFFER = 0b01110000,
        BSB_SOCKET3_RXBUFFER = 0b01111000,
        //          RESERVED = 0b10000000,
        BSB_SOCKET4_REGISTER = 0b10001000,
        BSB_SOCKET4_TXBUFFER = 0b10010000,
        BSB_SOCKET4_RXBUFFER = 0b10011000,
        //          RESERVED = 0b10100000,
        BSB_SOCKET5_REGISTER = 0b10101000,
        BSB_SOCKET5_TXBUFFER = 0b10110000,
        BSB_SOCKET5_RXBUFFER = 0b10111000,
        //          RESERVED = 0b11000000,
        BSB_SOCKET6_REGISTER = 0b11001000,
        BSB_SOCKET6_TXBUFFER = 0b11010000,
        BSB_SOCKET6_RXBUFFER = 0b11011000,
        //          RESERVED = 0b11100000,
        BSB_SOCKET7_REGISTER = 0b11101000,
        BSB_SOCKET7_TXBUFFER = 0b11110000,
        BSB_SOCKET7_RXBUFFER = 0b11111000,
        // clang-format on
    };
    enum ReadWriteBit : uint8_t {
        // clang-format off
        RWB_READ  = 0b00000000,
        RWB_WRITE = 0b00000100,
        // clang-format on
    };
    enum OperationModeBit : uint8_t {
        OM_VARIABLE = 0b00000000,
        OM_FIXED_1B = 0b00000001,
        OM_FIXED_2B = 0b00000010,
        OM_FIXED_4B = 0b00000011,
    };

    static BlockSelectBits SnToBlock(BlockSelectBits base, uint n);

    bool Read(BlockSelectBits block, uint16_t address, uint8_t* buffer, size_t length);
    bool Write(BlockSelectBits block, uint16_t address, const uint8_t* buffer, size_t length);

    SPIDevice m_spi;
    uint m_pin_rst;
};

void w5500_test();
