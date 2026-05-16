/**
 * @file TimerOne.h
 * @brief Утилиты прерываний и PWM для аппаратного Timer1 (AVR/Teensy совместимость).
 *
 * @details
 * Заголовок предоставляет класс TimerOne, который инкапсулирует настройку Timer1:
 * - задание периода (в микросекундах) через подбор предделителя и TOP;
 * - управление запуском/остановом таймера;
 * - генерация PWM на выводах Timer1 (для AVR 16-bit и Teensy);
 * - установка пользовательского callback для прерываний (см. attachInterrupt()).
 *
 * В текущем проекте данный файл используется как «легаси»-совместимость.
 * Основной принцип: ISR вызывает статический указатель TimerOne::isrCallback,
 * который настраивается пользователем.
 *
 * @note
 * Большинство методов определены inline для оптимизации (как в оригинальной библиотеке).
 * Для корректных таймингов необходимо корректное значение F_CPU.
 */

/*
 * Original code by Jesse Tane for http://labs.ideo.com August 2008
 * Modified March 2009 by Jérôme Despatis and Jesse Tane for ATmega328 support
 * Modified June 2009 by Michael Polli and Jesse Tane to fix a bug in setPeriod()
 * Modified April 2012 by Paul Stoffregen - portable to other AVR chips, use inline functions
 * Modified again, June 2014 by Paul Stoffregen - support Teensy 3.x & more AVR chips
 * Modified July 2017 by Stoyko Dimitrov - added support for ATTiny85 (except PWM)
 *
 * This is free software. You can redistribute it and/or modify it under
 * the terms of Creative Commons Attribution 3.0 United States License.
 */

#ifndef TIMER_ONE_H_
#define TIMER_ONE_H_

#include "ADC_init.h"               // F_CPU, базовые типы/макросы совместимости проекта
#include "known_16bit_timers.h"     // соответствие выводов Timer1

#if defined(__AVR_ATtiny85__)
/** @brief Разрядность Timer1 на ATtiny85 (8-bit). */
#  define TIMER1_RESOLUTION 256UL
#elif defined(__AVR__)
/** @brief Разрядность Timer1 на типовых AVR (16-bit). */
#  define TIMER1_RESOLUTION 65536UL
#else
/** @brief Для не-AVR платформ предполагаем 16-bit. */
#  define TIMER1_RESOLUTION 65536UL
#endif

/**
 * @class TimerOne
 * @brief Настройка Timer1: период, PWM и прерывания.
 *
 * @details
 * Для AVR (ATmega328P и совместимые) используется режим Phase & Frequency Correct PWM,
 * где TOP задаётся регистром ICR1.
 *
 * Для ATtiny85 используется локальная реализация Timer1 (8-bit), где TOP задаётся OCR1C.
 */
class TimerOne
{

#if defined(__AVR_ATtiny85__)

public:
	/* ========================= Configuration ========================= */

	/**
	 * @brief Инициализация Timer1 и задание периода.
	 * @param microseconds Период, мкс.
	 */
	void initialize(unsigned long microseconds = 1000000) __attribute__((always_inline))
	{
		// CTC: сброс Timer1 по совпадению с OCR1C, прерывание по OCR1A.
		TCCR1 = _BV(CTC1);
		TIMSK |= _BV(OCIE1A);
		setPeriod(microseconds);
	}

