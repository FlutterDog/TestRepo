#include "dm91_board.hpp"

/**
 * @file dm91_board.cpp
 * @brief Реализация аппаратного слоя платы датчика плотности DM-91x.
 */

namespace
{
	/**
	 * @brief Маска линии управления направлением RS-485.
	 */
	static const uint8_t RS485_DE_MASK = _BV(PD2);

	/**
	 * @brief Текущее состояние направления RS-485.
	 *
	 * @details
	 * Ненулевое значение соответствует режиму передачи.
	 */
	static volatile uint8_t s_rs485TransmitMode = 0U;

	/**
	 * @brief Устанавливает биты в регистре порта.
	 *
	 * @param port Регистр PORTx.
	 * @param mask Маска устанавливаемых битов.
	 */
	static inline void setPortBits(volatile uint8_t& port, uint8_t mask)
	{
		port |= mask;
	}

	/**
	 * @brief Сбрасывает биты в регистре порта.
	 *
	 * @param port Регистр PORTx.
	 * @param mask Маска сбрасываемых битов.
	 */
	static inline void clearPortBits(volatile uint8_t& port, uint8_t mask)
	{
		port &= static_cast<uint8_t>(~mask);
	}
}

namespace dm91_board
{
	void initIo(void)
	{
		/*
		 * PD2 — управление направлением RS-485.
		 * Низкий уровень соответствует режиму приёма.
		 */
		DDRD |= RS485_DE_MASK;
		clearPortBits(PORTD, RS485_DE_MASK);

		s_rs485TransmitMode = 0U;
	}

	uint8_t readSlaveAddress(void)
	{
		return 0U;
	}

	void statusLedWrite(bool state)
	{
		(void)state;
	}

	void statusLedToggle(void)
	{
	}

	void rs485TransmitMode(void)
	{
		setPortBits(PORTD, RS485_DE_MASK);
		s_rs485TransmitMode = 1U;
	}

	void rs485ReceiveMode(void)
	{
		clearPortBits(PORTD, RS485_DE_MASK);
		s_rs485TransmitMode = 0U;
	}

	bool rs485IsTransmitMode(void)
	{
		return (s_rs485TransmitMode != 0U);
	}

	void setShiftEnable(bool enabled)
	{
		(void)enabled;
	}

	void writeLedRegister(uint8_t value)
	{
		(void)value;
	}

	void writeModeRegister(uint8_t value)
	{
		(void)value;
	}
}
