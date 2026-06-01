/**
 * @file dm91_eeprom.cpp
 * @brief Реализация работы с EEPROM прошивки DM-91x.
 */

#include "dm91_eeprom.hpp"

#include <avr/eeprom.h>
#include <stdint.h>

/**
 * @brief Объединение для преобразования между float и массивом байтов.
 */
typedef union
{
	float val;
	uint8_t bytes[4];
} Dm91FloatBytes;

namespace dm91_eeprom
{
	uint8_t readByte(uint16_t address)
	{
		return eeprom_read_byte(reinterpret_cast<const uint8_t*>(address));
	}

	void writeByte(uint16_t address, uint8_t value)
	{
		eeprom_update_byte(reinterpret_cast<uint8_t*>(address), value);
	}

	uint8_t loadByte(uint16_t address, uint8_t defaultValue)
	{
		const uint8_t eepromBuffer = readByte(address);

		if (defaultValue == eepromBuffer)
		{
			return eepromBuffer;
		}

		if ((eepromBuffer == 0xFFU) || (eepromBuffer == 0U))
		{
			writeByte(address, defaultValue);
			return defaultValue;
		}

		return eepromBuffer;
	}

	float loadFloat(uint16_t address, float defaultValue)
	{
		Dm91FloatBytes castFloat;
		uint8_t error = 0U;

		for (uint8_t i = 0U; i <= 3U; i++)
		{
			castFloat.bytes[3U - i] = readByte(address + i);

			if (readByte(address + i) == 0xFFU)
			{
				error++;
			}
		}

		if (error == 4U)
		{
			return defaultValue;
		}

		return castFloat.val;
	}

	int loadInt(uint16_t address, uint16_t defaultValue)
	{
		const uint8_t lowByte = readByte(address);
		const uint8_t highByte = readByte(address + 1U);

		if ((lowByte == 0xFFU) && (highByte == 0xFFU))
		{
			return defaultValue;
		}

		const uint16_t castInt = static_cast<uint16_t>(lowByte) |
			(static_cast<uint16_t>(highByte) << 8);

		return static_cast<int>(castInt);
	}

	void saveInt(uint16_t address, int value)
	{
		writeByte(address, static_cast<uint8_t>(value & 0x00FF));
		writeByte(address + 1U, static_cast<uint8_t>(value >> 8));
	}

	int loadSignedInt(uint16_t address)
	{
		unsigned int buffer = static_cast<unsigned int>(readByte(address));
		buffer |= static_cast<unsigned int>(readByte(address + 1U)) << 8;

		if (buffer == 0xFFFFU)
		{
			buffer = 0U;
		}

		if (readByte(address + 2U) == 0U)
		{
			return static_cast<int>(buffer);
		}

		return -1 * static_cast<int>(buffer);
	}

	void saveSignedInt(uint16_t address, int value)
	{
		uint8_t sign;

		if (value < 0)
		{
			value = -1 * value;
			sign = 1U;
		}
		else
		{
			sign = 0U;
		}

		writeByte(address, static_cast<uint8_t>(value & 0x00FF));
		writeByte(address + 1U, static_cast<uint8_t>(value >> 8));
		writeByte(address + 2U, sign);
	}
}
