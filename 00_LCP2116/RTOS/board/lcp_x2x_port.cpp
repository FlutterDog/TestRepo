/**
 * @file lcp_x2x_port.cpp
 * @brief Реализация аппаратного транспорта X2X через UART0 ATSAM3X8E.
 */

#include "lcp_x2x_port.hpp"
#include "lcp_rs485.hpp"
#include "../hal/sam3x_uart.hpp"

namespace
{
size_t x2x_write(const uint8_t* data, size_t length)
{
    if (data == 0)
    {
        return 0U;
    }

    size_t written = 0U;

    while (written < length)
    {
        if (hal_uart_write(HAL_UART_PORT_0, data[written]) == 0U)
        {
            break;
        }

        ++written;
    }

    return written;
}

size_t x2x_available(void)
{
    return hal_uart_available(HAL_UART_PORT_0);
}

int x2x_read(void)
{
    return hal_uart_read(HAL_UART_PORT_0);
}

uint8_t x2x_tx_idle(void)
{
    return hal_uart_tx_idle(HAL_UART_PORT_0);
}

void x2x_clear_rx(void)
{
    hal_uart_clear_rx(HAL_UART_PORT_0);
}

const ModbusRtuTransport g_x2x_transport =
{
    x2x_write,
    x2x_available,
    x2x_read,
    x2x_tx_idle,
    x2x_clear_rx
};
}

void lcp_x2x_port_init(void)
{
    hal_uart_set_rs485_direction_pin(HAL_UART_PORT_0,
                                     LCP_RS485_PIN_X2X_TRANSMIT,
                                     LCP_RS485_TRANSMIT_LEVEL);
    hal_uart_begin(HAL_UART_PORT_0,
                   LCP_X2X_BAUDRATE,
                   HAL_UART_FRAME_8N1);
    hal_uart_clear_rx(HAL_UART_PORT_0);
}

const ModbusRtuTransport& lcp_x2x_port_transport(void)
{
    return g_x2x_transport;
}

uint32_t lcp_x2x_port_error_count(void)
{
    return hal_uart_error_count(HAL_UART_PORT_0);
}
