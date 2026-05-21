// 1.01 Added reboot CPU after 5 seconds of no incoming communication
// after 10 sec of no incoming addressed communication


#include "Main.h"


//#include <AltSoftSerial.h>
#include <avr/wdt.h>
#include <IGAS_485DN.h>
#include <EEPROM.h>
//#include <SHT31.h>
//#include <SHT31.h>
#include <Wire.h>
//#include <I2C.h>
//  ds TEMP SENSOR Start
//#include "DS18B20.h"
#define ONE_WIRE_BUS  A1

//OneWire oneWire(ONE_WIRE_BUS);
//DS18B20 sensor(&oneWire);
//  ds TEMP SENSOR Finish

IGAS_X2X_485 IGAS_mb_X;	// Hardware Serial+modbus lib

//uint32_t start;
//uint32_t stop;
//uint32_t connectionFails = 0; //
//SHT31 sht;  // Humidity sensor library

//typedef unsigned char uchar;

#define SHT31_ADDRESS   0x44  // humidity sensor address
//AltSoftSerial altSerial;	// Software Serial Library



uint16_t const hVersion = 5;		// Hardware version
uint16_t const sVersion = 15; // Software version



// Toggle RS enable pin 10

uint8_t slaveAddr = 1;		// DM91 slave address
uint8_t const RS_Pin = 2;	// RS485 pin enable Transmit
uint8_t const Heat_CPU = 3;	// RS485 pin enable Transmit
uint8_t const Heat_Sensor = 5;	// RS485 pin enable Transmit
uint8_t const Power_Sensor = 6;	// RS485 pin enable Transmit

int16_t  Heat_CPU_Current = 0;	// RS485 pin enable Transmit
//int16_t  CPU_Temp_Sensor = 0;	// RS485 pin enable Transmit
uint8_t enableCPUheat = 0;

byte lostTian = 0;

//float bufP,bufT;


int errorInt = 0;			// DM91 error bits

uint8_t shtSensorLost = 0;	// Humidity sensor fault flag


// Start  Measurements variables
float pressure_kPa;
float pressure_Bar;
float pressure_MPa;
float pressure_Pa;
float pressure_Psi;
float pressure_Ncm2;

float pressure;
float temperature;

float RH_Temp = 0;
float RH_RH = 0;

float temperatureC;
float temperatureK;
float temperatureF;

float gasDensityG;
uint8_t liquefaction;
uint8_t overdensity;
float pressure_Bar20;
uint8_t alarmLevel;
uint8_t warningLevel;

float pressure_Bar20Rel;
float pressure_MPa20;
float pressure_MPa20Rel;
// Finish  Measurements variables

float Pws = 0;
float Pw = 0;
float dewPoint = 0;
float dewPointAtm = 0;
float frostPointAtm = 0;
float PPMv = 0;
float PPMm = 0;
float RH_RH_Atm = 0;
float frostPoint = 0;

uint8_t avarPosition = 0;

float pressureRAW = 0;
float temperatureRAW = 0;

float SHT_RH_FL = 0;
float RH_Tuned = 0;
float SHT_Temp_FL = 0;
uint8_t negativeRHflag = 0;

uint16_t _rawHumidity;
uint16_t _rawTemperature;
uint16_t RAW_pressureTian;
uint16_t RAW_temperatureTian;


float tempAvar[12] = {0};
float RHAvar[12] = {0};
float pressAvar[12] = {0};
float AVARAGED[4] = {0};
float PIDFILTER[4] = {0};
//float GAPFILTERED[2] = {0};

float topLevel = 0;

uint8_t STRIKE_H[3] = {0};
uint8_t STRIKE_L[3] = {0};

uint8_t tail = 0;
const uint8_t trendLimit = 5;
float TREND[3][trendLimit+2];
float  tempFiltered;
float  pressureFiltered;
float const SMOOTHfilter[] = {0.03, 0.03, 0.03};
float const AGGRESSfilter[] = {0.5, 0.5, 0.5};

int16_t tianPressure = 0;

int16_t tianTemp = 0;
float pressureTian = 0;

float temperatureTian = 0;




// Serial number
int serialLow ;
int serialHigh ;

uint8_t baudRate = 0;  // addr 101, def 5 - 19200
uint8_t parity = 0;  // addr 100, def 5 - 19200





#define dm910H 1
#define dm910 0

uint8_t  modelDN = dm910H;


uint8_t gasPartner = 0;
unsigned int densitySet;
float densitySetFlt;


unsigned long resetWDtimer;
uint8_t watchDog = 0;

uint8_t serviceMode;
uint8_t reboot = 0;
uint8_t apply = 0;

unsigned long currentTime;
unsigned long loop_50_start;
uint8_t const loop_50ms = 120;
unsigned long loop_500_start;

uint16_t const loop_500ms = 500;
uint16_t incomingTimeOut = 5000;
uint8_t pinCode = 0;

uint8_t vipRespond[40];
uint8_t vipMB_ByteReply[24];

unsigned long readDelay = 0;

uint8_t getTempFromSHT = 0;


typedef union
{
	float val;
	uint8_t bytes[4];
} floatval;

floatval pressureVIP;
floatval themperatureVIP;
//floatval bufferFloat;


//typedef unsigned char uchar;

typedef void (*AppPtr_t) (void);
AppPtr_t GoToBootloader = (AppPtr_t)0x3F07;



uint8_t bridgeMode = 0;
float offsetTemp = 0;
float offsetHumidity_b = 0;
float offsetHumidity_K = 0;


// Sensor Settings
uint8_t tian_Pmin = 0;
uint8_t tian_Pmax = 0;
uint16_t tian_K = 0;
uint16_t tian_Offset = 0;
float tianTerm1 = 0;


uint8_t counterX = 0;

uint16_t stat;



// ---------------------------
// ---- PID Section Begin ----
// ---------------------------

float kP	= 1.0;		// Коэффициент P.
float kI	= 0.05;		// Коэффициент I.
float kD	= 0.1;		// Коэффициент D.

// Переменные ошибок:
float curr_error	= 0;
float prev_error	= 0;
float integral		= 0;

// #define DDR_HEATER		DDRD
// #define PORT_HEATER		PORTD
// #define HEATER			PIND3
// #define HEATER_ON()		PORT_HEATER |= (1 << HEATER)
// #define HEATER_OFF()	PORT_HEATER &= ~(1 << HEATER)

#define MIN_OUTPUT	0		// Минимально возможное значение.
#define MAX_OUTPUT	255		// Максимально возможное значение.

float tempSetPoint = 0;

// Функция ПИД-регулирования:

uint8_t pid_control(float setpoint, float input){
	// Расчёт ошибки:
	curr_error = setpoint - input;

	// Расчёт P:
	float proportional = kP * curr_error;

	// Расчёт I:
	integral += kI * curr_error;

	// Ограничение выхода за границы диапазона:
	if (integral > MAX_OUTPUT){
		integral = MAX_OUTPUT;
		} else if (integral < MIN_OUTPUT){
		integral = MIN_OUTPUT;
	};

	// Расчёт D:
	float derivative = kD * (curr_error - prev_error);
	prev_error = curr_error;

	// Расчёт выходного значения:
	float output = proportional + integral + derivative;

	// Ограничение выхода за границы диапазона:
	if (output > MAX_OUTPUT){
		output = MAX_OUTPUT;
		} else if (output < MIN_OUTPUT){
		output = MIN_OUTPUT;
	};

	// Возвращаем расчитанное значение, приведённое к беззнаковому 8-битному.
	return (uint8_t) output;
}

// ---- PID Section End ---
// ---------------------------


float DS18B20_Temp = 0;
#define TEMP_12_BIT             0x7F    //  12 bit

