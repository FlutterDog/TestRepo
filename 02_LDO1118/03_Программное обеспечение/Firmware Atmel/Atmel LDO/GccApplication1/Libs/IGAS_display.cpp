/**
 * @file IGAS_display.cpp
 * @brief Драйвер 4-разрядного семисегментного LED-индикатора (мультиплексирование через SPI).
 *
 * @details
 * Реализует функции класса ::IGAS_display_LED:
 * - подготовка отображаемых символов (разбиение числа на разряды, подавление ведущих нулей);
 * - циклический вывод одного разряда за вызов (мультиплексирование по digit_turn);
 * - передача данных в сдвиговые регистры по SPI и управление линией CS.
 *
 * @note displayMessage() должен вызываться периодически с частотой, достаточной для отсутствия мерцания.
 */

#include "IGAS_display.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>


/* -------------------------------------------------------------------------- */

void IGAS_display_LED::displayMessage(uint8_t linkDot)
{
	// Один вызов = обновление одного разряда.
	// linkDot применяется только к младшему разряду (ones).
	switch (digit_turn)
	{
		case 0:
			transferDigit(thousands);
			break;

		case 1:
			transferDigit(hundreds);
			break;

		case 2:
			transferDigit(tens);
			break;

		case 3:
			transferDigit((uint8_t)(ones | linkDot));
			digit_turn = 0; // завершили цикл 0..3
			break;

		default:
			// Защита от выхода digit_turn за допустимый диапазон.
			digit_turn = 0;
			transferDigit(thousands);
			break;
	}
}

void IGAS_display_LED::transferDigit(uint8_t segments)
{
	// Внутренняя защита от неверного значения digit_turn.
	// pos[] рассчитан на индексацию 0..3 (пятый элемент оставлен для совместимости).
	if (digit_turn > 3) {
		digit_turn = 0;
	}

	// Начало транзакции: CS активен (низкий уровень).
	CS_transferPin();

	// Передаём байт сегментов (катоды) и байт выбора позиции (аноды).
	// В текущей архитектуре "закрытие" CS выполняется отдельным вызовом CS_clampSPIPin().
	SPI.transfer(segments);
	SPI.transfer(pos[digit_turn]);

	// Переходим к следующему разряду (0..3).
	++digit_turn;
}

void IGAS_display_LED::calculateText(int16_t _number, uint8_t flDot_)
{
	// Разбиение числа на разряды с подавлением ведущих нулей.
	// Индекс 10 в digit[] трактуется как "пустой символ".
	thousandsInt = (int16_t)(_number * 0.001f);
	hundredsInt  = (int16_t)((_number - thousandsInt * 1000) * 0.01f);
	tensInt      = (int16_t)((_number - thousandsInt * 1000 - hundredsInt * 100) * 0.1f);
	onesInt      = (int16_t)(_number - thousandsInt * 1000 - hundredsInt * 100 - tensInt * 10);

	// Подавление ведущих нулей.
	if (thousandsInt == 0) {
		thousandsInt = 10;
	}
	if (thousandsInt == 10 && hundredsInt == 0) {
		hundredsInt = 10;
	}

	// Если тысяч/сотен нет и десятки равны нулю:
	// - без точки скрываем десятки;
	// - при наличии точки оставляем "0" для визуальной привязки точки к разряду.
	if (thousandsInt == 10 && hundredsInt == 10 && tensInt == 0) {
		tensInt = (flDot_ == 0) ? 10 : 0;
	}

	thousands = digit[thousandsInt];
	hundreds  = digit[hundredsInt];
	tens      = (uint8_t)(digit[tensInt] | flDot_);
	ones      = digit[onesInt];
}

void IGAS_display_LED::displayText(uint8_t L1, uint8_t L2, uint8_t L3, uint8_t L4)
{
	// Прямая установка разрядов по индексам таблицы digit[].
	thousands = digit[L1];
	hundreds  = digit[L2];
	tens      = digit[L3];
	ones      = digit[L4];
}

void IGAS_display_LED::CS_transferPin()
{
	// Критическая секция: модификация PORTB должна быть атомарной относительно ISR,
	// которые потенциально используют тот же порт.
	const uint8_t oldSREG = SREG;
	cli();

	// CS на PB0: active-low.
	PORTB &= (uint8_t)~_BV(PB0);
	dispTrigger = 1;

	SREG = oldSREG;
}

void IGAS_display_LED::CS_clampSPIPin()
{
	// Завершение транзакции: CS в неактивное состояние.
	const uint8_t oldSREG = SREG;
	cli();

	PORTB |= (uint8_t)_BV(PB0);
	dispTrigger = 0;

	SREG = oldSREG;
}
