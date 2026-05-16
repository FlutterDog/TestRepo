#pragma once
#ifndef IGAS_SPI_SHIM_H
#define IGAS_SPI_SHIM_H

#include <stdint.h>
#include "../hal/compat.hpp"

/**
 * @file SPI.h
 * @brief Минимальный совместимый интерфейс SPI поверх HAL (AVR).
 *
 * @details
 * Заголовок предоставляет:
 * - константы порядка бит (LSBFIRST/MSBFIRST);
 * - класс SPISettings (параметры транзакции, сохранены для совместимости API);
 * - класс SPIClass с begin()/transfer() и заглушками beginTransaction()/endTransaction();
 * - глобальный объект SPI (определение размещается в одном .cpp).
 *
 * Реальная настройка периферии (режим, делитель частоты) выполняется в hal::spi_init()
 * (см. compat_spi.cpp). На текущем этапе параметры SPISettings не применяются.
 */

/* Порядок бит (совместимость с Arduino-API). */
#ifndef LSBFIRST
#  define LSBFIRST 0u
#endif
#ifndef MSBFIRST
#  define MSBFIRST 1u
#endif

/**
 * @class SPISettings
 * @brief Контейнер параметров SPI-транзакции (совместимость).
 *
 * @details
 * В текущей реализации параметры хранятся, но не используются драйвером.
 * Сохранено, чтобы не переписывать код верхнего уровня.
 */
class SPISettings {
public:
	/**
	 * @brief Конструктор параметров транзакции.
	 * @param clock   Желаемая частота SPI (Гц).
	 * @param bitOrder Порядок бит: LSBFIRST или MSBFIRST.
	 * @param dataMode Режим SPI (CPOL/CPHA в виде значения 0..3).
	 */
	SPISettings(uint32_t clock, uint8_t bitOrder, uint8_t dataMode)
		: clock_(clock), bitOrder_(bitOrder), mode_(dataMode) {}

	/** @brief Запрошенная частота SPI (Гц). */
	uint32_t clock() const { return clock_; }
	/** @brief Порядок бит (LSBFIRST/MSBFIRST). */
	uint8_t bitOrder() const { return bitOrder_; }
	/** @brief Режим SPI (0..3). */
	uint8_t dataMode() const { return mode_; }

private:
	uint32_t clock_;
	uint8_t  bitOrder_;
	uint8_t  mode_;
};

/**
 * @class SPIClass
 * @brief Минимальный SPI-интерфейс, используемый проектом.
 *
 * @details
 * - begin() инициализирует SPI периферию через hal::spi_init().
 * - transfer() выполняет блокирующую передачу одного байта (tx/rx).
 * - beginTransaction()/endTransaction() оставлены как заглушки для совместимости.
 *
 * @note Если в проекте появятся несколько SPI-устройств с разными требованиями
 *       к скорости/режиму, требуется расширить hal::spi_init() и применить настройки
 *       из SPISettings.
 */
class SPIClass {
public:
	/** @brief Инициализация SPI-контроллера (режим ведущего). */
	inline void begin() { hal::spi_init(); }

	/**
	 * @brief Передать байт по SPI и получить байт в ответ.
	 * @param v Отправляемый байт.
	 * @return Принятый байт.
	 */
	inline uint8_t transfer(uint8_t v) { return hal::spi_txrx(v); }

	/**
	 * @brief Начало транзакции SPI (заглушка совместимости).
	 * @param settings Параметры транзакции (пока не применяются).
	 */
	inline void beginTransaction(const SPISettings& settings)
	{
		(void)settings;
		/* no-op */
	}

	/** @brief Завершение транзакции SPI (заглушка совместимости). */
	inline void endTransaction() { /* no-op */ }
};

/**
 * @brief Глобальный объект SPI.
 * @note Определение экземпляра должно находиться ровно в одном .cpp файле.
 */
extern SPIClass SPI;

/* Необязательные C-стайл обёртки (для легаси-кода). */
static inline void SPI_begin() { SPI.begin(); }
static inline void SPI_beginTransaction(const SPISettings& settings)
{
	(void)settings;
	/* no-op */
}
static inline void SPI_endTransaction() { /* no-op */ }

#endif // IGAS_SPI_SHIM_H
