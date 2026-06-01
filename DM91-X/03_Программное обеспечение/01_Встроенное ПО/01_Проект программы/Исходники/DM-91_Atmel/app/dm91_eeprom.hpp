#ifndef DM91_EEPROM_HPP
#define DM91_EEPROM_HPP

/**
 * @file dm91_eeprom.hpp
 * @brief Интерфейс работы с EEPROM прошивки DM-91x.
 *
 * @details
 * Файл содержит функции чтения, записи и загрузки параметров из внутренней
 * EEPROM микроконтроллера ATmega328P. Карта адресов параметров описана в
 * dm91_eeprom_map.hpp.
 */

#include "dm91_eeprom_map.hpp"

#include <stdint.h>

/**
 * @brief Пространство имён работы с EEPROM DM-91x.
 */
namespace dm91_eeprom
{
	/**
	 * @brief Читает один байт из EEPROM.
	 *
	 * @param address Адрес байта в EEPROM.
	 *
	 * @return Значение, прочитанное из EEPROM.
	 */
	uint8_t readByte(uint16_t address);

	/**
	 * @brief Записывает один байт в EEPROM.
	 *
	 * @param address Адрес байта в EEPROM.
	 * @param value Записываемое значение.
	 */
	void writeByte(uint16_t address, uint8_t value);

	/**
	 * @brief Загружает однобайтовый параметр из EEPROM.
	 *
	 * @details
	 * Если в EEPROM находится значение 0xFF или 0, функция записывает
	 * значение по умолчанию и возвращает его. Поведение сохранено по
	 * существующей логике прошивки DM-91x.
	 *
	 * @param address Адрес параметра.
	 * @param defaultValue Значение по умолчанию.
	 *
	 * @return Загруженное значение параметра.
	 */
	uint8_t loadByte(uint16_t address, uint8_t defaultValue);

	/**
	 * @brief Загружает параметр float из EEPROM.
	 *
	 * @param address Начальный адрес четырёхбайтового параметра.
	 * @param defaultValue Значение по умолчанию.
	 *
	 * @return Загруженное значение параметра.
	 */
	float loadFloat(uint16_t address, float defaultValue);

	/**
	 * @brief Загружает беззнаковый шестнадцатибитный параметр из EEPROM.
	 *
	 * @details
	 * Значение собирается из младшего и старшего байта в том же порядке,
	 * в котором saveInt() сохраняет его в EEPROM.
	 *
	 * @param address Начальный адрес параметра.
	 * @param defaultValue Значение по умолчанию.
	 *
	 * @return Загруженное значение параметра.
	 */
	int loadInt(uint16_t address, uint16_t defaultValue);

	/**
	 * @brief Сохраняет шестнадцатибитный параметр в EEPROM.
	 *
	 * @param address Начальный адрес параметра.
	 * @param value Записываемое значение.
	 */
	void saveInt(uint16_t address, int value);

	/**
	 * @brief Загружает знаковый параметр из EEPROM.
	 *
	 * @details
	 * Значение хранится в двух байтах, знак хранится отдельным байтом.
	 *
	 * @param address Начальный адрес параметра.
	 *
	 * @return Загруженное знаковое значение.
	 */
	int loadSignedInt(uint16_t address);

	/**
	 * @brief Сохраняет знаковый параметр в EEPROM.
	 *
	 * @param address Начальный адрес параметра.
	 * @param value Записываемое значение.
	 */
	void saveSignedInt(uint16_t address, int value);
}

#endif /* DM91_EEPROM_HPP */
