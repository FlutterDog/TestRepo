/**
 * @file sc16is_echo_test.cpp
 * @brief Реализация echo-test внешних UART SC16IS7xx.
 */

#include "sc16is_echo_test.hpp"
#include "../../board/lcp_sc16is.hpp"
#include "../../hal/sc16is7xx.hpp"
#include "../../platform/platform.hpp"

static const uint32_t SC16IS_ECHO_BAUDRATE = 9600U;
static const uint32_t SC16IS_INTERFRAME_GAP_MS = 5U;
static const uint32_t SC16IS_RESPONSE_DELAY_MS = 10U;
static const uint8_t SC16IS_MAX_RX_BYTES_PER_POLL = 16U;
static const uint16_t SC16IS_ECHO_BUFFER_SIZE = 128U;

struct Sc16isEchoPort
{
    LcpSc16isPort port;
    const char* name;
    uint8_t is_s1;
    uint8_t buffer[SC16IS_ECHO_BUFFER_SIZE];
    uint16_t length;
    uint32_t last_rx_ms;
    uint32_t response_due_ms;
    uint8_t response_pending;
};

static Sc16isEchoPort g_sc16is_echo_ports[3];
static uint8_t g_sc16is_echo_port_count = 0U;
static uint8_t g_probe_report_printed = 0U;
static uint8_t g_s1_echo_enabled = 1U;

static void sc16is_echo_add_port(const LcpSc16isPort& port,
                                 const char* name,
                                 uint8_t is_s1)
{
    if ((port.present == 0U) || (g_sc16is_echo_port_count >= 3U))
    {
        return;
    }

    Sc16isEchoPort& echo_port = g_sc16is_echo_ports[g_sc16is_echo_port_count];

    echo_port.port = port;
    echo_port.name = name;
    echo_port.is_s1 = (is_s1 != 0U) ? 1U : 0U;
    echo_port.length = 0U;
    echo_port.last_rx_ms = 0U;
    echo_port.response_due_ms = 0U;
    echo_port.response_pending = 0U;

    ++g_sc16is_echo_port_count;
}

static void sc16is_echo_drop_frame(Sc16isEchoPort& port, uint32_t now_ms)
{
    port.length = 0U;
    port.last_rx_ms = now_ms;
    port.response_due_ms = 0U;
    port.response_pending = 0U;
}

static void sc16is_echo_accept_rx(Sc16isEchoPort& port, uint32_t now_ms)
{
    uint8_t processed = 0U;

    while ((sc16is_available(port.port.chip_select, port.port.channel) > 0U) &&
           (processed < SC16IS_MAX_RX_BYTES_PER_POLL))
    {
        const int value = sc16is_read(port.port.chip_select, port.port.channel);

        if (value >= 0)
        {
            if (port.length < SC16IS_ECHO_BUFFER_SIZE)
            {
                port.buffer[port.length] = static_cast<uint8_t>(value);
                ++port.length;
                port.last_rx_ms = now_ms;
                port.response_pending = 0U;
            }
            else
            {
                sc16is_echo_drop_frame(port, now_ms);
            }
        }

        ++processed;
    }
}

static void sc16is_echo_arm_response_if_frame_complete(Sc16isEchoPort& port,
                                                         uint32_t now_ms)
{
    if ((port.length == 0U) || (port.response_pending != 0U))
    {
        return;
    }

    if ((uint32_t)(now_ms - port.last_rx_ms) < SC16IS_INTERFRAME_GAP_MS)
    {
        return;
    }

    port.response_due_ms = now_ms + SC16IS_RESPONSE_DELAY_MS;
    port.response_pending = 1U;
}

static void sc16is_echo_send_if_response_due(Sc16isEchoPort& port,
                                               uint32_t now_ms)
{
    if (port.response_pending == 0U)
    {
        return;
    }

    if ((int32_t)(now_ms - port.response_due_ms) < 0)
    {
        return;
    }

    const size_t written = sc16is_write_buffer(port.port.chip_select,
                                                port.port.channel,
                                                port.buffer,
                                                port.length);

    if (written == port.length)
    {
        port.length = 0U;
        port.response_due_ms = 0U;
        port.response_pending = 0U;
    }
}

void sc16is_echo_test_init(void)
{
    lcp_sc16is_init_pins();
    lcp_sc16is_probe();
    lcp_sc16is_begin_detected_ports(SC16IS_ECHO_BAUDRATE);

    const LcpSc16isMap& map = lcp_sc16is_get_map();

    g_sc16is_echo_port_count = 0U;
    g_s1_echo_enabled = 1U;

    sc16is_echo_add_port(map.pc, "PC", 0U);
    sc16is_echo_add_port(map.hmi, "HMI", 0U);
    sc16is_echo_add_port(map.s1, "S1", 1U);
}

void sc16is_echo_test_poll(void)
{
    const uint32_t now_ms = millis();

    for (uint8_t index = 0U; index < g_sc16is_echo_port_count; ++index)
    {
        Sc16isEchoPort& port = g_sc16is_echo_ports[index];

        if ((port.is_s1 != 0U) && (g_s1_echo_enabled == 0U))
        {
            continue;
        }

        sc16is_echo_accept_rx(port, now_ms);
        sc16is_echo_arm_response_if_frame_complete(port, now_ms);
        sc16is_echo_send_if_response_due(port, now_ms);
    }
}

void sc16is_echo_test_set_s1_enabled(uint8_t enabled)
{
    const uint8_t normalized = (enabled != 0U) ? 1U : 0U;

    if (normalized == g_s1_echo_enabled)
    {
        return;
    }

    g_s1_echo_enabled = normalized;

    for (uint8_t index = 0U; index < g_sc16is_echo_port_count; ++index)
    {
        if (g_sc16is_echo_ports[index].is_s1 != 0U)
        {
            sc16is_echo_drop_frame(g_sc16is_echo_ports[index], millis());
        }
    }
}

uint8_t sc16is_echo_test_s1_enabled(void)
{
    return g_s1_echo_enabled;
}

void sc16is_echo_test_print_report_once(void)
{
    if (g_probe_report_printed != 0U)
    {
        return;
    }

    if (!SerialUSB)
    {
        return;
    }

    lcp_sc16is_print_probe_report();
    g_probe_report_printed = 1U;
}
