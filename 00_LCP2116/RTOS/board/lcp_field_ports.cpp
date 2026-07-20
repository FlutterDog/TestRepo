/**
 * @file lcp_field_ports.cpp
 * @brief Реализация транспортов пользовательских RS-485 портов S1..S4.
 */

#include "lcp_field_ports.hpp"

#include "lcp_sc16is.hpp"
#include "../hal/sc16is7xx.hpp"
#include "../platform/platform.hpp"

#include <stddef.h>
#include <stdint.h>

namespace
{
static const uint32_t DEFAULT_BAUDRATE = 9600U;
static const uint32_t BITS_PER_CHARACTER_WITH_MARGIN = 11U;
static const uint8_t SC16IS_LSR_TRANSMITTER_EMPTY = 0x40U;

LcpFieldPortConfig g_configs[LCP_FIELD_PORT_COUNT] =
{
    { DEFAULT_BAUDRATE, HAL_UART_FRAME_8N1 },
    { DEFAULT_BAUDRATE, HAL_UART_FRAME_8N1 },
    { DEFAULT_BAUDRATE, HAL_UART_FRAME_8N1 },
    { DEFAULT_BAUDRATE, HAL_UART_FRAME_8N1 }
};

uint32_t g_tx_not_before_ms[LCP_FIELD_PORT_COUNT] = { 0U, 0U, 0U, 0U };
LcpSc16isPort g_s1_port = { 0U, SC16IS_CHANNEL_A, 0U };

uint32_t frame_duration_ms(LcpFieldPortId port_id, size_t byte_count)
{
    const uint32_t baudrate = g_configs[port_id].baudrate;

    if ((baudrate == 0U) || (byte_count == 0U))
    {
        return 0U;
    }

    return static_cast<uint32_t>(
        ((byte_count * BITS_PER_CHARACTER_WITH_MARGIN * 1000U) +
         (baudrate - 1U)) /
        baudrate);
}

void remember_tx_duration(LcpFieldPortId port_id, size_t byte_count)
{
    g_tx_not_before_ms[port_id] =
        millis() + frame_duration_ms(port_id, byte_count) + 1U;
}

uint8_t transmission_time_elapsed(LcpFieldPortId port_id)
{
    return (static_cast<int32_t>(millis() - g_tx_not_before_ms[port_id]) >= 0) ?
        1U : 0U;
}

size_t embedded_write(HalUartId uart_id,
                      LcpFieldPortId port_id,
                      const uint8_t* data,
                      size_t length)
{
    if (data == 0)
    {
        return 0U;
    }

    size_t written = 0U;

    while (written < length)
    {
        if (hal_uart_write(uart_id, data[written]) == 0U)
        {
            break;
        }

        ++written;
    }

    remember_tx_duration(port_id, written);
    return written;
}

size_t s1_write(const uint8_t* data, size_t length)
{
    if ((g_s1_port.present == 0U) || (data == 0))
    {
        return 0U;
    }

    const size_t written = sc16is_write_buffer(g_s1_port.chip_select,
                                                g_s1_port.channel,
                                                data,
                                                length);
    remember_tx_duration(LCP_FIELD_PORT_S1, written);
    return written;
}

size_t s1_available(void)
{
    return (g_s1_port.present != 0U) ?
        sc16is_available(g_s1_port.chip_select, g_s1_port.channel) : 0U;
}

int s1_read(void)
{
    return (g_s1_port.present != 0U) ?
        sc16is_read(g_s1_port.chip_select, g_s1_port.channel) : -1;
}

uint8_t s1_tx_idle(void)
{
    if (g_s1_port.present == 0U)
    {
        return 1U;
    }

    if (transmission_time_elapsed(LCP_FIELD_PORT_S1) == 0U)
    {
        return 0U;
    }

    return ((sc16is_line_status(g_s1_port.chip_select, g_s1_port.channel) &
             SC16IS_LSR_TRANSMITTER_EMPTY) != 0U) ? 1U : 0U;
}

void s1_clear_rx(void)
{
    while (s1_available() > 0U)
    {
        (void)s1_read();
    }
}

size_t s2_write(const uint8_t* data, size_t length)
{
    return embedded_write(HAL_UART_PORT_1, LCP_FIELD_PORT_S2, data, length);
}

size_t s2_available(void)
{
    return hal_uart_available(HAL_UART_PORT_1);
}

int s2_read(void)
{
    return hal_uart_read(HAL_UART_PORT_1);
}

uint8_t s2_tx_idle(void)
{
    return ((transmission_time_elapsed(LCP_FIELD_PORT_S2) != 0U) &&
            (hal_uart_tx_idle(HAL_UART_PORT_1) != 0U)) ? 1U : 0U;
}

void s2_clear_rx(void)
{
    hal_uart_clear_rx(HAL_UART_PORT_1);
}

size_t s3_write(const uint8_t* data, size_t length)
{
    return embedded_write(HAL_UART_PORT_3, LCP_FIELD_PORT_S3, data, length);
}

size_t s3_available(void)
{
    return hal_uart_available(HAL_UART_PORT_3);
}

int s3_read(void)
{
    return hal_uart_read(HAL_UART_PORT_3);
}

uint8_t s3_tx_idle(void)
{
    return ((transmission_time_elapsed(LCP_FIELD_PORT_S3) != 0U) &&
            (hal_uart_tx_idle(HAL_UART_PORT_3) != 0U)) ? 1U : 0U;
}

void s3_clear_rx(void)
{
    hal_uart_clear_rx(HAL_UART_PORT_3);
}

size_t s4_write(const uint8_t* data, size_t length)
{
    return embedded_write(HAL_UART_PORT_2, LCP_FIELD_PORT_S4, data, length);
}

size_t s4_available(void)
{
    return hal_uart_available(HAL_UART_PORT_2);
}

int s4_read(void)
{
    return hal_uart_read(HAL_UART_PORT_2);
}

uint8_t s4_tx_idle(void)
{
    return ((transmission_time_elapsed(LCP_FIELD_PORT_S4) != 0U) &&
            (hal_uart_tx_idle(HAL_UART_PORT_2) != 0U)) ? 1U : 0U;
}

void s4_clear_rx(void)
{
    hal_uart_clear_rx(HAL_UART_PORT_2);
}

const ModbusRtuTransport g_transports[LCP_FIELD_PORT_COUNT] =
{
    { s1_write, s1_available, s1_read, s1_tx_idle, s1_clear_rx },
    { s2_write, s2_available, s2_read, s2_tx_idle, s2_clear_rx },
    { s3_write, s3_available, s3_read, s3_tx_idle, s3_clear_rx },
    { s4_write, s4_available, s4_read, s4_tx_idle, s4_clear_rx }
};

const char* const g_names[LCP_FIELD_PORT_COUNT] = { "S1", "S2", "S3", "S4" };

LcpFieldPortId normalize_port_id(LcpFieldPortId port_id)
{
    return (port_id < LCP_FIELD_PORT_COUNT) ? port_id : LCP_FIELD_PORT_S1;
}
}

