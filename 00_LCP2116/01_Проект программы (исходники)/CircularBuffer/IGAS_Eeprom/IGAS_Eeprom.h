
#ifndef IGAS_Eeprom
#define IGAS_Eeprom

#include <Arduino.h>
#include <pins_arduino.h>
#include <EEPROM.h>



class IGAS_Eeprom_Class
{

	public:

	void saveInt(int addr_, int digit_);
	void saveSignedInt(int addr_, int digit_);
	int loadInt(int addr_);
	int loadSignedInt(int addr_);
	void saveSigned(int addr_, int digit_);
	int loadSign(int addr_);
	byte eepromRing();
	void saveByte(int addr_, byte digit_);

	
	private:


	void outRing();
	void inRing(int addr_, byte digit_);
	byte dataBuffer[100];   // lowByte, highByte
	int addrBuffer[100];    // eeprom address
	int ringAddr = 0;      
	byte ringData = 0;
	byte in_ = 0, out_ = 0;

};

#endif

