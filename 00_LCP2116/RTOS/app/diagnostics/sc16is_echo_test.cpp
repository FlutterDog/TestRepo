/**
 * @file sc16is_echo_test.cpp
 * @brief Неблокирующий echo-test внешних UART PC и HMI.
 *
 * S1 сразу принадлежит FieldSensor и здесь не инициализируется. Приём каждого
 * PC/HMI ограничен 16 байтами за poll, а частичная передача продолжается с
 * `tx_offset` на следующих вызовах без ожидания FIFO.
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
constexpr uint8_t SC16IS_ECHO_PORT_CAPACITY = 2U;

struct Sc16isEchoPort
{
    LcpSc16isPort port;                         /**< Физический chip-select и канал. */
    const char* name;                           /**< Диагностическое имя PC/HMI. */
    uint8_t buffer[SC16IS_ECHO_BUFFER_SIZE];    /**< Полный принятый кадр. */
    uint16_t length;                            /**< Количество байтов кадра. */
    uint16_t tx_offset;                         /**< Первый ещё не переданный байт. */
    uint32_t last_rx_ms;                        /**< Время последнего принятого байта. */
    uint32_t response_due_ms;                   /**< Момент разрешения ответа. */
    uint8_t response_pending;                   /**< Кадр завершён и ожидает/выполняет TX. */
};

Sc16isEchoPort g_ports[SC16IS_ECHO_PORT_CAPACITY];
uint8_t g_port_count = 0U;
uint8_t g_probe_report_printed = 0U;

void add_port(const LcpSc16isPort& port, const char* name)
{
    if ((port.present == 0U) || (g_port_count >= SC16IS_ECHO_PORT_CAPACITY))
    {
        return;
    }

    Sc16isEchoPort& echo_port = g_ports[g_port_count];
    echo_port.port = port;
    echo_port.name = name;
    echo_port.length = 0U;
    echo_port.tx_offset = 0U;
    echo_port.last_rx_ms = 0U;
    echo_port.response_due_ms = 0U;
    echo_port.response_pending = 0U;
    ++g_port_count;
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

void begin_port(const LcpSc16isPort& port)
{
    if (port.present == 0U)
    {
        return;
    }

    sc16is_begin(port.chip_select,
                 port.channel,
                 SC16IS_ECHO_BAUDRATE,
                 HAL_UART_FRAME_8N1);
}
}

void sc16is_echo_test_init(void)
{
    lcp_sc16is_probe();
    const LcpSc16isMap& map = lcp_sc16is_get_map();

    g_port_count = 0U;
    g_probe_report_printed = 0U;

    begin_port(map.pc);
    begin_port(map.hmi);
    add_port(map.pc, "PC");
    add_port(map.hmi, "HMI");
}

void sc16is_echo_test_poll(void)
{
    const uint32_t now_ms = millis();

    for (uint8_t index = 0U; index < g_port_count; ++index)
    {
        accept_rx(g_ports[index], now_ms);
        arm_response_if_frame_complete(g_ports[index], now_ms);
        send_if_response_due(g_ports[index], now_ms);
    }
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