void lcp_field_ports_init(const LcpFieldPortConfig configs[LCP_FIELD_PORT_COUNT])
{
    if (configs != 0)
    {
        for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
        {
            g_configs[index] = configs[index];

            if (g_configs[index].baudrate == 0U)
            {
                g_configs[index].baudrate = DEFAULT_BAUDRATE;
            }
        }
    }

    /*
     * S1 не должен зависеть от предварительного запуска диагностического
     * echo-test. Board-слой самостоятельно обнаруживает физическую схему
     * SC16IS740/752 и затем выбирает фактический канал S1.
     */
    lcp_sc16is_init_pins();
    lcp_sc16is_probe();
    g_s1_port = lcp_sc16is_get_map().s1;

    if (g_s1_port.present != 0U)
    {
        sc16is_begin(g_s1_port.chip_select,
                     g_s1_port.channel,
                     g_configs[LCP_FIELD_PORT_S1].baudrate);
        s1_clear_rx();
    }

    hal_uart_begin(HAL_UART_PORT_1,
                   g_configs[LCP_FIELD_PORT_S2].baudrate,
                   g_configs[LCP_FIELD_PORT_S2].frame);
    hal_uart_begin(HAL_UART_PORT_3,
                   g_configs[LCP_FIELD_PORT_S3].baudrate,
                   g_configs[LCP_FIELD_PORT_S3].frame);
    hal_uart_begin(HAL_UART_PORT_2,
                   g_configs[LCP_FIELD_PORT_S4].baudrate,
                   g_configs[LCP_FIELD_PORT_S4].frame);

    s2_clear_rx();
    s3_clear_rx();
    s4_clear_rx();

    const uint32_t now_ms = millis();

    for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
    {
        g_tx_not_before_ms[index] = now_ms;
    }
}

const ModbusRtuTransport& lcp_field_port_transport(LcpFieldPortId port_id)
{
    return g_transports[normalize_port_id(port_id)];
}

uint8_t lcp_field_port_present(LcpFieldPortId port_id)
{
    if (port_id >= LCP_FIELD_PORT_COUNT)
    {
        return 0U;
    }

    return (port_id == LCP_FIELD_PORT_S1) ? g_s1_port.present : 1U;
}

const char* lcp_field_port_name(LcpFieldPortId port_id)
{
    return g_names[normalize_port_id(port_id)];
}

const LcpFieldPortConfig& lcp_field_port_config(LcpFieldPortId port_id)
{
    return g_configs[normalize_port_id(port_id)];
}

uint32_t lcp_field_port_error_count(LcpFieldPortId port_id)
{
    switch (port_id)
    {
        case LCP_FIELD_PORT_S2:
            return hal_uart_error_count(HAL_UART_PORT_1);

        case LCP_FIELD_PORT_S3:
            return hal_uart_error_count(HAL_UART_PORT_3);

        case LCP_FIELD_PORT_S4:
            return hal_uart_error_count(HAL_UART_PORT_2);

        case LCP_FIELD_PORT_S1:
        default:
            /* SC16IS7xx HAL пока не ведёт накопительный счётчик LSR-ошибок. */
            return 0U;
    }
}
