/*
HardwareSerial.cpp - Hardware serial library for Wiring
Copyright (c) 2006 Nicholas Zambetti.  All right reserved.

Modified 23 November 2006 by David A. Mellis
Modified 28 September 2010 by Mark Sproul
Modified 14 August 2012 by Alarus
*/

#include "main.h"
//#include <stdlib.h>
//#include <stdio.h>
//#include <string.h>
//#include <inttypes.h>


// Serial_1
ring_buffer rx_buffer1  =  { { 0 }, 0, 0 };
ring_buffer tx_buffer1  =  { { 0 }, 0, 0 };

/*
// Serial_0
// ring_buffer rx_buffer = { { 0 }, 0, 0};
// ring_buffer tx_buffer = { { 0 }, 0, 0};

// Serial_2
// ring_buffer rx_buffer2  =  { { 0 }, 0, 0 };
// ring_buffer tx_buffer2  =  { { 0 }, 0, 0 };

// Serial_3
// ring_buffer rx_buffer3  =  { { 0 }, 0, 0 };
// ring_buffer tx_buffer3  =  { { 0 }, 0, 0 };
*/


inline void store_char(unsigned char c, ring_buffer *buffer)
{
	unsigned int i = (unsigned int)(buffer->head + 1) % SERIAL_BUFFER_SIZE;

	// if we should be storing the received character into the location
	// just before the tail (meaning that the head would advance to the
	// current location of the tail), we're about to overflow the buffer
	// and so we don't write the character or advance the head.
	if (i != buffer->tail) {
		buffer->buffer[buffer->head] = c;
		buffer->head = i;
	}
}


// Serial_0 ISR Start
/*
ISR(USART0_RX_vect)
{
unsigned char c;
if (bit_is_clear(UCSR0A, UPE0)) {
c = UDR0;
store_char(c, &rx_buffer);
} else {
c = UDR0;
};

}

*/
// Serial_0 ISR   Fin


// Serial_1 ISR Start
ISR(USART1_RX_vect)
{
	unsigned char c = UDR1;
	if (bit_is_clear(UCSR1A, UPE1)) store_char(c, &rx_buffer1);

	// 	if (bit_is_clear(UCSR1A, UPE1)) {
	// 		unsigned char c = UDR1;
	// 		store_char(c, &rx_buffer1);
	// 		} else {
	// 		unsigned char c = UDR1;
	// 	};
}
// Serial_1 ISR Fin


// Serial_2 ISR Start
/*

ISR(USART2_RX_vect)
{
if (bit_is_clear(UCSR2A, UPE2)) {
unsigned char c = UDR2;
store_char(c, &rx_buffer2);
} else {
unsigned char c = UDR2;
};
}
// Serial_2 ISR   Fin




// Serial_3 ISR Start

ISR(USART3_RX_vect)
{
if (bit_is_clear(UCSR3A, UPE3)) {
unsigned char c = UDR3;
store_char(c, &rx_buffer3);
} else {
unsigned char c = UDR3;
};
}
*/
// Serial_3 ISR   Fin


/*
ISR(USART0_UDRE_vect)
{
if (tx_buffer.head == tx_buffer.tail) {
// Buffer empty, so disable interrupts
cbi(UCSR0B, UDRIE0);
}
else {
// There is more data in the output buffer. Send the next byte
unsigned char c = tx_buffer.buffer[tx_buffer.tail];
tx_buffer.tail = (tx_buffer.tail + 1) % SERIAL_BUFFER_SIZE;

UDR0 = c;

}
}
*/


ISR(USART1_UDRE_vect)
{
	if (tx_buffer1.head == tx_buffer1.tail) {
		// Buffer empty, so disable interrupts
		cbi(UCSR1B, UDRIE1);
	}
	else {
		// There is more data in the output buffer. Send the next byte
		unsigned char c = tx_buffer1.buffer[tx_buffer1.tail];
		tx_buffer1.tail = (tx_buffer1.tail + 1) % SERIAL_BUFFER_SIZE;

		UDR1 = c;
	}
}


