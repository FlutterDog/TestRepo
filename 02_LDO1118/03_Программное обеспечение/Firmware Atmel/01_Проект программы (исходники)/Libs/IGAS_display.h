
#ifndef IGAS_DISPLAY_H
#define IGAS_DISPLAY_H

#include "ADC_init.h"  // было "libs/ADC_init.h" — неправильно
#include "SPI.h"



// Номер символа в массиве 0    1     2      3	   4     5   	6     7     8	 9    10      11   12	 13	    14   15   16     17    18   19    20     21   22     23   24   25     26	27		28
// Массив всех символов    0    1     2      3     4     5      6     7     8    9   empty    A     C     L     E     r    -     d     o     n    I       G   8.     H    .      t     F	 u		P

 const uint8_t digit[] = {0xF5, 0x84, 0xB3, 0x97, 0xC6, 0x57, 0x77, 0x85, 0xF7, 0xD7, 0x00, 0xE7, 0x71, 0x70, 0x73, 0x61, 0x02, 0xB6, 0x36, 0x26, 0x84, 0x75, 0xFF, 0xE6, 0x08,  0x72, 0x63, 0x34, 0xE3};

 const uint8_t _0 = 0;
 const uint8_t _1 = 1;
 const uint8_t _2 = 2;
 const uint8_t _3 = 3;
 const uint8_t _4 = 4;
 const uint8_t _5 = 5;
 const uint8_t _6 = 6;
 const uint8_t _7 = 7;
 const uint8_t _8 = 8;
 const uint8_t _9 = 9;
 const uint8_t _Empty = 10;
 const uint8_t _A = 11;
 const uint8_t _C = 12;
 const uint8_t _L = 13;
 const uint8_t _E = 14;
 const uint8_t _r = 15;
 const uint8_t _minus = 16;
 const uint8_t _d = 17;
 const uint8_t _o = 18;
 const uint8_t _n = 19;
 const uint8_t _I = 20;
 const uint8_t _G = 21;
 const uint8_t _FULL = 22;
 const uint8_t _H = 23;
 const uint8_t _dot = 24;
 const uint8_t _S = 5;
 const uint8_t _t = 25;
 const uint8_t _F = 26;
 const uint8_t _u = 27;
 const uint8_t _P = 28;
  
 const uint8_t pos[] = {0x1C, 0x1A, 0x16, 0x0E, 0x0E};



class IGAS_display_LED
{

	public:

	void displayMessage(uint8_t linkDot);
	void calculateText(int16_t _number, uint8_t flDot_ = 0);
	void displayText(uint8_t L1, uint8_t L2, uint8_t L3, uint8_t L4);
	void CS_clampSPIPin();
	
	uint8_t thousands;                    //выделяем сотни
	uint8_t hundreds;               //выделяем десятки
	uint8_t tens;         //выделяем единицы
	uint8_t ones;
	uint8_t dispTrigger;

	
	private:
	void transferDigit(uint8_t value_);
	void CS_transferPin();
	
	
	int16_t thousandsInt;                    //выделяем сотни
	int16_t hundredsInt;               //выделяем десятки
	int16_t tensInt;         //выделяем единицы
	int16_t onesInt;
	uint8_t digit_turn = 0;

};

#endif

#ifndef IGAS_DISPLAY_H
#define IGAS_DISPLAY_H

/**
 * @file IGAS_display.h
 * @brief Интерфейс драйвера 4-разрядного семисегментного LED-индикатора (SPI, мультиплексирование).
 *
 * @details
 * Заголовок содержит:
 * - таблицу сегментных кодов @ref digit и таблицу выбора позиции @ref pos;
 * - символьные индексы для формирования сообщений;
 * - класс @ref IGAS_display_LED, реализующий мультиплексированный вывод.
 *
 * @note
 * Таблицы @ref digit и @ref pos объявлены как `extern`, а определения вынесены в .cpp,
 * чтобы избежать множественных определений при линковке и обеспечить корректную сборку.
 */

#include <stdint.h>
#include "ADC_init.h"  // Используются бинарные маски (Bxxxx) и базовые определения проекта.
#include "SPI.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Таблица сегментных кодов для индикатора.
 *
 * @details
 * Индексы:
 * - 0..9  — цифры '0'..'9'
 * - 10    — пустой символ (blank)
 * - далее — служебные/буквенные символы (A, C, L, E, r, '-', d, o, n, I, G, FULL, H, '.', t, F, u, P)
 *
 * Значения — байты, соответствующие аппаратной распиновке сегментов (катоды/сегменты).
 *
 * @warning
 * Конкретная раскладка сегментов зависит от схемы подключения и типа индикатора.
 */
extern const uint8_t digit[];

