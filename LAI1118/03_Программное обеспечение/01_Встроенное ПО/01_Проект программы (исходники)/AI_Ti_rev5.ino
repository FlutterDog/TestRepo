#include <avr/wdt.h>
#include <IGAS_X2X.h>
#include "ADS1X15.h"
#include <SPI.h>
#include <EEPROM.h>
#include <IGAS_Eeprom.h>

IGAS_X2X_485 IGAS_mb_X;
IGAS_Eeprom_Class IGAS_EEPROM;

ADS1115 ADS_1(0x49);
ADS1115 ADS_2(0x48);


// Toggle RS enable pin 10

byte slaveAddr = 0;
byte const RS_Pin = 2;

byte const LED_SS = 3;
byte const MODE_SS = 4;
byte const shiftEnbld = 5;


unsigned int lostConnectionTime = 10000;
unsigned long resetWDtimer;
byte watchDog = 0;

byte pinCode;
byte reboot = 0;
unsigned long currentTime;
unsigned long loop_100_start;
byte const loop_100ms = 100;
unsigned long loop_500_start;
int const loop_500ms = 500;

byte toggleLed = 0;
byte x2xState = 0;

#define NUM_CHANNELS 8

float MOTOR_STOP_THRESHOLD = 0.5;

int gasConcentrationY[2*NUM_CHANNELS + 1];
int sensorAnalogX[2*NUM_CHANNELS + 1];

float k[NUM_CHANNELS + 1];
float b[NUM_CHANNELS + 1];

int16_t AIData[NUM_CHANNELS + 1] = {0};

#define EpromSTART 3
#define EpromShift 8
const byte EEPROM_Start_Adress[NUM_CHANNELS + 1] = {1, EpromSTART+EpromShift, EpromSTART+2*EpromShift, EpromSTART+3*EpromShift, EpromSTART+4*EpromShift, EpromSTART+5*EpromShift, EpromSTART+6*EpromShift, EpromSTART+7*EpromShift, EpromSTART+8*EpromShift};     // стартовые адреса ячеек EEPROM для загрузки данных


int const firmwareDef = 101;

typedef union
{
	float val;
	uint8_t bytes[4];
} floatval;



typedef void (*AppPtr_t) (void);
AppPtr_t GoToBootloader = (AppPtr_t)0x3F07;




uint8_t AI_MODE = 0xFF;


void setup()
{


	wdt_disable();                                                   // Отключение watchDog
	wdt_enable(WDTO_4S);                                             // Запуск watchDog таймера
	
	delay(20);
	
	pinMode(shiftEnbld, OUTPUT);
	digitalWrite(shiftEnbld, HIGH);

	pinMode(RS_Pin, OUTPUT);
	digitalWrite(RS_Pin, LOW);

	pinMode(LED_SS, OUTPUT);
	digitalWrite(LED_SS, HIGH);
	
	pinMode(MODE_SS, OUTPUT);
	digitalWrite(MODE_SS, HIGH);
	
	pinMode(A0, OUTPUT);
	digitalWrite(A0, LOW);
	
	
	// Address start
	
	for (byte i = 6; i <= 10; i++)  // Init pins
	pinMode(i, INPUT_PULLUP);
	
	
	for (byte i = 6; i <= 10; i++)   // Calc slave addr -> bit switch
	{
		if (digitalRead(i) == 0)
		slaveAddr = slaveAddr | (B00000001 << (i-6));
	}
	

	delay(20);
	
	
	// Delete
	//slaveAddr = 2;

	
	
	// Address fin
	
	if (slaveAddr == 0)
	slaveAddr = 1;
	
	initCommunication();        // Initialize communication, UART, SPI, etc
	



	

	resetWDtimer = currentTime;

	ADS_1.begin();
	ADS_2.begin();
	
	ADS_1.setGain(0);        // 6.144 volt
	ADS_2.setGain(0);        // 6.144 volt

	ADS_1.setMode(1);          // single mode
	ADS_2.setMode(1);          // single mode
	
	ADS_1.setDataRate(7);
	ADS_2.setDataRate(7);
	

	digitalWrite(LED_SS, LOW);
	SPI.transfer(0);
	digitalWrite(LED_SS, HIGH);

	AI_MODE = EEPROM.read(115);
	
	digitalWrite(MODE_SS, LOW);
	SPI.transfer(AI_MODE);   // currentLoop 0-20 ma
	digitalWrite(MODE_SS, HIGH);
	
	
	digitalWrite(shiftEnbld, LOW);
	
	//	digitalWrite(shiftEnbld, LOW);

	wdt_reset();   // Сброс WatchDog
	//digitalWrite(A0, HIGH);
	//delay(3000);

	for (int i = 1; i <= 8; i++)   // Загрузка данных кривой y = kx - b;
	calibrFromEEPROM(i);
	
	MOTOR_STOP_THRESHOLD = IGAS_EEPROM.loadFloat(110, 0.5);
	
	
}

