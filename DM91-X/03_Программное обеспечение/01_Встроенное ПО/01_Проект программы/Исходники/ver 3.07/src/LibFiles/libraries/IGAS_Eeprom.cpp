#include "IGAS_Eeprom.h"


void IGAS_Eeprom_Class::saveInt(int addr_, int digit_)
{
	inRing(addr_, digit_ & 0x00ff); // b1  ones
	inRing(addr_ + 1, digit_ >> 8); // b1  hundreds
}

void IGAS_Eeprom_Class::saveSignedInt(int addr_, int digit_)
{
	byte sign_;
	if (digit_ < 0)
	{
		digit_ = -1 * digit_;
		sign_ = 1;
	}
	else
	{
		sign_ = 0;
	}

	inRing(addr_, digit_ & 0x00ff); // b1  ones
	inRing(addr_ + 1, digit_ >> 8); // b1  hundreds
	inRing(addr_ + 2, sign_); // b1  hundreds

}



int IGAS_Eeprom_Class::loadInt(int addr_)
{
	int buffer_;

	buffer_ = EEPROM.read(addr_) | (EEPROM.read(addr_ + 1) << 8);
	return (buffer_);
}


int IGAS_Eeprom_Class::loadSignedInt(int addr_)
{
	int buffer_;

	buffer_ = EEPROM.read(addr_) | (EEPROM.read(addr_ + 1) << 8);

	if (EEPROM.read(addr_ + 2) == 0)
	return (buffer_);
	else
	return (-1 * buffer_);
}


void IGAS_Eeprom_Class::saveSigned(int addr_, int digit_)
{
	if (digit_ < 0)
	{
		digit_ = -1 * digit_;
		inRing(addr_, digit_); // b1  ones
		inRing(addr_ + 1, 1); // b1  hundreds
	}
	else
	{
		inRing(addr_, digit_); // b1  ones
		inRing(addr_ + 1, 0); // b1  hundreds
	}
}


int IGAS_Eeprom_Class::loadSign(int addr_)
{
	if (EEPROM.read(addr_ + 1) == 0)
	return (EEPROM.read(addr_));
	else
	return (-1 * EEPROM.read(addr_));
}



void IGAS_Eeprom_Class::saveByte(int addr_, byte digit_)
{
inRing(addr_, digit_);
}


void IGAS_Eeprom_Class::inRing(int addr_, byte digit_)
{

	addrBuffer[in_] = addr_;  // save EEPRom address to ring buffer
	dataBuffer[in_] = digit_; // save EEPRom data to ring buffer
	
	in_++; // Advance the index for next time
	if (in_ == 20)
	in_ = 0; // Wrap around!

}


void IGAS_Eeprom_Class::outRing()
{
	//byte data_;
	
	ringAddr = addrBuffer[out_];
	ringData = dataBuffer[out_];
	out_++;

	if (out_ == 20)
	out_ = 0; // wrap around

}


byte IGAS_Eeprom_Class::eepromRing()
{
	if (in_ != out_ && eeprom_is_ready())   // Check if buffer not empty and eeprom ready for writing data
	{
		outRing();
		//EEPROM.write(ringAddr ,ringData);
	}

	if (in_ == out_ && eeprom_is_ready())
	return 1;

	return 0;

}

