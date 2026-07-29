#ifndef SNB_MACROS_H_
#define SNB_MACROS_H_


// Светодиод Arduino.
#define LED_ENA		set_pin_output(DDRB, PINB5); set_pin_low(PORTB, PINB5)
#define LED_ON		set_pin_high(PORTB, PINB5)
#define LED_OFF		set_pin_low(PORTB, PINB5)


// Макросы для работы с битами:
#define	bit_set(x,bit)					((x) |=  (1 << bit))
#define	bit_clr(x,bit)					((x) &= ~(1 << bit))
#define bit_tgl(x,bit)					((x) ^=  (1 << bit))

#define bits_set(x,bit1,bit2)			((x) |=   (1 << bit1) | (1 << bit2))
#define bits_clr(x,bit1,bit2)			((x) &= ~((1 << bit1) | (1 << bit2)))
#define bits_tgl(x,bit1,bit2)			((x) ^=  ((1 << bit1) | (1 << bit2)))

#define is_bit_1(x,bit)					(((x) & (1 << bit)) != 0)
#define is_bit_0(x,bit)					(((x) & (1 << bit)) == 0)


// Макросы для работы с портами ввода-вывода:
#define	set_pin_input(port,bit)			bit_clr(port,bit)
#define	set_pin_output(port,bit)		bit_set(port,bit)
#define	set_pin_high(port,bit)			bit_set(port,bit)
#define	set_pin_low(port,bit)			bit_clr(port,bit)
#define	set_pin_tgl(port,bit)			bit_tgl(port,bit)
#define	pullup_on(port,bit)				bit_set(port,bit)
#define	pullup_off(port,bit)			bit_clr(port,bit)


// Макросы флаговых циклов:
#define while_bit_is_1(x,bit)			while(is_bit_1(x,bit))
#define while_bit_is_0(x,bit)			while(is_bit_0(x,bit))
#define WAIT_SPI_TRANSFER_B				while_bit_is_0(SPSR, SPIF)
#define WAIT_SPI_TRANSFER_TIMEOUT 1000  // Максимальное количество проверок

#define bit_is_0(reg, bit) (!(reg & (1 << bit)))
#define bit_is_1(reg, bit) (reg & (1 << bit))

#define WAIT_SPI_TRANSFER do { \
	uint16_t timeout = WAIT_SPI_TRANSFER_TIMEOUT; \
	while (bit_is_0(SPSR, SPIF) && --timeout); \
	if (timeout == 0) { \
		return; \
	} \
} while (0)





// Макросы преобразования чисел в текстовом виде:
#define dec2ascii(dec)					((dec)+0x30)
#define hex2ascii(hex)					(((hex)>0x09) ? ((hex) + 0x37): ((hex) + 0x30))
#define ascii2dec(asc)					((asc)-0x30)
#define ascii2hex(asc)					(((asc)>0x39) ? ((asc) - 0x37): ((asc) - 0x30))


// Макросы комманд:
#define nop()							asm volatile ("nop;")


// ===================================================================================
// Чужие наработки
// ===================================================================================
#define get_bits_0to7(x)				(u8)((x) & 0xFF)			// 0xFFu - так в оригинале - ПРОВЕРИТЬ РАБОТОСПОСОБНОСТЬ!!!
#define get_bits_8to15(x)				(u8)(((x) >> 8)  & 0xFF)	// 0xFFu - так в оригинале
#define	get_bits_16to28(x)				(u8)(((x) >> 16) & 0xFF)	// 0xFFu - так в оригинале
#define	get_bits_28to32(x)				(u8)(((x) >> 28) & 0xFF)	// 0xFFu - так в оригинале


// Macros to extract the nibbles:
#define get_nibble_0to3(x)				(u8)((x) & 0x0F)			// 0xFFu - так в оригинале - ПРОВЕРИТЬ РАБОТОСПОСОБНОСТЬ!!!
#define get_nibble_4to7(x)				(u8)(((x) >> 4)  & 0x0F)	// 0xFFu - так в оригинале
#define get_nibble_8to11(x)				(u8)(((x) >> 8)  & 0x0F)	// 0xFFu - так в оригинале
#define get_nibble_12to15(x)			(u8)(((x) >> 12) & 0x0F)	// 0xFFu - так в оригинале


#define getbitmask(bit)					(1 << bit)
#define getbitstatus(x,bit)				(((x) & (getbitmask(bit))) != 0)
#define bit_upd(x,bit,val)				((val) ? bit_set(x,bit) : bit_clr(x,bit))


#define	get_mod8(dividend,divisor)		(u8) (dividend - (divisor * (u8) (dividend/divisor)))
#define	get_mod16(dividend,divisor)		(u16)(dividend - (divisor * (u16)(dividend/divisor)))
#define	get_mod32(dividend,divisor)		(u32)(dividend - (divisor * (u32)(dividend/divisor)))

// #include "avr/sfr_defs.h" и там:
// #define _MMIO_BYTE(mem_addr)			(*(volatile u8 *)(mem_addr))
// #define _SFR_BYTE(sfr)					_MMIO_BYTE(_SFR_ADDR(sfr))


/*

Операция инвертирования порядка следования битов в байте будет выглядеть так:

u8 b = 0b10010011;

u8 result = (((((((0 | (((b >> 7) & 1) << 0)) |
(((b >> 6) & 1) << 1)) |
(((b >> 5) & 1) << 2)) |
(((b >> 4) & 1) << 3)) |
(((b >> 3) & 1) << 4)) |
(((b >> 2) & 1) << 5)) |
(((b >> 1) & 1) << 6)) |
(((b >> 0) & 1) << 7);    // 0b11001001


Или, если упростить и обернуть в функцию:

u8 invertBitOrder(byte b) {
	u8 result = 0;
	for (u8 i = 0; i <= 7; i++) {
		result = result | (((b >> (7 - i)) & 1) << i);
	}
	return result;
}

// использование:
u8 b = B10010011;
Serial.println(invertBitOrder(b), BIN);  // 0b11001001

*/


#endif /* SNB_MACROS_H_ */