void setup()
{
	

	// START I2C humidity TEST


	// FINISH I2C humidity TEST



	wdt_disable();                                                   // Отключение watchDog
	wdt_enable(WDTO_2S);                                             // Запуск watchDog таймера
	

	// 	DDR_HEATER |= (1 << HEATER);			// Настраиваем вывод управления нагревателем на выход.
	// 	HEATER_OFF();							// По-умолчанию нагреватель выключен.


	settings();  // get settings from eeprom


	pinMode(RS_Pin, OUTPUT);
	digitalWrite(RS_Pin, LOW);
	pinMode(Heat_CPU, OUTPUT);
	pinMode(Heat_Sensor, OUTPUT);
	pinMode(Power_Sensor, OUTPUT);
	digitalWrite(Power_Sensor, LOW);

	initCommunication();        // Initialize communication, UART, SPI, etc
	delay(50);

	
	UCSR0A = UCSR0A | (1 << TXC0);
	uint8_t oldSREG = SREG;
	cli();
	PORTD = PORTD | transmitMode;                  // Перевод драйвера RS485 в режим передачи сообщений
	SREG = oldSREG;
	
	Serial.write(slaveAddr);
	Serial.write(slaveAddr);
	Serial.write(slaveAddr);
	Serial.write(slaveAddr);
	//	sensor.begin();
	//	sensor.setResolution(12);
	
	
	//	initCommunication();        // Initialize communication, UART, SPI, etc
	
	//	modelDN = dm910;
	
	// 	if (modelDN == dm910H)
	// 	{
	// 		sht.begin(SHT31_ADDRESS);
	// 		Wire.setClock(100000);
	// 		stat = sht.readStatus();
	// 	}
	// 	else
	// 	{
	Wire.begin();
	Wire.setClock(100000);
	Wire.setTimeout(10);

	IGAS_mb_X.incomingWDT = millis();
	
	wdt_reset();   // Сброс WatchDog
	//digitalWrite(A0, HIGH);
	//delay(3000);
	//	}
}
uint8_t VIP_firstRead = 0;

uint8_t i=1;
uint8_t startMB = 0;
uint16_t watchdogQnt=0;

void loop()
{
	// put your main code here, to run repeatedly:

	currentTime = millis();

// 	if (bridgeMode == 1)
// 	{
// 		bridgeMode = 2;
// 		wdt_disable();
// 	}

	
	// 	if (bridgeMode)  // Service mode
	// 	{
	// 		bridgeVIP();
	// 	}
	// 	else
	// 	{
	if (startMB == 0 && currentTime > 3000 )
	{
		//	initCommunication();        // Initialize communication, UART, SPI, etc
		startMB = 1;
	}

	if (startMB)
	{
		loop_5000();
		loop_500();
		loop_50();
		IGAS_mb_X.update();
	}
	
	wdt_reset();   // Сброс WatchDog
	//	}
	
}


/*
void bridgeVIP()
{
while(1)
{
currentTime = millis();

if (checkIncoming(0,Serial.available()) )
while (Serial.available())
altSerial.write(Serial.read());

if (checkIncoming(1,altSerial.available()))
{
UCSR0A = UCSR0A | (1 << TXC0);
digitalWrite(2, HIGH);
while (altSerial.available())
Serial.write(altSerial.read());

}
}

}
*/


enum
{
	dmReady,
	powerUpTian,
	waitPowerUpTian,
	sendMsg,
	waitTransfer,
	waitReply,
	receiveMsg,
	requestRH,
	receiveRH,
	timeEqualization,
	averaging,
	trending,
	noiseCancel,
	gapCancel,
	calculate,
	pollDelay,
};


uint8_t sensorStep = dmReady;

uint8_t vipSend[] = {0x01, 0x03, 0x00, 0x27, 0x00, 0x04, 0xF4, 0x02};

uint8_t pressGaugeLost = 0;
uint8_t lostCntPressure = 0;
unsigned int totalLostPressureCnt = 0;

uint8_t lostCntRH = 0;
unsigned int totalLostRHCnt = 0;

byte vipReplyQnt = 0;
uint8_t DS_step = 0;

enum
{
	requestTemp,
	checkConversion,
	getValue,
	dsPollTimeOut,
};

void DS_proceed()
{
	// 	switch (DS_step)
	// 	{
	// 		case requestTemp:
	// 		sensor.requestTemperatures();
	// 		DS_step++;
	// 		break;
	//
	// 		case checkConversion:
	// 		if (sensor.isConversionComplete())
	// 		DS_step++;
	// 		break;
	//
	// 		case getValue:
	// 		DS18B20_Temp = sensor.getTempC();
	// 		DS_step++;
	// 		break;
	//
	// 		case dsPollTimeOut:
	// 		DS_step = requestTemp;
	// 		break;
	//
	// 	}
	
}




