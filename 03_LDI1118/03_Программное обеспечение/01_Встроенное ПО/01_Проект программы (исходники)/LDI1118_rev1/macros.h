#ifndef MACROS_H_
#define MACROS_H_


// Светодиод Arduino.
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

#endif /* MACROS_H_ */