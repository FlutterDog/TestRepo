#ifndef IGAS_INTERRUPTS_H_
#define IGAS_INTERRUPTS_H_


#include "main.h"


#define ISR_CT_FREQ			3				// How many timer iterations occur between procedure calls
#define ISR_ENCODER_FREQ	5				// How many timer iterations occur between procedure calls

class TIMER
{
	public:
		void init(void);			// Общая инициализация таймера
		void start(void);			// Процедура запуска прерывания ('sei();' нужно выполнять отдельно!)
		void stop(void);			// Процедура останова прерывания
		void isr(void);				// Обработчик прерывания

		uint16_t isr_call_counter;	// Счётчик вызовов обработчика прерывания
	private:
};


#endif /* IGAS_INTERRUPTS_H_ */
