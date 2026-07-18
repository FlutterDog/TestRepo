
#ifndef SC16IS7XX_H
#define SC16IS7XX_H

#include <Arduino.h>

#include <SPI.h>
#include <pins_arduino.h>


// SC16IS750 Register definitions
#define THR        0x00 << 3
#define RHR        0x00 << 3
#define IER        0x01 << 3
#define FCR        0x02 << 3
#define IIR        0x02 << 3
#define LCR        0x03 << 3
#define MCR        0x04 << 3
#define LSR        0x05 << 3
#define MSR        0x06 << 3
#define SPR        0x07 << 3
#define TXLVL      0x08 << 3
#define RXLVL      0x09 << 3
#define DLAB       0x80 << 3
#define IODIR      0x0A << 3
#define IOSTATE    0x0B << 3
#define IOINTMSK   0x0C << 3
#define IOCTRL     0x0E << 3
#define EFCR       0x0F << 3

#define DLL        0x00 << 3
#define DLM        0x01 << 3
#define EFR        0x02 << 3
#define XON1       0x04 << 3
#define XON2       0x05 << 3
#define XOFF1      0x06 << 3
#define XOFF2      0x07 << 3


// Chapter 11 of datasheet
#define SPI_READ_MODE_FLAG 0x80

#define BAUD_RATE_DEFAULT 9600 // Default baudrate

#define   CH_A    0x00
#define   CH_B    0x01
#define   SC16IS752_CHANNEL_BOTH 0x00


class SpiUartDevice
{

	public:
	void begin(unsigned long baudrate = BAUD_RATE_DEFAULT, byte parityMode = 0, byte chip_select = 53, uint8_t channel = 0);
	int available(byte chip_select, uint8_t channel);
	int read(byte chip_select, uint8_t channel);
	void write(byte value, byte chip_select, uint8_t channel);
	void flush(byte chip_select, uint8_t channel);

	// These are specific to the SPI UART
	void ioSetDirection(unsigned char bits, byte chip_select, uint8_t channel);
	void ioSetState(unsigned char bits, byte chip_select, uint8_t channel);
	void deselect(byte chip_select);
	void select(byte chip_select);
	void writeRegister(byte registerAddress, byte data, byte chip_select, uint8_t channel);
	byte readRegister(byte registerAddress,byte chip_select, uint8_t channel);
	void initUart(unsigned long baudrate, byte parityMode, byte chip_select, uint8_t channel);
	void configureUart(unsigned long baudrate, byte parityMode, byte chip_select, uint8_t channel);
	void setBaudRate(unsigned long baudrate,  byte chip_select, uint8_t channel);
	boolean uartConnected(byte chip_select,uint8_t channel);
};

#endif