/*
ISR(USART2_UDRE_vect)
{
if (tx_buffer2.head == tx_buffer2.tail) {
// Buffer empty, so disable interrupts
cbi(UCSR2B, UDRIE2);
}
else {
// There is more data in the output buffer. Send the next byte
unsigned char c = tx_buffer2.buffer[tx_buffer2.tail];
tx_buffer2.tail = (tx_buffer2.tail + 1) % SERIAL_BUFFER_SIZE;

UDR2 = c;
}
}




ISR(USART3_UDRE_vect)
{
if (tx_buffer3.head == tx_buffer3.tail) {
// Buffer empty, so disable interrupts
cbi(UCSR3B, UDRIE3);
}
else {
// There is more data in the output buffer. Send the next byte
unsigned char c = tx_buffer3.buffer[tx_buffer3.tail];
tx_buffer3.tail = (tx_buffer3.tail + 1) % SERIAL_BUFFER_SIZE;

UDR3 = c;
}
}


*/


void HardwareSerial::begin(unsigned long baud)
{
	uint16_t baud_setting;
	bool use_u2x = true;

	#if F_CPU == 16000000UL
	// hardcoded exception for compatibility with the bootloader shipped
	// with the Duemilanove and previous boards and the firmware on the 8U2
	// on the Uno and Mega 2560.
	if (baud == 57600) {
		use_u2x = false;
	}
	#endif

	try_again:

	if (use_u2x) {
		*_ucsra = 1 << _u2x;
		baud_setting = (F_CPU / 4 / baud - 1) / 2;
		} else {
		*_ucsra = 0;
		baud_setting = (F_CPU / 8 / baud - 1) / 2;
	}

	if ((baud_setting > 4095) && use_u2x)
	{
		use_u2x = false;
		goto try_again;
	}

	// assign the baud_setting, a.k.a. ubbr (USART Baud Rate Register)
	*_ubrrh = baud_setting >> 8;
	*_ubrrl = baud_setting;

	transmitting = false;

	sbi(*_ucsrb, _rxen);
	sbi(*_ucsrb, _txen);
	sbi(*_ucsrb, _rxcie);
	cbi(*_ucsrb, _udrie);
	
	PORTD &= ~(1 << TX_EN);
}

void HardwareSerial::begin(unsigned long baud, uint8_t config)
{
	uint16_t baud_setting;
	//uint8_t current_config;
	bool use_u2x = true;

	#if F_CPU == 16000000UL
	// hardcoded exception for compatibility with the bootloader shipped
	// with the Duemilanove and previous boards and the firmware on the 8U2
	// on the Uno and Mega 2560.
	if (baud == 57600) {
		use_u2x = false;
	}
	#endif

	try_again:

	if (use_u2x) {
		*_ucsra = 1 << _u2x;
		baud_setting = (F_CPU / 4 / baud - 1) / 2;
		} else {
		*_ucsra = 0;
		baud_setting = (F_CPU / 8 / baud - 1) / 2;
	}

	if ((baud_setting > 4095) && use_u2x)
	{
		use_u2x = false;
		goto try_again;
	}

	// assign the baud_setting, a.k.a. ubbr (USART Baud Rate Register)
	*_ubrrh = baud_setting >> 8;
	*_ubrrl = baud_setting;
	*_ucsrc = config;

	sbi(*_ucsrb, _rxen);
	sbi(*_ucsrb, _txen);
	sbi(*_ucsrb, _rxcie);
	cbi(*_ucsrb, _udrie);
	
	PORTD &= ~(1 << TX_EN);
}

// void HardwareSerial::end()
// {
// 	// wait for transmission of outgoing data
// 	while (_tx_buffer->head != _tx_buffer->tail)
// 	;
//
// 	cbi(*_ucsrb, _rxen);
// 	cbi(*_ucsrb, _txen);
// 	cbi(*_ucsrb, _rxcie);
// 	cbi(*_ucsrb, _udrie);
//
// 	// clear any received data
// 	_rx_buffer->head = _rx_buffer->tail;
// }

void HardwareSerial::end()
{
	cli();  // Disable global interrupts

	// Disable TX, RX, and all USART1 interrupts
	UCSR1B = 0;
	UCSR1A = 0;
	UCSR1C = 0;
	
	PORTD &= ~(1 << TX_EN);
	// Flush any remaining bytes in TX and RX buffers
	_tx_buffer->head = 0;
	_tx_buffer->tail = 0;
	_rx_buffer->head = 0;
	_rx_buffer->tail = 0;

	sei();  // Re-enable global interrupts
}



