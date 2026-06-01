/**
 * @file spi.hpp
 * @brief Платформенный интерфейс SPI.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../hal/sam3x_spi.hpp"

static const uint8_t MSBFIRST = 0U;
static const uint8_t LSBFIRST = 1U;

static const uint8_t SPI_MODE0 = 0U;
static const uint8_t SPI_MODE1 = 1U;
static const uint8_t SPI_MODE2 = 2U;
static const uint8_t SPI_MODE3 = 3U;

static const uint8_t SPI_CLOCK_DIV2   = 2U;
static const uint8_t SPI_CLOCK_DIV4   = 4U;
static const uint8_t SPI_CLOCK_DIV8   = 8U;
static const uint8_t SPI_CLOCK_DIV16  = 16U;
static const uint8_t SPI_CLOCK_DIV32  = 32U;
static const uint8_t SPI_CLOCK_DIV64  = 64U;
static const uint8_t SPI_CLOCK_DIV128 = 128U;

/**
 * @brief Настройки SPI-транзакции.
 */
class SPISettings
{
public:
    SPISettings();
    SPISettings(uint32_t clock_hz, uint8_t bit_order, uint8_t data_mode);

    uint32_t clock_hz(void) const;
    uint8_t bit_order(void) const;
    uint8_t data_mode(void) const;

private:
    uint32_t clock_hz_;
    uint8_t bit_order_;
    uint8_t data_mode_;
};

/**
 * @brief Платформенный SPI master.
 *
 * Класс управляет SPI0 микроконтроллера. Линии Chip Select управляются
 * прикладным кодом через GPIO.
 */
class SPIClass
{
public:
    SPIClass();

    void begin(void);
    void end(void);

    void beginTransaction(const SPISettings& settings);
    void endTransaction(void);

    uint8_t transfer(uint8_t value);
    uint16_t transfer16(uint16_t value);
    void transfer(void* buffer, size_t size);

    void setClockDivider(uint8_t divider);
    void setBitOrder(uint8_t bit_order);
    void setDataMode(uint8_t data_mode);

private:
    HalSpiConfig make_hal_config(const SPISettings& settings) const;
    HalSpiBitOrder convert_bit_order(uint8_t bit_order) const;
    HalSpiMode convert_data_mode(uint8_t data_mode) const;
    void apply_current_settings(void);

    SPISettings settings_;
    uint8_t initialized_;
};

extern SPIClass SPI;
