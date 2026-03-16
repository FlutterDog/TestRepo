#include <avr/wdt.h>
#include <IGAS_X2X.h>


IGAS_X2X_485 IGAS_mb_X;

// Toggle RS enable pin 10

byte relayState = 0;
byte errorByte = 0;
byte newState = 0;
byte slaveAddr = 0;
byte RS_Pin = 10;
unsigned int lostConnectionTime = 10000;
unsigned long resetWDtimer;
byte watchDog = 0;

byte serviceMode;
byte reboot = 0;
unsigned long currentTime;
unsigned long loop_100_start;
byte const loop_100ms = 100;
unsigned long loop_500_start;
int const loop_500ms = 500;

byte toggleLed = 0;
byte x2xState = 0;


typedef void (*AppPtr_t) (void);
AppPtr_t GoToBootloader = (AppPtr_t)0x3F07;

void setup()
{

	wdt_disable();                                                   // Отключение watchDog
	wdt_enable(WDTO_2S);                                             // Запуск watchDog таймера


	if (analogRead(A5) > 500)
	slaveAddr = slaveAddr | B00000001;

	if (analogRead(A4) > 500)
	slaveAddr = slaveAddr | B00000010;

	if (analogRead(A3) > 500)
	slaveAddr = slaveAddr | B00000100;

	if (analogRead(A2) > 500)
	slaveAddr = slaveAddr | B00001000;

	if (analogRead(A1) > 500)
	slaveAddr = slaveAddr | B00010000;


	if (slaveAddr == 0)
	slaveAddr = 1;

	for (byte i = 2; i < 10; i++)
	{
		pinMode(i, OUTPUT);
		digitalWrite(i, LOW);
	}


	initCommunication();        // Initialize communication, UART, SPI, etc


	// put your setup code here, to run once:
	pinMode(RS_Pin, OUTPUT);
	digitalWrite(RS_Pin, LOW);

	pinMode(A0, OUTPUT);
	digitalWrite(A0, LOW);
	resetWDtimer = currentTime;

	wdt_reset();   // Сброс WatchDog
	//digitalWrite(A0, HIGH);
	//delay(3000);

}

void loop()
{
	// put your main code here, to run repeatedly:
	currentTime = millis();

	loop_100();
	loop_500();

	IGAS_mb_X.update();

	wdt_reset();   // Сброс WatchDog
}


void loop_500()
{
	if (currentTime - loop_500_start > loop_500ms)
	{
		loop_500_start = currentTime;

		if (currentTime - resetWDtimer > lostConnectionTime)
		{
			x2xState = 0;
			newState = B00000000;
		}


		if (x2xState == 0)
		{
			toggleLed = !toggleLed;
			digitalWrite(A0, toggleLed);
		}
	}



}


void loop_100()
{
	if (currentTime - loop_100_start > loop_100ms)
	{
		loop_100_start = currentTime;

		if (relayState != newState)
		{
			relayState = newState;
			updateRelays();
		}


		if (reboot)
		{
			wdt_disable();
			wdt_enable(WDTO_15MS);
			for (;;)
			{
			}
		}

	}


}


void updateRelays()
{
	byte i = 9;
	for (byte mask = 00000001; mask > 0; mask <<= 1)      // iterate through bit mask
	{
		if (relayState & mask)         // if bitwise AND resolves to true
		digitalWrite(i, HIGH);             // Block Relay 1 range 1..8 => i
		else
		digitalWrite(i, LOW);
		i--;
	}

}






void initCommunication()
{
	IGAS_mb_X.initMB(9600, SERIAL_8N1, 1001);
	IGAS_mb_X.setDelegate(modbusProceed);        // To run library function from main loop
	IGAS_mb_X.slave = slaveAddr;
}










void modbusProceed(int *regs,  int start_addr_,  int reg_count_, int Value_)
{
	int registerAddr_ = start_addr_;
//	byte addr_;
	byte writeReg = 0;
	
	resetWDtimer = currentTime;

	if (IGAS_mb_X.func == FC_WRITE_REGS || IGAS_mb_X.func == FC_WRITE_REG)
	writeReg = 1;

	if (x2xState == 0) // Connection established
	{
		x2xState = 1;
		digitalWrite(A0, HIGH);
	}


	for (byte i = 0; i <= reg_count_ - 1; i++)
	{
		switch (registerAddr_)
		{
			case 0:

			if (writeReg)
			{
				newState = Value_;
			}
			else
			regs[i] = relayState;



			break;

			case 1:
			regs[i] = relayState;

			break;


			case 3:
			regs[i] = errorByte;

			break;

			// ************************************ Прошивка серийного номера устройства **********************************


			case 580:

			if ( Value_ == 20118)
			serviceMode = 1;


			break;



			case 590:

			if (serviceMode)
			{
				//    IGAS_EEPROM.saveInt(72, Value_);

			}
			break;

			// ************************************ Прошивка серийного номера устройства **********************************

			case 591:   // прошивка серийного номера устройства

			if (serviceMode)
			{
				//   IGAS_EEPROM.saveInt(74, Value_);

			}
			break;

			case 592:   // Поверка

			if (serviceMode)
			{
				//   year = Value_;
				//    IGAS_EEPROM.saveByte(111, year);  // k1  hundreds
			}
			break;



			case 593:   // Поверка

			if (serviceMode)
			{
				//    month = Value_;
				//   IGAS_EEPROM.saveByte(112, month);  // k1  hundreds
			}
			break;


			case 594:   // Поверка

			reboot = 1;

			break;




			case 900:   // Поверка

			noInterrupts();
			GoToBootloader();

			break;


			default:
			regs[i] = -1;     // reply -1, if option is not available.
			break;
		}
		registerAddr_++;
	}
}