	/**
	 * @brief Установить период Timer1.
	 * @param microseconds Период, мкс.
	 * @details Подбирает предделитель и TOP так, чтобы период поместился в разрядность таймера.
	 */
	void setPeriod(unsigned long microseconds) __attribute__((always_inline))
	{
		const unsigned long cycles = microseconds * ratio;

		if (cycles < TIMER1_RESOLUTION) {
			clockSelectBits = _BV(CS10);
			pwmPeriod = cycles;
		} else if (cycles < TIMER1_RESOLUTION * 2UL) {
			clockSelectBits = _BV(CS11);
			pwmPeriod = cycles / 2;
		} else if (cycles < TIMER1_RESOLUTION * 4UL) {
			clockSelectBits = _BV(CS11) | _BV(CS10);
			pwmPeriod = cycles / 4;
		} else if (cycles < TIMER1_RESOLUTION * 8UL) {
			clockSelectBits = _BV(CS12);
			pwmPeriod = cycles / 8;
		} else if (cycles < TIMER1_RESOLUTION * 16UL) {
			clockSelectBits = _BV(CS12) | _BV(CS10);
			pwmPeriod = cycles / 16;
		} else if (cycles < TIMER1_RESOLUTION * 32UL) {
			clockSelectBits = _BV(CS12) | _BV(CS11);
			pwmPeriod = cycles / 32;
		} else if (cycles < TIMER1_RESOLUTION * 64UL) {
			clockSelectBits = _BV(CS12) | _BV(CS11) | _BV(CS10);
			pwmPeriod = cycles / 64UL;
		} else if (cycles < TIMER1_RESOLUTION * 128UL) {
			clockSelectBits = _BV(CS13);
			pwmPeriod = cycles / 128;
		} else if (cycles < TIMER1_RESOLUTION * 256UL) {
			clockSelectBits = _BV(CS13) | _BV(CS10);
			pwmPeriod = cycles / 256;
		} else if (cycles < TIMER1_RESOLUTION * 512UL) {
			clockSelectBits = _BV(CS13) | _BV(CS11);
			pwmPeriod = cycles / 512;
		} else if (cycles < TIMER1_RESOLUTION * 1024UL) {
			clockSelectBits = _BV(CS13) | _BV(CS11) | _BV(CS10);
			pwmPeriod = cycles / 1024;
		} else if (cycles < TIMER1_RESOLUTION * 2048UL) {
			clockSelectBits = _BV(CS13) | _BV(CS12);
			pwmPeriod = cycles / 2048;
		} else if (cycles < TIMER1_RESOLUTION * 4096UL) {
			clockSelectBits = _BV(CS13) | _BV(CS12) | _BV(CS10);
			pwmPeriod = cycles / 4096;
		} else if (cycles < TIMER1_RESOLUTION * 8192UL) {
			clockSelectBits = _BV(CS13) | _BV(CS12) | _BV(CS11);
			pwmPeriod = cycles / 8192;
		} else if (cycles < TIMER1_RESOLUTION * 16384UL) {
			clockSelectBits = _BV(CS13) | _BV(CS12) | _BV(CS11) | _BV(CS10);
			pwmPeriod = cycles / 16384;
		} else {
			clockSelectBits = _BV(CS13) | _BV(CS12) | _BV(CS11) | _BV(CS10);
			pwmPeriod = TIMER1_RESOLUTION - 1;
		}

		OCR1A = pwmPeriod;
		OCR1C = pwmPeriod;
		TCCR1 = _BV(CTC1) | clockSelectBits;
	}

	/* ========================= Run Control ========================= */

	/** @brief Запуск Timer1 (сброс счётчика и включение). */
	void start() __attribute__((always_inline))
	{
		TCCR1 = 0;
		TCNT1 = 0;
		resume();
	}

	/** @brief Остановка Timer1 (CTC остаётся, счёт не идёт). */
	void stop() __attribute__((always_inline))
	{
		TCCR1 = _BV(CTC1);
	}

	/** @brief Перезапуск Timer1 (эквивалент start()). */
	void restart() __attribute__((always_inline)) { start(); }

	/** @brief Возобновить счёт Timer1 с ранее выбранным предделителем. */
	void resume() __attribute__((always_inline))
	{
		TCCR1 = _BV(CTC1) | clockSelectBits;
	}

	/* ========================= Interrupt Function ========================= */

	/**
	 * @brief Установить callback прерывания Timer1.
	 * @param isr Указатель на функцию без аргументов.
	 */
	void attachInterrupt(void (*isr)()) __attribute__((always_inline))
	{
		isrCallback = isr;
		TIMSK |= _BV(OCIE1A);
	}

	/**
	 * @brief Установить callback и (опционально) обновить период.
	 * @param isr Указатель на функцию.
	 * @param microseconds Период, мкс (если >0).
	 */
	void attachInterrupt(void (*isr)(), unsigned long microseconds) __attribute__((always_inline))
	{
		if (microseconds > 0) setPeriod(microseconds);
		attachInterrupt(isr);
	}

	/** @brief Отключить прерывание сравнения Timer1. */
	void detachInterrupt() __attribute__((always_inline))
	{
		// TIMSK используется совместно с другими таймерами, поэтому очищаем только OCIE1A.
		TIMSK &= (uint8_t)~_BV(OCIE1A);
	}

