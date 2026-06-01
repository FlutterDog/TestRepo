/**
 * @file sam3x_uart.hpp
 * @brief HAL последовательных портов ATSAM3X8E.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Идентификатор аппаратного последовательного порта.
 */
enum HalUartId
{
    HAL_UART_PORT_0 = 0,
    HAL_UART_PORT_1 = 1,
    HAL_UART_PORT_2 = 2,
    HAL_UART_PORT_3 = 3
};

/**
 * @brief Формат кадра последовательного порта.
 */
enum HalUartFrame
{
    HAL_UART_FRAME_8N1 = 0,
    HAL_UART_FRAME_8O1 = 1,
    HAL_UART_FRAME_8E1 = 2
};

/**
 * @brief Инициализирует аппаратный последовательный порт.
 *
 * @param port_id Идентификатор порта.
 * @param baudrate Скорость обмена в бод.
 * @param frame Формат кадра.
 */
void hal_uart_begin(HalUartId port_id, uint32_t baudrate, HalUartFrame frame);

/**
 * @brief Отключает приёмник и передатчик аппаратного последовательного порта.
 *
 * @param port_id Идентификатор порта.
 */
void hal_uart_end(HalUartId port_id);

/**
 * @brief Возвращает количество байтов, доступных для чтения без ожидания.
 *
 * @param port_id Идентификатор порта.
 * @return Единица, если байт доступен; ноль, если данных нет.
 */
uint32_t hal_uart_available(HalUartId port_id);

/**
 * @brief Считывает один байт из аппаратного последовательного порта.
 *
 * @param port_id Идентификатор порта.
 * @return Значение байта в диапазоне 0..255 или -1, если данных нет.
 */
int hal_uart_read(HalUartId port_id);

/**
 * @brief Передаёт один байт через аппаратный последовательный порт.
 *
 * @param port_id Идентификатор порта.
 * @param value Передаваемый байт.
 */
void hal_uart_write(HalUartId port_id, uint8_t value);

/**
 * @brief Ожидает завершения передачи данных аппаратным портом.
 *
 * @param port_id Идентификатор порта.
 */
void hal_uart_flush(HalUartId port_id);
