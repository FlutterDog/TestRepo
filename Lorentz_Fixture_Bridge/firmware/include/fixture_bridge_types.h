#pragma once

#include <stdint.h>

namespace lorentz::fixture {

enum class ChannelId : uint8_t {
    S1 = 0,
    S2 = 1,
    S3 = 2,
    S4 = 3,
    Hmi = 4,
    X2x = 5,
    Count = 6,
};

enum class ChannelState : uint8_t {
    Disabled = 0,
    Listening,
    Connected,
    Fault,
};

enum class SerialParity : uint8_t {
    None = 0,
    Even,
    Odd,
};

struct SerialFormat {
    uint32_t baudrate;
    uint8_t data_bits;
    SerialParity parity;
    uint8_t stop_bits;
};

struct ChannelConfig {
    ChannelId id;
    uint16_t tcp_port;
    SerialFormat serial;
    bool enabled;
};

struct ChannelCounters {
    uint32_t tcp_connections;
    uint32_t tcp_disconnects;
    uint64_t tcp_rx_bytes;
    uint64_t tcp_tx_bytes;
    uint64_t uart_rx_bytes;
    uint64_t uart_tx_bytes;
    uint32_t uart_overrun_errors;
    uint32_t uart_framing_errors;
    uint32_t uart_parity_errors;
    uint32_t tcp_errors;
    uint32_t rx_buffer_overflows;
    uint32_t tx_buffer_overflows;
    uint32_t last_activity_ms;
};

struct ChannelStatus {
    ChannelState state;
    ChannelConfig config;
    ChannelCounters counters;
    int32_t last_error_code;
};

constexpr uint8_t channel_count() {
    return static_cast<uint8_t>(ChannelId::Count);
}

}  // namespace lorentz::fixture