	/** @brief Указатель на текущий ISR callback (вызывается из ISR). */
	static void (*isrCallback)();

	/** @brief Пустой обработчик по умолчанию. */
	static void isrDefaultUnused();

private:
	static unsigned short pwmPeriod;
	static unsigned char clockSelectBits;

	/** @brief Коэффициент преобразования "мкс -> такты" для ATtiny85. */
	static const byte ratio = (F_CPU) / 1000000;

#elif defined(__AVR__)

public:
	/* ========================= Configuration ========================= */

	/**
	 * @brief Инициализация Timer1 и задание периода.
	 * @param microseconds Период, мкс.
	 */
	void initialize(unsigned long microseconds = 1000000) __attribute__((always_inline))
	{
		// Phase and Frequency Correct PWM, TOP=ICR1 (WGM13=1, остальные WGM=0), таймер остановлен.
		TCCR1B = _BV(WGM13);
		TCCR1A = 0;
		setPeriod(microseconds);
	}

	/**
	 * @brief Установить период Timer1.
	 * @param microseconds Период, мкс.
	 * @details В P&F Correct PWM частота счёта вдвое ниже (туда-обратно),
	 *          поэтому используется F_CPU/2_000_000 * microseconds.
	 */
	void setPeriod(unsigned long microseconds) __attribute__((always_inline))
	{
		const unsigned long cycles = (F_CPU / 2000000UL) * microseconds;

		if (cycles < TIMER1_RESOLUTION) {
			clockSelectBits = _BV(CS10);
			pwmPeriod = cycles;
		} else if (cycles < TIMER1_RESOLUTION * 8UL) {
			clockSelectBits = _BV(CS11);
			pwmPeriod = cycles / 8;
		} else if (cycles < TIMER1_RESOLUTION * 64UL) {
			clockSelectBits = _BV(CS11) | _BV(CS10);
			pwmPeriod = cycles / 64;
		} else if (cycles < TIMER1_RESOLUTION * 256UL) {
			clockSelectBits = _BV(CS12);
			pwmPeriod = cycles / 256;
		} else if (cycles < TIMER1_RESOLUTION * 1024UL) {
			clockSelectBits = _BV(CS12) | _BV(CS10);
			pwmPeriod = cycles / 1024;
		} else {
			clockSelectBits = _BV(CS12) | _BV(CS10);
			pwmPeriod = TIMER1_RESOLUTION - 1;
		}

		ICR1 = pwmPeriod;
		TCCR1B = _BV(WGM13) | clockSelectBits;
	}

	/* ========================= Run Control ========================= */

	/** @brief Запуск Timer1 (сброс и включение с текущими настройками). */
	void start() __attribute__((always_inline))
	{
		TCCR1B = 0;
		TCNT1 = 0;
		resume();
	}

	/** @brief Остановка Timer1 (режим остаётся, счёт остановлен). */
	void stop() __attribute__((always_inline))
	{
		TCCR1B = _BV(WGM13);
	}

	/** @brief Перезапуск Timer1 (эквивалент start()). */
	void restart() __attribute__((always_inline)) { start(); }

	/** @brief Возобновить счёт Timer1. */
	void resume() __attribute__((always_inline))
	{
		TCCR1B = _BV(WGM13) | clockSelectBits;
	}

	/* ========================= PWM outputs ========================= */

	/**
	 * @brief Установить duty PWM (0..1023) для выбранного канала Timer1.
	 * @param pin Номер вывода (TIMER1_A_PIN / TIMER1_B_PIN / TIMER1_C_PIN).
	 * @param duty Скважность 10-bit.
	 */
	void setPwmDuty(char pin, unsigned int duty) __attribute__((always_inline))
	{
		unsigned long dutyCycle = pwmPeriod;
		dutyCycle *= duty;
		dutyCycle >>= 10;

		if (pin == TIMER1_A_PIN) {
			OCR1A = dutyCycle;
		}
#ifdef TIMER1_B_PIN
		else if (pin == TIMER1_B_PIN) {
			OCR1B = dutyCycle;
		}
#endif
#ifdef TIMER1_C_PIN
		else if (pin == TIMER1_C_PIN) {
			OCR1C = dutyCycle;
		}
#endif
	}

