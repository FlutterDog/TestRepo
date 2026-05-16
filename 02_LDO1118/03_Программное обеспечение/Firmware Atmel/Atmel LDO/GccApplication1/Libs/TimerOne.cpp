#include "../Libs/TimerOne.h"

#include <avr/interrupt.h>

/**
 * @file TimerOne.cpp
 * @brief Реализация обработчика прерывания Timer1 и привязка пользовательского callback.
 *
 * @details
 * Файл содержит:
 * - глобальный экземпляр таймера ::Timer1 (класс TimerOne);
 * - статические поля TimerOne (период PWM, выбранный предделитель, указатель callback);
 * - ISR, который делегирует обработку в TimerOne::isrCallback().
 *
 * Примечания по архитектуре:
 * - Для классических AVR используется прерывание переполнения Timer1 (TIMER1_OVF_vect),
 *   если иное не задано конкретной платформой/реализацией TimerOne.h.
 * - Для ATtiny85 используется сравнение (TIMER1_COMPA_vect), как это принято в легаси-портах.
 *
 * @note
 * Пользовательский обработчик устанавливается через TimerOne::attachInterrupt()
 * (реализация — в TimerOne.h / TimerOne.cpp проекта, в зависимости от версии библиотеки).
 */

/// @brief Глобальный экземпляр таймера TimerOne (используется проектом как Timer1).
TimerOne Timer1; // pre-instantiate

/// @brief Период PWM (тика Timer1), задаётся конфигурацией TimerOne.
unsigned short TimerOne::pwmPeriod = 0;

/// @brief Биты выбора предделителя/источника тактирования (CS12..CS10), зависят от конфигурации.
unsigned char TimerOne::clockSelectBits = 0;

/// @brief Указатель на callback, вызываемый из ISR.
void (*TimerOne::isrCallback)() = TimerOne::isrDefaultUnused;

/* ========================= ISR-обвязка ========================= */

#if defined(__AVR_ATtiny85__)

/**
 * @brief ISR Timer1 compare match A (ATtiny85).
 * @details Делегирует выполнение в TimerOne::isrCallback().
 */
ISR(TIMER1_COMPA_vect)
{
	Timer1.isrCallback();
}

#elif defined(__AVR__)

/**
 * @brief ISR Timer1 overflow (классические AVR).
 * @details Делегирует выполнение в TimerOne::isrCallback().
 */
ISR(TIMER1_OVF_vect)
{
	Timer1.isrCallback();
}

#elif defined(__arm__) && defined(CORE_TEENSY)

/* Секция для Teensy оставлена для совместимости исходной библиотеки. */
void ftm1_isr(void)
{
	uint32_t sc = FTM1_SC;
#  ifdef KINETISL
	if (sc & 0x80) FTM1_SC = sc;
#  else
	if (sc & 0x80) FTM1_SC = sc & 0x7F;
#  endif
	Timer1.isrCallback();
}

#endif

/**
 * @brief Callback по умолчанию, если пользователь не установил обработчик.
 * @details Намеренно пустая функция; позволяет вызывать isrCallback() без проверки на nullptr.
 */
void TimerOne::isrDefaultUnused()
{
	/* no-op */
}
