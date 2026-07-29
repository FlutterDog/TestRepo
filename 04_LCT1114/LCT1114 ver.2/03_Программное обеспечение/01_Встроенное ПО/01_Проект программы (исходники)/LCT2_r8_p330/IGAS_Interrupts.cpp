#include "IGAS_Interrupts.h"


uint16_t isr_call_counter = 0;				// Счётчик вызовов обработчика прерывания






// Ф-ция настраивает режим Compare Match для Timer2 на частоту 5 000 Гц (каждые 200 мкс):
// Ф-ция настраивает режим Compare Match для Timer2 на частоту 5 000 Гц (каждые 200 мкс):

void TIMER::init(void){
	TCCR2 |= (1<<WGM21);					// Waveform Generation Mode: CTC Mode
	TCCR2 &= ~((1<<COM21)|(1<<COM20));		// Compare Output Mode: OC2 (PB7) pin is disabled
	TCCR2 &= ~(1<<CS22);					// Clock Select: Prescaler sets to /64 mode
	TCCR2 |= (1<<CS21)|(1<<CS20);			// --//--
	OCR2 = 24;								// Compare with 24 to get 100 us period
	//	OCR2 = 49;								// Compare with 49 to get 200 us period
	//	OCR2 = 9;								// Compare with 9 to get 40 us period
	//	OCR2 = 249;								// Compare with 49 to get 1000 us period
	//	OCR2 = 11;								// Compare with 11 to get 48 us period
	//	OCR2 = 16;								// Compare with 68 to get 48 us period
}























// 
// void TIMER::init(void){
// 	TCCR2 |=  (1<<WGM21);
// 	TCCR2 &= ~((1<<COM21)|(1<<COM20));
// 	TCCR2 &= ~((1<<CS21)|(1<<CS20));   // сбросить CS21, CS20
// 	TCCR2 |=  (1<<CS22);               // /64  (CS22=1, CS21=0, CS20=0)
// 
// 	// Выбери период:
// 	// OCR2=24 ? 100 мкс; 49 ? 200 мкс; 9 ? 40 мкс; 249 ? 1000 мкс; 11 ? 48 мкс; 16 ? 68 мкс
// 	OCR2 = 24;                                  // пример: 100 мкс на прерывание
// 
// 	TCNT2 = 0;
// 	TIFR  |= (1<<OCF2);                          // сброс флага
// 	TIMSK |= (1<<OCIE2);                         // разрешить прерывание Compare Match
// }
//
// void TIMER::init(void){
// 	TCCR2 |= (1<<WGM21);					// Waveform Generation Mode: CTC Mode
// 	TCCR2 &= ~((1<<COM21)|(1<<COM20));		// Compare Output Mode: OC2 (PB7) pin is disabled
// 	TCCR2 &= ~(1<<CS22);					// Clock Select: Prescaler sets to /64 mode
// 	TCCR2 |= (1<<CS21)|(1<<CS20);			// --//--
// //	  OCR2 = 24;								// 100 us period
// 	//	OCR2 = 49;								// 200 us period
// 	//	OCR2 = 9;								//  40 us period
// 	//	OCR2 = 249;								//  1000 us period
// 	//	OCR2 = 11;								//  48 us period
// 		OCR2 = 16;								//  68 us period
// }

// void TIMER::init(void) {
// 	TCCR2 |= (1 << WGM21);                 // Set CTC Mode
// 	TCCR2 &= ~((1 << COM21) | (1 << COM20)); // OC2 pin is disabled
// 	TCCR2 |= (1 << CS22) | (1 << CS21) | (1 << CS20); // Set Prescaler to 1024
//
// 	OCR2 = 78;                             // Set OCR2 to 78 for approximately 5 ms period
//
// //	sei();                                 // Enable global interrupts
// }









// Ф-ция сбрасывает счётчик вызова ISR и включает прерывание:
void TIMER::start(void){
	isr_call_counter = 0;
	TIMSK |= (1<<OCIE2);					// Timer2 OCM Interrupt enable
}

// Ф-ция выключает прерывание:
void TIMER::stop(void){
	TIMSK &= ~(1<<OCIE2);					// Timer2 OCM Interrupt disable
}

// Ф-ция, вызываемая обработчиком прерывания:
void TIMER::isr(void){
}