	/**
	 * @brief Включить PWM на выбранном выводе и задать duty.
	 * @param pin Вывод Timer1.
	 * @param duty Скважность (0..1023).
	 */
	void pwm(char pin, unsigned int duty) __attribute__((always_inline))
	{
		if (pin == TIMER1_A_PIN) { pinMode(TIMER1_A_PIN, OUTPUT); TCCR1A |= _BV(COM1A1); }
#ifdef TIMER1_B_PIN
		else if (pin == TIMER1_B_PIN) { pinMode(TIMER1_B_PIN, OUTPUT); TCCR1A |= _BV(COM1B1); }
#endif
#ifdef TIMER1_C_PIN
		else if (pin == TIMER1_C_PIN) { pinMode(TIMER1_C_PIN, OUTPUT); TCCR1A |= _BV(COM1C1); }
#endif
		setPwmDuty(pin, duty);
		TCCR1B = _BV(WGM13) | clockSelectBits;
	}

	/**
	 * @brief Включить PWM с обновлением периода.
	 * @param pin Вывод Timer1.
	 * @param duty Скважность.
	 * @param microseconds Период, мкс (если >0).
	 */
	void pwm(char pin, unsigned int duty, unsigned long microseconds) __attribute__((always_inline))
	{
		if (microseconds > 0) setPeriod(microseconds);
		pwm(pin, duty);
	}

	/**
	 * @brief Отключить PWM на выбранном выводе Timer1.
	 * @param pin Вывод Timer1.
	 */
	void disablePwm(char pin) __attribute__((always_inline))
	{
		if (pin == TIMER1_A_PIN) {
			TCCR1A &= (uint8_t)~_BV(COM1A1);
		}
#ifdef TIMER1_B_PIN
		else if (pin == TIMER1_B_PIN) {
			TCCR1A &= (uint8_t)~_BV(COM1B1);
		}
#endif
#ifdef TIMER1_C_PIN
		else if (pin == TIMER1_C_PIN) {
			TCCR1A &= (uint8_t)~_BV(COM1C1);
		}
#endif
	}

	/* ========================= Interrupt Function ========================= */

	/**
	 * @brief Установить callback прерывания Timer1 (overflow).
	 * @param isr Указатель на функцию.
	 */
	void attachInterrupt(void (*isr)()) __attribute__((always_inline))
	{
		isrCallback = isr;
		TIMSK1 = _BV(TOIE1);
	}

	/**
	 * @brief Установить callback и (опционально) обновить период.
	 * @param isr Указатель на функцию.
	 * @param microseconds Период, мкс (если >0).
	 */
	void attachInterrupt(void (*isr)(), unsigned long microseconds) __attribute__((always_inline))
	{
		if (microseconds > 0) setPeriod(microseconds);
		attachInterrupt(isr);
	}

	/** @brief Отключить прерывание Timer1. */
	void detachInterrupt() __attribute__((always_inline))
	{
		TIMSK1 = 0;
	}

	/** @brief Указатель на текущий ISR callback (вызывается из ISR). */
	static void (*isrCallback)();

	/** @brief Пустой обработчик по умолчанию. */
	static void isrDefaultUnused();

private:
	static unsigned short pwmPeriod;
	static unsigned char clockSelectBits;

#elif defined(__arm__) && defined(CORE_TEENSY)

/* Ветка Teensy сохранена функционально, но оставлена максимально близкой к upstream.
   Для проекта AVR эта секция обычно не компилируется. */

#  if defined(KINETISK)
#    define F_TIMER F_BUS
#  elif defined(KINETISL)
#    define F_TIMER (F_PLL/2)
#  endif

public:
	void initialize(unsigned long microseconds = 1000000) __attribute__((always_inline))
	{
		setPeriod(microseconds);
	}