enum
{
	AIC_1,
	AIC_2,
};


int ledData[9] = {0,0,0,0,0,0,0,0,0};

void loop()
{
	// put your main code here, to run repeatedly:
	currentTime = millis();

	loop_100();
	loop_500();

	IGAS_mb_X.update();
	handleConversion(AIC_1);
	handleConversion(AIC_2);
	
	processMotorCurrent();

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

		IGAS_EEPROM.eepromRing();
		if (reboot)
		rebootDM();

	}


}








void initCommunication()
{
	//slaveAddr = EEPROM.read(100);  // addr 100, def 1 - 1
	
	IGAS_mb_X.initMB(9600, SERIAL_8N1, 1001);
	IGAS_mb_X.setDelegate(modbusProceed);        // To run library function from main loop
	IGAS_mb_X.slave = slaveAddr;
	

	
	
	Wire.setClock(400000);
	SPI.begin();
	
}

enum
{
	inActive,
	waitStart,
	Active
};

uint8_t channelActive[NUM_CHANNELS + 1] = {inActive}; // Active state for each channel
float maxCurrArray[NUM_CHANNELS + 1] = {0}; // Final max current values
float buffer[NUM_CHANNELS + 1] = {0}; // Temporary buffer for max values
unsigned long stopTimers[NUM_CHANNELS + 1] = {0}; // Timer for each channel
#define STOP_DELAY_MS 5000 // 1 second delay (adjustable based on timing)
float ADC_Converted[NUM_CHANNELS + 1] = {0};
int16_t ADC_Buf = 0;



void processMotorCurrent(void)
{
	for (uint8_t i = 1; i <= NUM_CHANNELS; ++i)
	{
		ADC_Buf = abs(AIData[9-i]);
		ADC_Converted[i] = sensorToConcentration(i,ADC_Buf);
		
		if (ADC_Converted[i] > MOTOR_STOP_THRESHOLD)
		{
			
			switch (channelActive[i])
			{
				case inActive:
				{
					channelActive[i] = waitStart;
					buffer[i] = 0;
					stopTimers[i] = currentTime;
				}
				break;
				
				case waitStart:
				{
					if (currentTime - stopTimers[i] > 500)
					{
						channelActive[i] = Active;
						stopTimers[i] = currentTime;
					}
				}
				break;
				
				case Active:
				{
					if (ADC_Converted[i] > buffer[i])
					{
						buffer[i] = ADC_Converted[i];
					}
					stopTimers[i] = currentTime;
				}
				break;
				
			}
			
			// 			if (channelActive[i] == inActive)
			// 			{
			// 				// Channel just became active: reset buffer for new max finding
			// 				channelActive[i] = waitStart;
			// 				buffer[i] = 0;
			// 				stopTimers[i] = currentTime;
			// 			}

			// 			if (channelActive[i] == inActive)
			// 			{
			// 				// Channel just became active: reset buffer for new max finding
			// 				channelActive[i] = waitStart;
			// 				buffer[i] = 0;
			// 				stopTimers[i] = currentTime;
			// 			}

			//		stopTimers[i] = currentTime; // Reset stop timer since signal is active
			// Track maximum value for this channel
			// 			if (ADC_Converted[i] > buffer[i])
			// 			{
			// 				buffer[i] = ADC_Converted[i];
			// 			}
		}
		else
		{
			if (channelActive[i] == Active)
			{
				if (currentTime - stopTimers[i] > STOP_DELAY_MS)
				{
					channelActive[i] = inActive;
					maxCurrArray[i] = buffer[i];
					buffer[i] = 0; // Reset buffer for the next session
					stopTimers[i] = 0;
				}
				
				// Channel just became inactive: save max value and reset
				
			}
		}
	}
}





