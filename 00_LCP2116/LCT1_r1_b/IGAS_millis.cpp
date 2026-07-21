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


void millis_init(void){
	OCR1A = ((F_CPU / 1000) / 8);				// When Timer1 is this value, 1 ms has passed
	TCCR1B |= (1 << WGM12) | (1 << CS11);		// Clear the Timer when matching, Clock Divisor is 8
	TIMSK |= (1 << OCIE1A);						// Enable the Compare Match Interrupt

/*
	uint32_t ctc_match_overflow;

	ctc_match_overflow = ((F_CPU / 1000) / 8); //when timer1 is this value, 1ms has passed

	// (Set timer to clear when matching ctc_match_overflow) | (Set clock divisor to 8)
	TCCR1B |= (1 << WGM12) | (1 << CS11);

	// high byte first, then low byte
	OCR1AH = (ctc_match_overflow >> 8);
	OCR1AL = ctc_match_overflow;
//	OCR1A = ctc_match_overflow;

	// Enable the compare match interrupt
	TIMSK |= (1 << OCIE1A);

	// sei();

	//REMEMBER TO ENABLE GLOBAL INTERRUPTS AFTER THIS WITH sei(); !!!
*/
}


uint32_t millis(void){
	return milliseconds;
/*
//	uint32_t millis_return;
//
//	// Ensure this cannot be disrupted
//	ATOMIC_BLOCK(ATOMIC_FORCEON) {
//		millis_return = milliseconds;
//	}
//	return millis_return;
*/
}


ISR(TIMER1_COMPA_vect){
	milliseconds++;
}