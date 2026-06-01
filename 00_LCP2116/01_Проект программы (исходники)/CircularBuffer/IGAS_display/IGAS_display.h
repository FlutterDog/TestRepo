
#ifndef IGAS_display
#define IGAS_display

#include <Arduino.h>
#include <pins_arduino.h>
#include <SPI.h>


// Номер символа в массиве 0    1     2      3	   4     5   	6     7     8	 9    10      11   12	 13	    14   15   16     17    18   19    20     21   22     23   24   25     26	27		28
// Массив всех символов    0    1     2      3     4     5      6     7     8    9   empty    A     C     L     E     r    -     d     o     n    I       G   8.     H    .      t     F	 u		P

 const uint8_t digit[] = {0xF5, 0x84, 0xB3, 0x97, 0xC6, 0x57, 0x77, 0x85, 0xF7, 0xD7, 0x00, 0xE7, 0x71, 0x70, 0x73, 0x61, 0x02, 0xB6, 0x36, 0x26, 0x84, 0x75, 0xFF, 0xE6, 0x08,  0x72, 0x63, 0x34, 0xE3};

 const byte _0 = 0;
 const byte _1 = 1;
 const byte _2 = 2;
 const byte _3 = 3;
 const byte _4 = 4;
 const byte _5 = 5;
 const byte _6 = 6;
 const byte _7 = 7;
 const byte _8 = 8;
 const byte _9 = 9;
 const byte _Empty = 10;
 const byte _A = 11;
 const byte _C = 12;
 const byte _L = 13;
 const byte _E = 14;
 const byte _r = 15;
 const byte _minus = 16;
 const byte _d = 17;
 const byte _o = 18;
 const byte _n = 19;
 const byte _I = 20;
 const byte _G = 21;
 const byte _FULL = 22;
 const byte _H = 23;
 const byte _dot = 24;
 const byte _S = 5;
 const byte _t = 25;
 const byte _F = 26;
 const byte _u = 27;
 const byte _P = 28;
  
 const uint8_t pos[] = {0x1C, 0x1A, 0x16, 0x0E, 0x0E};



class IGAS_display_LED
{

	public:

	void displayMessage(byte linkDot);
	void calculateText(int _number, byte flDot_ = 0);
	void displayText(byte L1, byte L2, byte L3, byte L4);
	void CS_clampSPIPin();
	
	byte thousands;                    //выделяем сотни
	byte hundreds;               //выделяем десятки
	byte tens;         //выделяем единицы
	byte ones;
	byte dispTrigger;

	
	private:
	void transferDigit(byte value_);
	void CS_transferPin();
	
	
	int thousandsInt;                    //выделяем сотни
	int hundredsInt;               //выделяем десятки
	int tensInt;         //выделяем единицы
	int onesInt;
	byte digit_turn = 0;

};

#endif

