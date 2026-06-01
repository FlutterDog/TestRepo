#ifndef DM91_EEPROM_MAP_HPP
#define DM91_EEPROM_MAP_HPP

/**
 * @file dm91_eeprom_map.hpp
 * @brief Карта адресов EEPROM прошивки DM-91x.
 *
 * @details
 * Файл содержит именованные адреса параметров, сохраняемых во внутренней
 * EEPROM микроконтроллера ATmega328P. Адреса соответствуют существующей
 * карте параметров прошивки DM-91x и используются при загрузке настроек
 * устройства, а также при записи параметров через Modbus RTU.
 */

#include <stdint.h>

/**
 * @brief Пространство имён карты EEPROM DM-91x.
 */
namespace dm91_eeprom
{
	/** @brief Адрес устройства Modbus RTU. */
	static const uint16_t ADDR_SLAVE_ADDRESS = 100U;

	/** @brief Код скорости обмена Modbus RTU. */
	static const uint16_t ADDR_BAUD_RATE = 101U;

	/** @brief Код контроля чётности Modbus RTU. */
	static const uint16_t ADDR_PARITY = 102U;

	/** @brief Целочисленный параметр плотности газа, сохранённый для карты настроек. */
	static const uint16_t ADDR_DENSITY_SET = 104U;

	/** @brief Код дополнительного газового компонента бинарной смеси. */
	static const uint16_t ADDR_GAS_PARTNER = 105U;

	/** @brief Младшая часть серийного номера устройства. */
	static const uint16_t ADDR_SERIAL_LOW = 106U;

	/** @brief Старшая часть серийного номера устройства. */
	static const uint16_t ADDR_SERIAL_HIGH = 108U;

	/** @brief Код исполнения устройства: DM-91-0 или DM-91-H. */
	static const uint16_t ADDR_MODEL = 112U;

	/** @brief Источник температуры для расчётов. */
	static const uint16_t ADDR_TEMP_SOURCE = 113U;

	/** @brief Рабочий коэффициент плотности газа в формате float. */
	static const uint16_t ADDR_DENSITY_FLOAT = 130U;

	/** @brief Порог плотности, связанный с областью сжижения газа. */
	static const uint16_t ADDR_LIQUEFACTION = 134U;

	/** @brief Порог повышенной плотности газа. */
	static const uint16_t ADDR_OVERDENSITY = 135U;

	/** @brief Предупредительный уровень давления, приведённого к 20 °C. */
	static const uint16_t ADDR_WARNING_LEVEL = 136U;

	/** @brief Аварийный уровень давления, приведённого к 20 °C. */
	static const uint16_t ADDR_ALARM_LEVEL = 137U;

	/** @brief Смещение температуры. */
	static const uint16_t ADDR_OFFSET_TEMP = 138U;

	/** @brief Свободный член калибровки влажности. */
	static const uint16_t ADDR_OFFSET_HUMIDITY_B = 141U;

	/** @brief Коэффициент калибровки влажности. */
	static const uint16_t ADDR_OFFSET_HUMIDITY_K = 144U;

	/** @brief Нижняя граница диапазона датчика давления. */
	static const uint16_t ADDR_TIAN_PMIN = 148U;

	/** @brief Верхняя граница диапазона датчика давления. */
	static const uint16_t ADDR_TIAN_PMAX = 149U;

	/** @brief Масштабный коэффициент датчика давления. */
	static const uint16_t ADDR_TIAN_K = 150U;

	/** @brief Смещение датчика давления. */
	static const uint16_t ADDR_TIAN_OFFSET = 152U;

	/** @brief Уставка температуры. */
	static const uint16_t ADDR_TEMP_SETPOINT = 154U;

	/** @brief Разрешение системы нагрева. */
	static const uint16_t ADDR_ENABLE_CPU_HEAT = 159U;

	/** @brief Признак аппаратного исполнения Arktic. */
	static const uint16_t ADDR_ARKTIC_OPTION = 160U;

	/** @brief Тайм-аут отсутствия адресованных Modbus-запросов, мс. */
	static const uint16_t ADDR_INCOMING_TIMEOUT = 161U;
}

#endif /* DM91_EEPROM_MAP_HPP */