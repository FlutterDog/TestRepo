#ifndef MAIN_H_
#define MAIN_H_


// Частота работы микропроцессора:
#define F_CPU				16000000UL

// Скорость UART:
#define BAUDRATE			9600

// Количество цифровых входов:
#define INPUT_QTY			16


#define EEP_MB_ADDR			0				// Номер ячейки EEPROM, хранящей Modbus-адрес устройства.
#define EEP_DEV_SN			1				// Номер ячейки EEPROM, хранящей серийный номер устройства.
#define EEP_DEV_HW_VER		3				// Аппаратная ревизия устройства (4149).
#define DEV_SW_VER			1				// Версия ПО (прошивки).
#define DEV_ID				1118			// Номер модуля.

#define MAX_REGISTER_ID		2001			// Максимально возможный номер регистра Modbus.
#define LOOP_TIME			500				// Длина цикла, мс.
#define CONN_TIMEOUT		10000			// Таймаут с момента последнего принятого по RS485 пакета, адресованного этому модулю.


/*
// Параметры Modbus-сообщения:
#define MB_MSG_ADDR		0				// Позиция байта, содержащего Slave Address.
#define MB_MSG_CMD		1				// Позиция байта, содержащего номер команды.
#define MB_MSG_REG_FROM	2				// Позиция слова, содержащего адрес начального регистра.
#define MB_MSG_REG_QTY	4				// Позиция слова, содержащего количество регистров.
#define MB_MSG_CMD3_CRC	6				// Позиция слова, содержащего CRC16 для команды 03 - "Read Holding Registers".
*/


// Обязательные заголовочные файлы:
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include "definitions.h"
#include "macros.h"

// Стандартные определения:


// Порты ввода-вывода:
#define	DDR_ISP				DDRB			// Порт ISP-программатора.
#define	PORT_ISP			PORTB
#define	SS					PINB2
#define	MOSI				PINB3
#define	MISO				PINB5
#define	SCK					PINB5

#define DDR_ADDR			DDRC			// Задатчик Modbus-адреса (DIP-переключатель на плате).
#define PORT_ADDR			PORTC
#define PIN_ADDR			PINC
#define PIN_ADDR0			PINC0
#define PIN_ADDR1			PINC1
#define PIN_ADDR2			PINC2
#define PIN_ADDR3			PINC3
#define PIN_ADDR4			PINC4
#define ADDR_MASK			0b00011111		// Переключатель установки Modbus-адреса подключен на выводы 0...4 порта ADDR. Выводы 5...7 не используются, поэтому маскируются.

#define	DDR_485				DDRD			// Интерфейс RS-485.
#define	PORT_485			PORTD
#define	RX					PIND0
#define	TX					PIND1
#define	TX_EN				PIND2

#define	DDR_LED				DDRB			// Сдвиговый регистр 165 для индикации.
#define	PORT_LED			PORTB
#define	LED_X2X				PINB0			// Индикактор X2X подключен напрямую к МК, минуя сдвиговый регистр.
#define	SR_LED				PINB1			
#define SR_LED_MASK			(1 << SR_LED)

#define	DDR_SR				DDRD			// Сдвиговый регистр 595 для цифровых входов.
#define	PORT_SR				PORTD
#define	PIN_SR				PIND
#define	SR_LATCH			PIND3
#define	SR_CLK				PIND4
#define	SR_DI				PIND5
#define	SR_DI_MASK			(1 << SR_DI)
//#define	SR_ADDR			PIND6			// НЕ ИСПОЛЬЗУЕТСЯ!


// Служебные макросы:
#define sr_latch_595()		set_pin_high(PORT_SR, SR_LATCH); set_pin_low(PORT_SR, SR_LATCH)
#define sr_clock()			set_pin_high(PORT_SR, SR_CLK); set_pin_low(PORT_SR, SR_CLK)
#define sr_latch_165()		set_pin_low(PORT_SR, SR_LATCH); set_pin_high(PORT_SR, SR_LATCH)
#define sr_clock_165()		set_pin_low(PORT_SR, SR_CLK); set_pin_high(PORT_SR, SR_CLK)
#define sr_latch_low()		set_pin_low(PORT_SR, SR_LATCH)
#define sr_latch_high()		set_pin_high(PORT_SR, SR_LATCH)
#define sr_led_low()		set_pin_low(PORT_LED, SR_LED)
#define sr_led_high()		set_pin_high(PORT_LED, SR_LED)
#define rs485_tx_off()		set_pin_low(PORT_485, TX_EN)
#define rs485_tx_on()		set_pin_high(PORT_485, TX_EN)
#define rs485_delay()		_delay_us(850)
#define LED_X2X_ON()		set_pin_high(PORT_LED, LED_X2X)
#define LED_X2X_OFF()		set_pin_low(PORT_LED, LED_X2X)
#define LED_X2X_TGL()		set_pin_tgl(PORT_LED, LED_X2X)

#define reboot()			asm volatile ("ldi r30,0\r\n ldi r31,0\r\n icall");

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))

// Дополнительные заголовочные файлы:
#include "HardwareSerial.h"
#include "IGAS_485.h"


#endif /* MAIN_H_ */