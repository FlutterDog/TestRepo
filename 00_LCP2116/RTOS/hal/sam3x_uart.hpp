/**
 * @file sam3x_uart.hpp
 * @brief HAL встроенных UART/USART ATSAM3X8E.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Идентификатор встроенного последовательного порта.
 */
enum HalUartId
{
    HAL_UART_PORT_0 = 0,
    HAL_UART_PORT_1 = 1,
    HAL_UART_PORT_2 = 2,
    HAL_UART_PORT_3 = 3,
    HAL_UART_PORT_COUNT = 4
};

/**
 * @brief Формат кадра UART/USART.
 */
enum HalUartFrame
{
    HAL_UART_FRAME_8N1 = 0,
    HAL_UART_FRAME_8O1 = 1,
    HAL_UART_FRAME_8E1 = 2
};

/**
 * @brief Настраивает порт на работу с RS-485 direction pin.
 *
 * @param port_id Идентификатор порта.
 * @param direction_pin Номер дискретной линии управления DE/RE.
 * @param transmit_level Уровень, включающий передачу.
 */
void hal_uart_set_rs485_direction_pin(HalUartId port_id,
                                      uint32_t direction_pin,
                                      uint8_t transmit_level);

/**
 * @brief Инициализирует встроенный UART/USART.
 *
 * @param port_id Идентификатор порта.
 * @param baudrate Скорость обмена.
 * @param frame Формат кадра.
 */
void hal_uart_begin(HalUartId port_id, uint32_t baudrate, HalUartFrame frame);

/**
 * @brief Останавливает встроенный UART/USART.
 *
 * @param port_id Идентификатор порта.
 */
void hal_uart_end(HalUartId port_id);

/**
 * @brief Возвращает количество принятых байтов в RX-буфере.
 *
 * @param port_id Идентификатор порта.
 *
 * @return Количество байтов.
 */
size_t hal_uart_available(HalUartId port_id);

/**
 * @brief Читает один байт из RX-буфера.
 *
 * @param port_id Идентификатор порта.
 *
 * @return Байт 0..255 или -1 при пустом буфере.
 */
int hal_uart_read(HalUartId port_id);

/**
 * @brief Возвращает следующий байт RX-буфера без удаления.
 *
 * @param port_id Идентификатор порта.
 *
 * @return Байт 0..255 или -1 при пустом буфере.
 */
int hal_uart_peek(HalUartId port_id);

/**
 * @brief Передаёт один байт.
 *
 * @param port_id Идентификатор порта.
 * @param value Передаваемый байт.
 *
 * @return 1 при постановке байта в передачу, 0 при полном TX-буфере.
 */
size_t hal_uart_write(HalUartId port_id, uint8_t value);

/**
 * @brief Ожидает завершение передачи.
 *
 * @param port_id Идентификатор порта.
 */
void hal_uart_flush(HalUartId port_id);

/**
 * @brief Проверяет, полностью ли завершена передача.
 *
 * Функция не блокирует задачу и учитывает программный TX-буфер и аппаратный
 * флаг TXEMPTY.
 *
 * @return 1, если передача завершена или порт неактивен, иначе 0.
 */
uint8_t hal_uart_tx_idle(HalUartId port_id);

/**
 * @brief Очищает программный и аппаратный RX-буфер порта.
 *
 * Используется перед началом новой master-транзакции для удаления остатка
 * предыдущего или повреждённого кадра.
 */
void hal_uart_clear_rx(HalUartId port_id);

/**
 * @brief Возвращает количество ошибок приёма.
 *
 * @param port_id Идентификатор порта.
 *
 * @return Количество ошибок OVRE/FRAME/PARE.
 */
uint32_t hal_uart_error_count(HalUartId port_id);
