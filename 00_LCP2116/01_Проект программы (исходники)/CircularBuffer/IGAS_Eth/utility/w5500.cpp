/*
* Copyright (c) 2010 by WIZnet <support@wiznet.co.kr>
*
* This file is free software; you can redistribute it and/or modify
* it under the terms of either the GNU General Public License version 2
* or the GNU Lesser General Public License version 2.1, both as
* published by the Free Software Foundation.
*
* - 10 Apr. 2015
* Added support for Arduino Ethernet Shield 2
* by Arduino.org team
*/

#include <stdio.h>
#include <string.h>
#include "Arduino.h"

#include "utility/w5500.h"
//#if defined(W5500_ETHERNET_SHIELD)

// W5500 controller instance
W5500Class w5500;

#define SPI_CS 10
#define slaveSelectPin 10
SPISettings wiznet_SPI_settings(40000000, MSBFIRST, SPI_MODE0);

uint8_t W5500Class::slaveSelect = 10;


void W5500Class::selectSS(uint8_t _ss)
{
	slaveSelect = _ss;
}


void W5500Class::initSS(void)
{
	pinMode(slaveSelect, OUTPUT);
	digitalWrite(slaveSelect, HIGH);
}

void W5500Class::setSS(void)
{
	digitalWrite(slaveSelect, LOW);
}

void W5500Class::resetSS(void)
{
	digitalWrite(slaveSelect, HIGH);
}

void W5500Class::init(void)
{
	byte const w5500DebugMode = 0;

	delay(1000);

	initSS();
	SPI.begin();
	w5500.swReset();
	
	if (w5500DebugMode)
	SerialUSB.print("setPHYCFGR C8 -> ");
	setPHYCFGR(B11111000);	// 	All capable, Auto-negotiation enabled
	//	setPHYCFGR(B11001000);	// 10MBit				 // Preset Link Settings
	if (w5500DebugMode)
	SerialUSB.println(getPHYCFGR());
	
	delay(100);
	
	if (w5500DebugMode)
	SerialUSB.print("Reset 48 -> ");
	setPHYCFGR(B01111000);	// 	All capable, Auto-negotiation enabled
	//setPHYCFGR(B01001000);// 10MBit
	if (w5500DebugMode)				// Reset W5500
	SerialUSB.println(getPHYCFGR());
	
	delay(100);

	if (w5500DebugMode)
	SerialUSB.print("After PHY reset 1  -> ");
	setPHYCFGR(B11111000);	// 	All capable, Auto-negotiation enabled
	//setPHYCFGR(B11001000);			// 10MBit Full Duplex
	
	if (w5500DebugMode)
	SerialUSB.println(getPHYCFGR());
	
	for (int i=0; i<MAX_SOCK_NUM; i++)
	{
		//	w5500.execCmdSn(i,Sock_OPEN);
		//	w5500.execCmdSn(i,Sock_LISTEN);
		w5500.execCmdSn(i,Sock_CLOSE);
	}

	for (int i=0; i<MAX_SOCK_NUM; i++) {
		uint8_t cntl_byte = (0x0C + (i<<5));
		write( 0x1E, cntl_byte, 2); //0x1E - Sn_RXBUF_SIZE
		write( 0x1F, cntl_byte, 2); //0x1F - Sn_TXBUF_SIZE
	}
}


uint16_t W5500Class::getTXFreeSize(SOCKET s)
{
	unsigned long watchD = micros();
	uint16_t val=0, val1=0;

	do {
		val1 = readSnTX_FSR(s);
		if (val1 != 0)
		val = readSnTX_FSR(s);
	}
	while ((val != val1) && (micros() - watchD < 5000));
	return val;
}


uint16_t W5500Class::getRXReceivedSize(SOCKET s)
{
	uint16_t val=0,val1=0;
	unsigned long watchD = micros();
	do {
		val1 = readSnRX_RSR(s);
		if (val1 != 0)
		val = readSnRX_RSR(s);
		
		//SerialUSB.println("getRXReceivedSize");
	}
	while ((val != val1) && (micros() - watchD < 5000));
	return val;
}

void W5500Class::send_data_processing(SOCKET s, const uint8_t *data, uint16_t len)
{
	// This is same as having no offset in a call to send_data_processing_offset
	send_data_processing_offset(s, 0, data, len);

}

void W5500Class::send_data_processing_offset(SOCKET s, uint16_t data_offset, const uint8_t *data, uint16_t len)
{

	uint16_t ptr = readSnTX_WR(s);
	uint8_t cntl_byte = (0x14+(s<<5));
	ptr += data_offset;
	write(ptr, cntl_byte, data, len);
	ptr += len;
	writeSnTX_WR(s, ptr);
	
}

void W5500Class::recv_data_processing(SOCKET s, uint8_t *data, uint16_t len, uint8_t peek)
{
	uint16_t ptr;
	ptr = readSnRX_RD(s);

	read_data(s, ptr, data, len);
	if (!peek)
	{
		ptr += len;
		writeSnRX_RD(s, ptr);
	}
}

void W5500Class::read_data(SOCKET s, volatile uint16_t src, volatile uint8_t *dst, uint16_t len)
{
	uint8_t cntl_byte = (0x18+(s<<5));
	read((uint16_t)src , cntl_byte, (uint8_t *)dst, len);
}

uint8_t W5500Class::write(uint16_t _addr, uint8_t _cb, uint8_t _data)
{
	SPI.beginTransaction(wiznet_SPI_settings);
	setSS();
	//SerialUSB.println("dentro avr");
	SPI.transfer(_addr >> 8);
	SPI.transfer(_addr & 0xFF);
	SPI.transfer(_cb);
	SPI.transfer(_data);
	resetSS();
	SPI.endTransaction();
	return 1;
}

uint16_t W5500Class::write(uint16_t _addr, uint8_t _cb, const uint8_t *_buf, uint16_t _len)
{
	SPI.beginTransaction(wiznet_SPI_settings);
	setSS();
	SPI.transfer(_addr >> 8);
	SPI.transfer(_addr & 0xFF);
	SPI.transfer(_cb);
	for (uint16_t i=0; i<_len; i++){
		SPI.transfer(_buf[i]);
	}
	resetSS();
	SPI.endTransaction();
	return _len;
}

uint8_t W5500Class::read(uint16_t _addr, uint8_t _cb)
{
	SPI.beginTransaction(wiznet_SPI_settings);
	setSS();
	SPI.transfer(_addr >> 8);
	SPI.transfer(_addr & 0xFF);
	SPI.transfer(_cb);
	uint8_t _data = SPI.transfer(0);
	resetSS();
	SPI.endTransaction();
	return _data;
}

uint16_t W5500Class::read(uint16_t _addr, uint8_t _cb, uint8_t *_buf, uint16_t _len)
{
	SPI.beginTransaction(wiznet_SPI_settings);
	setSS();
	SPI.transfer(_addr >> 8);
	SPI.transfer(_addr & 0xFF);
	SPI.transfer(_cb);
	for (uint16_t i=0; i<_len; i++){
		_buf[i] = SPI.transfer(0);
	}
	resetSS();
	SPI.endTransaction();

	return _len;
}

void W5500Class::execCmdSn(SOCKET s, SockCMD _cmd) {
	// Send command to socket
	writeSnCR(s, _cmd);
	// Wait for command to complete
	while (readSnCR(s));
}
//#endif
