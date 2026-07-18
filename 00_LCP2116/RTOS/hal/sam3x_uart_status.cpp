/**
 * @file sam3x_uart_status.cpp
 * @brief Дополнительные неблокирующие операции состояния UART/USART.
 */

#include "sam3x_uart.hpp"
#include "sam3x_device.hpp"

uint8_t hal_uart_tx_idle(HalUartId port_id)
{
    switch (port_id)
    {
        case HAL_UART_PORT_0:
            return ((UART->UART_SR & UART_SR_TXEMPTY) != 0U) ? 1U : 0U;

        case HAL_UART_PORT_1:
            return ((USART0->US_CSR & US_CSR_TXEMPTY) != 0U) ? 1U : 0U;

        case HAL_UART_PORT_2:
            return ((USART1->US_CSR & US_CSR_TXEMPTY) != 0U) ? 1U : 0U;

        case HAL_UART_PORT_3:
            return ((USART3->US_CSR & US_CSR_TXEMPTY) != 0U) ? 1U : 0U;

        default:
            return 1U;
    }
}

void hal_uart_clear_rx(HalUartId port_id)
{
    while (hal_uart_available(port_id) > 0U)
    {
        (void)hal_uart_read(port_id);
    }
}