void mainDM()
{
	switch (sensorStep)
	{
		case powerUpTian:
		{
			digitalWrite(Power_Sensor, HIGH);
			sensorStep = waitPowerUpTian;
		}
		break;
		
		case waitPowerUpTian:
		{
			sensorStep = sendMsg;
		}
		break;
		
		
		case sendMsg:
		{
			getTian();
			sensorStep = waitTransfer;
		}
		break;
		
		
		case waitTransfer:
		{
			sensorStep = waitReply;
			readDelay = currentTime;
		}
		break;
		
		case waitReply:
		{
			if (currentTime - readDelay > 100)
			{
				if (modelDN == dm910H) // Если модель с влагой - измеряем. если без - прыгаем сразу на обработку замеров
				{
					sensorStep = requestRH;
				}
				
				else
				{
					readDelay = currentTime;
					sensorStep = averaging; // если нет сенсора влаги - то уравниваем время цикла за счет искуственной задержки.
				}
			}
		}
		break;
		

		// 		case receiveMsg:
		// 		{
		//
		// 			if (vipReplyQnt == 0)  // No reply
		// 			{
		// 				lostCntPressure++;
		// 				totalLostPressureCnt++;
		//
		// 				if (lostCntPressure > 10)
		// 				{
		// 					lostCntPressure = 20;
		// 					pressGaugeLost = 1;
		// 				}
		// 			}
		// 			else
		// 			{
		// 				pressGaugeLost = 0;
		// 				lostCntPressure = 0;
		// 			}
		// 			if (modelDN == dm910H) // Если модель с влагой - измеряем. если без - прыгаем сразу на обработку замеров
		// 			sensorStep = requestRH;
		// 			else
		// 			{
		// 				readDelay = currentTime;
		// 				sensorStep = timeEqualization; // если нет сенсора влаги - то уравниваем время цикла за счет искуственной задержки.
		// 			}
		// 		}
		// 		break;
		//
		
		case requestRH:
		{
			getSHT();
			sensorStep = averaging;
			
			// 			if (sht.requestData() == false)  // нет связи?
			// 			{
			// 				lostCntRH++;
			// 				totalLostRHCnt++;
			//
			// 				if (lostCntRH > 10)
			// 				{
			// 					lostCntRH = 20;
			// 					shtSensorFailure = 1; // set error humidity sensor
			// 				}
			// 				sensorStep = averaging; // Sensor not responded. Skip step "read form sht sensor"
			// 			}
			// 			else
			// 			{
			// 				sensorStep = receiveRH; // Sensor ok
			// 				lostCntRH = 0;
			// 				shtSensorFailure = 0; // set error humidity sensor
			// 			}
		}
		break;


		case timeEqualization:
		{
			if (currentTime - readDelay > 50)
			sensorStep = averaging;
		}
		break;

		case receiveRH:
		{
			// 			sht.readData();
			// 			SHT_RH_FL = sht.getHumidity();
			// 			SHT_Temp_FL = sht.getTemperature();
			// 			temperatureRAW = SHT_Temp_FL + offsetTemp;
			
			sensorStep = averaging;
		}
		break;
		
		
		case averaging:
		{
			digitalWrite(Power_Sensor, LOW);
			if (pressGaugeLost == 0 && lostCntPressure == 0)
			{
				// 				pressureVIP.bytes[3] = vipMB_ByteReply[0];
				// 				pressureVIP.bytes[2] = vipMB_ByteReply[1];
				// 				pressureVIP.bytes[1] = vipMB_ByteReply[2];
				// 				pressureVIP.bytes[0] = vipMB_ByteReply[3];
				//
				// 				themperatureVIP.bytes[3] = vipMB_ByteReply[4];
				// 				themperatureVIP.bytes[2] = vipMB_ByteReply[5];
				// 				themperatureVIP.bytes[1] = vipMB_ByteReply[6];
				// 				themperatureVIP.bytes[0] = vipMB_ByteReply[7];
				//
				// 				pressureRAW = 100 + pressureVIP.val;
				//	temperatureRAW = themperatureVIP.val + offsetTemp;
				
				// 				if (getTempFromSHT == 1)
				// 				temperatureRAW = SHT_Temp_FL + offsetTemp;
				// 				else
				// 				temperatureRAW = themperatureVIP.val + offsetTemp;
				
				
				if (VIP_firstRead == 0)
				//initArrays(pressureRAW, temperatureRAW, SHT_RH_FL);
				initArrays(pressureTian, temperatureTian, SHT_RH_FL);
			}
			
			//	if (getTempFromSHT == 1)
			//	temperatureRAW = SHT_Temp_FL + offsetTemp;

			checkError();
			//avarageIt(pressureRAW,temperatureRAW, SHT_RH_FL);
			avarageIt(pressureTian, temperatureTian, SHT_RH_FL);
			
			sensorStep = trending;
		}
		break;
		
		
		case trending:
		{
			trendBuffer();
			sensorStep = noiseCancel;
		}

		
		break;
		
		case noiseCancel:
		{
			PIDfilter();
			pressure = PIDFILTER[0];
			temperature = PIDFILTER[1];
			RH_RH = PIDFILTER[2];

			sensorStep = calculate;
		}
		break;
		
		// 	case gapCancel:
		//
		// 	noiseGAPFILTERED();
		// 	pressure = GAPFILTERED[0];
		// 	temperature = GAPFILTERED[1];
		//
		// 	sensorStep = calculate;
		//
		// 	break;
		
		
		case calculate:
		{
			pressure_Bar = pressure * 0.01;
			pressure_MPa = pressure_Bar *0.1;
			pressure_Pa = 1000.0 * pressure;
			pressure_kPa = pressure;
			pressure_Psi = pressure * 0.145038;
			pressure_Ncm2 = pressure * 0.1;
			
			temperatureC = temperature;
			temperatureK = temperature+273.15;
			temperatureF = temperature*1.8+32.0;
			
			float buffer;
			buffer = 293.15/ temperatureK;

			
			pressure_Bar20 = pressure_Bar * buffer;
			pressure_Bar20Rel = pressure_Bar20 - 1.0;
			pressure_MPa20 = pressure_Bar20 * 0.1;
			pressure_MPa20Rel = pressure_MPa20 - 0.1;
			
			gasDensityG = pressure_Bar20 * densitySetFlt;
			
			if (modelDN == dm910H)  // Если модель с влагой - производим расчет.
			getDewPoint();
			
			sensorStep = dmReady;
		}
		break;
		
		
		case pollDelay:
		
		break;
		
		default:
		
		break;
		
		
	}

}

//
// void noiseGAPFILTERED()
// {
// 	uint8_t const strikeLimit = 10;
//
// 	for (uint8_t i = 0; i <= 1; i++)
// 	{
// 		if (PIDFILTER[i] > GAPFILTERED[i] + topLevel)
// 		{
// 			STRIKE_H[i] ++;
// 			STRIKE_L[i] = 0;
//
// 		}
// 		else if (PIDFILTER[i] < GAPFILTERED[i] - topLevel)
// 		{
// 			STRIKE_H[i] = 0;
// 			STRIKE_L[i]++;
// 		}
//
// 		else
// 		{
// 			STRIKE_H[i] = 0;
// 			STRIKE_L[i] = 0;
// 		}
//
//
// 		if ((STRIKE_H[i] > strikeLimit) || (STRIKE_L[i] > strikeLimit))
// 		GAPFILTERED[i] = 0.5 * (PIDFILTER[i] + GAPFILTERED[i]);
//
// 	}
// }


void initArrays(float pressVal_, float tempVal_, float RHVal_)
{
	uint8_t i;
	VIP_firstRead = 1;
	// avaraged buffer
	for (i=0; i<=4; i++)
	{
		pressAvar[i] = pressVal_;
		tempAvar[i] = tempVal_;
		RHAvar[i] = RHVal_;
	}
	
	for (i=0; i<=trendLimit;i++ )
	TREND[0][i] = pressVal_;

	for (i=0; i<=trendLimit;i++ )
	TREND[1][i] = tempVal_;
	
	for (i=0; i<=trendLimit;i++ )
	TREND[2][i] = RHVal_;
	
	PIDFILTER[0] = pressVal_;
	PIDFILTER[1] = tempVal_;
	PIDFILTER[2] = RHVal_;
	
}

void trendBuffer()
{
	tail++;
	if (tail>trendLimit)
	tail = 0;

	for (uint8_t i = 0; i<=2; i++)
	TREND[i][tail] = AVARAGED[i];
}






void checkError()
{
	
	if (!pressGaugeLost)
	{
		if (pressureRAW < 0)  // Negative pressure
		errorInt |= 1;

		
		if (pressureRAW > 5000)  // Extremely high pressure
		errorInt |= 1 << 1;
		
		
		if (pressureRAW > 10000 || pressureRAW < -100)   // pressure sensor fault
		errorInt |= 1 << 2;

		
		if (temperatureRAW < (-60))
		errorInt |= 1 << 3;
		
		if (temperatureRAW > 80)
		errorInt |= 1 << 4;
		
		if (gasDensityG > liquefaction)
		errorInt |= 1 << 6;
		
		if (gasDensityG > overdensity)
		errorInt |= 1 << 7;
		
		if (pressure_Bar20 < alarmLevel)
		errorInt |= 1 << 11;
		
		if ((pressure_Bar20 > alarmLevel) && (pressure_Bar20 < warningLevel))
		errorInt |= 1 << 12;
	}
	

	// 	if (pressGaugeLost != 0)		// Comm error
	// 	{
	// 		pressGaugeLost = 2;
	// 		errorInt |= 1 << 5;
	// 		pressureTian = -100.0;
	// 		temperatureTian = -100.0;
	// 		initArrays(pressureTian, temperatureTian, SHT_RH_FL);
	// 	}


	
	// 	if (shtSensorLost)
	// 	{
	// 		errorInt |= 1 << 9;
	// 		SHT_RH_FL = -100;
	// 		SHT_Temp_FL = -100;
	// 	}
	//
	
	
	
	if (negativeRHflag)
	errorInt |= 1 << 13;
	
	
}


void PIDfilter()
{
	float noiseMax = -1000.0;
	float noiseMin = 100000.0;
	float powerShift;

	for (uint8_t i = 0; i <= 2; i++)
	{
		noiseMax = -1000.0;
		noiseMin = 100000.0;
		for (uint8_t j = 0; j < 5; j++)
		{
			if (TREND[i][j] > noiseMax)
			noiseMax = TREND[i][j];
			if (TREND[i][j] < noiseMin)
			noiseMin = TREND[i][j];
		}

		noiseMin = noiseMin * 0.99;
		noiseMax = noiseMax * 1.01;


		if  (PIDFILTER[i] > noiseMin &&  noiseMax > PIDFILTER[i])
		powerShift = SMOOTHfilter[i];
		else
		powerShift = AGGRESSfilter[i];

		PIDFILTER[i] += powerShift * (1.0 * AVARAGED[i] - PIDFILTER[i]);
	}

}





