
/**
 * @file platform_serial.cpp
 * @brief Глобальные аппаратные последовательные порты проекта.
 */

#include "platform.hpp"

SerialPort Serial(HAL_UART_PORT_0);
SerialPort Serial1(HAL_UART_PORT_1);
SerialPort Serial2(HAL_UART_PORT_2);
SerialPort Serial3(HAL_UART_PORT_3);