	void setPeriod(unsigned long microseconds) __attribute__((always_inline))
	{
		const unsigned long cycles = (F_TIMER / 2000000UL) * microseconds;

		if (cycles < TIMER1_RESOLUTION) {
			clockSelectBits = 0;
			pwmPeriod = cycles;
		} else if (cycles < TIMER1_RESOLUTION * 2UL) {
			clockSelectBits = 1;
			pwmPeriod = cycles >> 1;
		} else if (cycles < TIMER1_RESOLUTION * 4UL) {
			clockSelectBits = 2;
			pwmPeriod = cycles >> 2;
		} else if (cycles < TIMER1_RESOLUTION * 8UL) {
			clockSelectBits = 3;
			pwmPeriod = cycles >> 3;
		} else if (cycles < TIMER1_RESOLUTION * 16UL) {
			clockSelectBits = 4;
			pwmPeriod = cycles >> 4;
		} else if (cycles < TIMER1_RESOLUTION * 32UL) {
			clockSelectBits = 5;
			pwmPeriod = cycles >> 5;
		} else if (cycles < TIMER1_RESOLUTION * 64UL) {
			clockSelectBits = 6;
			pwmPeriod = cycles >> 6;
		} else if (cycles < TIMER1_RESOLUTION * 128UL) {
			clockSelectBits = 7;
			pwmPeriod = cycles >> 7;
		} else {
			clockSelectBits = 7;
			pwmPeriod = TIMER1_RESOLUTION - 1;
		}

		uint32_t sc = FTM1_SC;
		FTM1_SC = 0;
		FTM1_MOD = pwmPeriod;
		FTM1_SC = FTM_SC_CLKS(1) | FTM_SC_CPWMS | clockSelectBits | (sc & FTM_SC_TOIE);
	}

	void start() __attribute__((always_inline))
	{
		stop();
		FTM1_CNT = 0;
		resume();
	}

	void stop() __attribute__((always_inline))
	{
		FTM1_SC = FTM1_SC & (FTM_SC_TOIE | FTM_SC_CPWMS | FTM_SC_PS(7));
	}

	void restart() __attribute__((always_inline)) { start(); }

	void resume() __attribute__((always_inline))
	{
		FTM1_SC = (FTM1_SC & (FTM_SC_TOIE | FTM_SC_PS(7))) | FTM_SC_CPWMS | FTM_SC_CLKS(1);
	}

	void setPwmDuty(char pin, unsigned int duty) __attribute__((always_inline))
	{
		unsigned long dutyCycle = pwmPeriod;
		dutyCycle *= duty;
		dutyCycle >>= 10;

		if (pin == TIMER1_A_PIN) {
			FTM1_C0V = dutyCycle;
		} else if (pin == TIMER1_B_PIN) {
			FTM1_C1V = dutyCycle;
		}
	}

	void pwm(char pin, unsigned int duty) __attribute__((always_inline))
	{
		setPwmDuty(pin, duty);
		if (pin == TIMER1_A_PIN) {
			*portConfigRegister(TIMER1_A_PIN) = PORT_PCR_MUX(3) | PORT_PCR_DSE | PORT_PCR_SRE;
		} else if (pin == TIMER1_B_PIN) {
			*portConfigRegister(TIMER1_B_PIN) = PORT_PCR_MUX(3) | PORT_PCR_DSE | PORT_PCR_SRE;
		}
	}

	void pwm(char pin, unsigned int duty, unsigned long microseconds) __attribute__((always_inline))
	{
		if (microseconds > 0) setPeriod(microseconds);
		pwm(pin, duty);
	}

	void disablePwm(char pin) __attribute__((always_inline))
	{
		if (pin == TIMER1_A_PIN) {
			*portConfigRegister(TIMER1_A_PIN) = 0;
		} else if (pin == TIMER1_B_PIN) {
			*portConfigRegister(TIMER1_B_PIN) = 0;
		}
	}

	void attachInterrupt(void (*isr)()) __attribute__((always_inline))
	{
		isrCallback = isr;
		FTM1_SC |= FTM_SC_TOIE;
		NVIC_ENABLE_IRQ(IRQ_FTM1);
	}

	void attachInterrupt(void (*isr)(), unsigned long microseconds) __attribute__((always_inline))
	{
		if (microseconds > 0) setPeriod(microseconds);
		attachInterrupt(isr);
	}

	void detachInterrupt() __attribute__((always_inline))
	{
		FTM1_SC &= (uint32_t)~FTM_SC_TOIE;
		NVIC_DISABLE_IRQ(IRQ_FTM1);
	}

	static void (*isrCallback)();
	static void isrDefaultUnused();

private:
	static unsigned short pwmPeriod;
	static unsigned char clockSelectBits;

#  undef F_TIMER

#endif
};

/** @brief Глобальный экземпляр TimerOne (определение — в TimerOne.cpp). */
extern TimerOne Timer1;

#endif // TIMER_ONE_H_