void loop_50()
{
	if (currentTime - loop_50_start > loop_50ms)
	{
		loop_50_start = currentTime;
		mainDM();
		
		
		if (reboot)
		rebootDM();
		

		
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

void loop_5000()
{
	if (currentTime - IGAS_mb_X.incomingWDT > incomingTimeOut) // если входящих сообщений нет более incomingTimeOut - значит возможно сенсор завис. перезагружаем CPU
	{
		
		wdt_enable(WDTO_15MS);
		for (;;)
		{
		}
	}
}


uint8_t readWrite = 0;
uint8_t PWM_SP = 0;

void loop_500()
{
	if (currentTime - loop_500_start > loop_500ms)
	{
		loop_500_start = currentTime;
		if (sensorStep == dmReady)
		sensorStep = powerUpTian;
		
		watchdogQnt++;
		// Heater
		//	PWM_SP = pid_control(tempSetPoint, temperatureTian);	// Вычисляем для неё новое значение PID.
		
		// 		if (enableCPUheat)
		// 		analogWrite(Heat_CPU, PWM_SP);
		//
		//
		// 		analogWrite(Heat_Sensor, PWM_SP);
		//
		// 		Heat_CPU_Current = analogRead(A0);
		//CPU_Temp_Sensor = analogRead(A1);
		
		//DS_proceed();
	}
}

uint8_t BufWT[10];
uint8_t BufSHT[10];
uint32_t ad;
uint32_t ad2;
uint8_t addrWT = 0x28;

// void initTian()
// {
// 	// Continuous Working Mode
// 	Wire.beginTransmission(addrWT);
// 	Wire.write(0x30);
// 	Wire.write(0x00);
// 	Wire.endTransmission();
// 	delay(40);
//
//
// 	//  Continuous Working Mode
// 	Wire.beginTransmission(addrWT);
// 	Wire.write(0x30);
// 	Wire.write(0x03);
// 	Wire.endTransmission();
// 	delay(40);
//
// }






void getTian()  // прочитать данные с сенсора WT - давление и температура
{

	Wire.beginTransmission(addrWT);
	Wire.write(0x50);
	pressGaugeLost = Wire.endTransmission();
	
	
	if (pressGaugeLost == 0)
	{
		Wire.requestFrom((uint8_t)addrWT, static_cast<size_t>(4));

		for (uint8_t i = 0; i < 4; i++) {
			BufWT[i] = Wire.read();
		}


		// Start Proper calculations
		// 		RAW_pressureTian = (BufWT[0] & B00111111)<<8 | BufWT[1];
		//
		// 		pressureTian = 100.0 + 100.0*(((RAW_pressureTian-tian_Offset) * tianTerm1) - tian_Pmin);
		//
		// 		RAW_temperatureTian = (BufWT[2]<<3) | ((BufWT[3] >> 5) & B00000111);
		//
		// 		temperatureTian = offsetTemp + 200.0*RAW_temperatureTian/2047-50;
		
		// FIN Proper calculations
		
		RAW_pressureTian = (BufWT[0] & B00111111)<<8 | BufWT[1];

		pressureTian = 100.0 + 100.0*(((RAW_pressureTian-1638) * 0.001602) - 1.0);
		
		RAW_temperatureTian = (BufWT[2]<<3) | ((BufWT[3] >> 5) & B00000111);
		
		temperatureTian = 200.0*RAW_temperatureTian/2047-50;
		
	}
	delayMicroseconds(150);

}



void getSHT()  // прочитать данные с сенсора WT - давление и температура
{
	
	Wire.beginTransmission(SHT31_ADDRESS);
	Wire.write(SHT31_MEASUREMENT_SLOW >> 8 );
	Wire.write(SHT31_MEASUREMENT_SLOW & 0xFF);
	shtSensorLost = Wire.endTransmission();

	
	
	delayMicroseconds(150);
	
	if (shtSensorLost == 0)
	{
		Wire.requestFrom((uint8_t)SHT31_ADDRESS, static_cast<size_t>(6));

		for (uint8_t i = 0; i < 6; i++) {
			BufSHT[i] = Wire.read();
		}

		if (BufSHT[2] != crc8(BufSHT, 2))
		{
			shtSensorLost = 88;
			return;
		}
		if (BufSHT[5] != crc8(BufSHT + 3, 2))
		{
			shtSensorLost = 88;
			return;
		}
		
		
		
		_rawTemperature = (BufSHT[0] << 8) + BufSHT[1];
		_rawHumidity  = (BufSHT[3] << 8) + BufSHT[4];
		
		SHT_Temp_FL = (_rawTemperature * 175.0 / 65535.0) - 45;
		SHT_RH_FL  = (_rawHumidity * 100.0 / 65535.0);
		
	}


}


uint8_t crc8(const uint8_t *data, uint8_t len)
{
	// CRC-8 formula from page 14 of SHT spec pdf
	const uint8_t POLY(0x31);
	uint8_t crc(0xFF);

	for (uint8_t j = len; j; --j)
	{
		crc ^= *data++;

		for (uint8_t i = 8; i; --i)
		{
			crc = (crc & 0x80) ? (crc << 1) ^ POLY : (crc << 1);
		}
	}
	return crc;
}

void avarageIt(float press_, float temp_,float RH_)
{
	float bufferP_ = 0;
	float bufferT_ = 0;
	float bufferRH_ = 0;
	
	pressAvar[avarPosition] = press_;
	tempAvar[avarPosition] = temp_;
	RHAvar[avarPosition] = RH_;
	avarPosition++;
	
	if (avarPosition > 4)
	avarPosition = 0;
	
	for (uint8_t i=0; i<=4; i++)
	{
		bufferP_ +=  pressAvar[i];
		bufferT_ +=  tempAvar[i];
		bufferRH_ += RHAvar[i];
		
	}
	
	AVARAGED[0] = bufferP_*0.2;
	AVARAGED[1] = bufferT_*0.2;
	AVARAGED[2] = bufferRH_*0.2;
}





/*
void getReply(uint8_t * serBuf)
{
uint8_t i=1;

while (altSerial.available())
{
i++;
if (i>50)
{
i=60;
serBuf[0] = 50;
return;
}

serBuf[i] = altSerial.read();
}
serBuf[0] = i;

}
*/


float byte2Float(uint8_t Bytex1, uint8_t Bytex2,uint8_t Bytex3,uint8_t Bytex4)
{
	float flt_;
	
	uint8_t BB[] = {Bytex1,Bytex2,Bytex3,Bytex4};
	// BB[4] = {0x41, 0xE6, 0x6C, 0};
	memcpy(&flt_, &BB, sizeof(flt_));
	
	return flt_;
}




void initCommunication()
{
	unsigned int baud_;
	uint8_t parity_ = 0;
	
	baud_ =  getBaud(baudRate);
	
	if (parity == 1)
	parity_ = SERIAL_8E1;
	else
	parity_ = SERIAL_8N1;
	
	IGAS_mb_X.initMB(baud_, parity_, 1001);
	IGAS_mb_X.setDelegate(modbusProceed);        // To run library function from main loop
	IGAS_mb_X.slave = slaveAddr;
	//	IGAS_mb_X.slave = 1;
	//	altSerial.begin(9600, SERIAL_8E1);
	

	
	//	I2c.begin();
	//PORTB |= _BV(PORTB4);
}




unsigned long getBaud(uint8_t baudRate_)
{
	unsigned long retBaud = 0;
	
	switch (baudRate_) // addr 101, def 3 - 9600
	{
		case 1:
		retBaud = 2400;
		break;
		
		case 2:
		retBaud = 4800;
		break;
		
		case 3:
		retBaud = 9600;
		break;
		
		case 4:
		retBaud = 14400;
		break;
		
		case 5:
		retBaud = 19200;
		break;
		
		case 6:
		retBaud = 38400;
		break;
		
		case 7:
		retBaud = 56000;
		break;
		
		default:
		retBaud = 9600;
		break;
	}
	
	return retBaud;
}





void modbusProceed(int *regs,  int start_addr_,  int reg_count_, int Value_)
{
	int registerAddr_ = start_addr_;
	//	uint8_t addr_;
	uint8_t writeReg = 0;
	if (IGAS_mb_X.func == FC_WRITE_REGS || IGAS_mb_X.func == FC_WRITE_REG)
	writeReg = 1;
	
	IGAS_mb_X.incomingWDT = currentTime;  // если есть адрессное входящее сообщение - сбрасываем таймер WDT
	
	for (uint8_t i = 0; i <= reg_count_ - 1; i++)
	{
		switch (registerAddr_)
		{
			// Bar abs. pressure_Bar
			
			case 0:
			
			regs[i] = convertFloat(pressure_Bar, 0);

			break;
			
			
			case 1:
			regs[i] = convertFloat(pressure_Bar, 1);

			break;
			
			// MPa abs.
			case 2:

			regs[i] = convertFloat(pressure_MPa, 0);

			break;
			
			// pressure_MPa
			case 3:
			regs[i] = convertFloat(pressure_MPa, 1);

			break;
			
			// Pa abs.
			case 4:
			
			regs[i] = convertFloat(pressure_Pa, 0);

			break;
			
			
			case 5:
			
			regs[i] = convertFloat(pressure_Pa, 1);

			break;
			
			// kPa abs.
			case 6:
			
			regs[i] = convertFloat(pressure_kPa, 0);

			break;
			
			// kPa abs.
			case 7:

			regs[i] = convertFloat(pressure_kPa, 1);

			break;
			
			
			// Psi abs.
			case 8:
			

			regs[i] = convertFloat(pressure_Psi, 0);

			break;
			
			
			case 9:

			regs[i] = convertFloat(pressure_Psi, 1);

			break;
			
			
			// N/cm2 abs.
			case 10:
			
			regs[i] = convertFloat(pressure_Ncm2, 0);

			break;
			
			
			case 11:

			regs[i] = convertFloat(pressure_Ncm2, 1);

			break;
			
			
			// Temp C
			case 12:
			
			regs[i] = convertFloat(temperatureC, 0);
			
			break;
			
			
			case 13:

			regs[i] = convertFloat(temperatureC, 1);

			break;
			
			// Temp K
			case 14:
			
			regs[i] = convertFloat(temperatureK, 0);

			break;
			
			
			case 15:

			regs[i] = convertFloat(temperatureK, 1);

			break;
			
			// Temp F
			case 16:
			

			regs[i] = convertFloat(temperatureF, 0);

			break;
			
			
			case 17:

			regs[i] = convertFloat(temperatureF, 1);

			break;
			
			// Denc g/l
			case 18:
			

			regs[i] = convertFloat(gasDensityG, 0);

			break;
			
			
			case 19:

			regs[i] = convertFloat(gasDensityG, 1);

			break;
			
			
			// Denc kg/m3
			case 20:
			

			regs[i] = convertFloat(gasDensityG, 0);

			break;
			
			
			case 21:

			regs[i] = convertFloat(gasDensityG, 1);

			break;


			// Pressure  Bar 20
			case 22:
			

			regs[i] = convertFloat(pressure_Bar20, 0);

			break;
			
			
			case 23:

			regs[i] = convertFloat(pressure_Bar20, 1);

			break;
			
			// For Humidity version Reserved!
			//***************
			//
			case 24:

			regs[i] = convertFloat(frostPointAtm, 0);

			break;
			
			
			case 25:

			regs[i] = convertFloat(frostPointAtm, 1);

			break;
			
			
			case 26:

			regs[i] = convertFloat(dewPointAtm, 0);

			break;
			
			
			case 27:

			regs[i] = convertFloat(dewPointAtm, 1);

			break;
			
			
			case 28:

			regs[i] = convertFloat(frostPoint, 0);

			break;
			
			
			case 29:

			regs[i] = convertFloat(frostPoint, 1);

			break;
			
			
			
			case 30:

			regs[i] = convertFloat(dewPoint, 0);

			break;
			
			
			case 31:

			regs[i] = convertFloat(dewPoint, 1);

			break;
			
			
			// Start Not proper case!
			

			// Finish Not proper case!

			case 40:

			regs[i] = convertFloat(PPMv, 0); //convertFloat(RH_RH, 0);

			break;
			
			case 41:

			regs[i] = convertFloat(PPMv, 1);

			break;
			
			case 42:

			regs[i] = convertFloat(PPMm, 0); //convertFloat(RH_RH, 0);

			break;
			
			case 43:

			regs[i] = convertFloat(PPMm, 1);

			break;
			
			
			
			case 48:

			regs[i] = convertFloat(RH_Tuned, 0); //convertFloat(RH_RH, 0);

			break;
			
			case 49:

			regs[i] = convertFloat(RH_Tuned, 1);

			break;
			
			
			

			// Pressure  Bar 20 relative
			case 58:
			
			regs[i] = convertFloat(pressure_Bar20Rel, 0);

			break;
			
			
			case 59:

			regs[i] = convertFloat(pressure_Bar20Rel, 1);

			break;
			
			// Pressure  Mpa 20
			case 60:
			

			regs[i] = convertFloat(pressure_MPa20, 0);

			break;
			
			
			case 61:

			regs[i] = convertFloat(pressure_MPa20, 1);

			break;
			
			// Pressure  Bar 20 relative
			case 62:

			regs[i] = convertFloat(pressure_MPa20Rel, 0);

			break;
			
			
			case 63:

			regs[i] = convertFloat(pressure_MPa20Rel, 1);

			break;
			
			
			case 70:
			
			regs[i] = convertFloat(pressureRAW,0);
			
			break;
			
			case 71:
			
			regs[i] = convertFloat(pressureRAW,1);
			
			break;
			
			case 72:
			
			regs[i] = convertFloat(temperatureRAW,0);
			
			break;
			
			case 73:
			
			regs[i] = convertFloat(temperatureRAW,1);
			
			break;
			
			case 74:
			
			regs[i] = convertFloat(pressure,0);
			
			break;
			
			case 75:
			
			regs[i] = convertFloat(pressure,1);
			
			break;
			
			case 76:
			
			regs[i] = convertFloat(temperature,0);
			
			break;
			
			case 77:
			
			regs[i] = convertFloat(temperature,1);
			
			break;
			
			
			case 78:
			
			regs[i] =  lostCntPressure;  // Счетчик потерь пакетов с последней успешной связи

			break;
			
			
			case 79:
			
			regs[i] =  totalLostPressureCnt;	// Общий счетчик потерянных пакетов со времени включения питания

			break;
			
			case 80:
			
			regs[i] = pressGaugeLost;

			break;
			
			
			case 81:
			
			regs[i] = lostCntRH;

			break;
			
			case 82:
			
			regs[i] =  totalLostRHCnt;

			break;
			
			case 83:
			
			regs[i] =  shtSensorLost;

			break;



			case 99:
			// 			if (writeReg)
			// 			{
			// 				if (Value_ == 1) HEATER_ON();
			// 				if (Value_ == 0) HEATER_OFF();
			// 			};
			regs[i] =  Value_;
			break;
			
			// ************************************ Прошивка серийного номера устройства **********************************

			// Slave address
			case 100:

			if (writeReg)
			{
				slaveAddr = Value_;
				IGAS_mb_X.slave = slaveAddr;
				EEPROM.write(100, slaveAddr);
			}
			
			regs[i] = slaveAddr;

			break;
			
			
			// Baud rate
			case 101:

			if (writeReg)
			{
				baudRate = Value_;
				EEPROM.write(101, baudRate);
			}
			regs[i] = baudRate;

			break;
			
			
			// Parity
			case 102:

			if (writeReg)
			{
				parity = Value_;
				EEPROM.write(102, parity);
			}

			regs[i] = parity;

			break;
			
			case 104:

			if (writeReg)
			{
				densitySet = Value_;
				saveInt(104, densitySet);
			}
			
			regs[i] = densitySet;

			break;
			
			case 105:

			if (writeReg)
			{
				gasPartner = Value_;
				EEPROM.write(105, gasPartner);
			}
			
			regs[i] = gasPartner;

			break;
			
			
			case 106:

			if (writeReg && pinCode)
			{
				serialHigh = Value_;
				saveInt(108, serialHigh);
			}
			
			regs[i] = serialHigh;

			break;
			
			case 107:

			if (writeReg && pinCode)
			{
				serialLow = Value_;
				saveInt(106, serialLow);
			}
			regs[i] = serialLow;

			break;
			

			case 110:

			/*
			if (writeReg && pinCode)
			{
			hVersion = Value_;
			EEPROM.write(110, hVersion);
			}
			*/
			regs[i] = hVersion;

			break;

			case 111:
			//
			// 			if (writeReg && pinCode)
			// 			{
			// 				sVersion = Value_;
			// 				EEPROM.write(111, sVersion);
			// 			}
			regs[i] = sVersion;
			
			break;
			
			
			case 112:

			if (writeReg && pinCode)
			{
		 				modelDN = Value_;
								EEPROM.write(112, modelDN);
				
				
			}
			regs[i] = modelDN;

			break;
			
			case 113:

			if (writeReg && pinCode)
			{
				getTempFromSHT = Value_;
				EEPROM.write(113, getTempFromSHT);
				
				
			}
			regs[i] = getTempFromSHT;

			break;
			
			
			
			case 130:
			
			if (writeReg && pinCode)
			{
				EEPROM.write(132,(Value_ >> 8));
				EEPROM.write(133,(Value_ & 0x00FF));
			}
			
			regs[i] = convertFloat(densitySetFlt, 0);
			
			break;
			
			case 131:
			
			if (writeReg && pinCode)
			{
				EEPROM.write(130,(Value_ >> 8));
				EEPROM.write(131,(Value_ & 0x00FF));
				densitySetFlt = loadFloat(130, 6.176);
			}
			
			regs[i] = convertFloat(densitySetFlt, 1);
			
			break;
			
			
			case 134:
			
			if (writeReg)
			EEPROM.write(134,liquefaction);

			regs[i] = liquefaction;
			
			break;
			
			case 135:
			
			if (writeReg)
			EEPROM.write(135,overdensity);

			regs[i] = overdensity;
			
			break;
			
			
			case 136:
			
			if (writeReg)
			EEPROM.write(136,warningLevel);

			regs[i] = warningLevel;
			
			break;
			
			case 137:
			
			if (writeReg)
			EEPROM.write(137,alarmLevel);

			regs[i] = alarmLevel;
			
			break;
			
			
			case 138:
			
			if (writeReg && pinCode)
			{
				offsetTemp = Value_ * 0.1;
				saveSignedInt(138,Value_);
			}
			
			regs[i] = offsetTemp * 10;
			
			break;
			
			case 139:
			
			if (writeReg && pinCode)
			{
				offsetHumidity_b = Value_ * 0.01;
				saveSignedInt(141,Value_);
			}
			
			regs[i] = offsetHumidity_b * 100;
			
			break;
			
			
			
			case 140:  // Humidity Calibration
			
			if (writeReg && pinCode)
			{
				offsetHumidity_K = Value_ * 0.001;
				saveSignedInt(144,Value_);
			}
			
			regs[i] = offsetHumidity_K * 1000;
			
			break;
			
			
			
			case 141:   // Humidity Calibration
			
			if (writeReg && pinCode)
			{
				offsetHumidity_K = float(Value_*0.01f - offsetHumidity_b) / RH_RH;
				
				saveSignedInt(144,offsetHumidity_K * 1000);
			}
			
			regs[i] = offsetHumidity_K * 1000;
			
			break;
			
			
			
			case 142:  // Calibr Zero
			
			if (writeReg && pinCode)
			{
				offsetHumidity_b = 0.01 - offsetHumidity_K * RH_RH;
				saveSignedInt(141,offsetHumidity_b*100);
			}
			
			regs[i] = offsetHumidity_b * 100;
			
			break;
			
			
			case 143:   // Reset Calibration
			
			if (writeReg && pinCode)
			{
				offsetHumidity_b = 0.0;
				saveSignedInt(141,offsetHumidity_b*100);
				offsetHumidity_K = 1.0;
				saveSignedInt(144,offsetHumidity_K * 1000);
				regs[i] = 1;
			}
			
			regs[i] = 0;
			
			break;
			

			
// 			case 148:
// 			
// 			if (writeReg && pinCode)
// 			{
// 				tian_Pmin = Value_;
// 				EEPROM.write(148, tian_Pmin);
// 				tianTerm1 = (1.0 * (tian_Pmax+tian_Pmin))/(1.0*tian_K);
// 			}
// 			
// 			regs[i] = tian_Pmin;
// 			
// 			break;
// 			
// 			case 149:
// 			
// 			if (writeReg && pinCode)
// 			{
// 				tian_Pmax = Value_;
// 				EEPROM.write(149, tian_Pmax);
// 				tianTerm1 = (1.0 * (tian_Pmax+tian_Pmin))/(1.0*tian_K);
// 			}
// 
// 			regs[i] = tian_Pmax;
// 			
// 			break;
// 			
// 			
// 			case 150:
// 			
// 			if (writeReg && pinCode)
// 			{
// 				tian_K = Value_;
// 				saveInt(150, tian_K);
// 				tianTerm1 = (1.0 * (tian_Pmax+tian_Pmin))/(1.0*tian_K);
// 			}
// 
// 			regs[i] = tian_K;
// 			
// 			break;
// 			
// 			
// 			case 151:
// 			
// 			if (writeReg && pinCode)
// 			{
// 				tian_Offset = Value_;
// 				saveInt(150, tian_Offset);
// 			}
// 			regs[i] = tian_Offset;
// 			
// 			break;
// 			
			
			
// 			
// 			case 152:
// 
// 			if (writeReg && pinCode)
// 			{
// 				EEPROM.write(156,(Value_ >> 8));
// 				EEPROM.write(157,(Value_ & 0x00FF));
// 			}
// 			
// 			regs[i] = convertFloat(tempSetPoint, 0);
// 
// 			break;
// 			
// 			
// 			
// 			case 153:
// 			
// 			if (writeReg && pinCode)
// 			{
// 				EEPROM.write(154,(Value_ >> 8));
// 				EEPROM.write(155,(Value_ & 0x00FF));
// 				
// 				tempSetPoint = loadFloat(154, -20.0);
// 			}
// 			
// 			regs[i] = convertFloat(tempSetPoint, 1);
// 			
// 			break;
// 			
			
			
// 			case 154:
// 			
// 			regs[i] = convertFloat(DS18B20_Temp, 0);
// 
// 			break;
// 			
// 			
// 			
// 			case 155:
// 			
// 			regs[i] = convertFloat(DS18B20_Temp, 1);
// 			
// 			break;
			
			
// 			case 156:
// 			
// 			regs[i] = PWM_SP;
// 			
// 			break;
// 			
// 			case 157:
// 			
// 			regs[i] = Heat_CPU_Current;
// 			
// 			break;
// 			
// 			case 158:
// 			
// 			if (writeReg && pinCode)
// 			{
// 				EEPROM.write(159,Value_);
// 				enableCPUheat = Value_;
// 				analogWrite(Heat_CPU, 0);
// 			}
// 			regs[i] = enableCPUheat;
// 			
// 			break;
// 			
// 
// 			case 161:
// 			
// 			if (writeReg && pinCode)
// 			{
// 				saveInt(161, Value_);
// 				incomingTimeOut = Value_;
// 
// 			}
// 			regs[i] = incomingTimeOut;
// 			
// 			break;
// 			
// 			
// 			
// 			case 180 ... 185:
// 			
// 			regs[i] = BufSHT[registerAddr_ - 180];
// 
// 			break;
// 			
// 			case 186:
// 			
// 			regs[i] = _rawHumidity;
// 
// 			break;
// 			
// 			case 187:
// 			
// 			regs[i] = _rawTemperature;
// 
// 			break;
// 			
// 			case 188:
// 			
// 			regs[i] = RAW_pressureTian;
// 
// 			break;
// 			
// 			case 189:
// 			
// 			regs[i] = RAW_temperatureTian;
// 
// 			break;
// 			
// 			
// 
// 			
// 			
// 			case 190 ... 193:
// 			
// 			regs[i] = BufWT[registerAddr_ - 190];
// 
// 			break;
// 			
// 			
// 			
// 			
// 			case 194 ... 195: // pressureTian
// 			
// 			regs[i] = convertFloat(pressureTian,registerAddr_ - 194);
// 			
// 
// 			break;
// 			
// 			case 196 ... 197:   // temperatureTian
// 			
// 			regs[i] = convertFloat(temperatureTian,registerAddr_ - 196);
// 
// 			break;
// 			

			
			
			
			case 200:   // Reboot
			
			regs[i] = errorInt;

			break;
			
			case 201:   // Reset error
			
			if (writeReg)
			if (Value_ == 1)
			errorInt = 0;

			break;
			
			
			case 202:   // Reboot and apply new settings
			
			if (writeReg)
			if (Value_ == 1)
			{
				reboot = 1;
				apply = 1;
			}
			
			regs[i] = 0;
			
			break;
			
// 			case 399:
// 
// 			regs[i] = vipReplyQnt;
// 
// 			break;
// 			
// 			
// 			case 400 ... 438:
// 			
// 			regs[i] = vipRespond[registerAddr_ - 400];
// 
// 			break;
// 			
// 			
// 			
// 			
// 			case 450 ... 472:
// 			
// 			regs[i] = vipMB_ByteReply[registerAddr_ - 450];
// 
// 			break;
// 			
// 			
// 			case 530:
// 
// 			regs[i] = convertFloat(SHT_Temp_FL, 0);
// 
// 			break;
// 			
// 			case 531:
// 
// 			regs[i] = convertFloat(SHT_Temp_FL, 1);
// 
// 			break;
// 			
// 			
// 			case 532:
// 
// 			regs[i] = convertFloat(SHT_RH_FL, 0);
// 
// 			break;
// 			
// 			case 533:
// 
// 			regs[i] = convertFloat(SHT_RH_FL, 1);
// 
// 			break;
// 			
// 			
// 			case 594:   // Reboot
// 			
// 			if (writeReg)
// 			if (Value_ == 1)
// 			reboot = 1;
// 
// 			regs[i] = 0;
// 
// 			break;
// 			
// 			case 600:
// 			
// 			if (writeReg)
// 			if (Value_ == 1)
// 			bridgeMode = 1;
// 
// 			regs[i] = bridgeMode*2;
// 			
// 			break;
// 			
			
			case 777:   // heartbit

			regs[i] = watchdogQnt;

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
			
			
			
			
			case 900:   // BootLoader

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



/*
int convertFloat(float flt_, uint8_t oreder_)
{
uint8_t bytes_[4];

*(float*)(bytes_) = flt_;
return (bytes_[1 + 2*oreder_]<<8 | bytes_[2*oreder_]);

}

*/


int convertFloat(float flt_, uint8_t oreder_)
{
	union {
		float flt;
		int32_t i32;
		uint8_t bytes[4];
	} u;
	
	u.flt = flt_;
	
	return (u.bytes[1 + 2 * oreder_] << 8) | u.bytes[2 * oreder_];
}




void settings()
{

	
	// Get slave Address
	slaveAddr = loadByte(100, 1);  // addr 100, def 1 - 1

	// Baud rate
	baudRate = loadByte(101, 3);
	
	//Parity

	parity = loadByte(102, 0);  // addr 102, def 0 - NONE
	

	




	// Density

	densitySet = loadInt(104, 617);  // addr 104-105, def 617 - SF6

	gasPartner = loadByte(105, 0);
	// Serial number
	serialLow = loadInt(106, 0xAAAA);  // addr 106-107, def FFFF - Year
	serialHigh = loadInt(108, 0xAAAA);  // addr 108-109, def FFFF - Number
	
	
	//hVersion = loadByte(110, 0xFF);  // addr 110, def FF
	// sVersion = loadByte(111, 0xFF);  // addr 111, def FF
modelDN = loadByte(112, 0);		// addr 112, def 0 - DN910 non humidity
	getTempFromSHT = loadByte(113, 0);		// addr 113, def 0 - DN910 read Temperature from pressure sensor
	
	
	
	
	// 	bufferFloat.bytes[3] = loadByte(130, 0x40);
	// 	bufferFloat.bytes[2] = loadByte(131, 0xC5);
	// 	bufferFloat.bytes[1] = loadByte(132, 0xA4);
	// 	bufferFloat.bytes[0] = loadByte(133, 0x70);
	//
	// 	densitySetFlt = bufferFloat.val;
	
	densitySetFlt = loadFloat(130, 6.176);
	
	
	liquefaction = loadByte(134, 133);
	overdensity = loadByte(135, 80);
	warningLevel = loadByte(136, 60);
	alarmLevel = loadByte(137, 50);
	offsetTemp = 0.1 * loadSignedInt(138);
	offsetHumidity_b =  0.01 * loadSignedInt(141);  // Koeff b
	offsetHumidity_K = 0.001 * loadSignedInt(144);
	
	if (offsetHumidity_K == 0)
	offsetHumidity_K = 1;
	
	
	tian_Pmin = loadByte(148,1);
	//tian_Pmin = 1.0;
	tian_Pmax = loadByte(149,20);
	
	tian_K = loadInt(150, 13110);
	tian_Offset = loadInt(152, 1638);
	
	
	tianTerm1 = (1.0 * (tian_Pmax+tian_Pmin))/(1.0*tian_K);
	tianTerm1 = 0.001602;
	
	
	tempSetPoint = loadFloat(154, -20.0);
	
	enableCPUheat = loadByte(159,0);
	
	
	incomingTimeOut = loadInt(161, 5000);
	
}


uint8_t loadByte(uint8_t addr_, uint8_t defValue_)
{
	uint8_t eepromBuffer;
	
	eepromBuffer = EEPROM.read(addr_);
	
	if (defValue_ == eepromBuffer)
	return eepromBuffer;
	
	if (eepromBuffer == 0xFF)
	{
		EEPROM.write(addr_,defValue_);
		return defValue_;
	}
	return eepromBuffer;
}

//
// unsigned int loadByteInt(int addr_, int defValue_)
// {
// 	unsigned int  eepromBuffer;
//
// 	eepromBuffer = loadInt(addr_);
//
// 	if (eepromBuffer == 0xFFFF || eepromBuffer == 0)
// 	{
// 		saveInt(addr_,defValue_);
//
// 		return defValue_;
// 	}
// 	return eepromBuffer;
// }



float loadFloat(int addr_, float default_)
{
	floatval castFloat_;
	uint8_t error_ = 0;
	
	for (uint8_t i=0; i<=3; i++)
	{
		castFloat_.bytes[3-i] = EEPROM.read(addr_+i);
		if (EEPROM.read(addr_+i) == 0xFF)
		error_++;
	}
	if (error_ == 4)
	return default_;
	
	return castFloat_.val;
}



int loadInt(uint16_t addr_, uint16_t default_)
{
	uint8_t lowByte_;
	uint8_t highByte_;
	uint16_t castInt = 0;
	
	lowByte_ = EEPROM.read(addr_);
	highByte_ = EEPROM.read(addr_ + 1);

	if (lowByte_ == 0xFF && highByte_ == 0xFF)
	return default_;

	castInt = lowByte_ | highByte_;
	return (castInt);
}


void saveInt(int addr_, int digit_)
{
	EEPROM.write(addr_, digit_ & 0x00ff); // b1  ones
	EEPROM.write(addr_ + 1, digit_ >> 8); // b1  hundreds
}


int loadSignedInt(int addr_)
{
	unsigned int buffer_;

	buffer_ = EEPROM.read(addr_) | (EEPROM.read(addr_ + 1) << 8);
	if (buffer_ == 0xFFFF)
	buffer_ = 0;
	
	
	if (EEPROM.read(addr_ + 2) == 0)
	return (buffer_);
	else
	return (-1 * buffer_);
}



void saveSignedInt(int addr_, int digit_)
{
	uint8_t sign_;
	if (digit_ < 0)
	{
		digit_ = -1 * digit_;
		sign_ = 1;
	}
	else
	{
		sign_ = 0;
	}

	EEPROM.write(addr_, digit_ & 0x00ff); // b1  ones
	EEPROM.write(addr_ + 1, digit_ >> 8); // b1  hundreds
	EEPROM.write(addr_ + 2, sign_); // b1  hundreds

}



unsigned long Nowdt_[2] = {0};
unsigned int lastBytesReceived_[2];
const uint8_t interframe_delay_ = 20;  /* Modbus t3.5 = 2 ms */

uint8_t checkIncoming(uint8_t ch_, unsigned int length_)
{


	if (length_ == 0)
	{
		lastBytesReceived_[ch_] = 0;
		return 0;
	}


	if (lastBytesReceived_[ch_] != length_)
	{
		lastBytesReceived_[ch_] = length_;
		Nowdt_[ch_] = currentTime;
		return 0;
	}

	if (currentTime - Nowdt_[ch_] < interframe_delay_)
	return 0;

	lastBytesReceived_[ch_] = 0;
	return 1;

}




void getDewPoint()
{
	// 	if (shtSensorLost == 0)  // Sensor ok
	// 	{
	const float koef_A = 6.116441;
	const float koef_m = 7.591386;
	const float koef_Tn = 240.7263;
	const float Mw = 18.01528;
	const float Msf = 146.06;
	float Pwatm;
	float determinant_;

	RH_Tuned = offsetHumidity_K * RH_RH + offsetHumidity_b;

	// Добавлено в rev3 - RH может принимать отрицательные значения на измерителе влажности, чего не может быть физически.
	if (RH_Tuned<0.01)
	{
		RH_Tuned = 0.01;
		negativeRHflag = 1;
	}
	else
	negativeRHflag = 0;


	Pws = koef_A * pow(10, (koef_m* temperatureC / (temperatureC + koef_Tn)));
	Pw = Pws * RH_Tuned * 0.01;
	dewPoint = koef_Tn / ( (koef_m / (log10(Pw/koef_A))) - 1 );

	if (dewPoint < (-65.0))
	dewPoint = (-65.0);


	determinant_ = 1.286081 - 0.004152 * (0.009109 - dewPoint);
	frostPoint = (-1.134055 + sqrt(determinant_)) / 0.002076;
	if (frostPoint>0)
	frostPoint = 0;


	RH_RH_Atm = RH_Tuned * 100.0f/pressure;
	Pwatm = Pws * RH_RH_Atm * 0.01;
	dewPointAtm = koef_Tn / ( (koef_m / (log10(Pwatm/koef_A))) - 1 );

	determinant_ = 1.286081 - 0.004152 * (0.009109 - dewPointAtm);
	frostPointAtm = (-1.134055 + sqrt(determinant_)) / 0.002076;

	if (frostPointAtm>0)
	frostPointAtm = 0;




	PPMv = 1000.0f * 1000.0f * Pw / (pressure * 10.0f - Pw);
	PPMm = PPMv * Mw/Msf;
	// 	}
	// 	else
	// 	{
	// 		RH_Tuned = -100;
	// 		dewPoint = -100;
	// 		frostPoint = -100;
	//
	//
	// 		RH_RH_Atm = -100;
	// 		dewPointAtm = -100;
	// 		frostPointAtm = -100;
	// 		PPMv = -100;
	// 		PPMm = -100;
	// 	}

}





// Modbus read incoming


#define TIMEOUT 50    /* 1 second */
#define MAX_RESPONSE_LENGTH 48
#define PRESET_QUERY_SIZE 256
/* errors */
#define PORT_ERROR -5
uint8_t const echo_ = 8;


/*
uint8_t receive_incoming_data_read(byte* data_)
{
int length;

if (altSerial.available() == 0)
return 0;  // lost connection


length = read_reg_response(data_);

// 	SerialUSB.print("read_reg_response  ");
// 	SerialUSB.println(length);


return (length);

}
*/



int read_reg_response(byte* data_)
{

	byte recievedBytes[MAX_RESPONSE_LENGTH+2];
	int raw_response_length;
	int i;

	raw_response_length = modbus_response(recievedBytes);

	if (raw_response_length > 0)
	raw_response_length -= 2;


	if (raw_response_length > 0)
	for (i = 0;  i < (recievedBytes[2]) && i < (raw_response_length); i++)
	{
		data_[i] = recievedBytes[3 + i];
	}

	/*
	if (port_num == port_S3)
	{
	v.bytes[0] = data[6];
	v.bytes[1] = data[5];
	v.bytes[2] = data[4];
	v.bytes[3] = data[3];
	}
	*/
	// 	SerialUSB.print("raw_response_length  ");
	// 	SerialUSB.print(currentSlave[flow_]);
	// 	SerialUSB.print(" ->  ");
	// 	SerialUSB.println(raw_response_length);
	
	return (raw_response_length);
}



int modbus_response(byte* recievedBytes)
{
	int response_length;

	unsigned int crc_calc = 0;
	unsigned int crc_received = 0;
	//unsigned char recv_crc_hi;
	//unsigned char recv_crc_lo;
	

	//  do {        // repeat if unexpected slave replied
	response_length = receive_response(recievedBytes);
	//  } while ((response_length > 0) && (data[0] != portArray[0]));

	response_length = recievedBytes[2] + 5;


	if (response_length)
	{
		crc_calc = IGAS_mb_X.crc(recievedBytes, 0, response_length - 2);

		//	recv_crc_hi = (unsigned) recievedBytes[response_length - 2];
		//	recv_crc_lo = (unsigned) recievedBytes[response_length - 1];

		crc_received = recievedBytes[response_length - 2];
		crc_received = (unsigned) crc_received << 8;
		crc_received =
		crc_received | (unsigned) recievedBytes[response_length - 1];

		// check CRC of response ***********

		if (crc_calc != crc_received)
		{
			return 0;
		}

		// check for exception response

		if (recievedBytes[SLAVE] != 1)
		{
			return 0;
		}



		//	if (response_length && recievedBytes[1] != packet[flow_][1])
		//	{
		//		response_length = 0 - recievedBytes[2];
		//	}

	}


	return (response_length);
}


/*
int receive_response(uint8_t *received_string)
{

int bytes_received = 0;
int i = 0;



while (altSerial.available() == 0)
{
if (i++ > TIMEOUT)
return bytes_received;
}


while (altSerial.available())
{
vipRespond[bytes_received+1] = altSerial.read();


if (bytes_received >= echo_)
{
received_string[bytes_received - echo_] = vipRespond[bytes_received+1];
}

//	SerialUSB.print(received_string[bytes_received]);
//	SerialUSB.print(" ");
bytes_received++;
if (bytes_received >= MAX_RESPONSE_LENGTH-2)
{
vipRespond[0] = bytes_received;
return PORT_ERROR;
}

}
vipRespond[0] = bytes_received;
// SerialUSB.println();

//	if (bytes_received > 0)
return (bytes_received - echo_);

}
*/