void modbusProceed(int *regs,  int start_addr_,  int reg_count_, int Value_)
{
	int registerAddr_ = start_addr_;

	byte writeReg = 0;
	if (IGAS_mb_X.func == FC_WRITE_REGS || IGAS_mb_X.func == FC_WRITE_REG)
	writeReg = 1;
	
	
	resetWDtimer = currentTime;

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

			regs[i] = AIData[0];

			break;
			
			
			case 1:

			regs[i] = convertFloat(ADC_Converted[8], 0);

			break;


			case 2:

			regs[i] = convertFloat(ADC_Converted[8], 1);
			break;



			case 3:

			regs[i] = convertFloat(ADC_Converted[7], 0);

			break;


			case 4:

			regs[i] = convertFloat(ADC_Converted[7], 1);
			break;



			case 5:

			regs[i] = convertFloat(ADC_Converted[6], 0);

			break;


			case 6:

			regs[i] = convertFloat(ADC_Converted[6], 1);
			break;
			
			
			case 7:

			regs[i] = convertFloat(ADC_Converted[5], 0);

			break;


			case 8:

			regs[i] = convertFloat(ADC_Converted[5], 1);
			break;
			
			
			
			
			case 9:

			regs[i] = convertFloat(ADC_Converted[4], 0);

			break;


			case 10:

			regs[i] = convertFloat(ADC_Converted[4], 1);
			break;
			
			
			
			case 11:

			regs[i] = convertFloat(ADC_Converted[3], 0);

			break;


			case 12:

			regs[i] = convertFloat(ADC_Converted[3], 1);
			break;
			
			
			
			case 13:

			regs[i] = convertFloat(ADC_Converted[2], 0);

			break;


			case 14:

			regs[i] = convertFloat(ADC_Converted[2], 1);
			break;
			
			
			
			case 15:

			regs[i] = convertFloat(ADC_Converted[1], 0);

			break;


			case 16:

			regs[i] = convertFloat(ADC_Converted[1], 1);
			break;
			
			// 			case 1 ... 8:
			//
			// 			regs[i] = sensorToConcentration(AIData[9-registerAddr_]);
			//
			// 			break;
			
			

			
			// ************************************ Чтение серийного номера устройства **********************************

			case 39:

			regs[i] = IGAS_EEPROM.loadInt(72);

			break;


			// ************************************ Чтение версии прошивки устройства **********************************

			case 40:

			regs[i] = firmwareDef; // IGAS_EEPROM.loadInt(74);

			break;
			
			
			// 			case 48:
			// 			{
			// 				for (byte i = 1; i < 8; i++)
			// 				{
			// 					gasConcentrationY[i] = i * 100;
			// 					sensorAnalogX[i] = i * 274;
			// 					calibrToEEPROM(i);
			// 				}
			//
			// 				while (!IGAS_EEPROM.eepromRing())
			// 				{
			// 					;
			// 				}

			// 			}
			// 			break;
			
			case 51:

			regs[i] = convertFloat(maxCurrArray[1], 0);

			break;


			case 52:

			regs[i] = convertFloat(maxCurrArray[1], 1);
			break;


			case 53:

			regs[i] = convertFloat(maxCurrArray[2], 0);

			break;


			case 54:

			regs[i] = convertFloat(maxCurrArray[2], 1);
			
			break;


			case 55:

			regs[i] = convertFloat(maxCurrArray[3], 0);

			break;


			case 56:

			regs[i] = convertFloat(maxCurrArray[3], 1);
			
			break;


			case 57:

			regs[i] = convertFloat(maxCurrArray[4], 0);

			break;


			case 58:

			regs[i] = convertFloat(maxCurrArray[4], 1);
			
			break;

			case 59:

			regs[i] = convertFloat(maxCurrArray[5], 0);

			break;


			case 60:

			regs[i] = convertFloat(maxCurrArray[5], 1);
			break;


			case 61:

			regs[i] = convertFloat(maxCurrArray[6], 0);

			break;


			case 62:

			regs[i] = convertFloat(maxCurrArray[6], 1);
			break;


			case 63:

			regs[i] = convertFloat(maxCurrArray[7], 0);

			break;


			case 64:

			regs[i] = convertFloat(maxCurrArray[7], 1);
			
			break;

			case 65:

			regs[i] = convertFloat(maxCurrArray[8], 0);

			break;


			case 66:

			regs[i] = convertFloat(maxCurrArray[8], 1);
			
			break;
			// 			case 51 ... 58:
			//
			// 			regs[i] = maxCurrArray[registerAddr_ - 50];
			//
			// 			break;
			//
			
			


			case 90:

			if (writeReg)
			for (byte j = 1; j <= 8; j++ )
			calibrToEEPROM(j);


			break;

			
			case 100:

			if (writeReg)
			{
				slaveAddr = Value_;
				EEPROM.write(100, slaveAddr);
			}

			regs[i] = slaveAddr;

			break;



			case 101:
			
			if (writeReg && pinCode)
			{
				EEPROM.write(112,(Value_ >> 8));
				EEPROM.write(113,(Value_ & 0x00FF));
			}
			
			regs[i] = convertFloat(MOTOR_STOP_THRESHOLD, 0);
			
			break;
			
			case 102:
			
			if (writeReg && pinCode)
			{
				EEPROM.write(110,(Value_ >> 8));
				EEPROM.write(111,(Value_ & 0x00FF));
				MOTOR_STOP_THRESHOLD = IGAS_EEPROM.loadFloat(110, 0.5);
			}
			
			regs[i] = convertFloat(MOTOR_STOP_THRESHOLD, 1);
			
			break;
			
			


			case 115:

			if (writeReg && pinCode)
			{
				AI_MODE = Value_;
				EEPROM.write(115,AI_MODE);
				
				digitalWrite(MODE_SS, LOW);
				SPI.transfer(AI_MODE);   // currentLoop 0-20 ma
				digitalWrite(MODE_SS, HIGH);
			}

			regs[i] = AI_MODE;


			break;

			
			
			// ************************************ Перекалибровка датчика - управление каждой точкой (ось Y - концентрация) кусочно линейной кривой в отдельности **********************************


			case 201 ... 216:

			regs[i] = sensorAnalogX[registerAddr_ - 200];

			break;


			
			case 301 ... 316:

			regs[i] = gasConcentrationY[registerAddr_ - 300];

			break;
			
			
			case 401 ... 416:

			if (writeReg)
			sensorAnalogX[registerAddr_ - 400] = Value_;
			
			regs[i] = sensorAnalogX[registerAddr_ - 400];

			break;
			

			case 501 ... 516:
			
			if (writeReg)
			gasConcentrationY[registerAddr_ - 500] = Value_;
			regs[i] = gasConcentrationY[registerAddr_ - 500];

			break;



			


			case 594:   // Reboot
			
			if (writeReg)
			if (Value_ == 1)
			reboot = 1;

			regs[i] = 0;

			break;


			case 600:

			regs[i] = AIData[0];


			break;
			
			case 601 ... 608:

			regs[i] = AIData[609-registerAddr_];

			break;
			
			
			break;
			
			case 611 ... 618:

			regs[i] = channelActive[619-registerAddr_];

			break;
			
			
			case 621:

			regs[i] = convertFloat(buffer[1], 0);

			break;


			case 622:

			regs[i] = convertFloat(buffer[1], 1);
			
			break;

			
			case 623:

			regs[i] = convertFloat(buffer[2], 0);

			break;


			case 624:

			regs[i] = convertFloat(buffer[2], 1);
			
			break;

			
			
			case 625:

			regs[i] = convertFloat(buffer[3], 0);

			break;


			case 626:

			regs[i] = convertFloat(buffer[3], 1);
			
			break;
			
			
			
			case 627:

			regs[i] = convertFloat(buffer[4], 0);

			break;


			case 628:

			regs[i] = convertFloat(buffer[4], 1);
			
			break;
			
			
			
			case 629:

			regs[i] = convertFloat(buffer[5], 0);

			break;


			case 630:

			regs[i] = convertFloat(buffer[5], 1);
			
			break;
			
			
			
			case 631:

			regs[i] = convertFloat(buffer[6], 0);

			break;


			case 632:

			regs[i] = convertFloat(buffer[6], 1);
			
			break;
			
			
			case 633:

			regs[i] = convertFloat(buffer[7], 0);

			break;


			case 634:

			regs[i] = convertFloat(buffer[7], 1);
			
			break;
			
			case 635:

			regs[i] = convertFloat(buffer[8], 0);

			break;


			case 636:

			regs[i] = convertFloat(buffer[8], 1);
			
			break;
			
			
			
			case 701 ... 708:
			
			regs[i] = 100 * ADC_Converted[709-registerAddr_];
			
			break;
			
			
			
			
			case 800:   // PinCode Sevice

			if (writeReg)
			{
				if (Value_ == 2210)
				pinCode = 1;
				else pinCode = 0;
			}


			regs[i] = pinCode;

			break;


			case 900:   // Поверка

			noInterrupts();
			GoToBootloader();

			break;
			
			
			
			case 1000:   // Module ID

			regs[i] = 4149;

			break;
			
			case 1001:   // Module ID

			regs[i] = 1118;

			break;
			
			
			case 1100:   // Module ID

			regs[i] = 2;

			break;
			
			case 1101:   // Soft ID

			regs[i] = 171;

			break;

			default:
			regs[i] = -1;     // reply -1, if option is not available.
			break;
		}
		registerAddr_++;
	}
}


