
/**
 * @file lcp_rs485.cpp
 * @brief Инициализация встроенных RS-485 линий контроллера LCP.
 */

#include "lcp_rs485.hpp"
#include "../platform/platform.hpp"
#include "../hal/sam3x_uart.hpp"

void lcp_rs485_init_builtin_ports(void)
{
    pinMode(LCP_RS485_PIN_X2X_TRANSMIT, OUTPUT);
    pinMode(LCP_RS485_PIN_S2_TRANSMIT, OUTPUT);
    pinMode(LCP_RS485_PIN_S4_TRANSMIT, OUTPUT);
    pinMode(LCP_RS485_PIN_S3_TRANSMIT, OUTPUT);

    digitalWrite(LCP_RS485_PIN_X2X_TRANSMIT, LCP_RS485_RECEIVE_LEVEL);
    digitalWrite(LCP_RS485_PIN_S2_TRANSMIT, LCP_RS485_RECEIVE_LEVEL);
    digitalWrite(LCP_RS485_PIN_S4_TRANSMIT, LCP_RS485_RECEIVE_LEVEL);
    digitalWrite(LCP_RS485_PIN_S3_TRANSMIT, LCP_RS485_RECEIVE_LEVEL);

    hal_uart_set_rs485_direction_pin(HAL_UART_PORT_0, LCP_RS485_PIN_X2X_TRANSMIT, LCP_RS485_TRANSMIT_LEVEL);
    hal_uart_set_rs485_direction_pin(HAL_UART_PORT_1, LCP_RS485_PIN_S2_TRANSMIT, LCP_RS485_TRANSMIT_LEVEL);
    hal_uart_set_rs485_direction_pin(HAL_UART_PORT_2, LCP_RS485_PIN_S4_TRANSMIT, LCP_RS485_TRANSMIT_LEVEL);
    hal_uart_set_rs485_direction_pin(HAL_UART_PORT_3, LCP_RS485_PIN_S3_TRANSMIT, LCP_RS485_TRANSMIT_LEVEL);
}
