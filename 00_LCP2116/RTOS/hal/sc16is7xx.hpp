/**
 * @file sc16is7xx.hpp
 * @brief HAL внешних UART SC16IS7xx по SPI.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "sam3x_uart.hpp"

/**
 * @brief Канал SC16IS7xx.
 */
enum Sc16isChannel
{
    SC16IS_CHANNEL_A = 0,
    SC16IS_CHANNEL_B = 1
};

/**
 * @brief Результат проверки канала SC16IS7xx.
 */
enum Sc16isProbeResult
{
    SC16IS_PROBE_ABSENT = 0,
    SC16IS_PROBE_PRESENT = 1
};

/** @brief Инициализирует общий доступ к SC16IS7xx. */
void sc16is_init_bus(void);

/** @brief Проверяет наличие канала через scratchpad register. */
Sc16isProbeResult sc16is_probe_channel(uint32_t chip_select,
                                       Sc16isChannel channel);

/** @brief Проверяет независимость каналов A/B одной микросхемы. */
uint8_t sc16is_probe_dual_channel(uint32_t chip_select);

/**
 * @brief Инициализирует UART-канал SC16IS7xx.
 *
 * @param chip_select Номер линии CS.
 * @param channel Канал микросхемы.
 * @param baudrate Скорость обмена.
 * @param frame Формат кадра 8N1, 8O1 или 8E1.
 */
void sc16is_begin(uint32_t chip_select,
                  Sc16isChannel channel,
                  uint32_t baudrate,
                  HalUartFrame frame);

/** @brief Возвращает количество байтов в RX FIFO. */
uint8_t sc16is_available(uint32_t chip_select, Sc16isChannel channel);

/** @brief Читает один байт из RX FIFO либо возвращает -1. */
int sc16is_read(uint32_t chip_select, Sc16isChannel channel);

/** @brief Возвращает свободное место в TX FIFO. */
uint8_t sc16is_tx_available(uint32_t chip_select, Sc16isChannel channel);

/** @brief Записывает один байт в TX FIFO. */
size_t sc16is_write(uint32_t chip_select,
                    Sc16isChannel channel,
                    uint8_t value);

/** @brief Записывает буфер в TX FIFO. */
size_t sc16is_write_buffer(uint32_t chip_select,
                           Sc16isChannel channel,
                           const uint8_t* buffer,
                           size_t length);

/** @brief Возвращает регистр LSR. */
uint8_t sc16is_line_status(uint32_t chip_select, Sc16isChannel channel);