void rebootDM()
{
	wdt_disable();
	wdt_enable(WDTO_15MS);
	for (;;)
	{
	}
}



byte AIBuffer = 0;
byte ADSstate[2]  = {0,0};
byte AIX[2] = {0,0} ;

unsigned long startAItime[2] = {0,0};

enum
{
	requestAI,
	delayADC,
	checkAIReady,
	getAIx,
	calcOutput,

};

void handleConversion(byte chipID)
{
	ADS1115* _ADS;
	
	switch (chipID)
	{
		case 0:
		
		_ADS = &ADS_1;

		break;
		
		case 1:

		_ADS = &ADS_2;
		
		break;
		
	}


	switch (ADSstate[chipID])
	{
		case requestAI:
		
		_ADS -> startSingleShot(AIX[chipID]);
		startAItime[chipID] = currentTime;
		
		
		ADSstate[chipID] = delayADC;
		
		break;
		
		case delayADC:
		
		if (currentTime - ADSstate[chipID] > 2)
		ADSstate[chipID] = checkAIReady;
		
		break;
		
		
		case checkAIReady:
		
		if (_ADS->isBusy() == false)
		ADSstate[chipID] = getAIx;
		
		break;
		
		
		case getAIx:
		
		AIData[8 - 4*chipID - AIX[chipID]] = _ADS->getValue();
		
		AIX[chipID]++;
		if (AIX[chipID] > 3)
		AIX[chipID] = 0;
		
		ADSstate[chipID] = calcOutput;
		
		break;
		
		
		case calcOutput:
		{
			byte LEDbit = 0;
			byte MBbit = 0;
			
			for (byte i=1; i<=4; i++)
			{
				ledData[2*i] = AIData[2*i-1];
				ledData[2*i-1] = AIData[2*i];
			}
			
			for (byte i=0; i<8; i++)
			{
				if (ledData[i+1] > 5000)
				LEDbit = LEDbit | B00000001 << i;
				
				if (AIData[i+1] > 5000)
				MBbit = MBbit | B00000001 << (7-i);
			}
			
			
			
			
			if (AIBuffer != LEDbit )
			{
				AIBuffer = LEDbit;
				
				
				digitalWrite(LED_SS, LOW);
				SPI.transfer(AIBuffer);
				digitalWrite(LED_SS, HIGH);
				
				AIData[0] = MBbit;
				ledData[0] = AIBuffer;
			}

			ADSstate[chipID] = requestAI;
		}
		break;
		
		
		
		
	}
	

}



