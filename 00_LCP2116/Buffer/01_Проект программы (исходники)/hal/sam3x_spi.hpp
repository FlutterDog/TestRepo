/**
 * @file sam3x_spi.hpp
 * @brief HAL SPI для ATSAM3X8E.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Порядок передачи битов в SPI-слове.
 */
enum HalSpiBitOrder
{
    HAL_SPI_MSB_FIRST = 0,
    HAL_SPI_LSB_FIRST = 1
};

/**
 * @brief Режим SPI по полярности и фазе тактового сигнала.
 */
enum HalSpiMode
{
    HAL_SPI_MODE_0 = 0,
    HAL_SPI_MODE_1 = 1,
    HAL_SPI_MODE_2 = 2,
    HAL_SPI_MODE_3 = 3
};

/**
 * @brief Конфигурация SPI-шины.
 */
struct HalSpiConfig
{
    uint32_t clock_hz;
    HalSpiBitOrder bit_order;
    HalSpiMode mode;
};

/**
 * @brief Инициализирует SPI0 в режиме master.
 *
 * Функция настраивает линии MISO, MOSI и SCK на периферийную функцию SPI0.
 * Линии Chip Select управляются прикладным кодом через GPIO.
 */
void hal_spi_begin(void);

/**
 * @brief Отключает SPI0.
 */
void hal_spi_end(void);

/**
 * @brief Применяет конфигурацию SPI0.
 *
 * @param config Конфигурация частоты, порядка битов и режима SPI.
 */
void hal_spi_configure(const HalSpiConfig& config);

/**
 * @brief Передаёт и принимает один байт по SPI0.
 *
 * @param value Передаваемый байт.
 * @return Принятый байт.
 */
uint8_t hal_spi_transfer(uint8_t value);
