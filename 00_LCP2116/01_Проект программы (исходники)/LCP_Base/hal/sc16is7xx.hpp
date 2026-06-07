/**
 * @file sc16is7xx.hpp
 * @brief HAL внешних UART SC16IS7xx по SPI.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

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

/**
 * @brief Инициализирует общий доступ к SC16IS7xx.
 */
void sc16is_init_bus(void);

/**
 * @brief Проверяет наличие канала SC16IS7xx через scratchpad register.
 *
 * @param chip_select Номер линии CS.
 * @param channel Канал микросхемы.
 *
 * @return SC16IS_PROBE_PRESENT при успешной проверке.
 */
Sc16isProbeResult sc16is_probe_channel(uint32_t chip_select, Sc16isChannel channel);

/**
 * @brief Проверяет независимость каналов A/B одной микросхемы.
 *
 * @param chip_select Номер линии CS.
 *
 * @return 1 если каналы A/B работают независимо, иначе 0.
 */
uint8_t sc16is_probe_dual_channel(uint32_t chip_select);

/**
 * @brief Инициализирует UART-канал SC16IS7xx.
 *
 * @param chip_select Номер линии CS.
 * @param channel Канал микросхемы.
 * @param baudrate Скорость обмена.
 */
void sc16is_begin(uint32_t chip_select, Sc16isChannel channel, uint32_t baudrate);

/**
 * @brief Возвращает количество байтов в RX FIFO.
 *
 * @param chip_select Номер линии CS.
 * @param channel Канал микросхемы.
 *
 * @return Количество байтов в FIFO.
 */
uint8_t sc16is_available(uint32_t chip_select, Sc16isChannel channel);

/**
 * @brief Читает один байт из RX FIFO.
 *
 * @param chip_select Номер линии CS.
 * @param channel Канал микросхемы.
 *
 * @return Байт 0..255 или -1 при пустом FIFO.
 */
int sc16is_read(uint32_t chip_select, Sc16isChannel channel);

/**
 * @brief Возвращает свободное место в TX FIFO.
 *
 * @param chip_select Номер линии CS.
 * @param channel Канал микросхемы.
 *
 * @return Количество свободных байтов в TX FIFO.
 */
uint8_t sc16is_tx_available(uint32_t chip_select, Sc16isChannel channel);

/**
 * @brief Записывает один байт в TX FIFO.
 *
 * @param chip_select Номер линии CS.
 * @param channel Канал микросхемы.
 * @param value Передаваемый байт.
 *
 * @return 1 при записи байта, 0 если TX FIFO заполнен.
 */
size_t sc16is_write(uint32_t chip_select, Sc16isChannel channel, uint8_t value);

/**
 * @brief Записывает буфер в TX FIFO.
 *
 * @param chip_select Номер линии CS.
 * @param channel Канал микросхемы.
 * @param buffer Передаваемый буфер.
 * @param length Количество байтов.
 *
 * @return Количество записанных байтов.
 */
size_t sc16is_write_buffer(uint32_t chip_select, Sc16isChannel channel, const uint8_t* buffer, size_t length);

/**
 * @brief Возвращает регистр LSR.
 *
 * @param chip_select Номер линии CS.
 * @param channel Канал микросхемы.
 *
 * @return Значение LSR.
 */
uint8_t sc16is_line_status(uint32_t chip_select, Sc16isChannel channel);