byte calibrToEEPROM(byte channel_)
{

	k[channel_] = 1.0 * (gasConcentrationY[2*channel_] - gasConcentrationY[2*channel_ - 1]) / (sensorAnalogX[2*channel_] - sensorAnalogX[2*channel_ - 1]);
	b[channel_] = sensorAnalogX[2*channel_ - 1] - gasConcentrationY[2*channel_ - 1] / k[channel_];
	// ************ Запись значений во внутреннюю память контроллера ******************************************
	
	IGAS_EEPROM.saveInt(EEPROM_Start_Adress[channel_], sensorAnalogX[2*channel_]);
	IGAS_EEPROM.saveInt(EEPROM_Start_Adress[channel_] + 2, gasConcentrationY[2*channel_]);
	
	IGAS_EEPROM.saveInt(EEPROM_Start_Adress[channel_] + 4, sensorAnalogX[2*channel_-1]);
	IGAS_EEPROM.saveInt(EEPROM_Start_Adress[channel_] + 6, gasConcentrationY[2*channel_-1]);
	
}






void calibrFromEEPROM(byte pointer)   // Загрузка данных кривой y = kx - b;
{
	
	sensorAnalogX[2*pointer] = IGAS_EEPROM.loadInt(EEPROM_Start_Adress[pointer]);
	gasConcentrationY[2*pointer] = IGAS_EEPROM.loadInt(EEPROM_Start_Adress[pointer] + 2);
	
	sensorAnalogX[2*pointer-1] = IGAS_EEPROM.loadInt(EEPROM_Start_Adress[pointer] + 4);
	gasConcentrationY[2*pointer-1] = IGAS_EEPROM.loadInt(EEPROM_Start_Adress[pointer] + 6);
	
	
	
	k[pointer] = 1.0 * (gasConcentrationY[2*pointer] - gasConcentrationY[2*pointer - 1]) / (sensorAnalogX[2*pointer] - sensorAnalogX[2*pointer - 1]);
	b[pointer] = sensorAnalogX[2*pointer - 1] - gasConcentrationY[2*pointer - 1] / k[pointer];
	
}




// float sensorToConcentration(int16_t f_)
// {
// 	/*
// 	checkStability();
// 	if (refStable == 0)
// 	{
// 	outputSignals[concentration] = 0;
// 	return 0;
// 	}
// 	*/
//
// 	if (f_ <= sensorAnalogX[0])
// 	f_ = sensorAnalogX[0];
//
// 	for (byte i = 1; i <= 6; i++)
// 	if (f_ >= sensorAnalogX[i - 1] && f_ < sensorAnalogX[i])                       // График до первой точки
// 	{
//
// 		return  k[i] * (f_ - b[i]);
// 	}
//
//
// 	if (f_ >= sensorAnalogX[6])                     // График после первой точки
// 	{
// 		return  k[7] * (f_ - b[7]);
// 	}
//
//
// }

float sensorToConcentration(uint8_t channel_, int16_t f_)
{
	return  (k[channel_] * (f_ - b[channel_]));
}

int16_t convertFloat(float flt_, uint8_t oreder_)
{
	union {
		float flt;
		int32_t i32;
		uint8_t bytes[4];
	} u;
	
	u.flt = flt_;
	
	return (u.bytes[1 + 2 * oreder_] << 8) | u.bytes[2 * oreder_];
}