int HardwareSerial::available(void)
{
	return (int)(SERIAL_BUFFER_SIZE + _rx_buffer->head - _rx_buffer->tail) % SERIAL_BUFFER_SIZE;
}

int HardwareSerial::peek(void)
{
	if (_rx_buffer->head == _rx_buffer->tail) {
		return -1;
		} else {
		return _rx_buffer->buffer[_rx_buffer->tail];
	}
}

int HardwareSerial::read(void)
{
	// if the head isn't ahead of the tail, we don't have any characters
	if (_rx_buffer->head == _rx_buffer->tail) {
		return -1;
		} else {
		unsigned char c = _rx_buffer->buffer[_rx_buffer->tail];
		_rx_buffer->tail = (unsigned int)(_rx_buffer->tail + 1) % SERIAL_BUFFER_SIZE;
		return c;
	}
}

void HardwareSerial::flush()
{
	// UDR is kept full while the buffer is not empty, so TXC triggers when EMPTY && SENT
	while (transmitting && ! (*_ucsra & _BV(TXC0)));
	transmitting = false;
}

uint8_t HardwareSerial::write(uint8_t c)
{
	unsigned int i = (_tx_buffer->head + 1) % SERIAL_BUFFER_SIZE;

	// If the output buffer is full, there's nothing for it other than to
	// wait for the interrupt handler to empty it a bit
	// ???: return 0 here instead?
	while (i == _tx_buffer->tail)
	;

	_tx_buffer->buffer[_tx_buffer->head] = c;
	_tx_buffer->head = i;

	sbi(*_ucsrb, _udrie);
	// clear the TXC bit -- "can be cleared by writing a one to its bit location"
	transmitting = true;
	sbi(*_ucsra, TXC0);

	return 1;
}


HardwareSerial::HardwareSerial(
ring_buffer *rx_buffer,
ring_buffer *tx_buffer,
volatile uint8_t *ubrrh,
volatile uint8_t *ubrrl,
volatile uint8_t *ucsra,
volatile uint8_t *ucsrb,
volatile uint8_t *ucsrc,
volatile uint8_t *udr,
uint8_t rxen,
uint8_t txen,
uint8_t rxcie,
uint8_t udrie,
uint8_t u2x)
{
	_rx_buffer	= rx_buffer;
	_tx_buffer	= tx_buffer;
	_ubrrh		= ubrrh;
	_ubrrl		= ubrrl;
	_ucsra		= ucsra;
	_ucsrb		= ucsrb;
	_ucsrc		= ucsrc;
	_udr		= udr;
	_rxen		= rxen;
	_txen		= txen;
	_rxcie		= rxcie;
	_udrie		= udrie;
	_u2x		= u2x;
}


HardwareSerial Serial1(&rx_buffer1, &tx_buffer1, &UBRR1H, &UBRR1L, &UCSR1A, &UCSR1B, &UCSR1C, &UDR1, RXEN1, TXEN1, RXCIE1, UDRIE1, U2X1);
//HardwareSerial Serial(&rx_buffer, &tx_buffer, &UBRRH, &UBRRL, &UCSRA, &UCSRB, &UCSRC, &UDR, RXEN, TXEN, RXCIE, UDRIE, U2X);
//HardwareSerial Serial(&rx_buffer, &tx_buffer, &UBRR0H, &UBRR0L, &UCSR0A, &UCSR0B, &UCSR0C, &UDR0, RXEN0, TXEN0, RXCIE0, UDRIE0, U2X0);
//HardwareSerial Serial2(&rx_buffer2, &tx_buffer2, &UBRR2H, &UBRR2L, &UCSR2A, &UCSR2B, &UCSR2C, &UDR2, RXEN2, TXEN2, RXCIE2, UDRIE2, U2X2);
//HardwareSerial Serial3(&rx_buffer3, &tx_buffer3, &UBRR3H, &UBRR3L, &UCSR3A, &UCSR3B, &UCSR3C, &UDR3, RXEN3, TXEN3, RXCIE3, UDRIE3, U2X3);
