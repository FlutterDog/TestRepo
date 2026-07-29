
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
    /**
     * @brief Создаёт настройки SPI по умолчанию.
     */
    SPISettings();

    /**
     * @brief Создаёт настройки SPI.
     *
     * @param clock_hz Частота SPI в Hz.
     * @param bit_order Порядок битов: `MSBFIRST` или `LSBFIRST`.
     * @param data_mode Режим SPI: `SPI_MODE0`..`SPI_MODE3`.
     */
    SPISettings(uint32_t clock_hz, uint8_t bit_order, uint8_t data_mode);

    /**
     * @brief Возвращает частоту SPI.
     */
    uint32_t clock_hz(void) const;

    /**
     * @brief Возвращает порядок битов.
     */
    uint8_t bit_order(void) const;

    /**
     * @brief Возвращает режим SPI.
     */
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
    /**
     * @brief Создаёт объект SPI master.
     */
    SPIClass();

    /**
     * @brief Инициализирует SPI master.
     */
    void begin(void);

    /**
     * @brief Останавливает SPI master.
     */
    void end(void);

    /**
     * @brief Начинает SPI-транзакцию с указанными параметрами.
     *
     * @param settings Настройки транзакции.
     */
    void beginTransaction(const SPISettings& settings);

    /**
     * @brief Завершает SPI-транзакцию.
     */
    void endTransaction(void);

    /**
     * @brief Передаёт и принимает один байт.
     *
     * @param value Передаваемый байт.
     *
     * @return Принятый байт.
     */
    uint8_t transfer(uint8_t value);

    /**
     * @brief Передаёт и принимает шестнадцатиразрядное слово.
     *
     * @param value Передаваемое слово.
     *
     * @return Принятое слово.
     */
    uint16_t transfer16(uint16_t value);

    /**
     * @brief Выполняет обмен массивом байтов на месте.
     *
     * @param buffer Буфер передачи и приёма.
     * @param size Количество байтов.
     */
    void transfer(void* buffer, size_t size);

    /**
     * @brief Устанавливает делитель частоты SPI для последующих транзакций.
     *
     * @param divider Делитель `SPI_CLOCK_DIVx`.
     */
    void setClockDivider(uint8_t divider);

    /**
     * @brief Устанавливает порядок битов для последующих транзакций.
     *
     * @param bit_order Порядок битов: `MSBFIRST` или `LSBFIRST`.
     */
    void setBitOrder(uint8_t bit_order);

    /**
     * @brief Устанавливает режим SPI для последующих транзакций.
     *
     * @param data_mode Режим SPI: `SPI_MODE0`..`SPI_MODE3`.
     */
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