/**
 * @brief Таблица выбора позиции (аноды) для мультиплексирования.
 *
 * @details
 * pos[digit_turn] передаётся вторым байтом после сегментов и определяет активный разряд.
 * Дополнительный элемент оставлен для совместимости с существующим кодом.
 */
extern const uint8_t pos[];

#ifdef __cplusplus
} // extern "C"
#endif

/* ===== Символьные индексы в таблице digit[] =====
 * Сохранены как константы для минимального вмешательства в существующий код.
 * Рекомендуемый следующий шаг — заменить на enum class, но здесь не меняем API.
 */
static constexpr uint8_t _0     = 0;
static constexpr uint8_t _1     = 1;
static constexpr uint8_t _2     = 2;
static constexpr uint8_t _3     = 3;
static constexpr uint8_t _4     = 4;
static constexpr uint8_t _5     = 5;
static constexpr uint8_t _6     = 6;
static constexpr uint8_t _7     = 7;
static constexpr uint8_t _8     = 8;
static constexpr uint8_t _9     = 9;
static constexpr uint8_t _Empty = 10;

static constexpr uint8_t _A     = 11;
static constexpr uint8_t _C     = 12;
static constexpr uint8_t _L     = 13;
static constexpr uint8_t _E     = 14;
static constexpr uint8_t _r     = 15;
static constexpr uint8_t _minus = 16;
static constexpr uint8_t _d     = 17;
static constexpr uint8_t _o     = 18;
static constexpr uint8_t _n     = 19;
static constexpr uint8_t _I     = 20;
static constexpr uint8_t _G     = 21;
static constexpr uint8_t _FULL  = 22;
static constexpr uint8_t _H     = 23;
static constexpr uint8_t _dot   = 24;

// Алиас: 'S' совпадает с кодом цифры 5 в текущей таблице символов.
static constexpr uint8_t _S     = _5;

static constexpr uint8_t _t     = 25;
static constexpr uint8_t _F     = 26;
static constexpr uint8_t _u     = 27;
static constexpr uint8_t _P     = 28;

/**
 * @class IGAS_display_LED
 * @brief Мультиплексированный вывод на 4-разрядный семисегментный индикатор через SPI.
 *
 * @details
 * Типовой сценарий:
 * 1) calculateText() формирует буферы тысяч/сотен/десятков/единиц;
 * 2) displayMessage() вызывается периодически (например, из таймера) и обновляет один разряд за вызов.
 *
 * Управление CS вынесено в отдельные методы: CS_transferPin() (начало транзакции) и
 * CS_clampSPIPin() (завершение транзакции).
 */
class IGAS_display_LED
{
public:
	/**
	 * @brief Обновить один разряд индикатора.
	 * @param linkDot Маска точки/индикатора связи (применяется к младшему разряду).
	 */
	void displayMessage(uint8_t linkDot);

	/**
	 * @brief Преобразовать число в разряды и подготовить буферы отображения.
	 * @param _number Значение для отображения.
	 * @param flDot_ Маска десятичной точки для разряда tens (0 — без точки).
	 */
	void calculateText(int16_t _number, uint8_t flDot_ = 0);

	/**
	 * @brief Отобразить 4 символа по индексам таблицы @ref digit.
	 * @param L1 Индекс для разряда тысяч.
	 * @param L2 Индекс для разряда сотен.
	 * @param L3 Индекс для разряда десятков.
	 * @param L4 Индекс для разряда единиц.
	 */
	void displayText(uint8_t L1, uint8_t L2, uint8_t L3, uint8_t L4);

	/**
	 * @brief Завершить SPI-транзакцию: CS в неактивное состояние.
	 */
	void CS_clampSPIPin();

	/* Буферы отображения (сегментные коды, а не "цифры"). */
	uint8_t thousands = 0;
	uint8_t hundreds  = 0;
	uint8_t tens      = 0;
	uint8_t ones      = 0;

	/**
	 * @brief Флаг активности передачи на дисплей.
	 * @details
	 * Устанавливается/сбрасывается в CS_transferPin()/CS_clampSPIPin().
	 * Используется как диагностический/сервисный признак.
	 */
	uint8_t dispTrigger = 0;

private:
	/**
	 * @brief Передать на дисплей байт сегментов и байт позиции.
	 * @param value_ Сегментный код (катоды/сегменты).
	 */
	void transferDigit(uint8_t value_);

	/**
	 * @brief Начать SPI-транзакцию: CS в активное состояние.
	 */
	void CS_transferPin();

	/* Внутренние разряды до преобразования в сегментный код. */
	int16_t thousandsInt = 0;
	int16_t hundredsInt  = 0;
	int16_t tensInt      = 0;
	int16_t onesInt      = 0;

	/**
	 * @brief Индекс текущего разряда для мультиплексирования (0..3).
	 */
	uint8_t digit_turn = 0;
};

#endif // IGAS_DISPLAY_H
