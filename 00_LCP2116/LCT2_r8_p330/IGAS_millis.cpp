/*
* Project: Lightweight millisecond tracking library
* Author: Zak Kemble, contact@zakkemble.net
* Copyright: (C) 2018 by Zak Kemble
* License: GNU GPL v3 (see License_GPL-3.0.txt) or MIT (see License_MIT.txt)
* Web: http://blog.zakkemble.net/millisecond-tracking-library-for-avr/
*/

// Here we're using C code in a .cpp file so Arduino compiles with the correct settings (mainly C99)

#include "IGAS_millis.h"


#ifndef F_CPU
#error "F_CPU not defined!"
#endif

// "REMEMBER TO ENABLE GLOBAL INTERRUPTS AFTER THIS WITH sei()"

volatile uint32_t milliseconds;

// 
// void millis_init(void){
// 	OCR1A = ((F_CPU / 1000) / 8);				// When Timer1 is this value, 1 ms has passed
// 	TCCR1B |= (1 << WGM12) | (1 << CS11);		// Clear the Timer when matching, Clock Divisor is 8
// 	TIMSK |= (1 << OCIE1A);						// Enable the Compare Match Interrupt
// }

/** @brief Init millis() םא Timer1 (CTC, 1לס) */
void millis_init(void) {
	TCCR1A = 0;
	TCCR1B = 0;
	TCNT1  = 0;

	OCR1A = (uint16_t)((F_CPU / 8UL) / 1000UL - 1UL); // ןנט 16 ÌÃצ ? 1999
	TCCR1B = (1 << WGM12) | (1 << CS11); // CTC, /8
	TIMSK |= (1 << OCIE1A);              // interrupt enable
}


uint32_t millis(void){
	uint32_t m;
	ATOMIC_BLOCK(ATOMIC_FORCEON) {
		m = milliseconds;
	}
	return m;
}

ISR(TIMER1_COMPA_vect){
	milliseconds++;
}
