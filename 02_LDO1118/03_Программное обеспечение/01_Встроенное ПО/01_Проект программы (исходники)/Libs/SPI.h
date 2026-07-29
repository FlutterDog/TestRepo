#pragma once

#ifndef IGAS_SPI_H
#define IGAS_SPI_H

#include <stdint.h>

#include "../hal/compat.hpp"

/**
 * @file SPI.h
 * @brief Проектный SPI-интерфейс поверх HAL.
 *
 * @details
 * Заголовок предоставляет:
 * - константы порядка передачи битов;
 * - класс SPISettings для хранения параметров SPI-обмена;
 * - класс SPIClass для инициализации SPI и передачи байтов;
 * - глобальный объект SPI.
 *
 * Низкоуровневая работа с периферией выполняется функциями hal::spi_init()
 * и hal::spi_txrx().
 *
 * @note
 * В текущей реализации параметры SPISettings сохраняются в объекте, но не
 * применяются при настройке аппаратного SPI. Если в проекте потребуется
 * несколько SPI-устройств с разными режимами обмена, драйвер SPI необходимо
 * расширить.
 */

/**
 * @brief Передача младшего бита первым.
 */
#ifndef LSBFIRST
#  define LSBFIRST 0u
#endif

/**
 * @brief Передача старшего бита первым.
 */
#ifndef MSBFIRST
#  define MSBFIRST 1u
#endif

/**
 * @class SPISettings
 * @brief Контейнер параметров SPI-обмена.
 *
 * @details
 * Хранит требуемую частоту SPI, порядок передачи битов и режим SPI.
 * Параметры доступны вызывающему коду через методы clock(), bitOrder()
 * и dataMode().
 */
class SPISettings
{
public:
    /**
     * @brief Создаёт набор параметров SPI-обмена.
     *
     * @param clock Требуемая частота SCK, Гц.
     * @param bitOrder Порядок передачи битов: LSBFIRST или MSBFIRST.
     * @param dataMode Режим SPI: значение 0..3, соответствующее CPOL/CPHA.
     */
    SPISettings(uint32_t clock, uint8_t bitOrder, uint8_t dataMode)
        : clock_(clock),
          bitOrder_(bitOrder),
          mode_(dataMode)
    {
    }

    /**
     * @brief Возвращает требуемую частоту SPI.
     *
     * @return Частота SCK, Гц.
     */
    uint32_t clock(void) const
    {
        return clock_;
    }

    /**
     * @brief Возвращает порядок передачи битов.
     *
     * @return LSBFIRST или MSBFIRST.
     */
    uint8_t bitOrder(void) const
    {
        return bitOrder_;
    }

    /**
     * @brief Возвращает режим SPI.
     *
     * @return Режим SPI 0..3.
     */
    uint8_t dataMode(void) const
    {
        return mode_;
    }

private:
    /**
     * @brief Требуемая частота SCK, Гц.
     */
    uint32_t clock_;

    /**
     * @brief Порядок передачи битов.
     */
    uint8_t bitOrder_;

    /**
     * @brief Режим SPI.
     */
    uint8_t mode_;
};

/**
 * @class SPIClass
 * @brief Интерфейс доступа к аппаратному SPI.
 *
 * @details
 * Класс предоставляет операции:
 * - begin() — инициализация SPI-контроллера;
 * - transfer() — блокирующая передача одного байта с одновременным приёмом;
 * - beginTransaction() — начало логической транзакции;
 * - endTransaction() — завершение логической транзакции.
 *
 * В текущей реализации beginTransaction() и endTransaction() не изменяют
 * регистры SPI. Настройка периферии выполняется в begin().
 */
class SPIClass
{
public:
    /**
     * @brief Инициализирует аппаратный SPI-контроллер.
     *
     * @details
     * Настройка выполняется функцией hal::spi_init().
     */
    inline void begin(void)
    {
        hal::spi_init();
    }

    /**
     * @brief Передаёт один байт по SPI и одновременно принимает один байт.
     *
     * @param v Передаваемый байт.
     *
     * @return Принятый байт.
     */
    inline uint8_t transfer(uint8_t v)
    {
        return hal::spi_txrx(v);
    }

    /**
     * @brief Начинает логическую SPI-транзакцию.
     *
     * @param settings Параметры SPI-обмена.
     *
     * @details
     * В текущей реализации параметры не применяются к аппаратным регистрам.
     */
    inline void beginTransaction(const SPISettings& settings)
    {
        (void)settings;
    }

    /**
     * @brief Завершает логическую SPI-транзакцию.
     */
    inline void endTransaction(void)
    {
    }
};

/**
 * @brief Глобальный объект SPI-интерфейса.
 *
 * @details
 * Определение экземпляра должно находиться ровно в одном .cpp файле.
 */
extern SPIClass SPI;

/**
 * @brief Инициализирует аппаратный SPI-контроллер.
 */
static inline void SPI_begin(void)
{
    SPI.begin();
}

/**
 * @brief Начинает логическую SPI-транзакцию.
 *
 * @param settings Параметры SPI-обмена.
 */
static inline void SPI_beginTransaction(const SPISettings& settings)
{
    SPI.beginTransaction(settings);
}

/**
 * @brief Завершает логическую SPI-транзакцию.
 */
static inline void SPI_endTransaction(void)
{
    SPI.endTransaction();
}

#endif /* IGAS_SPI_H */