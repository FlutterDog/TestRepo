/**
 * @file sc16is_echo_test.cpp
 * @brief Неблокирующий echo-test внешних UART SC16IS7xx.
 *
 * Echo предназначен только для аппаратной диагностики PC/HMI и свободного S1.
 * После передачи S1 сервису FieldSensor echo этого канала отключается. Приём
 * ограничен SC16IS_MAX_RX_BYTES_PER_POLL, а частичная передача продолжается с
 * `tx_offset` на следующих poll-вызовах. Поэтому ни RX, ни TX не удерживают
 * общую LCP task в ожидании FIFO.
 */

#include "sc16is_echo_test.hpp"

#include "../../board/lcp_sc16is.hpp"
#include "../../hal/sc16is7xx.hpp"
#include "../../platform/platform.hpp"

namespace
{
constexpr uint32_t SC16IS_ECHO_BAUDRATE = 9600U;
constexpr uint32_t SC16IS_INTERFRAME_GAP_MS = 5U;
constexpr uint32_t SC16IS_RESPONSE_DELAY_MS = 10U;
constexpr uint8_t SC16IS_MAX_RX_BYTES_PER_POLL = 16U;
constexpr uint16_t SC16IS_ECHO_BUFFER_SIZE = 128U;
constexpr uint8_t SC16IS_ECHO_PORT_CAPACITY = 3U;

struct Sc16isEchoPort
{
    LcpSc16isPort port;                         /**< Физический chip-select и канал. */
    const char* name;                           /**< Диагностическое имя PC/HMI/S1. */
    uint8_t is_s1;                              /**< 1 для канала, передаваемого FieldSensor. */
    uint8_t buffer[SC16IS_ECHO_BUFFER_SIZE];    /**< Полный принятый кадр. */
    uint16_t length;                            /**< Количество байтов кадра. */
    uint16_t tx_offset;                         /**< Первый ещё не переданный байт. */
    uint32_t last_rx_ms;                        /**< Время последнего принятого байта. */
    uint32_t response_due_ms;                   /**< Момент разрешения ответа. */
    uint8_t response_pending;                   /**< Кадр завершён и ожидает/выполняет TX. */
};

Sc16isEchoPort g_sc16is_echo_ports[SC16IS_ECHO_PORT_CAPACITY];
uint8_t g_sc16is_echo_port_count = 0U;
uint8_t g_probe_report_printed = 0U;
uint8_t g_s1_echo_enabled = 1U;

void add_port(const LcpSc16isPort& port,
              const char* name,
              uint8_t is_s1)
{
    if ((port.present == 0U) ||
        (g_sc16is_echo_port_count >= SC16IS_ECHO_PORT_CAPACITY))
    {
        return;
    }

    Sc16isEchoPort& echo_port =
        g_sc16is_echo_ports[g_sc16is_echo_port_count];

    echo_port.port = port;
    echo_port.name = name;
    echo_port.is_s1 = (is_s1 != 0U) ? 1U : 0U;
    echo_port.length = 0U;
    echo_port.tx_offset = 0U;
    echo_port.last_rx_ms = 0U;
    echo_port.response_due_ms = 0U;
    echo_port.response_pending = 0U;

    ++g_sc16is_echo_port_count;
}

void drop_frame(Sc16isEchoPort& port, uint32_t now_ms)
{
    port.length = 0U;
    port.tx_offset = 0U;
    port.last_rx_ms = now_ms;
    port.response_due_ms = 0U;
    port.response_pending = 0U;
}

void accept_rx(Sc16isEchoPort& port, uint32_t now_ms)
{
    /* После начала ответа half-duplex кадр должен быть передан целиком. */
    if (port.tx_offset != 0U)
    {
        return;
    }

    uint8_t processed = 0U;

    while ((sc16is_available(port.port.chip_select,
                             port.port.channel) > 0U) &&
           (processed < SC16IS_MAX_RX_BYTES_PER_POLL))
    {
        const int value = sc16is_read(port.port.chip_select,
                                      port.port.channel);

        if (value < 0)
        {
            break;
        }

        if (port.length >= SC16IS_ECHO_BUFFER_SIZE)
        {
            drop_frame(port, now_ms);
            return;
        }

        port.buffer[port.length++] = static_cast<uint8_t>(value);
        port.last_rx_ms = now_ms;
        port.response_pending = 0U;
        port.response_due_ms = 0U;
        ++processed;
    }
}

void arm_response_if_frame_complete(Sc16isEchoPort& port,
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
    port.tx_offset = 0U;
}

void send_if_response_due(Sc16isEchoPort& port, uint32_t now_ms)
{
    if ((port.response_pending == 0U) ||
        (static_cast<int32_t>(now_ms - port.response_due_ms) < 0))
    {
        return;
    }

    if (port.tx_offset >= port.length)
    {
        drop_frame(port, now_ms);
        return;
    }

    const size_t remaining =
        static_cast<size_t>(port.length - port.tx_offset);
    const size_t written = sc16is_write_buffer(
        port.port.chip_select,
        port.port.channel,
        &port.buffer[port.tx_offset],
        remaining);

    port.tx_offset = static_cast<uint16_t>(port.tx_offset + written);

    if (port.tx_offset >= port.length)
    {
        drop_frame(port, now_ms);
    }
}
}

void sc16is_echo_test_init(void)
{
    lcp_sc16is_init_pins();
    lcp_sc16is_probe();
    lcp_sc16is_begin_detected_ports(SC16IS_ECHO_BAUDRATE);

    const LcpSc16isMap& map = lcp_sc16is_get_map();

    g_sc16is_echo_port_count = 0U;
    g_probe_report_printed = 0U;
    g_s1_echo_enabled = 1U;

    add_port(map.pc, "PC", 0U);
    add_port(map.hmi, "HMI", 0U);
    add_port(map.s1, "S1", 1U);
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

        accept_rx(port, now_ms);
        arm_response_if_frame_complete(port, now_ms);
        send_if_response_due(port, now_ms);
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
            drop_frame(g_sc16is_echo_ports[index], millis());
        }
    }
}

uint8_t sc16is_echo_test_s1_enabled(void)
{
    return g_s1_echo_enabled;
}

void sc16is_echo_test_print_report_once(void)
{
    if ((g_probe_report_printed != 0U) || (!SerialUSB))
    {
        return;
    }

    lcp_sc16is_print_probe_report();
    g_probe_report_printed = 1U;
}
