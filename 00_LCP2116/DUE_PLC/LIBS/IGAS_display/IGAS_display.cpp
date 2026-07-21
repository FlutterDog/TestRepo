#include "IGAS_display.h"




void IGAS_display_LED::displayMessage(byte linkDot)
{
	switch (digit_turn)
	{
		case 0:

		transferDigit(thousands);

		break;

		case 1:

		transferDigit(hundreds);

		break;

		case 2:

		transferDigit(tens);

		break;

		case 3:
		
		transferDigit(ones | linkDot);
		digit_turn = 0;
		break;

	}

}




void IGAS_display_LED::transferDigit(byte HEX_)
{
	CS_transferPin();	     // прит€гиваем CS к земле - начало передачи
	SPI.transfer(HEX_);        // передаем катодный байт
	SPI.transfer(pos[digit_turn]);                // передаем анодный байт
	digit_turn++;
}


void IGAS_display_LED::calculateText(int _number, byte flDot_)
{  
	thousandsInt = _number * 0.001;                    //выдел€ем сотни
	hundredsInt = (_number - thousandsInt * 1000) * 0.01;               //выдел€ем дес€тки
	tensInt = (_number - thousandsInt * 1000 - hundredsInt * 100) * 0.1;         //выдел€ем единицы
	onesInt = _number - thousandsInt * 1000 - hundredsInt * 100 - tensInt * 10;     //если тыс€ч нет, не отображаем ничего в 3м разр€де
	if (thousandsInt == 0) thousandsInt = 10;                     //если сотен нет, не отображаем ничего в 3м разр€де
	if (thousandsInt == 10 && hundredsInt == 0) 
    hundredsInt = 10;    //если сотен и дес€тков нет, не отображаем ничего во 2м разр€де
   
   
	if (thousandsInt == 10 && hundredsInt == 10 && tensInt == 0)    
     if (flDot_ == 0)
         tensInt = 10;
      else tensInt = 0;    //float dot means 0.0, not .0 on display   
      

	thousands = digit[thousandsInt];
	hundreds = digit[hundredsInt];
	tens = digit[tensInt] | flDot_;
	ones = digit[onesInt];

}


void IGAS_display_LED::displayText(byte L1, byte L2, byte L3, byte L4)
{
	thousands = digit[L1];       // —имвол "8." дл€ проверки диспле€ в стартовый момент
	hundreds = digit[L2];        // —имвол "8." дл€ проверки диспле€ в стартовый момент
	tens = digit[L3];        // —имвол "8." дл€ проверки диспле€ в стартовый момент
	ones = digit[L4];          // —имвол "8." дл€ проверки диспле€ в стартовый момент
}




void IGAS_display_LED::CS_transferPin()
{
	uint8_t oldSREG = SREG;
	byte switchOff_ = B11111110;

	cli();

	PORTB = PORTB & switchOff_;
	dispTrigger = 1;

	SREG = oldSREG;
}



void IGAS_display_LED::CS_clampSPIPin()
{
	uint8_t oldSREG = SREG;
	byte turnOn_ = B00000001;
	cli();

	PORTB = PORTB |  turnOn_;
	dispTrigger = 0;

	SREG = oldSREG;
}
