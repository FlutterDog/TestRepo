/**
 * @file IGAS_Eeprom.h
 * @brief Буферизованная запись EEPROM и примитивы хранения параметров (очередь отложенной записи).
 *
 * @details
 * Класс ::IGAS_Eeprom_Class реализует неблокирующую модель сохранения:
 * - save*() помещают операции в кольцевую очередь (RAM);
 * - eepromRing() периодически выполняет физическую запись в EEPROM небольшими порциями;
 * - запись выполняется через EEPROM.update(), уменьшая износ памяти.
 *
 * Такой подход предотвращает длинные блокирующие задержки в момент сохранения параметров.
 * Требование: eepromRing() должен вызываться регулярно из основного цикла/тикера.
 *
 * @note
 * Данный интерфейс сохранён совместимым с историческим кодом проекта.
 */

#pragma once
#ifndef IGAS_EEPROM_H
#define IGAS_EEPROM_H

#include <stdint.h>

#include "ADC_init.h"   // byte и совместимые макросы/типы проекта
#include "EEPROM.h"     // shim над <avr/eeprom.h>

/**
 * @class IGAS_Eeprom_Class
 * @brief Очередь отложенной записи EEPROM и функции сохранения/загрузки типовых значений.
 *
 * @details
 * Внутренняя очередь хранит пары {адрес, байт}. Более крупные значения (int16_t, float)
 * раскладываются на байты и также попадают в очередь.
 *
 * При переполнении очереди вытесняются самые старые операции (lossy queue), что подходит
 * для параметров, которые могут обновляться часто, и где важнее «последнее состояние».
 */
class IGAS_Eeprom_Class {
public:
	/**
	 * @brief Конструктор (инициализирует очередь и внутренние индексы).
	 */
	IGAS_Eeprom_Class();

	/**
	 * @brief Периодическая обработка очереди EEPROM.
	 * @return 1, если очередь пуста и EEPROM готов к записи; 0 — если есть работа или EEPROM занят.
	 *
	 * @details
	 * Рекомендуется вызывать в основном цикле с постоянной периодичностью.
	 */
	uint8_t eepromRing();

	/**
	 * @brief Сохранить 16-битное значение (2 байта, little-endian).
	 * @param addr_  Базовый адрес.
	 * @param digit_ Значение.
	 */
	void saveInt(int16_t addr_, int16_t digit_);

	/**
	 * @brief Загрузить 16-битное значение (2 байта, little-endian).
	 * @param addr_ Базовый адрес.
	 * @return Прочитанное значение.
	 */
	int16_t loadInt(int16_t addr_);

	/**
	 * @brief Сохранить знаковое 16-битное значение в формате {magnitude(lo/hi), sign}.
	 * @param addr_  Базовый адрес (занимает 3 байта).
	 * @param digit_ Значение.
	 *
	 * @details
	 * Формат совместим с историческим кодом: два байта величины и один байт знака (0/1).
	 */
	void saveSignedInt(int16_t addr_, int16_t digit_);

	/**
	 * @brief Загрузить знаковое 16-битное значение из формата {magnitude, sign}.
	 * @param addr_ Базовый адрес (3 байта).
	 * @return Значение со знаком.
	 */
	int16_t loadSignedInt(int16_t addr_);

	/**
	 * @brief Сохранить «короткий signed» в формате {magnitude(u8), sign(u8)}.
	 * @param addr_  Базовый адрес (2 байта).
	 * @param digit_ Значение.
	 *
	 * @details
	 * Формат используется для параметров, где диапазона 0..255 по модулю достаточно.
	 */
	void saveSigned(int16_t addr_, int16_t digit_);

	/**
	 * @brief Загрузить «короткий signed» из формата {magnitude, sign}.
	 * @param addr_ Базовый адрес (2 байта).
	 * @return Значение со знаком.
	 */
	int16_t loadSign(int16_t addr_);

	/**
	 * @brief Поставить в очередь запись одного байта.
	 * @param addr_  Адрес EEPROM.
	 * @param digit_ Байт данных.
	 */
	void saveByte(int16_t addr_, uint8_t digit_);

	/**
	 * @brief Загрузить float из EEPROM (4 байта).
	 * @param addr_     Базовый адрес.
	 * @param default_  Значение по умолчанию, если область «пустая» (0xFF x4).
	 * @return Прочитанное значение или default_.
	 *
	 * @details
	 * Для совместимости используется «обратный порядок байт» относительно памяти.
	 */
	float loadFloat(int16_t addr_, float default_);

	/**
	 * @brief Сохранить float в EEPROM (4 байта) через очередь.
	 * @param addr_  Базовый адрес.
	 * @param value  Значение.
	 *
	 * @details
	 * Используется «обратный порядок байт» для совместимости с историческим форматом.
	 */
	void saveFloat(int16_t addr_, float value);

private:
	/**
	 * @brief Извлечь следующий элемент очереди в ringAddr/ringData.
	 */
	void outRing();

	/**
	 * @brief Поместить запись {addr,data} в очередь.
	 * @param addr_  Адрес EEPROM.
	 * @param digit_ Байт данных.
	 *
	 * @details
	 * Защищает индексы очереди от гонок с ISR (короткая критическая секция).
	 */
	void inRing(int16_t addr_, uint8_t digit_);

	/**
	 * @brief Размер кольцевой очереди (число элементов {addr,data}).
	 *
	 * @note Это не степень двойки. Индексация реализована через сравнение и сброс в 0.
	 *       При необходимости оптимизации допускается увеличить и перейти на power-of-two
	 *       с маскированием, но это потребует изменения реализации.
	 */
	static const uint8_t RING_SIZE = 100;

	/** @brief Буфер данных (байты) для отложенной записи. */
	uint8_t dataBuffer[RING_SIZE];

	/** @brief Буфер адресов EEPROM, соответствующих dataBuffer[]. */
	int16_t addrBuffer[RING_SIZE];

	/** @brief Текущий адрес, извлечённый из очереди (временное поле). */
	int16_t ringAddr;

	/** @brief Текущий байт, извлечённый из очереди (временное поле). */
	uint8_t ringData;

	/** @brief Индекс «входа» (куда пишем следующий элемент). */
	volatile uint8_t in_;

	/** @brief Индекс «выхода» (откуда читаем следующий элемент). */
	volatile uint8_t out_;
};

#endif // IGAS_EEPROM_H
