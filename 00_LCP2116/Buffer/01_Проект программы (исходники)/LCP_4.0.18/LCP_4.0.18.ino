/*

4.0.17 FIRM



relay
1 - ventilation
2 - Ok-Alarm
3 - buzzer
4 - Emergency Light
5 - ARM fault message




Gas Detection System PLC module
Sergey Zakrzhevski

IGAS Detection Ltd


*/

#include <SPI.h>
#include <SC16IS7xx.h>
#include <Modbus.h>
#include <ModbusIP.h>
#include <SD.h>
#include <IGAS_Eth.h>
#include <IGAS_RTC.h>
#include <IGAS_104.h>


// Select the Slowclock source
// RTC_clock rtc_clock(RC);
RTC_clock rtc_clock(XTAL);


// NTP added in ver 22 Start
EthernetUDP Udp;

unsigned int localPort = 8888;      // local port to listen for UDP packets

IPAddress timeServer(192, 43, 244, 18); // time.nist.gov NTP server

const int NTP_PACKET_SIZE= 48; // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// NTP added in ver 22 Finish


char* daynames[] = {"Понедельник", "Вторник", "Среда", "Четверг", "Пятница", "Суббота", "Воскресенье"};

byte const initSerialDebug = 0;

int startScreen;

#define WDT_KEY (0xA5)

SpiUartDevice UARTX;
ModbusIP mb1;
//ModbusIP mb2;

#define   CH_A    0x00
#define   CH_B    0x01
#define   SC16IS752_CHANNEL_BOTH 0x00


#define sc740 0
#define sc752 1

byte uartVer = sc740;

byte const ETH1_CS = 10;     // CS SPI pin
byte const ETH2_CS = 48;      // CS SPI pin
IEC_104 IEC104_E1(ETH1_CS);
IEC_104 IEC104_E2(ETH2_CS);

const byte oldVersion = 1; // For non automatic HMI


const byte LostPacketError = 5;
const byte BR_lostpacketSP = 5;
const unsigned long BuzzerOffTime = 600000;
const unsigned int WaitResponceHMI = 700;
const unsigned int waitResponceBR = 1000;
const byte HMI_Init_Retry = 11;
const byte BufCheckTimeOut = 2  ;                  // Максимальное время проверки и очистки буффера перед отправкой нового сообщения
const byte LineBlockedLimit = 3;
const byte SSerialTxControl_0 = 45;                       // Разрешающий пин для порта 0
const byte SSerialTxControl_1 = 7;                        // Разрешающий пин для порта 1
const byte SSerialTxControl_2 = 8;                        // Разрешающий пин для порта 2
const byte SSerialTxControl_3 = 9;                        // Разрешающий пин для порта 3
const byte RS485Transmit = HIGH;                          // Флаг передачи сообщения
const byte RS485Receive = LOW ;                           // Флаг ожидания ответа
const unsigned int WaitResponceGauge = 700;
const byte detectors_5 = 32;                // HMI start page
const byte detectors_10 = 33;
const byte detectors_16 = 34;
const byte detectors_20 = 35;                // HMI start page
const byte detectors_30 = 36;
const byte detectors_40 = 37;

const byte TCPIP = 0;
const byte IEC104mode = 1;


byte const outStatusOffset = 20; // Calc offset according to total sensors*************************************
const unsigned long pollTaskTime = 50; // Task poll timer, ms
const unsigned long pollMbSlaveTime = 20; // Task poll timer, ms



unsigned int pollDelay[] = {100, 200, 200, 200, 200, 200, 200, 200};
unsigned long startTimeOut[] = {400, 400, 400, 400, 400, 400, 400, 400}; // Array of timeout - waiting for respond on each channel


byte totalTasks = 0;

//Used Pins


byte const wizINT = 2;
byte const UART1_CS = 3;      // CS SPI pin
byte const UART2_CS = 5;      // CS SPI pin
byte const UART3_CS = 6;      // CS SPI pin
//byte const ETH1_CS = 10;     // CS SPI pin
//byte const ETH2_CS = 48;      // CS SPI pin
byte const microSD_CS = 23;   // CS SPI pin
byte const X2X_LED = 24;      // status LED pin
const byte PLC_ok = 40;   // status LED pin
byte const SD_OK = 41;      // status LED pin
byte const X2X_Transmit = 45;   // transmit enable pin



const byte spontaneosActivate = 1;
// EthernetClient client;
// EthernetServer iec104Server(2404);

// int alarmBuffer[40][2][50]; // slave, ref/gas, point


// Debug flags
const byte X2X_debug = 0;
const byte mbDebug = 0;
const byte calcDebug = 0;


byte BR_lostpacket[5];


byte lowLim[8];
byte hiLim[8];


byte currentTask = 0;


byte relayModule;
byte relayPurpose;
byte relayMask;


byte serialFlag[9];
byte redundantMode[8];
byte redundantBuffer[9][30];
byte redundantFlag[9];
byte taskFlag[] = {0, 0, 0, 0, 0, 0, 0, 0};

byte logYellow[50];
byte logRed[50];
byte logDisconnect[50];
byte logError[50];
byte logDisabled[50];
byte logHMI[5];


byte logX2X[5];


byte EIA_Lines;

byte BR_Position = 1;
byte sensorPosition;

byte nextCommand;


byte MBposition;
byte StatusByte;
byte serviceMode = 0;







// Internal Serial flags

enum // Internal Status flags
{
	taskComplete,
	sendMsg,
	sendingMsg,
	receiveMsg,
	msgDone,
	msgLost,
	reconnectLoop,
	sendingLoop,
	receiveLoop,
	msgDoneLoop,
	pollWait,
	inProgress,
};


enum // Internal Status flags
{
	disable,
	gas_SF6,
	gas_H2,
	gas_O2,
	gas_AsH3,
	gas_PH3,
	gas_Fire,
	gas_CH4,
	gas_empty,

};


const byte loopFail = 1;
const byte loopOk = 0;
const byte active = 1;

// External Serial flags
const byte portReady = 3;
byte Ethernet1Mode = 0;
byte Ethernet2Mode = 0;
byte relayMessage[5];

byte bridgePort;
byte bridgeByte;

unsigned long currentTime;
unsigned long lastTaskTime;
unsigned long lastMbSlaveTime;

unsigned long waitTillSent[8];

unsigned long X2XSend;

File dataFile;
String sdString = "";

// On the Ethernet Shield, CS is pin 4. Note that even if it's not
// used as the CS pin, the hardware CS pin (10 on most Arduino boards,
// 53 on the Mega) must be left as an output or the SD library
// functions will not work.

int initSubTask;
byte BRSlice;
byte slavesOnChannel[8];

byte BuzzerOffRelay[10];

byte DisconnectRelayMask[10];
byte LDIRelayMask[8][8];


byte watchDogRelay;



byte systemFaultRelay[10];
uint8_t reverseLDO[10];



byte lostPackets[50];

const int baudrate_gauge = 9600;                                                   // Baudrate of 2 gauge channels
const unsigned long baudrate_HMI = 19200;                                                   // Baudrate of 2 gauge channels
const int baudrate_PC = 9600;                                                   // Baudrate of 2 gauge channels
unsigned long RelayProtectionTimer;


unsigned long startPollTimer[8];




// byte SensorStatusByte[12];


int X2X_Data_Array[20][14];

enum                 // Not used at the moment
{
	TypeModule,
	DataQnt,
	Status,
	Data_0,
	Data_1,
	Data_2,
	Data_3,
	Data_4,
	Data_5,
	Data_6,
	Data_7,
	Data_8,
	Data_9,
};

enum
{
	LCP,
	LDO,	// 1
	LAI,	// 2
	LDI8,	// 3
	LCT,	// 4
	LDI16,	// 5
	X2Xsize,
	
	IR_SF6 = 101,
	DM910,		 //102
	DM910H,		 //103
	PVT100,		 //104
	ERIS210,	 //105
	fireDet,	 //106
	binar,		 //107
	proSense,	 //108
	SensSize
	
};



enum
{
	RS_serv,
	RS_timeOut,
	RS_respond,
	RS_fin,
};



int   lastBytesReceived[9];             // Array of quantity received in buffer after last check



unsigned long Interframe_Silence[8];  // Interframe silence - interframe delay without delay
unsigned long interframe_delay = 5;    /* Modbus t3.5 = 2 ms */
unsigned long interframe_delay_slave = 5;    /* Modbus t3.5 = 2 ms */
unsigned long soundTimer;


byte RelayWD;


byte totalSensors = 0;

int           warningRelayMask[6][50];
int           alarmRelayMask[6][50];
int           warningSetpointH[50];
int           alarmSetpointH[50];
int           warningSetpointL[50];
int           alarmSetpointL[50];


byte RelayProtectionTrigger = 0;
byte AlarmSoundOff;


uint8_t MLTPL[50];

int emulationActive = 0;
int  sensorEmul = 0;
int emulStatus = 0;
unsigned int emulConcentration = 0;

typedef union
{
	float val;
	uint8_t bytes[4];
} floatval;
floatval v;
floatval binarConcentration;

byte const standBy = 0;  //
byte const doTask = 1;

byte HMIRedScreen = B00000010;     // Фон красного цвета
byte HMIYellowScreen = B00000100;    // Фон желтого цвета
byte HMIRedAlarm = B00001000;    // Превышен второй порог - красный  фон
byte HMIYellowAlarm = B00010000;     // Превышен первый порог - желтый фон
byte sysFault = B00100000;       // Авария в системе автоматики
byte lostConnect = B01000000;      // Потеряна связь с детекторами



byte BR_Fail_Mask[10] = {0, B00000001, B00000010, B00000100, B00001000, B00010000, B00100000, B01000000, B10000000}; // HMI diagnostic page
byte BR_Fail[8];

byte ScreenState[85];                   // Array of Desktop HMI color - Alarm-Warning
int gaugeData[85] = {-1};               // Array of bytes from gauge - channel 3
int vendor[50];
int gasType[50];
int enabled[50];
int units[50];
byte gaugeStatus[50]; // Показания датчика
float convertion[50];

// HMI Status byte
int hide = -1;

enum
{
	alarm = 1,
	warning,
	ok,
	lost,
	fault,
	disconnected,
	fireAlarm,
};
// const byte alarm = 1;
// const byte warning = 2;
// const byte ok = 3;
// const byte lost = 4;
// const byte fault = 5;
// const byte disconnected = 6;



const byte initFlag = 10;

byte HMIstatus[] = {initFlag, initFlag, initFlag, initFlag, initFlag, initFlag};

byte TRUE = 1;
byte FALSE = 0;

byte colorGrey = 0;

int outArray[70];

byte currentSlave[9] = {1};                // slave number that should be proceed next - channel 1

// byte slaveAddr[5][35];             // Array of HMI command regs

byte totalDisplays;

byte const X2X = 0;
byte const RS_Slave = 1;
byte const HMI = 2;
byte const RS1 = 3;
byte const RS2 = 4;
byte const RS3 = 5;
byte const RS4 = 6;

byte const portQnt = 8;

byte PORT[] = {0, 1, 2, 3, 4, 5, 6};


int data_X2X[40];
int data_PC[120];          // Array of data message contain command (x03, x10), slave number, data and CRC sum.
int data_HMI[120];          // Array of data message contain command (x03, x10), slave number, data and CRC sum.
int data_S1[40];        // Array of data message contain command (x03, x10), slave number, data and CRC sum.
int data_S2[40];        // Array of data message contain command (x03, x10), slave number, data and CRC sum.
int data_S3[40];        // Array of data message contain command (x03, x10), slave number, data and CRC sum.
int data_S4[40];        // Array of data message contain command (x03, x10), slave number, data and CRC sum.


byte taskNum[9];

byte subTask_X2X;
byte subTask_PC;
byte subTask_HMI;
byte subTask_S1;
byte subTask_S2;
byte subTask_S3;
byte subTask_S4;

byte subsubTask[8];
byte rtcTask = 0;

byte packet_size[8];

byte packet[7][120];        // Array of data message contain command (x03, x10), slave number, data and CRC sum.







String dataString;

byte manualRelayMaskBuffer;
byte manualRelayMask;
byte manualSlice;
byte manualSliceBuffer;

byte SerialUSB_Ok = 0;
byte HMI_debug = 0;
const byte sdDebug = 0;

byte  PLC_SlaveAdress;

const byte fulData = 0;
const byte changeData = 0;

int16_t IEC104data_M_SP[500];
int16_t IEC104spont_M_SP[500];

int16_t IEC104data_M_ME[50];
int16_t IEC104spont_M_ME[50];

byte IEC104_M_ME_QDS[200];
byte IEC104_M_TF_QDS[200];

float IEC104data_M_TF[10];
int16_t IEC104spont_M_TF[20];

int RS485_Holding[59];
byte RS485_Coil[300];


int changeBuffer[500];
int mbMESize = 0;
int mbCoilSize = 0;

uint16_t mbCoilShift = 20001;
uint16_t mbIregShift = 40001;




void watchdogSetup(void) {
	/*** watchdogDisable (); ***/
}





void setup()
{
	delay(2000);
	SPI.begin();
	initPins();

	initSDcard();


	
	/*
	while (!SerialUSB)  {
	;  // wait USB to connect. Needed for Leonardo only
	}
	*/





	if (SerialUSB_Ok)
	SerialUSB.println("Карта microSD подключена к системе");
	digitalWrite(SD_OK, HIGH);
	delay(100);


	// Load redundant line
	loadConfig("loop.txt", outArray);
	for (int i = 1; i <= outArray[0]; i++) // количество датчиков
	{
		redundantMode[i] = outArray[i];
	}

	loadConfig("SC16.txt", outArray);
	
	if (outArray[1] == 752)
	uartVer = sc752;
	else
	uartVer = sc740;


	initSerial();
	// redundantMode[3]=1;


	// Load BR config for special situation
	loadConfig("buzzer.txt", outArray);
	for (int i = 1; i <= outArray[0]; i++) // количество датчиков
	BuzzerOffRelay[i] = outArray[i];

	// Load BR config for special situation
	loadConfig("DetFlt.txt", outArray);
	for (int i = 1; i <= outArray[0]; i++)
	DisconnectRelayMask[i] = outArray[i];
	
	
	


	//	loadConfig("wtdg.txt", outArray);
	//	watchDogRelay = outArray[1];    // Overrided for first relay BR1
	watchDogRelay = B00000001;

	// Load BR config for special situation
	loadConfig("rvrs.txt", outArray);
	for (int i = 1; i <= outArray[0]; i++)
	reverseLDO[i] = outArray[i];
	

	// Load BR config for general system Fault
	loadConfig("SysFlt.txt", outArray);
	for (int i = 1; i <= outArray[0]; i++)
	systemFaultRelay[i] = outArray[i];



	// Load HMI in system
	loadConfig("HMIs.txt", outArray);
	totalDisplays = outArray[1];
	

	/*
	// Load Slave quantity on each line
	loadConfig("slaves.txt", outArray);

	slavesOnChannel[RS1] = outArray[1];
	slavesOnChannel[RS2] = outArray[2];
	slavesOnChannel[RS3] = outArray[3];
	slavesOnChannel[RS4] = outArray[4];
	*/


	// Load Slave adresses on line 1
	loadConfig("ch1.txt", outArray);
	slavesOnChannel[RS1] = outArray[1];


	/*
	if (outArray[0] != 0)
	for (int i = 1; i <= slavesOnChannel[RS1]; i++)
	slaveAddr[1][i] = outArray[i];
	*/



	// Load Slave adresses on line 2
	loadConfig("ch2.txt", outArray);
	slavesOnChannel[RS2] = outArray[1];


	/*
	if (outArray[0] != 0)
	for (int i = 1; i <= slavesOnChannel[RS2]; i++)
	slaveAddr[2][i] = outArray[i];
	*/


	// Load Slave adresses on line 3

	loadConfig("ch3.txt", outArray);
	slavesOnChannel[RS3] = outArray[1];



	/*
	if (outArray[0] != 0)
	for (int i = 1; i <= slavesOnChannel[RS3]; i++)
	slaveAddr[3][i] = outArray[i];
	*/

	// ***** Отправка настроек в панель *****

	loadConfig("ch4.txt", outArray);
	slavesOnChannel[RS4] = outArray[1];

	// 	loadConfig("chX.txt", outArray);
	// 	slavesOnChannel[RS4] = outArray[1];


	/*
	if (outArray[0] != 0)
	for (int i = 1; i <= slavesOnChannel[RS4]; i++)
	slaveAddr[4][i] = outArray[i];
	*/

	// ***** Отправка настроек в панель *****

	loadConfig("MLTPL.txt", outArray);
	for (int i = 1; i <= outArray[0]; i++) // множитель
	{
		MLTPL[i] = outArray[i];
	}
	

	for (byte i = RS1; i <= RS4; i++)
	{
		totalSensors += slavesOnChannel[i];
		if (SerialUSB_Ok)
		SerialUSB.println(totalSensors);
	}

	lowLim[RS1] = 1;
	hiLim[RS1] = slavesOnChannel[RS1];
	currentSlave[RS1] = 1;

	if (SerialUSB_Ok)
	{
		SerialUSB.print("totalSensors -> ");
		SerialUSB.println(totalSensors);

		SerialUSB.print("lowLim[RS1] -> ");
		SerialUSB.println(lowLim[RS1]);

		SerialUSB.print("hiLim[RS1] -> ");
		SerialUSB.println(hiLim[RS1]);
	}


	for (byte i = RS2; i <= RS4; i++)
	{
		if (slavesOnChannel[i] != 0)
		{
			lowLim[i] += hiLim[i - 1] + 1;
			hiLim[i] += hiLim[i - 1] + slavesOnChannel[i];
		}
		else
		{
			lowLim[i] += hiLim[i - 1] + 1;
			hiLim[i] += hiLim[i - 1] + 2;
		}
		currentSlave[i] = lowLim[i];
	}

	if (SerialUSB_Ok)
	{
		SerialUSB.print("lowLim[RS2] -> ");
		SerialUSB.println(lowLim[RS2]);
		SerialUSB.print("hiLim[RS2] -> ");
		SerialUSB.println(hiLim[RS2]);


		SerialUSB.print("lowLim[RS3] -> ");
		SerialUSB.println(lowLim[RS3]);
		SerialUSB.print("hiLim[RS3] -> ");
		SerialUSB.println(hiLim[RS3]);

		SerialUSB.print("lowLim[RS4] -> ");
		SerialUSB.println(lowLim[RS4]);
		SerialUSB.print("hiLim[RS4] -> ");
		SerialUSB.println(hiLim[RS4]);
		SerialUSB.println();


		SerialUSB.print("totalSensors -> ");
		SerialUSB.println(totalSensors);
	}


	rtc_clock.init();
	IEC104_E1.init104(rtc_clock);
	IEC104_E2.init104(rtc_clock);


	// Load BR modules quantity
	loadConfig("BRX.txt", outArray);
	BRSlice = outArray[0];

	for (int i = 1; i <= BRSlice + 1; i++)               // количество датчиков
	X2X_Data_Array[i][TypeModule] = outArray[i];



	// Load Yellow Warning setpoint

	loadConfig("Yellow.txt", outArray);

	for (int i = 1; i <= totalSensors + 1; i++)               // количество датчиков
	warningSetpointH[i] = outArray[i];


	// Load Red Alarm setpoint

	loadConfig("Red.txt", outArray);

	for (int i = 1; i <= totalSensors + 1; i++) // количество датчиков
	alarmSetpointH[i] = outArray[i];

	// Load Red Alarm Fin

	loadConfig("Yellow.txt", outArray);

	for (int i = 1; i <= totalSensors + 1; i++)               // количество датчиков
	warningSetpointH[i] = outArray[i];


	// Load Red Alarm setpoint

	loadConfig("Red.txt", outArray);

	for (int i = 1; i <= totalSensors + 1; i++) // количество датчиков
	alarmSetpointH[i] = outArray[i];

	// Load Red Alarm Fin
	loadConfig("YellowL.txt", outArray);

	for (int i = 1; i <= totalSensors + 1; i++)               // количество датчиков
	warningSetpointL[i] = outArray[i];


	// Load Red Alarm setpoint

	loadConfig("RedL.txt", outArray);

	for (int i = 1; i <= totalSensors + 1; i++) // количество датчиков
	alarmSetpointL[i] = outArray[i];

	// Load Red Alarm Fin


	// Relay mask - BR1
	if (BRSlice > 0)
	{
		loadConfig("RMask1.txt", outArray);
		for (int i = 1; i <= totalSensors + 1; i++) // количество датчиков
		alarmRelayMask[1][i] = outArray[i];

		loadConfig("YMask1.txt", outArray);
		for (int i = 1; i <= totalSensors + 1; i++) // количество датчиков
		warningRelayMask[1][i] = outArray[i];
	}

	// Relay mask - BR2

	if (BRSlice > 1)
	{
		loadConfig("RMask2.txt", outArray);
		for (int i = 1; i <= totalSensors + 1; i++) // количество датчиков
		alarmRelayMask[2][i] = outArray[i];


		loadConfig("YMask2.txt", outArray);
		for (int i = 1; i <= totalSensors + 1; i++) // количество датчиков
		warningRelayMask[2][i] = outArray[i];
	}


	// Relay mask - BR3
	if (BRSlice > 2)
	{
		loadConfig("RMask3.txt", outArray);
		for (int i = 1; i <= totalSensors + 1; i++) // количество датчиков
		alarmRelayMask[3][i] = outArray[i];


		loadConfig("YMask3.txt", outArray);
		for (int i = 1; i <= totalSensors + 1; i++) // количество датчиков
		warningRelayMask[3][i] = outArray[i];
	}

	// Relay mask - BR3
	if (BRSlice > 3)
	{
		loadConfig("RMask4.txt", outArray);
		for (int i = 1; i <= totalSensors + 1; i++) // количество датчиков
		alarmRelayMask[4][i] = outArray[i];


		loadConfig("YMask4.txt", outArray);
		for (int i = 1; i <= totalSensors + 1; i++) // количество датчиков
		warningRelayMask[4][i] = outArray[i];
	}

	if (BRSlice > 4)
	{
		loadConfig("RMask5.txt", outArray);
		for (int i = 1; i <= totalSensors + 1; i++) // количество датчиков
		alarmRelayMask[5][i] = outArray[i];


		loadConfig("YMask5.txt", outArray);
		for (int i = 1; i <= totalSensors + 1; i++) // количество датчиков
		warningRelayMask[5][i] = outArray[i];
	}

	// *************** Serial ***************************

	loadConfig("EIA.txt", outArray);
	EIA_Lines = outArray[0];

	for (byte i = 1; i <= 4; i++)
	PORT[i + 2] = outArray[i];


	// Added new
	totalTasks = 0;
	for (byte i = RS1; i <= RS4; i++)
	if (slavesOnChannel[i] != 0)
	totalTasks++;

	totalTasks = totalTasks + 3; // Add 3 basic Serials - HMI, PC slave and X2X



	// *************** Sensor ***************************
	if (SerialUSB_Ok)
	{
		SerialUSB.print("EIA_Lines -> ");
		SerialUSB.println(EIA_Lines);
	}

	loadConfig("vendor.txt", outArray);

	for (int i = 1; i <= outArray[0]; i++)
	{
		vendor[i] = outArray[i];
		//  IEC104data_M_SP[i] = 0;
		//  IEC104data_M_SP[2*i] = ok;
	}


	loadConfigFloat("cnvrt.txt",convertion);

	loadConfig("gases.txt", outArray);

	for (int i = 1; i <= totalSensors; i++)
	{
		gasType[i] = outArray[i];
		//  IEC104data_M_SP[i] = 0;
		//  IEC104data_M_SP[2*i] = ok;
	}

	loadConfig("Enbld.txt", outArray);


	for (int i = 1; i <= totalSensors; i++)
	{
		enabled[i] = outArray[i];
		//  IEC104data_M_SP[i] = 0;
		//  IEC104data_M_SP[2*i] = ok;
	}


	// Added 4.0.3_1 -> Start
	loadConfig("ASDU.txt", outArray);

	IEC104_E1.ASDU = outArray[1];

	loadConfig("ASDU2.txt", outArray);

	IEC104_E2.ASDU = outArray[1];
	// Added 4.0.5_1 -> Finish

	delay(1000);
	//SerialUSB.print("E1");
	//SerialUSB.println(IEC104_E1.ASDU);

	//SerialUSB.print("E2");
	//SerialUSB.println(IEC104_E2.ASDU);

	//  IEC104data_M_SP[0]=0;

	initEthernet();

	// Modbus TCP init START
	if (Ethernet1Mode == TCPIP || Ethernet2Mode == TCPIP)
	{
		//mbMESize = totalSensors * 9 + 3 + BRSlice;
		
		mbCoilSize = calcSPsize();
		mbMESize = calcMEsize();
		
		if (mbDebug)
		{
			SerialUSB.print("Coil size: ");
			SerialUSB.println(mbCoilSize);
			SerialUSB.print("Ireg size: ");
			SerialUSB.println(mbMESize);
		}
		
		for (byte j = 0; j < mbCoilSize; j++)  // Add status bit data
		mb1.addCoil(mbCoilShift+j,0);
		
		for (byte i = 0; i < mbMESize; i++)  // add concentration data
		mb1.addIreg(mbIregShift+i, 0);
	}

	// Modbus TCP init FIN

	// *************** Reset Screen State ***************************

	for (int j = 1; j <= totalSensors; j++) // Reset all HMI's gauges fields to normal
	{
		ScreenState[j] = 0;
	}

	for (int j = 1; j <= totalSensors; j++) // Reset all HMI's gauges fields to normal
	{
		gaugeData[2 * j - 1] = hide;
		gaugeData[2 * j] = ok;
	}



	// ***** Preset HMI settings

	//  while (initHMI() != 99 || HMIstatus != ok);


	currentSlave[HMI] = 1;

	RelayProtectionTimer = millis();

	// Set user Array 104 size


	IEC104_E1.size_M_SP = calcSPsize();
	IEC104_E1.size_M_ME = calcMEsize();

	IEC104_E2.size_M_SP = IEC104_E1.size_M_SP;
	IEC104_E2.size_M_ME = IEC104_E1.size_M_ME;

	
	sdString = "IGAS. Включение питания.";
	printData();

	digitalWrite(PLC_ok, HIGH);


	//	memset(IEC104_M_ME_QDS, 0, sizeof(IEC104_M_ME_QDS)); // Обнуление массива SDname
	//	memset(IEC104_M_TF_QDS, 0, sizeof(IEC104_M_TF_QDS)); // Обнуление массива SDname
	
	
	if (SerialUSB_Ok)
	{
		SerialUSB.println("Система готова к работе.");


		SerialUSB.print(rtc_clock.get_hours());
		SerialUSB.print(": ");
		SerialUSB.print(rtc_clock.get_minutes());
		SerialUSB.print(": ");
		SerialUSB.println(rtc_clock.get_seconds());
		SerialUSB.print(" ");
		SerialUSB.print(rtc_clock.get_days());
		SerialUSB.print(".");
		SerialUSB.print(rtc_clock.get_months());
		SerialUSB.print(".");
		SerialUSB.println(rtc_clock.get_years());

		SerialUSB.println("********************************************"); //Printing for debugging purpose

	}


	RTC->RTC_CR &= (uint32_t)(~RTC_CR_UPDTIM) ;
	RTC->RTC_CR &= (uint32_t)(~RTC_CR_UPDCAL) ;
	RTC->RTC_SCCR |= RTC_SCCR_SECCLR ;

	X2XSend = millis();
	digitalWrite(X2X_LED, HIGH);



	// Enable watchdog.
	WDT->WDT_MR = WDT_MR_WDD(0xFFF)
	| WDT_MR_WDRPROC
	| WDT_MR_WDRSTEN
	| WDT_MR_WDV(256 * 4); // Watchdog triggers a reset after 2 seconds if underflow
	// 2 seconds equal 84000000 * 2 = 168000000 clock cycles
	/* Slow clock is running at 32.768 kHz
	watchdog frequency is therefore 32768 / 128 = 256 Hz
	WDV holds the periode in 256 th of seconds  */
	
	uint32_t status = (RSTC->RSTC_SR & RSTC_SR_RSTTYP_Msk) >> RSTC_SR_RSTTYP_Pos; // Get status from the last Reset
	GPBR->SYS_GPBR[0] += 1;
	
	
	// 2 seconds equal 84000000 * 2 = 168000000 clock cycles
	/* Slow clock is running at 32.768 kHz
	watchdog frequency is therefore 32768 / 128 = 256 Hz
	WDV holds the periode in 256 th of seconds  */
	
}



byte hmiSendSize = 0;


int const pollRTC = 100;
unsigned long RTCLast;
unsigned long  updIecData = 0;

int const pollTimeRTC = 1000;
unsigned long RTCLastTime;
byte PCLEDstate = 0;

void loop()
{
	currentTime = millis();

	//Restart watchdog
	WDT->WDT_CR = WDT_CR_KEY(WDT_KEY)
	| WDT_CR_WDRSTT;
	
	
	if ( (rtc_clock.rtcFlag != RTC_FLAG_IDLE) && (currentTime - RTCLast > pollRTC))
	{
		RTCLast = currentTime;
		rtc_clock.rtcLOOP();
	}



	if (Ethernet1Mode == TCPIP)
	mb1.task();    // Slave output to customer PC via Ethernet Modbus TCP line
	else
	{
		//Ethernet.select(ETH1_CS);
		IEC104_E1.LOOP(IEC104data_M_SP, IEC104spont_M_SP, IEC104data_M_ME, IEC104spont_M_ME, IEC104_M_ME_QDS, IEC104data_M_TF, IEC104spont_M_TF, IEC104_M_TF_QDS);
	}

	if (Ethernet2Mode == TCPIP)
	;// mb1.task();    // Slave output to customer PC via Ethernet Modbus TCP line
	else
	{
		//	Ethernet.select(ETH2_CS);
		IEC104_E2.LOOP(IEC104data_M_SP, IEC104spont_M_SP, IEC104data_M_ME, IEC104spont_M_ME, IEC104_M_ME_QDS, IEC104data_M_TF, IEC104spont_M_TF, IEC104_M_TF_QDS);
	}
	
	
	
	

	if (currentTime - updIecData > 500)
	{
		UpdateRelays();
		proceedProtocolData();
		
		// 		if (Ethernet1Mode == TCPIP)
		// 		moveDataMB();
		updIecData = currentTime;
	}

	

	//delay(100);
	




	



	taskTimer();  //  Run all system task every poll time ms.


	// serviceMode = 1;
	if (serviceMode == 1)      // Activate USB-RS485 bridge
	mbBridge();


	if (currentTime - X2XSend > 5000)    // X2X active bus LED watchDog
	digitalWrite(X2X_LED, LOW);


	//*******************************************************************
	//             Gas Concentration Request - Channel 1
	//*******************************************************************

	
	if (taskFlag[RS1] == doTask)   // Task timer triggers new subTask on branch 1
	switch (subTask_S1)
	{
		case 0:   // Get gas Analyzer data  ***************************************
		
		subTask_S1 = getRSdata(RS1, subTask_S1, data_S1);
		
		break;

		case 1:   // If Service Mode activated ***************************************

		subTask_S1 = serviceModeCheck(RS1, subTask_S1);
		break;

		default: // Start from beginning **********************************
		subTask_S1 = 0;
		break;
	}



	//*******************************************************************
	//             Gas Concentration Request - Channel 2
	//*******************************************************************
	if (taskFlag[RS2] == doTask)   // Task timer triggers new subTask on bransh 1
	switch (subTask_S2)
	{
		case 0:   // Get gas Analyzer data  ***************************************
		subTask_S2 = getRSdata(RS2, subTask_S2, data_S2);

		break;

		case 1:   // If Service Mode activated ***************************************
		subTask_S2 = serviceModeCheck(RS2, subTask_S2);
		break;

		default: // Start from beginning **********************************
		subTask_S2 = 0;
		break;
	}



	//*******************************************************************
	//             Gas Concentration Request - Channel 3
	//*******************************************************************

	if (taskFlag[RS3] == doTask)   // Task timer triggers new subTask on bransh 1
	switch (subTask_S3)
	{
		case 0:   // Get gas Analyzer data  ***************************************
		subTask_S3 =  getRSdata(RS3, subTask_S3, data_S3); // Enable for SGOES detectors -> getESPData(port_S3, RS3, subTask_S3, packet_ch5);
		break;

		case 1:   // If Service Mode activated ***************************************
		subTask_S3 = serviceModeCheck(RS3, subTask_S3);
		break;

		default: // Start from beginning **********************************
		subTask_S3 = 0;
		break;
	}




	//*******************************************************************
	//             Gas Concentration Request - Channel 4
	//*******************************************************************
	if (taskFlag[RS4] == doTask)   // Task timer triggers new subTask on bransh 1
	switch (subTask_S4)
	{
		case 0:   // Get gas Analyzer data  ***************************************

		subTask_S4 = getRSdata(RS4, subTask_S4, data_S4);

		break;

		case 1:   // If Service Mode activated ***************************************
		subTask_S4 = serviceModeCheck(RS4, subTask_S4);
		break;

		default: // Go back to Start **********************************
		subTask_S4 = 0;
		break;
	}

	



	//*******************************************************************
	//            X2X Line ->
	//*******************************************************************

	if (taskFlag[X2X] == doTask)   // Task timer triggers new subTask on direction 1
	switch (subTask_X2X)
	{
		case 0:  //  Send/receive data to relay module via X2X line
		X2X_mb(); //X2X_Go();

		break;

		default: // Go back to start
		subTask_X2X = 0;
		break;
	}





	//*************************************************************************
	//               HMI ->
	//*************************************************************************



	if (taskFlag[HMI] == doTask)   // Task timer triggers new subTask on branch 1
	{

		switch (subTask_HMI)
		{

			case 0:
			{
				updateColor();
				
				if (HMIstatus[currentSlave[HMI]] != ok)
				{
					//SerialUSB.print("Init status -> ");
					initHMI();
					//SerialUSB.println(HMIstatus[HMI_Slave]);

					if (HMIstatus[currentSlave[HMI]] == lost)
					{
						if (logHMI[currentSlave[HMI]] == 1)
						{
							logHMI[currentSlave[HMI]] = 0;

							sdString = "Панель -> ";
							sdString += currentSlave[HMI];
							sdString += " потеряна связь";

							printData();
						}
						currentSlave[HMI]++;
					}
					if (currentSlave[HMI] > totalDisplays)
					currentSlave[HMI] = 1;
				}


				if (HMIstatus[currentSlave[HMI]] == ok)
				{

					subTask_HMI++;

					//	SerialUSB.println("************************************************************* ");
					//	SerialUSB.print("Current Slave -> ");
					//	SerialUSB.println(HMI_Slave);
					//	SerialUSB.println("Next task -> ");

				}
				//else
				//	SerialUSB.println("Same task -> ");

			}


			break;


			case 1:  // Send Sensor Data to HMI ***************************************

			// 			if (serialFlag[HMI] == taskComplete)
			// 			{
			// 				for (byte i=0; i<(2 * totalSensors + 1); i++)
			// 				{
			// 					SerialUSB.print(i);
			// 					SerialUSB.print(" -> ");
			// 					SerialUSB.println(gaugeData[i]);
			// 				}
			//
			// 				SerialUSB.println("*************");
			//
			// 			}

			switch (preset_multiple_registersX(HMI, 0, (2 * totalSensors + 1), gaugeData))
			{
				case 1: // not responded


				HMIstatus[currentSlave[HMI]] = lost;
				if (SerialUSB_Ok)
				{
					SerialUSB.print("LOST -> ");
					SerialUSB.println(currentSlave[HMI]);
				}
				//   subTask_HMI = 0;
				break;

				case portReady:

				taskFlag[HMI] = standBy;  // Ready for new task
				subTask_HMI++;


				break;
			}

			break;


			case 2: // Send relay module status data to HMI

			data_HMI[0] = BR_Fail[0];

			//SerialUSB.println(char_ch1[0]);
			switch (preset_multiple_registers(HMI, 200, 2, data_HMI)) // Preset registers
			{
				case 1:  // Lost


				HMIstatus[currentSlave[HMI]] = lost;
				break;

				case portReady:
				subTask_HMI++;
				break;

			}
			break;


			case 3:  // Check if any command btn pressed on HMI

			if (IEC104_E1.ipUpdate != 0 || mb1.ipUpdateMb != 0)  //  New IP adresses?
			{
				if (IEC104_E1.ipUpdate == 1 || mb1.ipUpdateMb == 1)  // Move IP data base to UART buffer
				{
					byte cnt104 = 0;
					for (byte i = 0; i < 6; i++)
					for (byte j = 0; j < 4; j++)
					{

						// start Added for MB TCP
						if (Ethernet1Mode == TCPIP)
						{
							data_HMI[cnt104] = mb1.IPlistMB[i][j];
						}
						else
						// finish Added for MB TCP

						data_HMI[cnt104] = IEC104_E1.IPlist[i][j];
						cnt104++;
					}
					mb1.ipUpdateMb == 2;
					IEC104_E1.ipUpdate = 2;   // Move data to buffer once for each update
				}

				if (preset_multiple_registers(HMI, 301, 24, data_HMI) == portReady) // Preset registers
				{
					IEC104_E1.ipUpdate = 0;  // Done - standby IP update to HMI
					mb1.ipUpdateMb = 0;
					subTask_HMI++;          // Go to next task

				}
			}
			else
			subTask_HMI++;


			break;



			case 4:  // Any time SYNC from IEC 104?

			if (rtc_clock.rtcFlag == RTC_FLAG_VALID) // Any time SYNC from IEC 104?
			{
				switch (rtcTask)
				{
					case 0:  // Preset time and date


					if (serialFlag[HMI] == taskComplete)
					{
						//	SerialUSB.println("Preset data HMI");
						data_HMI[0] = rtc_clock.get_minutes();
						data_HMI[1] = rtc_clock.get_hours();
						data_HMI[2] = rtc_clock.get_days();
						data_HMI[3] = rtc_clock.get_months();
						data_HMI[4] = rtc_clock.get_years();
						data_HMI[5] = 1; // Set
					}

					if (preset_multiple_registers(HMI, 401, 6, data_HMI) == portReady)
					rtcTask = 1;

					break;

					case 1:  // Trigger Time data to move to HMI RTC

					if (serialFlag[HMI] == taskComplete)
					data_HMI[0] = 16;

					if (preset_multiple_registers(HMI, 1802, 1, data_HMI) == portReady)
					{
						rtc_clock.rtcFlag = RTC_FLAG_IDLE;
						subTask_HMI = 20; // Reset command buttons
						rtcTask = 0;  // Reset rtc HMI update task
						//	SerialUSB.println("HMI RTC Done");
					}
					break;

				}
			}
			else

			subTask_HMI++;




			break;



			case 5:  // Check if any command btn pressed on HMI
			checkNewCommand();
			break;


			case 11: // Show relay config on HMI panel
			relayToHMI();
			break;

			case 12:  // Save Relay confog from HMI to Memory card
			saveRelayToMemory();
			break;

			case 13:  // Show Gas Type config on HMI panel
			gasTypesToHMI();
			break;

			case 14: // Enable or disable Detectors in system
			enableDisableDetector();
			break;

			case 15: // Set date and time
			setRTCdata();
			break;

			case 16: // Send Serial line port number to HMI
			serialPortsToHMI();
			break;

			case 17:  // Save new port number from HMI to SD memory card
			saveSerialPortToSD();
			break;

			case 18:  // Turn off alarm sound relays
			switchOffSound();

			break;

			case 20: // Reset all pushed comand buttons on HMI
			resetBtns();
			break;


			case 21: // Reset all pushed comand buttons on HMI
			toolBox();
			break;

			case 22: // Reset all pushed comand buttons on HMI
			resetManualRelay();
			break;

			case 23:  // Emulate Signal from detector
			emuleDetector();
			break;

			case 24: // Reset all pushed comand buttons on HMI
			setManualRelay();
			break;

			case 25: // Reset all pushed comand buttons on HMI
			loadServiceRelay();

			break;

			// Added in 2_0_5 Version

			case 26: // Reset all pushed comand buttons on HMI
			saveServiceRelay();

			break;

			case 27:  // reboot plc

			softReset();

			break;


			case 28:

			outProtocol();

			break;







			default: // Go back to start

			currentSlave[HMI]++;
			if (currentSlave[HMI] > totalDisplays)
			currentSlave[HMI] = 1;

			subTask_HMI = 0;
			break;
		}

	}



	/*
	if (currentTime - lastMbSlaveTime > pollMbSlaveTime)
	{
	update_mb_slave(PLC_SlaveAdress, RS485_Holding, RS485_Coil, totalSensors, mbMESize); // Slave output to customer PC via RS485 Modbus RTU line
	lastMbSlaveTime = currentTime;
	}
	*/

}









/****************************************************************************
BEGIN MODBUS RTU SLAVE FUNCTIONS
****************************************************************************/

/* global variables */



/* enum of supported modbus function codes. If you implement a new one, put its function code here ! */
enum
{
	FC_READ_COILS  = 0x01,   //Read contiguous block of holding register
	FC_READ_REGS  = 0x03,   //Read contiguous block of holding register
	FC_READ_INPUTS = 0x04,   //Read contiguous block of holding register
	FC_WRITE_REG  = 0x06,   //Write single holding register
	FC_WRITE_REGS = 0x10    //Write block of contiguous registers
};

/* supported functions. If you implement a new one, put its function code into this array! */
const unsigned char fsupported[] = {FC_READ_COILS, FC_READ_REGS, FC_READ_INPUTS, FC_WRITE_REG, FC_WRITE_REGS };

/* constants */
enum
{
	MAX_READ_REGS = 0x20,
	MAX_WRITE_REGS = 0x20,
	MAX_MESSAGE_LENGTH = 120
};


enum
{
	RESPONSE_SIZE = 6,
	EXCEPTION_SIZE = 3,
	CHECKSUM_SIZE = 2
};

/* exceptions code */
enum
{
	NO_REPLY = -1,
	EXC_FUNC_CODE = 1,
	EXC_ADDR_RANGE = 2,
	EXC_REGS_QUANT = 3,
	EXC_EXECUTE = 4
};

/* positions inside the query/response array */
enum
{
	SLAVE = 0,
	FUNC,
	START_H,
	START_L,
	REGS_H,
	REGS_L,
	BYTE_CNT
};















/****************************************************************************
BEGIN MODBUS RTU MASTER FUNCTIONS
****************************************************************************/

#define TIMEOUT 50    /* 1 second */
#define MAX_READ_REGS 125
#define MAX_WRITE_REGS 125
#define MAX_RESPONSE_LENGTH 64
#define PRESET_QUERY_SIZE 256
/* errors */
#define PORT_ERROR -5

/*
CRC

INPUTS:
buf   ->  Array containing message to be sent to controller.
start ->  Start of loop in crc counter, usually 0.
cnt   ->  Amount of bytes in message being sent to controller/
OUTPUTS:
temp  ->  Returns crc byte for message.
COMMENTS:
This routine calculates the crc high and low byte of a message.
Note that this crc is only used for Modbus, not Modbus+ etc.
****************************************************************************/

unsigned int crc(unsigned char *buf, int start, int cnt)
{
	int i, j;
	unsigned temp, temp2, flag;

	temp = 0xFFFF;

	for (i = start; i < cnt; i++)
	{
		temp = temp ^ buf[i];

		for (j = 1; j <= 8; j++)
		{
			flag = temp & 0x0001;
			temp = temp >> 1;
			if (flag)
			temp = temp ^ 0xA001;
		}
	}

	/* Reverse byte order. */

	temp2 = temp >> 8;
	temp = (temp << 8) | temp2;
	temp &= 0xFFFF;

	return (temp);
}

/*
Start of the packet of a read_holding_register response
*/


void build_coil_packet(unsigned char slave, unsigned char function,
unsigned char count, unsigned char *packet)
{
	packet[SLAVE] = slave;
	packet[FUNC] = function;
	packet[2] = count;
}

void build_read_packet(unsigned char slave, unsigned char function,
unsigned char count, unsigned char *packet)
{
	packet[SLAVE] = slave;
	packet[FUNC] = function;
	packet[2] = count * 2;
}


/*
Start of the packet of a preset_multiple_register response
*/
void build_write_packet(unsigned char slave, unsigned char function,
unsigned int start_addr,
unsigned char count,
unsigned char *packet)
{
	packet[SLAVE] = slave;
	packet[FUNC] = function;
	packet[START_H] = start_addr >> 8;
	packet[START_L] = start_addr & 0x00ff;
	packet[REGS_H] = 0x00;
	packet[REGS_L] = count;
}

/*
Start of the packet of a write_single_register response
*/
void build_write_single_packet(unsigned char slave, unsigned char function,
unsigned int write_addr, unsigned int reg_val, unsigned char* packet)
{
	packet[SLAVE] = slave;
	packet[FUNC] = function;
	packet[START_H] = write_addr >> 8;
	packet[START_L] = write_addr & 0x00ff;
	packet[REGS_H] = reg_val >> 8;
	packet[REGS_L] = reg_val & 0x00ff;
}



/*
Start of the packet of an exception response
*/
void build_error_packet(unsigned char slave, unsigned char function,
unsigned char exception, unsigned char *packet)
{
	packet[SLAVE] = slave;
	packet[FUNC] = function + 0x80;
	packet[2] = exception;
}




/***********************************************************************

The following functions construct the required query into
a modbus query packet.

***********************************************************************/

#define REQUEST_QUERY_SIZE 6  /* the following packets require          */
#define CHECKSUM_SIZE 2   /* 6 unsigned chars for the packet plus   */
/* 2 for the checksum.                    */

void build_request_packet(int slave, int function, int start_addr,
int count, unsigned char *packet)
{
	packet[0] = slave;
	packet[1] = function;
	packet[2] = start_addr >> 8;
	packet[3] = start_addr & 0x00ff;
	packet[4] = count >> 8;
	packet[5] = count & 0x00ff;

}


/*************************************************************************

modbus_query( packet, length)

Function to add a checksum to the end of a packet.
Please note that the packet array must be at least 2 fields longer than
string_length.
**************************************************************************/

void modbus_reply(unsigned char *packet, unsigned char string_length)
{
	int temp_crc;

	temp_crc = crc(packet, 0, string_length);
	packet[string_length] = temp_crc >> 8;
	string_length++;
	packet[string_length] = temp_crc & 0x00FF;
}



/*********************************************************************

modbus_request(slave_id, request_data_array)

Function to the correct request is returned and that the checksum
is correct.

Returns:  string_length if OK
0 if failed
Less than 0 for exception errors

Note: All functions used for sending or receiving data via
modbus return these return values.

**********************************************************************/

int modbus_request(unsigned char slave, unsigned char *data)
{
	int response_length;
	unsigned int crc_calc = 0;
	unsigned int crc_received = 0;
	unsigned char recv_crc_hi;
	unsigned char recv_crc_lo;

	response_length = receive_request(data);

	if (response_length > 0)
	{
		crc_calc = crc(data, 0, response_length - 2);
		recv_crc_hi = (unsigned) data[response_length - 2];
		recv_crc_lo = (unsigned) data[response_length - 1];
		crc_received = data[response_length - 2];
		crc_received = (unsigned) crc_received << 8;
		crc_received = crc_received | (unsigned) data[response_length - 1];

		/*********** check CRC of response ************/
		if (crc_calc != crc_received)
		{
			return NO_REPLY;
		}

		/* check for slave id */
		if (slave != data[SLAVE])
		{
			return NO_REPLY;
		}


	}
	return (response_length);
}

/*********************************************************************

validate_request(request_data_array, request_length, available_regs)

Function to check that the request can be processed by the slave.

Returns:  0 if OK
A negative exception code on error

**********************************************************************/




int validate_request(unsigned char *data, unsigned char length,
unsigned int regs_size, unsigned int coil_size)
{
	int i, fcnt = 0;
	unsigned int regs_num = 0;
	unsigned int start_addr = 0;
	unsigned char max_regs_num;

	/* check function code */
	for (i = 0; i < sizeof(fsupported); i++)
	{
		if (fsupported[i] == data[FUNC])
		{
			fcnt = 1;
			break;
		}
	}
	if (0 == fcnt)
	return EXC_FUNC_CODE;

	/* For functions read/write regs, this is the range. */
	regs_num = ((int) data[REGS_H] << 8) + (int) data[REGS_L];

	switch (data[FUNC])
	{

		case FC_READ_COILS:
		if ((regs_num) > coil_size)
		return EXC_ADDR_RANGE;

		break;

		case FC_READ_REGS:
		if ((regs_num) > regs_size)
		return EXC_ADDR_RANGE;

		break;

		case FC_WRITE_REGS:
		if ((regs_num) > regs_size)
		return EXC_ADDR_RANGE;

		return 0;

		break;

		case FC_WRITE_REG:

		regs_num = ((int) data[START_H] << 8) + (int) data[START_L];
		if (regs_num >= regs_size)
		return EXC_ADDR_RANGE;

		break;

		//  case FC_READ_INPUTS:

		//    regs_num = ((int) data[START_H] << 8) + (int) data[START_L];
		//    if (regs_num >= regs_size)
		//      return EXC_ADDR_RANGE;


		//     break;
	}



	/* check quantity of registers */
	if (FC_READ_REGS == data[FUNC])
	max_regs_num = MAX_READ_REGS;
	else if (FC_WRITE_REGS == data[FUNC])
	max_regs_num = MAX_WRITE_REGS;

	if ((regs_num < 1) || (regs_num > max_regs_num))
	return EXC_REGS_QUANT;

	/* check registers range, start address is 0 */
	start_addr = ((int) data[START_H] << 8) + (int) data[START_L];
	//  if ((start_addr + regs_num) > regs_size)



	return 0;     /* OK, no exception */
}


/************************************************************************

write_regs(first_register, data_array, registers_array)

writes into the slave's holding registers the data in query,
starting at start_addr.

Returns:   the number of registers written
************************************************************************/

int write_regs(unsigned int start_addr, unsigned char *query, int *regs)
{
	int temp;
	unsigned int i;

	for (i = 0; i < query[REGS_L]; i++)
	{
		/* shift reg hi_byte to temp */
		temp = (int) query[(BYTE_CNT + 1) + i * 2] << 8;
		/* OR with lo_byte           */
		temp = temp | (int) query[(BYTE_CNT + 2) + i * 2];

		regs[start_addr + i] = temp;
	}
	return i;
}




/************************************************************************

preset_multiple_registers(slave_id, first_register, number_of_registers,
data_array, registers_array)

Write the data from an array into the holding registers of the slave.

*************************************************************************/

int preset_multiple_registers_Slave(unsigned char slave,
unsigned int start_addr,
unsigned char count,
unsigned char *query,
int *regs)
{
	unsigned char function = FC_WRITE_REGS; /* Preset Multiple Registers */
	int status = 0;
	unsigned char packet[RESPONSE_SIZE + CHECKSUM_SIZE];

	build_write_packet(slave, function, start_addr, count, packet);

	if (write_regs(start_addr, query, regs))
	{
		status = send_reply(packet, RESPONSE_SIZE);
	}

	return (status);
}



/************************************************************************

write_single_register(slave_id, write_addr, data_array, registers_array)

Write a single int val into a single holding register of the slave.

*************************************************************************/

int write_single_register_Slave(unsigned char slave,
unsigned int write_addr, unsigned char *query, int *regs)
{
	unsigned char function = FC_WRITE_REG; /* Function: Write Single Register */
	int status = 0;
	unsigned int reg_val;
	unsigned char packet[RESPONSE_SIZE + CHECKSUM_SIZE];

	reg_val = query[REGS_H] << 8 | query[REGS_L];
	build_write_single_packet(slave, function, write_addr, reg_val, packet);
	regs[write_addr] = (int) reg_val;
	/*
	written.start_addr=write_addr;
	written.num_regs=1;
	*/
	status = send_reply(packet, RESPONSE_SIZE);

	return (status);
}





/*************************************************************************

modbus_query( packet, length)

Function to add a checksum to the end of a packet.
Please note that the packet array must be at least 2 fields longer than
string_length.
**************************************************************************/

void modbus_query(unsigned char *packet, size_t string_length)
{
	int temp_crc;

	temp_crc = crc(packet, 0, string_length);

	packet[string_length++] = temp_crc >> 8;
	packet[string_length++] = temp_crc & 0x00FF;
	packet[string_length] = 0;
}



/***********************************************************************

send_reply( query_string, query_length)

Function to send a reply to a modbus master.
Returns: total number of characters sent
************************************************************************/

int send_reply(unsigned char *query, unsigned char string_length)
{
	unsigned char i;

	modbus_reply(query, string_length);
	string_length += 2;

	for (i = 0; i < string_length; i++)
	UARTX.write(query[i], UART1_CS, CH_A);



	return i;     /* it does not mean that the write was successful, though */
}



/***********************************************************************

receive_request( array_for_data )

Function to monitor for a request from the modbus master.

Returns:  Total number of characters received if OK
0 if there is no request
A negative error code on failure
***********************************************************************/

int receive_request(unsigned char *received_string)
{
	int bytes_received = 0;

	/* FIXME: does SerialUSB.available wait 1.5T or 3.5T before exiting the loop? */
	while (UARTX.available(UART1_CS, CH_A))
	{
		received_string[bytes_received] = UARTX.read(UART1_CS, CH_A);
		//	SerialUSB.println(received_string[bytes_received]);
		bytes_received++;
		
		// delay(1);  //interframe_delay
		if (bytes_received >= MAX_MESSAGE_LENGTH)
		return NO_REPLY;  /* port error */
	}

	return (bytes_received);
}



/***********************************************************************

send_query(query_string, query_length )

Function to send a query out to a modbus slave.
************************************************************************/

int send_query(unsigned char *query, size_t string_length, int port_num)
{
	int i;

	modbus_query(query, string_length); // d
	string_length += 2;


	switch (port_num)
	{
		case 0:  // X2X

		X2XSend = currentTime;

		digitalWrite(X2X_Transmit, HIGH);


		for (i = 0; i < string_length; i++)
		{
			Serial.write(query[i]);
		}


		break;

		/*
		case 1:  // PC

		for (i = 0; i < string_length; i++)
		UARTX.write(query[i], UART1_CS, CH_A);

		break;
		*/

		case 2:  // HMI

		for (i = 0; i < string_length; i++)
		UARTX.write(query[i], UART2_CS, CH_A);

		break;

		case 3: // S1

		for (i = 0; i < string_length; i++)
		{
			if (uartVer == sc740)
			UARTX.write(query[i], UART3_CS, CH_A);
			else
			UARTX.write(query[i], UART2_CS, CH_B);
		}


		break;

		case 4: // S2

		digitalWrite(SSerialTxControl_1, RS485Transmit);   // Set default state of Enable Pin

		for (i = 0; i < string_length; i++)
		Serial1.write(query[i]);

		break;


		case 5:  //S3

		digitalWrite(SSerialTxControl_3, RS485Transmit);   // Set default state of Enable Pin

		for (i = 0; i < string_length; i++)
		Serial3.write(query[i]);

		break;


		case 6:  // S4

		digitalWrite(SSerialTxControl_2, RS485Transmit);   // Set default state of Enable Pin

		for (i = 0; i < string_length; i++)
		Serial2.write(query[i]);

		break;

		case 7: // USB

		for (i = 0; i < string_length; i++)
		SerialUSB.write(query[i]);

		break;


	}
	/* without the following delay, the reading of the response might be wrong
	apparently, */


	return i;     /* it does not mean that the write was succesful, though */

}


/***********************************************************************

receive_response( array_for_data )

Function to monitor for the reply from the modbus slave.
This function blocks for timeout seconds if there is no reply.

Returns:  Total number of characters received.
***********************************************************************/

int receive_response(byte portNum, byte *received_string)
{

	int bytes_received = 0;
	int i = 0;
	unsigned long now = millis();



	switch (portNum)
	{

		case 0:  // X2X

		while ( Serial.available() == 0)
		{
			if (i++ > TIMEOUT)
			return bytes_received;
		}

		// 		SerialUSB.println();
		// 		SerialUSB.print("RX: ");
		while (Serial.available())
		{
			received_string[bytes_received] = Serial.read();
			// 			SerialUSB.print(received_string[bytes_received]);
			// 			SerialUSB.print(" ");

			bytes_received++;
			if (bytes_received >= MAX_RESPONSE_LENGTH)
			return PORT_ERROR;
		}
		//SerialUSB.println();

		break;

		/*
		case 1:  // PC

		while ( UARTX.available(UART1_CS, CH_A) == 0)
		{
		delay(1);
		if (i++ > TIMEOUT)
		return bytes_received;
		}

		while (UARTX.available(UART1_CS, CH_A))
		{
		received_string[bytes_received] = UARTX.read(UART1_CS, CH_A);
		bytes_received++;
		if (bytes_received >= MAX_RESPONSE_LENGTH)
		return PORT_ERROR;
		}

		break;
		*/


		case 2: // HMI

		while ( UARTX.available(UART2_CS, CH_A) == 0)
		{
			if (i++ > TIMEOUT)
			return bytes_received;
		}
		while (UARTX.available(UART2_CS, CH_A))
		{
			received_string[bytes_received] = UARTX.read(UART2_CS, CH_A);
			//  SerialUSB.println(received_string[bytes_received]);
			bytes_received++;
			if (bytes_received >= MAX_RESPONSE_LENGTH)
			return PORT_ERROR;
		}
		break;



		case 3: // S1

		if (uartVer == sc740)
		{
			while ( UARTX.available(UART3_CS, CH_A) == 0)
			{
				if (i++ > TIMEOUT)
				return bytes_received;
			}
			while (UARTX.available(UART3_CS, CH_A))
			{
				received_string[bytes_received] = UARTX.read(UART3_CS, CH_A);
				//	received_string[bytes_received] = UARTX.read(UART2_CS, CH_B);
				// 			SerialUSB.print(received_string[bytes_received]);
				// 			SerialUSB.print(" ");
				bytes_received++;
				if (bytes_received >= MAX_RESPONSE_LENGTH)
				return PORT_ERROR;
			}
		}
		else
		{
			while ( UARTX.available(UART2_CS, CH_B) == 0)
			{
				if (i++ > TIMEOUT)
				return bytes_received;
			}

			while (UARTX.available(UART2_CS, CH_B))
			{
				received_string[bytes_received] = UARTX.read(UART2_CS, CH_B);
				bytes_received++;

				if (bytes_received >= MAX_RESPONSE_LENGTH)
				return PORT_ERROR;
			}
		}
		break;



		case 4: // S2

		while ( Serial1.available() == 0)
		{
			if (i++ > TIMEOUT)
			return bytes_received;
		}

		while (Serial1.available())
		{
			received_string[bytes_received] = Serial1.read();
			bytes_received++;

			if (bytes_received >= MAX_RESPONSE_LENGTH)
			return PORT_ERROR;
			// SerialUSB.println();
		}

		break;


		case 5:


		while ( Serial3.available() == 0)
		{
			if (i++ > TIMEOUT)
			return bytes_received;
		}

		while (Serial3.available())
		{
			received_string[bytes_received] = Serial3.read();
			bytes_received++;
			if (bytes_received >= MAX_RESPONSE_LENGTH)
			return PORT_ERROR;
		}

		break;


		case 6:

		while ( Serial2.available() == 0)
		{
			if (i++ > TIMEOUT)
			return bytes_received;
		}

		while (Serial2.available())
		{
			received_string[bytes_received] = Serial2.read();
			bytes_received++;
			if (bytes_received >= MAX_RESPONSE_LENGTH)
			return PORT_ERROR;
		}
		break;


		case 7:

		while ( SerialUSB.available() == 0)
		{
			if (i++ > TIMEOUT)
			return bytes_received;
		}

		while (SerialUSB.available())
		{
			received_string[bytes_received] = SerialUSB.read();
			bytes_received++;
			if (bytes_received >= MAX_RESPONSE_LENGTH)
			return PORT_ERROR;
		}
		break;


	}


	if (bytes_received > 0)
	return (bytes_received);

}


/*********************************************************************

( response_data_array, query_array )

Function to the correct response is returned and that the checksum
is correct.

Returns:  string_length if OK
0 if failed
Less than 0 for exception errors

Note: All functions used for sending or receiving data via
modbus return these return values.

**********************************************************************/

int modbus_response(byte flow_, byte* recievedBytes)
{
	int response_length;

	unsigned int crc_calc = 0;
	unsigned int crc_received = 0;
	unsigned char recv_crc_hi;
	unsigned char recv_crc_lo;

	byte portNum = PORT[flow_];





	//  do {        // repeat if unexpected slave replied
	response_length = receive_response(portNum, recievedBytes);
	//  } while ((response_length > 0) && (data[0] != portArray[0]));


	if (response_length)
	{
		crc_calc = crc(recievedBytes, 0, response_length - 2);

		recv_crc_hi = (unsigned) recievedBytes[response_length - 2];
		recv_crc_lo = (unsigned) recievedBytes[response_length - 1];

		crc_received = recievedBytes[response_length - 2];
		crc_received = (unsigned) crc_received << 8;
		crc_received =
		crc_received | (unsigned) recievedBytes[response_length - 1];

		/*********** check CRC of response ************/

		if (crc_calc != crc_received)
		{
			// 			SerialUSB.print(currentSlave[flow_]);
			// 			SerialUSB.println(" CRC bad ");
			return 0;
		}

		/********** check for exception response *****/

		if (currentSlave[flow_] != recievedBytes[SLAVE])
		{
			if (X2X_debug)
			{
				SerialUSB.print(currentSlave[flow_]);
				SerialUSB.print(" - Slave - ");
				SerialUSB.print(currentSlave[flow_]);
				SerialUSB.print(" - Slave MB - ");
				SerialUSB.print(recievedBytes[SLAVE]);
				SerialUSB.println(" slave bad ");
			}
			return 0;
		}



		if (response_length && recievedBytes[1] != packet[flow_][1])
		{
			response_length = 0 - recievedBytes[2];
		}

	}


	return (response_length);
}


/************************************************************************

read_reg_response

reads the response data from a slave and puts the data into an
array.

************************************************************************/



int read_reg_response(byte flow_, int* data_)
{

	byte recievedBytes[MAX_RESPONSE_LENGTH];
	int raw_response_length;
	int temp, i;



	raw_response_length = modbus_response(flow_, recievedBytes);

	if (raw_response_length > 0)
	raw_response_length -= 2;


	if (raw_response_length > 0)
	for (i = 0;  i < (recievedBytes[2] * 0.5) && i < (raw_response_length * 0.5); i++)
	{
		temp = recievedBytes[3 + i * 2] << 8;
		temp = temp | recievedBytes[4 + i * 2];
		data_[i] = temp;
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
	return (raw_response_length);
}


/***********************************************************************

preset_response

Gets the raw data from the input stream.

***********************************************************************/
/*
int preset_response(unsigned char *query, int port_num)
{
unsigned char data[MAX_RESPONSE_LENGTH];
int raw_response_length;

raw_response_length = modbus_response(data, query, port_num);

return (raw_response_length);
}
*/

/************************************************************************

read_holding_registers

Read the holding registers in a slave and put the data into
an array.

*************************************************************************/
/*
taskData[RS_][task_][0] = slave_;
taskData[RS_][task_][1] = startA;
taskData[RS_][task_][2] = regs_;

makeTask(byte RS_, byte task_, byte slave_, int startA, byte regs_)
makeTask(RS2, 0, 1, 1, 2);
port_S1
*/

byte read_holding_registers(byte flow_, int start_addr, byte count, int *data_ )
{
	int function = 0x03;  /* Function: Read Holding Registers */
	int ret;
	byte slave_ = currentSlave[flow_];
	

	//  unsigned char packet[REQUEST_QUERY_SIZE + CHECKSUM_SIZE];

	byte port_num = PORT[flow_];
	switch (serialFlag[flow_])
	{

		case taskComplete:
		serialFlag[flow_] = sendMsg;
		return 0;
		break;



		case sendMsg:  // Send packet

		build_request_packet(slave_, function, start_addr, count, packet[flow_]);


		if (redundantMode[flow_] == active && port_num != PORT[HMI])
		{
			build_request_packet(slave_, function, start_addr, count, redundantBuffer[flow_]);
			flushPort(port_num);
		}

		send_query(packet[flow_], REQUEST_QUERY_SIZE, port_num);

		
		serialFlag[flow_] = sendingMsg;
		waitTillSent[flow_] = millis();
		packet_size[flow_] = count;
		return 0;  // do nothing

		break;



		case sendingMsg: // wait till packet left Serial buffer

		if (currentTime - waitTillSent[flow_] > (2 * (REQUEST_QUERY_SIZE + packet_size[flow_])))
		{
			serialFlag[flow_] = receiveMsg;   //
			startTimeOut[flow_] = currentTime;
		}
		return 0;  // do nothing

		break;



		case receiveMsg: // wait till packet left Serial buffer
		switch (receive_incoming_data_read(flow_, data_))   // Прокерка ответа - 0 - входной буффер пуст, сообщения нет.Проверяем пока не вышло время тайм аута, >0 - овтет получен, <0 - ошибка
		{
			case 0:                                                                           // Входной буфер пуст
			if (currentTime - startTimeOut[flow_] > WaitResponceGauge)                             // Входной буфер пуст - проверяем. как долго ожидался ответ?
			{
				if (redundantMode[flow_] == active && flow_ != PORT[HMI])
				{
					//  SerialUSB.println("Detector not responded, Check loop");
					redundancyCheck(flow_);

					if (redundantFlag[flow_] == loopFail)
					{
						//  SerialUSB.println("Loop fault, try connect detector another way");
						serialFlag[flow_] = reconnectLoop;
						return 0;
					}
					//  SerialUSB.println("Loop ok, detector lost");
					serialFlag[flow_] = msgDone;
					return 1;  // TimeOut - not responded

				}
				serialFlag[flow_] = msgDone;
				return 1;  // TimeOut - not responded

			}

			return 0;  // do nothing
			break;


			default: // Respond received

			serialFlag[flow_] = msgDone;


			if (redundantMode[flow_] == active && port_num != PORT[HMI])
			{
				redundancyCheck(flow_);
				//   SerialUSB.println("Line ok, clear buffer");
			}

			return 2;  // Respond received
			break;
		}

		break;


		case msgDone:  // respond received, serial job done

		startPollTimer[flow_] = currentTime;   // 30 - delay between polls. When poll time expired - system can send new message via RS485
		serialFlag[flow_] = pollWait;   // wait poll time before next Serial send

		return 0;
		break;

		/*
		case msgLost:  // respond received, serial job done

		if (redundantMode[port_num] == active && port_num != port_HMI)
		{
		if (redundantFlag[port_num] == loopFail) // If loop fail, possibly detector lost on line 1 and connected to line 2
		{
		serialFlag[port_num] = reconnectLoop;   // try to get connection via redundant line
		SerialUSB.println("Loop fail - try another line");
		return 0;
		}
		}
		else
		{
		serialFlag[port_num] = msgDone;
		SerialUSB.println("3. Loop ok - possibly detector lost");
		return 1;
		}

		break;
		*/

		case reconnectLoop:  // Send packet

		// build_request_packet(slave, function, start_addr, count, packet[flow_]);
		port_num = PORT[flow_] + 1;
		send_query(packet[flow_], REQUEST_QUERY_SIZE, port_num);

		serialFlag[flow_] = sendingLoop;
		waitTillSent[flow_] = millis();
		return 0;  // do nothing

		break;



		case sendingLoop: // wait till packet left Serial buffer


		if (currentTime - waitTillSent[flow_] > (2 * (REQUEST_QUERY_SIZE + count)))
		{
			serialFlag[flow_] = receiveLoop;   //
			startTimeOut[flow_] = currentTime;
		}
		return 0;  // do nothing

		break;



		case receiveLoop: // wait till packet left Serial buffer
		switch (receive_incoming_data_read(flow_, data_)) // Прокерка ответа - 0 - входной буффер пуст, сообщения нет.Проверяем пока не вышло время тайм аута, >0 - овтет получен, <0 - ошибка
		{
			case 0:                                                                           // Входной буфер пуст
			if (currentTime - startTimeOut[flow_] > WaitResponceGauge)                             // Входной буфер пуст - проверяем. как долго ожидался ответ?
			{
				serialFlag[flow_] = msgDone;
				//SerialUSB.println("Double check fail - detector lost");
				return 1;  // TimeOut - not responded
			}
			return 0;  // do nothing
			break;


			default:
			serialFlag[flow_] = msgDoneLoop;

			return 2;  // Respond received
			break;
		}

		break;



		case msgDoneLoop:  // respond recieved, serial job done

		startPollTimer[flow_] = currentTime;   // 30 - delay between polls. When poll time expired - system can send new message via RS485
		serialFlag[flow_] = pollWait;   // wait poll time before next Serial send

		return 0;
		break;


		case pollWait:  // wait poll time before next Serial send

		if (currentTime - startPollTimer[flow_] > pollDelay[flow_])
		{
			serialFlag[flow_] = taskComplete;   // Serial Port Ready for new subTask
			//  SerialUSB.print("Serial port -> ");
			//  SerialUSB.println(port_num);
			return 3;
		}
		return 0;
		break;
		// wait till packet left Serial buffer

		default:
		return 0;
		break;

	}

}



int read_coils_Slave(unsigned char slave, unsigned int start_addr,
unsigned char reg_count, byte *regs)
{
	unsigned char function = FC_READ_COILS;  /* Function 03: Read Holding Registers */
	byte byteCount = 3;
	int status;
	unsigned int i;
	unsigned int j = 0;
	unsigned char packet[MAX_MESSAGE_LENGTH];


	for (int i = 0; i < 59; i++)
	packet[i] = 0;

	int fullBytes = reg_count * 0.125;
	int rest = reg_count - 8 * fullBytes;


	for (i = 0; i < fullBytes; i++)
	{
		for (j = 0; j < 8; j++)
		if (coilArrayRead(start_addr + 8 * i + j) == 1)
		bitWrite(packet[byteCount], j, 1);
		byteCount++;
	}


	for (j = 0; j < rest; j++)
	{
		if (coilArrayRead(start_addr + 8 * fullBytes + j) == 1)
		bitWrite(packet[byteCount], j, 1);
	}

	if (rest != 0)
	{
		reg_count = fullBytes + 1;
		byteCount++;
	}
	else
	reg_count = fullBytes;


	build_coil_packet(slave, function, reg_count, packet);
	status = send_reply(packet, byteCount);
	packet_size[RS_Slave] = byteCount;
	return (status);
}


/************************************************************************

_Slave(slave_id, first_register, number_of_registers,
registers_array)

reads the slave's holdings registers and sends them to the Modbus master

*************************************************************************/

int read_holding_registers_Slave(unsigned char slave, unsigned int start_addr,

unsigned char reg_count, int *regs)
{
	unsigned char function = 0x03;  /* Function 03: Read Holding Registers */
	byte byteCount = 3;

	int status;
	unsigned int i;
	unsigned char packet[MAX_MESSAGE_LENGTH];

	build_read_packet(slave, function, reg_count, packet);

	for (i = start_addr; i < (start_addr + (unsigned int) reg_count);
	i++)
	{
		packet[byteCount] = regs[i] >> 8;
		byteCount++;
		packet[byteCount] = regs[i] & 0x00FF;
		byteCount++;
	}

	status = send_reply(packet, byteCount);
	packet_size[RS_Slave] = byteCount;

	return (status);
}


int read_input_registers_Slave(unsigned char slave, unsigned int start_addr,

unsigned char reg_count, int *regs)
{
	unsigned char function = 0x04;  /* Function 03: Read Holding Registers */
	byte byteCount = 3;

	int status;
	unsigned int i;
	unsigned char packet[MAX_MESSAGE_LENGTH];

	build_read_packet(slave, function, reg_count, packet);

	for (i = start_addr; i < (start_addr + (unsigned int) reg_count);
	i++)
	{
		packet[byteCount] = regs[i] >> 8;
		byteCount++;
		packet[byteCount] = regs[i] & 0x00FF;
		byteCount++;
	}

	status = send_reply(packet, byteCount);
	packet_size[RS_Slave] = byteCount;

	return (status);
}


/************************************************************************

check_warning




*************************************************************************/


/************************************************************************

preset_multiple_registers

Write the data from an array into the holding registers of a
slave.

*************************************************************************/


byte preset_multiple_registers(byte flow_, int start_addr, int reg_count,  int *data_)
{
	int function = 0x10;  /* Function 16: Write Multiple Registers */
	int byte_count, i;
	int ret;
	byte byteCount = 6;
	byte port_num = PORT[flow_];
	byte slave_ = currentSlave[flow_];

	switch (serialFlag[flow_])
	{

		case taskComplete:
		serialFlag[flow_] = sendMsg;
		return 0;
		break;

		case sendMsg:  // Send packet
		{
			build_request_packet(slave_, function, start_addr, reg_count, packet[flow_]);
			byte_count = reg_count * 2;
			packet[flow_][6] = (unsigned char)byte_count;

			for (i = 0; i < reg_count; i++)
			{
				byteCount++;
				packet[flow_][byteCount] = data_[i] >> 8;
				byteCount++;
				packet[flow_][byteCount] = data_[i] & 0x00FF;
			}
			byteCount++;
			send_query(packet[flow_], byteCount, port_num);


			serialFlag[flow_] = sendingMsg;
			waitTillSent[flow_] = millis();
			packet_size[flow_] = byteCount;

			return 0;  // do nothing
		}
		break;



		case sendingMsg: // wait till packet left Serial buffer



		if (currentTime - waitTillSent[flow_] > (2 * (REQUEST_QUERY_SIZE + packet_size[flow_])))
		{
			// SerialUSB.println(REQUEST_QUERY_SIZE);
			// SerialUSB.println(packet_size[flow_]);
			serialFlag[flow_] = receiveMsg;   //
			startTimeOut[flow_] = currentTime;

		}
		return 0;  // do nothing

		break;


		case receiveMsg: // wait till packet left Serial buffer
		switch (receive_incoming_data_preset(flow_, data_))   // Прокерка ответа - 0 - входной буффер пуст, сообщения нет.Проверяем пока не вышло время тайм аута, >0 - овтет получен, <0 - ошибка
		{
			case 0:                                                                           // Входной буфер пуст
			if (currentTime - startTimeOut[flow_] > (WaitResponceGauge))                             // Входной буфер пуст - проверяем. как долго ожидался ответ?
			{
				serialFlag[flow_] = msgDone;


				return 1;  // TimeOut - not responded
			}
			return 0;  // do nothing
			break;


			default:



			serialFlag[flow_] = msgDone;

			return 2;  // Respond received
			break;
		}

		break;


		case msgDone:  // respond received, serial job done
		startPollTimer[flow_] = currentTime;   // 30 - delay between polls. When poll time expired - system can send new message via RS485
		serialFlag[flow_] = pollWait;   // wait poll time before next Serial send
		return 0;
		break;

		case pollWait:  // wait poll time before next Serial send

		if (currentTime - startPollTimer[flow_] > pollDelay[flow_])
		{
			serialFlag[flow_] = taskComplete;   // Serial Port Ready fot new subTask
			//  SerialUSB.print("Preset -> ");
			//  SerialUSB.println(port_num);
			return 3;
		}
		return 0;
		break;
		// wait till packet left Serial buffer

		default:
		return 0;
		break;

	}
}






/************************************************************************

preset_multiple_registers

Write the data from an array into the holding registers of a
slave.

*************************************************************************/

byte currentCycle = 0;



byte fullMBPackageNumber[portQnt];
byte lastMBPackageLength[portQnt];									// Calc packets to transfer full data
byte addrShift_[portQnt];

unsigned int start_addr_[portQnt];
byte reg_count_[portQnt];

byte preset_multiple_registersX(byte flow_, int start_addr, int reg_count,  int *data_)
{
	int function = 0x10;  /* Function 16: Write Multiple Registers */
	int byte_count, i;
	int ret;
	byte byteCount = 6;
	byte port_num = PORT[flow_];
	byte slave_ = currentSlave[flow_];
	

	switch (serialFlag[flow_])
	{

		case taskComplete:
		
		if (currentCycle == 0)
		{
			fullMBPackageNumber[flow_] = reg_count * 0.04;
			lastMBPackageLength[flow_] = reg_count - fullMBPackageNumber[flow_] * 25;
			
			if (fullMBPackageNumber[flow_])
			reg_count_[flow_] = 25;
			else
			reg_count_[flow_] = reg_count;

			start_addr_[flow_] = start_addr;
			addrShift_[flow_] = 0;
			
			
			// 			SerialUSB.print("fullMBPackageNumber -> ");
			// 			SerialUSB.println(fullMBPackageNumber[flow_]);
			// 			SerialUSB.print("lastMBPackageLength -> ");
			// 			SerialUSB.println(lastMBPackageLength[flow_]);
			//
			// 			SerialUSB.print(start_addr_[flow_]);
			// 			SerialUSB.print("  ");
			// 			SerialUSB.print(addrShift_[flow_]);
			// 			SerialUSB.print(" ");
			// 			SerialUSB.print(reg_count_[flow_]);
			// 			SerialUSB.println();
			
		}
		else
		{
			if (currentCycle != fullMBPackageNumber[flow_])   // Check if current cycle is a full 25 regs send or last regs send
			{
				start_addr_[flow_] = start_addr_[flow_] + 25;    // if current send cycle == 0 -> start addr = start addr
			}
			else
			{
				start_addr_[flow_] = start_addr_[flow_] + 25;
				reg_count_[flow_] = lastMBPackageLength[flow_];
				
			}
			addrShift_[flow_] = start_addr_[flow_] - start_addr;
			
			// 			SerialUSB.print(start_addr_[flow_]);
			// 			SerialUSB.print("  ");
			// 			SerialUSB.print(addrShift_[flow_]);
			// 			SerialUSB.print(" ");
			// 			SerialUSB.print(reg_count_[flow_]);
			// 			SerialUSB.println();
		}
		
		
		
		serialFlag[flow_] = sendMsg;
		return 0;
		break;

		case sendMsg:  // Send packet
		{
			
			build_request_packet(slave_, function, start_addr_[flow_], reg_count_[flow_], packet[flow_]);
			
			byte_count = reg_count_[flow_] * 2;
			packet[flow_][6] = (unsigned char)byte_count;

			for (i = 0; i < reg_count_[flow_]; i++)
			{
				byteCount++;
				packet[flow_][byteCount] = data_[i+addrShift_[flow_]] >> 8;
				byteCount++;
				packet[flow_][byteCount] = data_[i+addrShift_[flow_]] & 0x00FF;
			}
			byteCount++;
			send_query(packet[flow_], byteCount, port_num);


			serialFlag[flow_] = sendingMsg;
			waitTillSent[flow_] = millis();
			packet_size[flow_] = byteCount;

			return 0;  // do nothing
		}
		break;



		case sendingMsg: // wait till packet left Serial buffer



		if (currentTime - waitTillSent[flow_] > (2 * (REQUEST_QUERY_SIZE + packet_size[flow_])))
		{
			// SerialUSB.println(REQUEST_QUERY_SIZE);
			// SerialUSB.println(packet_size[flow_]);
			serialFlag[flow_] = receiveMsg;   //
			startTimeOut[flow_] = currentTime;

		}
		return 0;  // do nothing

		break;


		case receiveMsg: // wait till packet left Serial buffer
		switch (receive_incoming_data_preset(flow_, data_))   // Прокерка ответа - 0 - входной буффер пуст, сообщения нет.Проверяем пока не вышло время тайм аута, >0 - овтет получен, <0 - ошибка
		{
			case 0:                                                                           // Входной буфер пуст
			if (currentTime - startTimeOut[flow_] > (WaitResponceGauge))                             // Входной буфер пуст - проверяем. как долго ожидался ответ?
			{
				serialFlag[flow_] = msgDone;


				return 1;  // TimeOut - not responded
			}
			return 0;  // do nothing
			break;


			default:



			serialFlag[flow_] = msgDone;

			return 2;  // Respond received
			break;
		}

		break;


		case msgDone:  // respond received, serial job done
		startPollTimer[flow_] = currentTime;   // 30 - delay between polls. When poll time expired - system can send new message via RS485
		serialFlag[flow_] = pollWait;   // wait poll time before next Serial send
		return 0;
		break;

		case pollWait:  // wait poll time before next Serial send

		if (currentTime - startPollTimer[flow_] > pollDelay[flow_])
		{
			serialFlag[flow_] = taskComplete;   // Serial Port Ready fot new subTask
			
			if (currentCycle == fullMBPackageNumber[flow_] )
			{
				currentCycle = 0;
				return 3;
			}
			
			
			currentCycle++;
			// 			SerialUSB.print("currentCycle ");
			// 			SerialUSB.print(currentCycle);
			// 			SerialUSB.println();
			
			//  SerialUSB.print("Preset -> ");
			//  SerialUSB.println(port_num);
			
		}
		return 0;
		break;
		// wait till packet left Serial buffer

		default:
		return 0;
		break;

	}
}

/*

int preset_multiple_registers(byte port_num, int slave, int start_addr,
int reg_count, int *data )
{
int function = 0x10;  // Function 16: Write Multiple Registers
int byte_count, i, packet_size = 6;
int ret;


build_request_packet(slave, function, start_addr, reg_count, packet_ch2);


byte_count = reg_count * 2;
packet_ch2[6] = (unsigned char)byte_count;

for (i = 0; i < reg_count; i++) {
packet_size++;
packet_ch2[packet_size] = data[i] >> 8;
packet_size++;
packet_ch2[packet_size] = data[i] & 0x00FF;
}

packet_size++;

if (send_query(packet_ch2, packet_size, port_num) > -1) {
//  ret = preset_response(port,packet);
}
else {
ret = -1;
}

return (ret);
}

*/





// *****************************  Check received data  ***************************************
/*
receive_incoming_data(serial port, slave_id, array, number_of_regs, port number)

checks if there is any valid request from the modbus master. If there is,
performs the action requested
*/




byte receive_incoming_Discrete_read(byte flow_, int* data_)
{
	int length;
	unsigned long now = millis();
	byte port_num = PORT[flow_];


	switch (port_num)
	{

		case 0:
		length = Serial.available();

		break;

		case 1:
		//	length = UARTX.available(UART1_CS, CH_A);

		break;

		case 2:

		length = UARTX.available(UART2_CS, CH_A);

		break;


		case 3:
		if (uartVer == sc740)
		length = UARTX.available(UART3_CS, CH_A);
		else
		length = UARTX.available(UART2_CS, CH_B);
		//
		// 		SerialUSB.print("Incoming ");
		// 		SerialUSB.println(length);

		break;


		case 4:
		length = Serial1.available();
		break;

		case 5:
		length = Serial3.available();

		break;

		case 6:
		length = Serial2.available();
		break;

		case 7:
		length = SerialUSB.available();
		break;
	}



	if (length == 0)
	{
		lastBytesReceived[port_num] = 0;
		return 0;
	}



	if (lastBytesReceived[port_num] != length)
	{
		lastBytesReceived[port_num] = length;
		Interframe_Silence[port_num] = now;
		return 0;
	}



	if (now - Interframe_Silence[port_num] < interframe_delay)
	return 0;

	// 	SerialUSB.print("Got Respond  ");
	// 	SerialUSB.println(currentSlave[flow_]);

	lastBytesReceived[port_num] = 0;

	length = 0;

	length = read_Discrete_response(flow_, data_);

	

	// 	SerialUSB.print("read_reg_response  ");
	// 	SerialUSB.println(length);
	

	return (length);

}



int read_Discrete_response(byte flow_, int* data_)
{

	byte recievedBytes[MAX_RESPONSE_LENGTH];
	int raw_response_length;
	int temp, i;



	raw_response_length = modbus_response(flow_, recievedBytes);


	// 	SerialUSB.print("modbus_response  ");
	// 	SerialUSB.print(currentSlave[flow_]);
	// 	SerialUSB.print("  ");
	// 	SerialUSB.println(raw_response_length);
	
	

	if (raw_response_length > 0)
	raw_response_length -= 2;


	if (raw_response_length > 0)
	for (i = 0;  i < recievedBytes[2]; i++)
	{
		for (byte j = 0;  j < 8; j++)
		data_[i*8 + j] = bitRead(recievedBytes[3+i], j);
	}
	
	return (raw_response_length);
}



byte receive_incoming_data_read(byte flow_, int* data_)
{
	int length;
	unsigned long now = millis();
	byte port_num = PORT[flow_];



	switch (port_num)
	{
		case 0:
		length = Serial.available();

		break;
		
		/*
		case 1:
		length = UARTX.available(UART1_CS, CH_A);

		break;
		*/
		case 2:

		length = UARTX.available(UART2_CS, CH_A);

		break;


		case 3:

		if (uartVer == sc740)
		length = UARTX.available(UART3_CS, CH_A);
		else
		length = UARTX.available(UART2_CS, CH_B);

		break;


		case 4:
		length = Serial1.available();
		break;

		case 5:
		length = Serial3.available();

		break;

		case 6:
		length = Serial2.available();
		break;

		case 7:
		length = SerialUSB.available();
		break;
	}



	if (length == 0)
	{
		lastBytesReceived[port_num] = 0;
		return 0;
	}



	if (lastBytesReceived[port_num] != length)
	{
		lastBytesReceived[port_num] = length;
		Interframe_Silence[port_num] = now;
		return 0;
	}



	if (now - Interframe_Silence[port_num] < interframe_delay)
	return 0;


	lastBytesReceived[port_num] = 0;

	length = 0;

	length = read_reg_response(flow_, data_);


	return (length);

}





byte checkIncoming(byte port_num)
{
	int length;
	unsigned long now = millis();

	switch (port_num)
	{
		/*
		case 1:
		length = UARTX.available(UART1_CS, CH_A);

		break;
		*/

		case 2:

		length = UARTX.available(UART2_CS, CH_A);

		break;


		case 3:

		
		if (uartVer == sc740)
		length = UARTX.available(UART3_CS, CH_A);
		else
		length = UARTX.available(UART2_CS, CH_B);

		break;

		case 4:
		length = Serial1.available();

		break;

		case 5:
		length = Serial3.available();

		break;

		case 6:
		length = Serial2.available();
		break;

		case 7:
		length = SerialUSB.available();
		break;

		default:

		return 0;

		break;
	}







	if (length == 0)
	{
		lastBytesReceived[port_num] = 0;
		return 0;
	}



	if (lastBytesReceived[port_num] != length)
	{
		lastBytesReceived[port_num] = length;
		Interframe_Silence[port_num] = now;
		return 0;
	}



	if (now - Interframe_Silence[port_num] < interframe_delay)
	return 0;



	lastBytesReceived[port_num] = 0;

	return (1);

}










// *****************************  Check received data  ***************************************
/*
receive_incoming_data(serial port, slave_id, array, number_of_regs, port number)

checks if there is any valid request from the modbus master. If there is,
performs the action requested
*/


byte receive_incoming_data_preset(byte flow_, int* data_)
{
	byte length;
	unsigned long now = millis();
	byte packet[REQUEST_QUERY_SIZE + CHECKSUM_SIZE];

	byte port_num = PORT[flow_];

	switch (port_num)
	{

		case 0:
		length =  Serial.available();
		break;

		case 2:
		length =  UARTX.available(UART2_CS, CH_A);

		break;
		
		
		case 3:
		
		if (uartVer == sc740)
		length = UARTX.available(UART3_CS, CH_A);
		else
		length = UARTX.available(UART2_CS, CH_B);
		

		break;
		case 4:
		length = Serial1.available();
		break;



		
		/*
		case 1:
		length =  UARTX.available(UART1_CS, CH_A);
		break;
		*/
		


		case 7:
		length = SerialUSB.available();
		break;


	}




	if (length == 0)
	{
		lastBytesReceived[port_num] = 0;
		return 0;
	}



	if (lastBytesReceived[port_num] != length)
	{
		lastBytesReceived[port_num] = length;
		Interframe_Silence[port_num] = now ;
		return 0;
	}



	if (now - Interframe_Silence[port_num] < interframe_delay)
	{
		return 0;
	}



	lastBytesReceived[port_num] = 0;

	length = read_reg_response(flow_, data_);


	return (length);

}


















































/*
update_mb_slave(slave_id, holding_regs_array, number_of_regs)

checks if there is any valid request from the modbus master. If there is,
performs the action requested
*/

unsigned long Nowdt = 0;
unsigned int lastBytesReceivedSlave;


int update_mb_slave(byte slave, int *regsH, byte *regsC,
unsigned int regs_size, unsigned int coil_size)
{
	unsigned char query[MAX_MESSAGE_LENGTH];
	unsigned char errpacket[EXCEPTION_SIZE + CHECKSUM_SIZE];
	unsigned int start_addr;
	int exception;
	int length = UARTX.available(UART1_CS, CH_A);




	if (length == 0)
	{
		lastBytesReceivedSlave = 0;
		return 0;
	}

	unsigned long now = millis();

	if (lastBytesReceivedSlave != length)
	{
		lastBytesReceivedSlave = length;
		Nowdt = now;
		return 0;
	}
	if (now - Nowdt < interframe_delay_slave)
	return 0;



	lastBytesReceivedSlave = 0;


	length = modbus_request(slave, query);
	if (length < 1)
	return length;


	exception = validate_request(query, length, regs_size, coil_size);
	if (exception)
	{
		build_error_packet(slave, query[FUNC], exception,
		errpacket);
		send_reply(errpacket, EXCEPTION_SIZE);
		return (exception);
	}


	start_addr = ((int) query[START_H] << 8) +
	(int) query[START_L];



	switch (query[FUNC])
	{

		case FC_READ_COILS:
		return read_coils_Slave(slave,
		start_addr,
		query[REGS_L],
		regsC);

		break;



		case FC_READ_REGS:
		return read_holding_registers_Slave(slave,
		start_addr,
		query[REGS_L],
		regsH);

		case FC_READ_INPUTS:
		return read_input_registers_Slave(slave,
		start_addr,
		query[REGS_L],
		regsH);

		break;
		case FC_WRITE_REGS:
		return preset_multiple_registers_Slave(slave,
		start_addr,
		query[REGS_L],
		query,
		regsH);
		break;
		case FC_WRITE_REG:
		write_single_register_Slave(slave,
		start_addr,
		query,
		regsH);
		break;
	}

}










void UpdateRelays()
{
	//******** Проверить отключенные датчики ***********

	for (byte j = 1; j <= BRSlice; j++)
	{
		if (X2X_Data_Array[j][TypeModule] == LDO)
		X2X_Data_Array[j][Data_0] = gaugeSlice(j);
		
	}
	
}




byte gaugeSlice(byte slice_)
{
	byte DoItRelay = B00000000;


	if (manualRelayMask != 0 && slice_ == manualSlice)      // If slice in manual mode
	return manualRelayMask;

	for (int j = 1; j <= totalSensors; j++)
	switch (gaugeData[j * 2])
	{

		case alarm:
		DoItRelay = DoItRelay | alarmRelayMask[slice_][j];
		break;

		case warning:
		DoItRelay = DoItRelay | warningRelayMask[slice_][j];

		break;

		case ok:
		// do nothing
		break;

		case lost:
		DoItRelay = DoItRelay | DisconnectRelayMask[slice_];
		break;

		case fault:
		DoItRelay = DoItRelay | DisconnectRelayMask [slice_];
		break;

		case disconnected:
		// do nothing
		break;
		
		case fireAlarm:
		DoItRelay = DoItRelay | alarmRelayMask[slice_][j];
		break;

	}


	if (BR_Fail[0] || (HMIstatus[currentSlave[HMI]] == lost))                     // If any slice fault
	DoItRelay = DoItRelay | systemFaultRelay [slice_];

	if (AlarmSoundOff == 1)                 // If Switch off beacon btn pressed
	DoItRelay = DoItRelay & BuzzerOffRelay[slice_];

	DoItRelay ^= reverseLDO[slice_];

	// 	if (slice_ == 1)
	// 	{
	// 		if (DoItRelay & watchDogRelay)
	// 		DoItRelay = DoItRelay & (~watchDogRelay);   // WatchDog relay is True when PLC ok. And goes False in case of fault
	// 		else
	// 		DoItRelay = DoItRelay | watchDogRelay;
	// 	}


	return DoItRelay;

}

















int check_HMI_command_regs(int respond)
{
	byte mask_HMI = 0;
	int HMI_i = 1;
	bool action;


	for (mask_HMI = 00000001; mask_HMI > 0; mask_HMI <<= 1)        // itarate through bit mask
	{
		action = respond & mask_HMI;                               // if bitwise AND resolves to true

		if (action)
		return (HMI_i);

		HMI_i++;
	}
	// SerialUSB.println ("Action ZERO");
	return 0;

}





int BufferCheck(int port_num)
{
	unsigned long now = millis();
	byte DirtInBuffer = 0;


	switch (port_num)
	{

		/*
		case 1:
		while (UARTX.available(UART1_CS, CH_A))
		{
		DirtInBuffer = 1;
		if (millis() - now <= BufCheckTimeOut)                             // Check for Timeout                                                                             // Data was not received - Timeout
		{
		UARTX.read(UART1_CS, CH_A);
		}
		else
		return (1);
		}

		break;
		*/

		case 2:
		while (UARTX.available(UART2_CS, CH_A))
		{
			DirtInBuffer = 1;
			if (millis() - now <= BufCheckTimeOut)                             // Check for Timeout                                                                             // Data was not received - Timeout
			{
				UARTX.read(UART2_CS, CH_A);
			}
			else
			return (1);
		}

		break;


		case 3:
		if (uartVer == sc740)
		{
			while (UARTX.available(UART3_CS, CH_A))
			{
				DirtInBuffer = 1;
				if (millis() - now <= BufCheckTimeOut)                             // Check for Timeout                                                                             // Data was not received - Timeout
				{
					UARTX.read(UART3_CS, CH_A);
				}
				else
				return (1);
			}
		}
		else
		{
			while (UARTX.available(UART2_CS, CH_B))
			{
				DirtInBuffer = 1;
				if (millis() - now <= BufCheckTimeOut)                             // Check for Timeout                                                                             // Data was not received - Timeout
				{
					UARTX.read(UART2_CS, CH_B);
				}
				else
				return (1);
			}
		}

		break;


		case 4:
		while (Serial1.available())
		{
			DirtInBuffer = 1;
			if (millis() - now <= BufCheckTimeOut)                             // Check for Timeout                                                                             // Data was not received - Timeout
			{
				Serial1.read();
			}
			else
			return (1);
		}

		break;

		case 5:
		while (Serial3.available())
		{
			DirtInBuffer = 1;
			if (millis() - now <= BufCheckTimeOut)                             // Check for Timeout                                                                             // Data was not received - Timeout
			{
				Serial3.read();
			}
			else
			return (1);
		}

		break;


		case 6:
		while (Serial2.available())
		{
			DirtInBuffer = 1;
			if (millis() - now <= BufCheckTimeOut)                             // Check for Timeout                                                                             // Data was not received - Timeout
			{
				Serial2.read();
			}
			else
			return (1);
		}

		break;


		if (DirtInBuffer != 0)
		return (1);
		return (0);

	}

}



void clearHMIdata(int clearByte_)
{
	for (int i = 0; i <= 110; i++)
	data_HMI[i] = clearByte_;
}



byte mbBridge()
{
	if (checkIncoming(7) == 1)
	{
		if (!SerialUSB)
		return 0;

		bridgeByte = SerialUSB.read();
		bridgePort = getPortNum(bridgeByte);


		switch (bridgePort)
		{
			/*
			case 1:
			UARTX.write(bridgeByte, UART1_CS, CH_A);

			while (SerialUSB.available())
			UARTX.write(SerialUSB.read(), UART1_CS, CH_A);

			break;
			*/

			case 2:
			UARTX.write(bridgeByte, UART2_CS, CH_A);

			while (SerialUSB.available())
			UARTX.write(SerialUSB.read(), UART2_CS, CH_A);

			break;


			case 3:
			if (uartVer == sc740)
			UARTX.write(bridgeByte, UART3_CS, CH_A);
			else
			UARTX.write(bridgeByte, UART2_CS, CH_B);
			

			while (SerialUSB.available())
			{
				if (uartVer == sc740)
				UARTX.write(SerialUSB.read(), UART3_CS, CH_A);
				else
				UARTX.write(SerialUSB.read(), UART2_CS, CH_B);
			}

			break;


			case 4:

			digitalWrite(SSerialTxControl_1, RS485Transmit);
			Serial1.write(bridgeByte);

			while (SerialUSB.available())
			Serial1.write(SerialUSB.read());

			break;


			case 5:

			digitalWrite(SSerialTxControl_3, RS485Transmit);
			Serial3.write(bridgeByte);

			while (SerialUSB.available())
			Serial3.write(SerialUSB.read());

			break;




			case 6:

			digitalWrite(SSerialTxControl_2, RS485Transmit);
			Serial2.write(bridgeByte);

			while (SerialUSB.available())
			Serial2.write(SerialUSB.read());


			break;

		}

	}

	if (checkIncoming(bridgePort) == 1)

	switch (bridgePort)
	{
		/*
		case 1:

		while (UARTX.available(UART1_CS, CH_A))
		SerialUSB.write(UARTX.read(UART1_CS, CH_A));
		break;
		*/

		case 2:
		while (UARTX.available(UART2_CS, CH_A))
		SerialUSB.write(UARTX.read(UART2_CS, CH_A));
		break;


		case 3:
		if (uartVer == sc740)
		{
			while (UARTX.available(UART3_CS, CH_A))
			SerialUSB.write(UARTX.read(UART3_CS, CH_A));
		}
		else
		{
			while (UARTX.available(UART2_CS, CH_B))
			SerialUSB.write(UARTX.read(UART2_CS, CH_B));
		}
		break;


		case 4:
		while ( Serial1.available())
		SerialUSB.write(Serial1.read());
		break;


		case 5:
		while ( Serial3.available())
		SerialUSB.write(Serial3.read());
		break;

		case 6:
		while (Serial2.available())
		SerialUSB.write(Serial2.read());
		break;

	}
}







byte firmIncoming(byte port_num)
{
	int length;
	unsigned long now = millis();

	switch (port_num)
	{

		case 1:
		length = UARTX.available(UART1_CS, CH_A);

		break;

		case 2:

		length = UARTX.available(UART2_CS, CH_A);

		break;


		case 3:

		if (uartVer == sc740)
		length = UARTX.available(UART3_CS, CH_A);
		else
		length = UARTX.available(UART2_CS, CH_B);


		break;

		case 4:
		length = Serial1.available();

		break;

		case 5:
		length = Serial3.available();

		break;

		case 6:
		length = Serial2.available();
		break;

		case 7:
		length = SerialUSB.available();
		break;

	}



	if (length == 0)
	{
		lastBytesReceived[port_num] = 0;
		return 0;
	}



	if (lastBytesReceived[port_num] != length)
	{
		lastBytesReceived[port_num] = length;
		Interframe_Silence[port_num] = now;
		return 0;
	}



	if (now - Interframe_Silence[port_num] < interframe_delay)
	return 0;



	lastBytesReceived[port_num] = 0;

	return (1);

}











void initSerial()
{
	if (SerialUSB_Ok == 1)  // Activate USB -> Service text mode
	SerialUSB.begin(115200, SERIAL_8N1);            // Serial USB

	Serial.begin(9600, SERIAL_8N1);   // X2X line
	
	if (uartVer==sc740)
	UARTX.begin(9600, 0, UART1_CS, CH_A);   // PC - RS485 output


	UARTX.begin(19200, 0, UART2_CS, CH_A);  // HMI line



	
	
	// Load Parity Config
	loadConfig("Parity.txt", outArray);

	// added 752 port
	if (uartVer == sc740)
	UARTX.begin(9600, outArray[1], UART3_CS, CH_A); // S1
	else
	UARTX.begin(9600, outArray[1], UART2_CS, CH_B); // S1 in 752 NXP
	

	switch (outArray[2])
	{
		case 0:
		Serial1.begin(9600, SERIAL_8N1);       // Serial port initialization - HMI output
		break;

		case 1:
		Serial1.begin(9600, SERIAL_8O1);       // Serial port initialization - HMI output
		break;

		case 2:
		Serial1.begin(9600, SERIAL_8E1);       // Serial port initialization - HMI output
		break;

		default:
		Serial1.begin(9600, SERIAL_8N1);       // Serial port initialization - HMI output
		break;
	}


	switch (outArray[3])
	{
		case 0:
		Serial2.begin(9600, SERIAL_8N1);       // Serial port initialization - HMI output
		break;

		case 1:
		Serial2.begin(9600, SERIAL_8O1);       // Serial port initialization - HMI output
		break;

		case 2:
		Serial2.begin(9600, SERIAL_8E1);       // Serial port initialization - HMI output
		break;

		default:
		Serial2.begin(9600, SERIAL_8N1);       // Serial port initialization - HMI output
		break;
	}

	switch (outArray[4])
	{
		case 0:
		Serial3.begin(9600, SERIAL_8N1);       // Serial port initialization - HMI output
		break;

		case 1:
		Serial3.begin(9600, SERIAL_8O1);       // Serial port initialization - HMI output
		break;

		case 2:
		Serial3.begin(9600, SERIAL_8E1);       // Serial port initialization - HMI output
		break;

		default:
		Serial3.begin(9600, SERIAL_8N1);       // Serial port initialization - HMI output
		break;
	}

}



void initPins()
{

	pinMode(wizINT, INPUT);
	pinMode(UART1_CS, OUTPUT);
	pinMode(UART2_CS, OUTPUT);
	pinMode(UART3_CS, OUTPUT);
	pinMode(microSD_CS, OUTPUT);
	pinMode(ETH1_CS, OUTPUT);
	pinMode(ETH2_CS, OUTPUT);
	pinMode(X2X_LED, OUTPUT);
	pinMode(PLC_ok, OUTPUT);
	pinMode(SD_OK, OUTPUT);
	pinMode(X2X_Transmit, OUTPUT);

	pinMode(A7, OUTPUT);
	
	digitalWrite(A7, LOW);
	digitalWrite(A0, HIGH);
	digitalWrite(X2X_LED, LOW);
	digitalWrite(UART1_CS, HIGH);
	digitalWrite(UART2_CS, HIGH);
	digitalWrite(UART3_CS, HIGH);
	digitalWrite(microSD_CS, HIGH);
	digitalWrite(ETH1_CS, HIGH);
	digitalWrite(ETH2_CS, HIGH);
	digitalWrite(microSD_CS, HIGH);
	digitalWrite(X2X_Transmit, LOW);

	

	pinMode(SSerialTxControl_0, OUTPUT);             // Initialize receive-transmit enable PIN
	pinMode(SSerialTxControl_1, OUTPUT);             // Initialize receive-transmit enable PIN
	pinMode(SSerialTxControl_2, OUTPUT);             // Initialize receive-transmit enable PIN
	pinMode(SSerialTxControl_3, OUTPUT);             // Initialize receive-transmit enable PIN

	digitalWrite(SSerialTxControl_0, RS485Receive);   // Set default state of Enable Pin
	digitalWrite(SSerialTxControl_1, RS485Receive);   // Set default state of Enable Pin
	digitalWrite(SSerialTxControl_2, RS485Receive);   // Set default state of Enable Pin
	digitalWrite(SSerialTxControl_3, RS485Receive);   // Set default state of Enable Pin

	delay(100);
	digitalWrite(SSerialTxControl_0, RS485Transmit);   // Set default state of Enable Pin
	delay(100);
	digitalWrite(SSerialTxControl_1, RS485Transmit);   // Set default state of Enable Pin
	delay(100);
	digitalWrite(SSerialTxControl_2, RS485Transmit);   // Set default state of Enable Pin
	delay(100);
	digitalWrite(SSerialTxControl_3, RS485Transmit);   // Set default state of Enable Pin
	delay(100);

	digitalWrite(SSerialTxControl_0, RS485Receive);   // Set default state of Enable Pin
	digitalWrite(SSerialTxControl_1, RS485Receive);   // Set default state of Enable Pin
	digitalWrite(SSerialTxControl_2, RS485Receive);   // Set default state of Enable Pin
	digitalWrite(SSerialTxControl_3, RS485Receive);   // Set default state of Enable Pin
}



void initEthernet()
{

	loadConfig("RTU.txt", outArray);
	PLC_SlaveAdress = outArray[1];
	// The media access control (ethernet hardware) address for the shield


	loadConfig("mac.txt", outArray);
	byte mac[] = { outArray[1], outArray[2], outArray[3], outArray[4], outArray[5], outArray[6]};
	loadConfig("IP.txt", outArray);
	byte ip[] = { outArray[1], outArray[2], outArray[3], outArray[4]};
	loadConfig("SUBNET.txt", outArray);
	IPAddress subnet(outArray[1], outArray[2], outArray[3], outArray[4]);         //Маска
	loadConfig("GATE.txt", outArray);
	IPAddress gateway(outArray[1], outArray[2], outArray[3], outArray[4]);          //Шлюз
	//IPAddress gateway(192, 168, 92, 1);          //Шлюз

	loadConfig("DCP.txt", outArray);
	Ethernet1Mode = outArray[1];

	loadConfig("DCP2.txt", outArray);
	Ethernet2Mode = outArray[1];

	//
	if (Ethernet1Mode == TCPIP) // Modbus TCP connection
	{
		Ethernet.select(ETH1_CS);
		mb1.config(mac, ip);  //Config Modbus IP
	}
	else
	{
		Ethernet1Mode = IEC104mode;
		Ethernet.select(ETH1_CS);
		Ethernet.begin(mac, ip, gateway, subnet);
	}
	
	
	loadConfig("mac2.txt", outArray);
	for (byte i=1; i<=6; i++)
	mac[i-1] = outArray[i];
	
	loadConfig("IP2.txt", outArray);
	for (byte i=1; i<=4; i++)
	ip[i-1] = outArray[i];
	
	
	loadConfig("SUBNET2.txt", outArray);
	for (byte i=1; i<=4; i++)
	subnet[i-1] = outArray[i];
	
	
	loadConfig("GATE2.txt", outArray);
	for (byte i=1; i<=4; i++)
	gateway[i-1] = outArray[i];
	
	if (Ethernet2Mode == TCPIP) // Modbus TCP connection
	{
		; //Ethernet.select(ETH2_CS);
		//	mb2.config(mac, ip);  //Config Modbus IP
	}
	else
	{
		Ethernet.select(ETH2_CS);
		Ethernet.begin(mac, ip, gateway, subnet);
	}
	
}

// 
// 
// byte loadConfigFloat(char* fileName, float* output_)
// {
// 	byte i = 1;
// 	String sdLine;
// 	byte totalStrings_ = 255;
// 
// 	digitalWrite(SD_OK, LOW);
// 
// 	File file_ = SD.open(fileName);
// 
// 	if (SerialUSB_Ok)
// 	{
// 		SerialUSB.print("SD Opening -> ");
// 		SerialUSB.println(fileName);
// 	}
// 
// 	for (byte k = 0; k < 40; k++)
// 	output_[k] = 0;
// 
// 	if (!file_)
// 	{
// 		if (SerialUSB_Ok)
// 		{
// 			SerialUSB.print(fileName);
// 			SerialUSB.println(" -> файл не найден ");
// 		}
// 		return (0);
// 	}
// 
// 
// 
// 	if (SerialUSB_Ok)
// 	{
// 		SerialUSB.print(fileName);
// 		SerialUSB.println(" -> Loaded");
// 	}
// 
// 
// 
// 	if (file_.available())
// 	totalStrings_ = file_.parseInt();
// 	else
// 	{
// 		digitalWrite(SD_OK, HIGH);
// 		return 0;
// 	}
// 
// 
// 	//	output_[0] = totalStrings_;
// 
// 	if (totalStrings_ == 0)
// 	{
// 		digitalWrite(SD_OK, HIGH);
// 		return 0;
// 	}
// 
// 
// 	while (file_.available() && (i <= totalStrings_))
// 	{
// 		output_[i] = file_.parseFloat();
// 
// 		if (SerialUSB_Ok)
// 		SerialUSB.println(output_[i]); //Printing for debugging purpose
// 		i++;
// 	}
// 
// 
// 	file_.close();
// 	digitalWrite(SD_OK, HIGH);
// 
// 	if (SerialUSB_Ok)
// 	SerialUSB.println("********************************************"); //Printing for debugging purpose
// 
// 	return (totalStrings_);
// }


byte loadConfigFloat(char* fileName, float* output_)
{
	byte i = 0;
	String sdLine;
	byte totalStrings_ = 255;

	digitalWrite(SD_OK, LOW);

	if (!SD.exists(fileName))
	return 0;

	File file_ = SD.open(fileName);

	if (sdDebug) {
		SerialUSB.print("SD Opening -> ");
		SerialUSB.println(fileName);
	}

	for (byte k = 0; k < 40; k++)
	output_[k] = 0;

	if (!file_) {
		//	SDconfigState = 0;
		if (sdDebug) {
			SerialUSB.print(fileName);
			SerialUSB.println(" -> file not found");
		}
		return 0;
	}

	if (sdDebug) {
		SerialUSB.print(fileName);
		SerialUSB.println(" -> Loaded");
	}

	int lineNumber = 0;
	while (file_.available()) {
		sdLine = file_.readStringUntil('\n');
		sdLine.trim(); // Remove any leading/trailing whitespace

		// Ignore comments
		int commentIndex = sdLine.indexOf("//");
		if (commentIndex != -1) {
			sdLine = sdLine.substring(0, commentIndex);
			sdLine.trim(); // Remove any leading/trailing whitespace again
		}

		// Skip empty lines
		if (sdLine.length() == 0) continue;

		if (lineNumber == 0) {
			totalStrings_ = sdLine.toInt();
			output_[0] = totalStrings_;
			if (totalStrings_ == 0) {
				digitalWrite(SD_OK, HIGH);
				return 0;
			}
			} else if (lineNumber <= totalStrings_) {
			output_[lineNumber] = sdLine.toFloat();
			if (sdDebug)
			SerialUSB.println(output_[lineNumber]); // Printing for debugging purpose
			} else {
			break; // If we have read all required lines, break out of the loop
		}

		lineNumber++;
	}

	file_.close();
	digitalWrite(SD_OK, HIGH);

	if (sdDebug)
	SerialUSB.println("********************************************"); // Printing for debugging purpose

	return 1;
}




byte loadConfig(char* fileName, int* output_) {
	byte i = 0;
	String sdLine;
	byte totalStrings_ = 255;

	digitalWrite(SD_OK, LOW);

	if (!SD.exists(fileName))
	return 0;

	File file_ = SD.open(fileName);

	if (sdDebug) {
		SerialUSB.print("SD Opening -> ");
		SerialUSB.println(fileName);
	}

	for (byte k = 0; k < 40; k++)
	output_[k] = 0;

	if (!file_) {
		//	SDconfigState = 0;
		if (sdDebug) {
			SerialUSB.print(fileName);
			SerialUSB.println(" -> file not found");
		}
		return 0;
	}

	if (sdDebug) {
		SerialUSB.print(fileName);
		SerialUSB.println(" -> Loaded");
	}

	int lineNumber = 0;
	while (file_.available()) {
		sdLine = file_.readStringUntil('\n');
		sdLine.trim(); // Remove any leading/trailing whitespace

		// Ignore comments
		int commentIndex = sdLine.indexOf("//");
		if (commentIndex != -1) {
			sdLine = sdLine.substring(0, commentIndex);
			sdLine.trim(); // Remove any leading/trailing whitespace again
		}

		// Skip empty lines
		if (sdLine.length() == 0) continue;

		if (lineNumber == 0) {
			totalStrings_ = sdLine.toInt();
			output_[0] = totalStrings_;
			if (totalStrings_ == 0) {
				digitalWrite(SD_OK, HIGH);
				return 0;
			}
			} else if (lineNumber <= totalStrings_) {
			output_[lineNumber] = sdLine.toInt();
			if (sdDebug)
			SerialUSB.println(output_[lineNumber]); // Printing for debugging purpose
			} else {
			break; // If we have read all required lines, break out of the loop
		}

		lineNumber++;
	}

	file_.close();
	digitalWrite(SD_OK, HIGH);

	if (sdDebug)
	SerialUSB.println("********************************************"); // Printing for debugging purpose

	return 1;
}




byte saveConfig(char* fileName, int* input_, byte size_)
{
	String sdLine;

	if (SD.exists(fileName))
	SD.remove(fileName);
	else return (0);


	File file_ = SD.open(fileName, FILE_WRITE);


	if (!file_)
	{
		if (SerialUSB_Ok)
		{
			SerialUSB.print(fileName);
			SerialUSB.println(" -> file cannot be opened");
		}
		return (0);
	}

	if (SerialUSB_Ok)
	{
		SerialUSB.print(fileName);
		SerialUSB.println(" -> Saving ....");
	}

	file_.println(size_);            // First line - total number of strings

	for (byte i = 1; i <= size_; i++)
	{
		dataString = String(input_[i]);
		file_.println(dataString);
	}

	file_.println("Fin");   // Last String - "Fin"

	file_.close();

	if (SerialUSB_Ok)
	SerialUSB.println(" -> Saved");

	return (1);
}






byte regsNoRespond(byte flow_)
{

	byte port_num = PORT[flow_];
	serialFlag[flow_] = msgDone;

	byte currentSensor = currentSlave[flow_];

	if (logDisabled[currentSensor] == 1)
	{
		logDisabled[currentSensor] = 0;

		sdString = "Газоанализатор -> ";
		sdString += currentSensor;
		sdString += " подключен к системе";

		printData();
	}


	if (lostPackets[currentSensor] < LostPacketError)
	{
		lostPackets[currentSensor]++;
		return (0);
	}


	startPollTimer[flow_] = millis();
	gaugeData[currentSensor * 2 - 1] = hide;                // HMI: Датчик не ответил - надпись на поле датчика "не подключен"- код " - 1"
	gaugeData[currentSensor * 2] = lost;                     // HMI: Датчик не ответил - цвет поля красный - код 0                                                                    // HMI: Датчик не ответил - цвет общего фона красный - код B00000001
	gaugeStatus[currentSensor] = 0; // Показания датчика



	//   if (EthernetMode == TCPIP)
	//     mb1.Ireg(currentSensor-1, 0);


	//  IEC104data_M_SP[2 * currentSensor - 1] = 0;
	//  IEC104data_M_SP[2 * currentSensor] = lost;


	if (logDisconnect[currentSensor] == 0)
	{	
		logDisconnect[currentSensor] = 1;
		sdString = "Газоанализатор ->";
		sdString += currentSensor;
		sdString += " не отвечает";
		printData();
	}

}

byte levelDebug = 0;

byte regsResponded(byte flow_, int *data_)
{
	// 1) Базовые проверки/подготовка
	const byte currentSensor = currentSlave[flow_];
	if (currentSensor == 0) {
		serialFlag[flow_] = msgDone;
		startPollTimer[flow_] = millis();
		return 0;
	}

	// port_num не используется — оставляю только чтобы не потерять контекст
	// const byte port_num = PORT[flow_];

	serialFlag[flow_] = msgDone;
	startPollTimer[flow_] = millis();
	lostPackets[currentSensor] = 0;

	// Индексы в массивах (1-based сенсоры):
	const int idxValue  = (int)currentSensor * 2 - 1;  // значение
	const int idxStatus = (int)currentSensor * 2;      // статус/цвет

	// 2) Измерение: пишем gaugeData и используем то же значение для сравнения порогов
	if ((emulationActive == 1) && (sensorEmul == currentSlave[flow_])) {
		gaugeData[idxValue] = data_[0];
		} else {
		gaugeData[idxValue] = convertion[currentSensor] * data_[0];
	}

	// То, с чем дальше сравниваем пороги (чтобы не было рассинхрона raw vs converted)
	const auto meas = gaugeData[idxValue];

	// 3) Статус датчика
	gaugeStatus[currentSensor] = data_[1];

	// 4) Логи «подключен/на связи»
	if (logDisabled[currentSensor] == 1) {
		logDisabled[currentSensor] = 0;
		sdString = "Газоанализатор -> ";
		sdString += currentSensor;
		sdString += " подключен к системе";
		printData();
	}

	if (logDisconnect[currentSensor] == 1) {
		logDisconnect[currentSensor] = 0;
		sdString = "Газоанализатор -> ";
		sdString += currentSensor;
		sdString += " на связи";
		printData();
	}

	// 5) Ошибка датчика
	if (data_[1] != 0) {
		if (logError[currentSensor] == 0) {
			logError[currentSensor] = 1;
			sdString = "Газоанализатор ->";
			sdString += currentSensor;
			sdString += " сообщает об ошибке -> ";
			sdString += data_[1];
			printData();
		}

		// HMI: скрыть значение и показать fault
		gaugeData[idxValue]  = hide;
		gaugeData[idxStatus] = fault;
		return 0;
	}

	// 6) Ошибки сняты (в исходнике не было printData())
	if (logError[currentSensor] == 1) {
		logError[currentSensor] = 0;
		sdString = "Газоанализатор ->";
		sdString += currentSensor;
		sdString += ". Все ошибки сняты ";
		sdString += data_[1];
		printData();
	}

	// 7) Debug
	if (levelDebug) {
		SerialUSB.print("meas -> ");
		SerialUSB.println(meas);
		SerialUSB.print("warnL -> ");
		SerialUSB.println(warningSetpointL[currentSensor]);
		SerialUSB.print("warnH -> ");
		SerialUSB.println(warningSetpointH[currentSensor]);
	}

	// 8) OK зона
	if (meas < warningSetpointH[currentSensor] && meas > warningSetpointL[currentSensor]) {
		ScreenState[currentSensor] = colorGrey;
		gaugeData[idxStatus] = ok;

		if (levelDebug) {
			SerialUSB.println("OK");
		}

		if (logYellow[currentSensor] == 1) {
			logYellow[currentSensor] = 0;
			sdString = "Газоанализатор -> ";
			sdString += currentSensor;
			sdString += " < ";
			sdString += warningSetpointH[currentSensor];
			sdString += ". Концентрация ниже первого порога";
			printData();
		}

		return 0;
	}

	if (levelDebug) {
		SerialUSB.print("alarmL -> ");
		SerialUSB.println(alarmSetpointL[currentSensor]);
		SerialUSB.print("alarmH -> ");
		SerialUSB.println(alarmSetpointH[currentSensor]);
	}

	// 9) ALARM зона
	if (meas >= alarmSetpointH[currentSensor] || meas <= alarmSetpointL[currentSensor]) {
		gaugeData[idxStatus] = alarm;

		if (levelDebug) {
			SerialUSB.println("ALARM");
		}

		if (logRed[currentSensor] == 0) {
			logRed[currentSensor] = 1;
			sdString = "Газоанализатор -> ";
			sdString += currentSensor;
			sdString += " > ";
			sdString += alarmSetpointH[currentSensor];
			sdString += ". Авария, превышен второй порог.";
			printData();
		}

		return 0;
	}

	// 10) WARNING зона
	if (levelDebug) {
		SerialUSB.println("warn? -> ");
	}

	if (((meas >= warningSetpointH[currentSensor] && meas < alarmSetpointH[currentSensor]) ||
	(meas <= warningSetpointL[currentSensor] && meas > alarmSetpointL[currentSensor])) &&
	gaugeData[idxStatus] != alarm)
	{
		gaugeData[idxStatus] = warning;

		if (levelDebug) {
			SerialUSB.println("WARNING");
		}

		if (logYellow[currentSensor] == 0) {
			logYellow[currentSensor] = 1;
			sdString = "Газоанализатор ->";
			sdString += currentSensor;
			sdString += " > ";
			sdString += warningSetpointH[currentSensor];
			sdString += ". Превышен первый порог.";
			printData();
		}

		if (logRed[currentSensor] == 1) {
			logRed[currentSensor] = 0;
			sdString = "Газоанализатор ->";
			sdString += currentSensor;
			sdString += " < ";
			sdString += alarmSetpointH[currentSensor];
			sdString += ". Концентрация упала ниже второго порога";
			printData();
		}
	}

	if (levelDebug) {
		SerialUSB.println("no");
	}

	// 11) Обязательный return (иначе UB)
	return 0;
}


/*
byte regsResponded(byte flow_, int *data_)
{

	byte currentSensor = currentSlave[flow_];
	byte port_num = PORT[flow_];


	serialFlag[flow_] = msgDone;
	startPollTimer[flow_] = millis();   // Записать текущее время и засечь интервал
	lostPackets[currentSensor] = 0;
	
	if ((emulationActive == 1) && (sensorEmul == currentSlave[flow_]))
	{
		gaugeData[currentSensor * 2 - 1] = data_[0];
	}
	else
	gaugeData[currentSensor * 2 - 1] = convertion[currentSensor] * data_[0]; // Показания датчика
	
	gaugeStatus[currentSensor] = data_[1]; // Ошибка датчика


// 	SerialUSB.print(currentSensor);
// 	SerialUSB.print(" -> ");
// 	SerialUSB.print(currentSensor);
// 	SerialUSB.print(" : convertion -> ");
// 	SerialUSB.print(convertion[currentSensor]);
// 	SerialUSB.print(" : gaugeData -> ");
// 	SerialUSB.println(gaugeData[currentSensor * 2 - 1]);

	//  IEC104data_M_SP[2 * currentSensor - 1] = ModbusCharBuffer[0];


	if (logDisabled[currentSensor] == 1)
	{
		logDisabled[currentSensor] = 0;

		sdString = "Газоанализатор -> ";
		sdString += currentSensor;
		sdString += " подключен к системе";

		printData();
	}


	if (logDisconnect[currentSensor] == 1)
	{
		logDisconnect[currentSensor] = 0;
		sdString = "Газоанализатор -> ";
		sdString += currentSensor;
		sdString += " на связи";

		printData();
	}



	if (data_[1] != 0)
	{
		if (logError[currentSensor] == 0)
		{
			logError[currentSensor] = 1;
			sdString = "Газоанализатор ->";
			sdString += currentSensor;
			sdString += " сообщает об ошибке -> ";
			sdString += data_[1];

			printData();
		}


		//  IEC104data_M_SP[2 * currentSensor] = fault;

		// PC: Массив показаний датчиков для отправки на пульт PC
		gaugeData[currentSensor * 2 - 1] = hide;                // HMI: Датчик не ответил - надпись на поле датчика "не подключен"- код " - 1"
		gaugeData[currentSensor * 2] = fault;                     // HMI: Датчик не ответил - цвет поля красный - код 0
		
		return 0;                                                               // HMI: Датчик не ответил - цвет общего фона красный - код B00000001
	}



	if (logError[currentSensor] == 1)
	{
		logError[currentSensor] = 0;
		sdString = "Газоанализатор ->";
		sdString += currentSensor;
		sdString += ". Все ошибки сняты ";
		sdString += data_[1];
	}

	if (levelDebug)
	{
		SerialUSB.print("data_[0] -> ");
		SerialUSB.println(data_[0]);
		SerialUSB.print("warnL -> ");
		SerialUSB.println(warningSetpointL[currentSensor]);
		SerialUSB.print("warnH -> ");
		SerialUSB.println(warningSetpointH[currentSensor]);
	}

	if (data_[0] < warningSetpointH[currentSensor] && data_[0] > warningSetpointL[currentSensor])                 // Показания не превышают первый порог?
	{
		ScreenState[currentSensor] = colorGrey;                                               // Массив цветов панели - для текущего датчика цвет ЗЕЛЕНЫЙ
		gaugeData[currentSensor * 2] = ok;                                        // Цвет поля датчика на панели ЗЕЛЕНЫЙ, КОД 2

		//  IEC104data_M_SP[2 * currentSensor] = ok;

		if (levelDebug)
		{
			SerialUSB.println("Ok");
		}

		if (logYellow[currentSensor] == 1)
		{
			logYellow[currentSensor] = 0;
			sdString = "Газоанализатор -> ";
			sdString += currentSensor;
			sdString += " < ";
			sdString += warningSetpointH[currentSensor];
			sdString += ". Концентрация ниже первого порога";

			printData();
		}
		return 0;
	}

	if (levelDebug)
	{
		SerialUSB.print("alarmL -> ");
		SerialUSB.println(alarmSetpointL[currentSensor]);
		SerialUSB.print("alarmnH -> ");
		SerialUSB.println(alarmSetpointH[currentSensor]);
	}
	
	if (data_[0] >= alarmSetpointH[currentSensor] || data_[0] <= alarmSetpointL[currentSensor])          // Проверка на превышение второго порога  Alarm
	{
		gaugeData[currentSensor * 2] = alarm;                                       // Превышен второй порог, фон датчика красный


		if (levelDebug)
		{
			SerialUSB.println("Ok");
		}
		//  IEC104data_M_SP[2 * currentSensor] = alarm;



		if (logRed[currentSensor] == 0)
		{
			logRed[currentSensor] = 1;
			sdString = "Газоанализатор -> ";
			sdString += currentSensor;
			sdString += " > ";
			sdString += alarmSetpointH[currentSensor];
			sdString += ". Авария, превышен второй порог.";

			printData();
		}
		return 0;
	}

	if (levelDebug)
	{
		SerialUSB.println("warn? -> ");
	}

	if (((data_[0] >= warningSetpointH[currentSensor] &&  data_[0] < alarmSetpointH[currentSensor]) || (data_[0] <= warningSetpointL[currentSensor] &&  data_[0] > alarmSetpointL[currentSensor])) && gaugeData[currentSensor * 2] != alarm)                   // Если показания превысили первый порог но не привысели второй - ЖЕЛТАЯ ЗОНА
	{
		gaugeData[currentSensor * 2] = warning;                                        // Превышен первый порог, фон датчика желтый

		if (levelDebug)
		{
			SerialUSB.println("yes");
		}

		//  IEC104data_M_SP[2 * currentSensor] = warning;


		if (logYellow[currentSensor] == 0)
		{
			logYellow[currentSensor] = 1;
			sdString = "Газоанализатор ->";
			sdString += currentSensor;
			sdString += " > ";
			sdString += warningSetpointH[currentSensor];
			sdString += ". Превышен первый порог.";

			printData();
		}

		if (logRed[currentSensor] == 1)
		{
			logRed[currentSensor] = 0;
			sdString = "Газоанализатор ->";
			sdString += currentSensor;
			sdString += " < ";
			sdString += alarmSetpointH[currentSensor];
			sdString += ".  Концентрация упала ниже второго порога";

			printData();
		}

	}
	
	
	if (levelDebug)
	{
		SerialUSB.println("no");
	}
	
	
	// gaugeData[currentSlave_1 * 2] = gaugeData[currentSlave_1 * 2]; //| (SensorStatusByte[currentSlave_1] << 2);
}

*/


byte fireResponded(byte flow_, int *data_)
{
	byte currentSensor = currentSlave[flow_];
	byte port_num = PORT[flow_];

	serialFlag[flow_] = msgDone;
	startPollTimer[flow_] = millis();   // Записать текущее время и засечь интервал
	lostPackets[currentSensor] = 0;
	//gaugeData[currentSensor * 2 - 1] = data_[0]; // Показания датчика
	
	gaugeData[currentSensor * 2 - 1] = hide; // Показания датчика
	gaugeStatus[currentSensor] = data_[1]; // Ошибка датчика


	//  IEC104data_M_SP[2 * currentSensor - 1] = ModbusCharBuffer[0];


	if (logDisabled[currentSensor] == 1)
	{
		logDisabled[currentSensor] = 0;

		sdString = "Газоанализатор -> ";
		sdString += currentSensor;
		sdString += " подключен к системе";

		printData();
	}


	if (logDisconnect[currentSensor] == 1)
	{
		logDisconnect[currentSensor] = 0;
		sdString = "Газоанализатор -> ";
		sdString += currentSensor;
		sdString += " на связи";

		printData();
	}



	if (data_[1] != 0)
	{
		if (logError[currentSensor] == 0)
		{
			logError[currentSensor] = 1;
			sdString = "Газоанализатор ->";
			sdString += currentSensor;
			sdString += " сообщает об ошибке -> ";
			sdString += data_[1];

			printData();
		}


		//  IEC104data_M_SP[2 * currentSensor] = fault;

		// PC: Массив показаний датчиков для отправки на пульт PC
		gaugeData[currentSensor * 2 - 1] = hide;                // HMI: Датчик не ответил - надпись на поле датчика "не подключен"- код " - 1"
		gaugeData[currentSensor * 2] = fault;                     // HMI: Датчик не ответил - цвет поля красный - код 0                                                                    // HMI: Датчик не ответил - цвет общего фона красный - код B00000001
	}



	if (logError[currentSensor] == 1 &&  data_[1] == 0)
	{
		logError[currentSensor] = 0;
		sdString = "Газоанализатор ->";
		sdString += currentSensor;
		sdString += ". Все ошибки сняты ";
		sdString += data_[1];
	}



	if (data_[0] == 1 && data_[1] == 0)          // Проверка на превышение второго порога  Alarm
	{
		gaugeData[currentSensor * 2] = fireAlarm;                                       // Превышен второй порог, фон датчика красный

		if (logRed[currentSensor] == 0)
		{
			logRed[currentSensor] = 1;
			sdString = "Датчик пламени -> ";
			sdString += currentSensor;
			sdString += " > ";
			sdString += alarmSetpointH[currentSensor];
			sdString += ". Обнаружено пламя";

			printData();
		}

	}
	
	if (data_[0] == 0 && data_[1] == 0)                 // Показания не превышают первый порог?
	{
		ScreenState[currentSensor] = colorGrey;                                               // Массив цветов панели - для текущего датчика цвет ЗЕЛЕНЫЙ
		gaugeData[currentSensor * 2] = ok;                                        // Цвет поля датчика на панели ЗЕЛЕНЫЙ, КОД 2

		//  IEC104data_M_SP[2 * currentSensor] = ok;


		if (logRed[currentSensor] == 1)
		{
			logRed[currentSensor] = 0;
			sdString = "Датчик пламени -> ";
			sdString += currentSensor;
			sdString += " < ";
			sdString += warningSetpointH[currentSensor];
			sdString += ". Нет пламени";

			printData();
		}
	}
	


	// gaugeData[currentSlave_1 * 2] = gaugeData[currentSlave_1 * 2]; //| (SensorStatusByte[currentSlave_1] << 2);
}



void regsDisabled(byte currentSensor)
{
	gaugeData[currentSensor * 2 - 1] = hide;
	gaugeData[currentSensor * 2] = ok;                                        // Цвет поля датчика на панели ЗЕЛЕНЫЙ, КОД 2

	if (Ethernet1Mode == TCPIP)
	{
		//  mb1.Ireg(currentSensor-1, 0);
	}

	//  IEC104data_M_SP[2 * currentSensor - 1] = 0;
	//  IEC104data_M_SP[2 * currentSensor] = disconnected;



	if (logDisabled[currentSensor] == 0)
	{
		// Увеличиваем значение текущего датчика на 1, т.е. переходим к следующему по счету.

		logDisabled[currentSensor] = 1;
		sdString = "Газоанализатор ->";
		sdString += currentSensor;
		sdString += " отключен программно";

		printData();
	}

}



void updateColor()
{

	byte color = B00000000;

	color = gaugeColor();  // Get common gauge color

	color = color | systemColor();  // Get common gauge color
	gaugeData[0] = color;           // Common system status screen
	

}


void moveDataMB()
{
	int j = 0;
	
	for (int i = 0; i < mbMESize; i++) // Clear data
	RS485_Coil[i] = 0;
	
	for (int i = 0; i < mbCoilSize; i++) // Clear data
	mb1.Coil(mbCoilShift+i, 0);
	
	mbMoveCoils();


	j = 0;  // Start from beginning

	for (int i = 1; i <= totalSensors; i++)    // move concentration data to IEC buffer
	{
		mb1.Ireg(mbIregShift+i - 1, gaugeData[i * 2 - 1]);
		j++;                    // Data moved counter;
	}

	
}


uint16_t IGAS_SP(uint8_t slave_, uint16_t j)
{
	switch (gaugeData[slave_ * 2])  // Warning, alarm?
	{
		case warning:
		checkAndMove(changeBuffer, j, 1);    //1 Warning
		coilArrayWrite(j, 1);
		mb1.Coil(mbCoilShift+j, 1);

		break;

		case alarm:
		checkAndMove(changeBuffer, j + 1, 1); // Alarm
		coilArrayWrite(j + 1, 1);
		mb1.Coil(mbCoilShift+j+1, 1);
		break;

		default:

		break;
	}


	if (gaugeStatus[slave_] & B00000001)
	{
		checkAndMove(changeBuffer, j + 2, 1);        // Ref PIR fault
		coilArrayWrite(j + 2, 1);
		mb1.Coil(mbCoilShift+j+2, 1);
	}
	if (gaugeStatus[slave_] & B00000010)
	{
		checkAndMove(changeBuffer, j + 3, 1);        // Gas PIR fault
		coilArrayWrite(j + 3, 1);
		mb1.Coil(mbCoilShift+j+3, 1);
	}


	if (gaugeStatus[slave_] & B00000100)
	{
		checkAndMove(changeBuffer, j + 4, 1); // IR source fault
		coilArrayWrite(j + 4, 1);
		mb1.Coil(mbCoilShift+j+4, 1);

	}

	if (gaugeStatus[slave_] & B00001000)
	{
		checkAndMove(changeBuffer, j + 5, 1); // EEPROM fault
		coilArrayWrite(j + 5, 1);
		mb1.Coil(mbCoilShift+j+5, 1);
	}

	if (gaugeStatus[slave_] & B00010000)
	{
		checkAndMove(changeBuffer, j + 6, 1); // Optical pass contaminated
		coilArrayWrite(j + 6, 1);
		mb1.Coil(mbCoilShift+j+6, 1);
	}

	if (gaugeStatus[slave_] & B00100000)
	{
		checkAndMove(changeBuffer, j + 7, 1); // Heater fail
		coilArrayWrite(j + 7, 1);
		mb1.Coil(mbCoilShift+j+7, 1);
	}

// 	if  (gaugeData[slave_ * 2] == lost)
// 	{
// 		checkAndMove(changeBuffer, j + 8, 1); // Connection lost
// 		coilArrayWrite(j + 8, 1);
// 		mb1.Coil(mbCoilShift+j+8, 1);
// 	}
	return 8;
}


uint16_t binar_SP(uint8_t slave_, uint16_t j)
{
	switch (gaugeData[slave_ * 2])  // Warning, alarm?
	{
		case warning:
		checkAndMove(changeBuffer, j, 1);    // Warning
		coilArrayWrite(j, 1);
		mb1.Coil(mbCoilShift+j, 1);
		break;

		case alarm:
		checkAndMove(changeBuffer, j + 1, 1); // Alarm
		coilArrayWrite(j + 1, 1);
		mb1.Coil(mbCoilShift+j+1, 1);
		break;

		default:

		break;
	}


	if (gaugeStatus[slave_])
	{
		checkAndMove(changeBuffer, j + 2, 1);   // Sensor Fail
		coilArrayWrite(j + 2, 1);
		mb1.Coil(mbCoilShift+j+2, 1);
	}
	
	if  (gaugeData[slave_ * 2] == lost)
	{
		checkAndMove(changeBuffer, j + 3, 1); // Connection lost
		coilArrayWrite(j + 3, 1);
		mb1.Coil(mbCoilShift+j+3, 1);
	}
	return 4;
}



uint16_t proSense_SP(uint8_t slave_, uint16_t j)
{
	switch (gaugeData[slave_ * 2])  // Warning, alarm?
	{
		case warning:
		checkAndMove(changeBuffer, j, 1);    // Warning
		coilArrayWrite(j, 1);
		mb1.Coil(mbCoilShift+j, 1);
		break;

		case alarm:
		checkAndMove(changeBuffer, j + 1, 1); // Alarm
		coilArrayWrite(j + 1, 1);
		mb1.Coil(mbCoilShift+j+1, 1);
		break;

		default:

		break;
	}


	if (gaugeStatus[slave_])
	{
		checkAndMove(changeBuffer, j + 2, 1);   // Sensor Fail
		coilArrayWrite(j + 2, 1);
		mb1.Coil(mbCoilShift+j+2, 1);
	}
	
	if  (gaugeData[slave_ * 2] == lost)
	{
		checkAndMove(changeBuffer, j + 3, 1); // Connection lost
		coilArrayWrite(j + 3, 1);
		mb1.Coil(mbCoilShift+j+3, 1);
	}
	return 4;
}



uint16_t ERIS_SP(uint8_t slave_, uint16_t j)
{
	switch (gaugeData[slave_ * 2])  // Warning, alarm?
	{
		case warning:
		checkAndMove(changeBuffer, j, 1);    // Warning
		coilArrayWrite(j, 1);
		mb1.Coil(mbCoilShift+j, 1);
		break;

		case alarm:
		checkAndMove(changeBuffer, j + 1, 1); // Alarm
		coilArrayWrite(j + 1, 1);
		mb1.Coil(mbCoilShift+j+1, 1);
		break;

		default:

		break;
	}


	if (gaugeStatus[slave_] & B00000001)
	{
		checkAndMove(changeBuffer, j + 2, 1);   // Sensor Fail
		coilArrayWrite(j + 2, 1);
		mb1.Coil(mbCoilShift+j+2, 1);
	}
	
	if  (gaugeData[slave_ * 2] == lost)
	{
		checkAndMove(changeBuffer, j + 3, 1); // Connection lost
		coilArrayWrite(j + 3, 1);
		mb1.Coil(mbCoilShift+j+3, 1);
	}
	return 4;
}




uint16_t  FIRE_SP(uint8_t slave_, uint16_t j)
{
	switch (gaugeData[slave_ * 2])  // Warning, alarm?
	{
		case alarm:
		checkAndMove(changeBuffer, j, 1); // Alarm
		coilArrayWrite(j, 1);
		mb1.Coil(mbCoilShift+j, 1);
		break;

		default:

		break;
	}
	
	
	if  (gaugeData[slave_ * 2] == lost)
	{
		checkAndMove(changeBuffer, j + 1, 1); // Connection lost
		coilArrayWrite(j + 1, 1);
		mb1.Coil(mbCoilShift+j+1, 1);
	}
	return 2;
}




















void proceedProtocolData()
{
	int j = 0;
	
	for (int i = 0; i < mbMESize; i++) // Clear data
	RS485_Coil[i] = 0;
	
	for (int i = 0; i < mbCoilSize; i++) // Clear data
	mb1.Coil(mbCoilShift+i, 0);
	
	

	for (int i = 1; i <= totalSensors; i++) // save Main room detectors data
	{
		switch(vendor[i])
		{
			case IR_SF6:
			j = j + IGAS_SP(i, j);	// IGAS SP Data

			break;
			
			case DM910:

			break;
			
			case DM910H:
			
			break;
			
			case PVT100:
			
			break;
			
			case ERIS210:
			
			j = j + ERIS_SP(i, j);	// ERIS210 SP Data
			
			break;
			
			case fireDet:
			
			j = j + FIRE_SP(i, j);	// fireDet SP Data
			
			break;
			
			case binar:
			
			j = j + binar_SP(i, j);	// proSense SP Data
			
			break;
			
			case proSense:
			
			j = j + proSense_SP(i, j);	// proSense SP Data
			
			break;
			
		}

	}


	for (int i = 1; i <= totalSensors; i++)
	if (gaugeData[i * 2] == lost || gaugeData[i * 2] == fault )
	{
		checkAndMove(changeBuffer, j, 1); // Common alarm
		coilArrayWrite(j, 1);
		mb1.Coil(mbCoilShift+j, 1);
	}

	checkAndMove(changeBuffer, j + 1, redundantFlag[PORT[RS1]]); // Redundant loop 1
	mb1.Coil(mbCoilShift+j+1, 1);
	checkAndMove(changeBuffer, j + 2, redundantFlag[PORT[RS2]]); // Redundant loop 2
	mb1.Coil(mbCoilShift+j+2, 1);

	j = j + 3;

	for (byte i = 1; i <= BRSlice; i++)
	{
		if (BR_Fail[i])
		{
			checkAndMove(changeBuffer, j, 1); // BR fault
			coilArrayWrite(j, 1);
			mb1.Coil(mbCoilShift+j, 1);
		}
		j++;
	}



	// Check if any data has changed in M_SP buffer
	int dataChng_M_SP_ = 0;

	for (int i = 0; i <= j; i++)
	{
		IEC104spont_M_SP[dataChng_M_SP_] = 0;

		if (IEC104data_M_SP[i] != changeBuffer[i])
		{
			// IEC104.spontaneousSend = spontaneosActivate;
			IEC104spont_M_SP[dataChng_M_SP_] = i;
			dataChng_M_SP_++;
		}


		IEC104data_M_SP[i] = changeBuffer[i];
		changeBuffer[i] = 0;
	}


	// Move concentration to Modbus TCP data array

	j = 0;  // Start from beginning

	for (int i = 1; i <= totalSensors; i++)    // move concentration data to IEC buffer
	{
		changeBuffer[j] = gaugeData[i * 2 - 1];
		RS485_Holding[j] = gaugeData[i * 2 - 1];
		mb1.Ireg(mbIregShift+i - 1, gaugeData[i * 2 - 1]);
		j++;                    // Data moved counter;
	}

	//    SerialUSB.print(" --------------> ");

	// Check if any data has changed in M_ME buffer

	int dataChng_M_ME_ = 0;

	for (int i = 0; i <= j; i++)
	{
		IEC104spont_M_ME[dataChng_M_ME_] = 0;

		if (IEC104data_M_ME[i] != changeBuffer[i])
		{
			// IEC104.spontaneousSend = spontaneosActivate;
			IEC104spont_M_ME[dataChng_M_ME_] = i;
			dataChng_M_ME_++;
		}

		IEC104data_M_ME[i] = changeBuffer[i];
		changeBuffer[i] = 0;
	}


	// If any changes happen - send as spontaneos data IEC104

	if (dataChng_M_SP_ + dataChng_M_ME_)
	{
		IEC104_E1.spontaneousSend = spontaneosActivate;
		IEC104_E1.dataChng_M_SP_ = dataChng_M_SP_;
		IEC104_E1.dataChng_M_ME_ = dataChng_M_ME_;

		IEC104_E2.spontaneousSend = spontaneosActivate;
		IEC104_E2.dataChng_M_SP_ = dataChng_M_SP_;
		IEC104_E2.dataChng_M_ME_ = dataChng_M_ME_;

	}
}


void checkAndMove(int *changeBuffer_, int register_, int newValue_)
{
	changeBuffer_[register_] = newValue_;

	/*
	if (IEC104data_M_SP[register_] != newValue_)
	{
	changeBuffer_[register_] = newValue_;
	IEC104.spontaneousSend = spontaneosActivate;
	}
	else changeBuffer_[register_] = IEC104data_M_SP[register_];
	*/

}



byte gaugeColor()
{
	byte color = B00000000;


	for (int j = 1; j <= totalSensors; j++)
	switch (gaugeData[j * 2])
	{

		case alarm:
		color = color | HMIRedScreen | HMIRedAlarm;

		break;

		case warning:
		color = color | HMIYellowScreen | HMIYellowAlarm;
		break;

		case ok:
		color = color | colorGrey;
		break;

		case lost:
		color = color | HMIRedScreen;
		break;

		case fault:
		color = color | HMIRedScreen;
		break;

		case disconnected:
		color = color | colorGrey;
		break;

		default:
		color = color | HMIRedScreen;
		break;

		case fireAlarm:
		color = color | HMIRedScreen;
		break;
	}

	return color;


}


/*
byte HMIRedScreen = B00000001;     // Фон красного цвета
byte HMIYellowScreen = B00000010;    // Фон желтого цвета
byte HMIRedAlarm = B00000100;    // Превышен второй порог - красный  фон
byte HMIYellowAlarm = B00001000;     // Превышен первый порог - желтый фон
byte sysFault = B00010000;       // Авария в системе автоматики
byte lostConnect = B00100000;      // Потеряна связь с детекторами
*/

byte systemColor()
{
	byte color = B00000000;

	BR_Fail[0] = 0;

	for (byte i = 1; i <= BRSlice; i++)        // Check slice Status
	if (BR_Fail[i] == TRUE)
	{
		BR_Fail[0] = BR_Fail[0] | BR_Fail_Mask[i];
		color = color | HMIRedScreen | sysFault;
	}

	if (redundantFlag[PORT[RS1]] == loopFail)         // Check Loop status
	{
		BR_Fail[0] = BR_Fail[0] | BR_Fail_Mask[7];
		color = color | HMIRedScreen | lostConnect;
	}

	if (redundantFlag[PORT[RS2]] == loopFail)         // Check Loop status
	{
		BR_Fail[0] = BR_Fail[0] | BR_Fail_Mask[8];
		color = color | HMIRedScreen | lostConnect;
	}
	//  SerialUSB.print("Color -> ");
	//  SerialUSB.println(BR_Fail[0]);
	return color;


}




void softReset()
{
	const int RSTC_KEY = 0xA5;
	RSTC->RSTC_CR = RSTC_CR_KEY(RSTC_KEY) | RSTC_CR_PROCRST | RSTC_CR_PERRST;
	while (true);
}




void printData()
{
	dataFile = SD.open("log.txt", FILE_WRITE);
	// if the file is available, write to it:
	if (dataFile)
	{
		dataFile.println(sdString);

		dataFile.print(rtc_clock.get_hours());
		dataFile.print(": ");
		dataFile.print(rtc_clock.get_minutes());
		dataFile.print(": ");
		dataFile.println(rtc_clock.get_seconds());
		dataFile.print(" ");
		dataFile.print(daynames[rtc_clock.get_day_of_week() - 1]);
		dataFile.print(": ");
		dataFile.print(rtc_clock.get_days());
		dataFile.print(".");
		dataFile.print(rtc_clock.get_months());
		dataFile.print(".");
		dataFile.println(rtc_clock.get_years());
		dataFile.println("");
		dataFile.close();

		sdString = "";
		// print to the serial port too:
	}

}





//      serialFlag[RS_Line_] == initNewTask

byte taskTimer()
{
	if (currentTime - lastTaskTime > pollTaskTime)
	{
		nextTask();

		if  (taskFlag[currentTask] == standBy)
		{
			taskFlag[currentTask] = doTask;
			lastTaskTime = currentTime;
		}
	}
}

//********************************************************************
//             Gas Concentration Request - Channel 1
//********************************************************************

/*
const byte taskComplete = 0;
const byte initNewTask = 1;
const byte sendMsg = 2;
const byte sendingMsg = 3;
const byte receiveMsg = 4;
const byte msgDone = 5;
const byte pollWait = 6;
*/


byte nextSlave(byte flow_)
{

	byte currentSlave_;

	if (currentSlave[flow_] < lowLim[flow_])
	currentSlave_ = lowLim[flow_];
	else
	currentSlave_ =  currentSlave[flow_] + 1;


	if (currentSlave_ > hiLim[flow_])                                             // Если это был последний датчик в цепи, то переходим к первому.
	currentSlave_ = lowLim[flow_];


	currentSlave[flow_] = currentSlave_;


}


void nextX2X()
{
	currentSlave[X2X]++;
	//SerialUSB.println(currentSlave[X2X]);
	if (currentSlave[X2X] > BRSlice)
	currentSlave[X2X] = 1;
	
	if (X2X_debug)
	{
		SerialUSB.print("*** X2X Next Slave -> ");
		SerialUSB.println(currentSlave[X2X]);
	}


	taskFlag[X2X] = standBy;  // Ready for new task
}



byte getPortNum(byte bridgeByte_)
{

	if (lowLim[RS1] <= bridgeByte_ && bridgeByte_ <= hiLim[RS1])
	return PORT[RS1];


	if (lowLim[RS2] <= bridgeByte_ && bridgeByte_ <= hiLim[RS2])
	return PORT[RS2];

	if (lowLim[RS3] <= bridgeByte_ && bridgeByte_ <= hiLim[RS3])
	return PORT[RS3];

	if (lowLim[RS4] <= bridgeByte_)
	return PORT[RS4];

}


byte nextTask()
{
	currentTask++;

	if (currentTask > totalTasks)
	currentTask = 0;
}




byte what_to_do()
{
	byte command_;

	/*  SerialUSB.println("Start Buffer ");
	for (byte x = 0; x<=4; x++)
	SerialUSB.println(packet_ch2[x]);
	*/



	command_ = check_HMI_command_regs(data_HMI[1]);

	manualRelayMaskBuffer = data_HMI[2];
	manualSliceBuffer = data_HMI[4];

	data_HMI[1] = 0;

	if (command_ != 0) //&& command_ != 8)
	return (command_ + 20);     // first command byte starts from 11. First command bit = 12, second = 13, etc


	command_ = check_HMI_command_regs(data_HMI[0]);
	data_HMI[0] = 0;

	//  if (AlarmSoundOff == 1 && command_ == 8)
	//     return 0;

	if (command_ != 0) // && command_ != 8)
	return (command_ + 10);   // first command byte starts from 11. First command bit = 12, second = 13, etc




	//  if (command_ == 8)
	//  return (command_ + 10);

	return 999; // No action needed.

}


void checkNewCommand()
{
	switch (read_holding_registers(HMI, 1802, 5, data_HMI))    // packet_ch2
	{
		case 2:
		// Data received
		nextCommand = what_to_do();

		break;

		case 3:
		subTask_HMI = nextCommand;
		break;
	}
}



void relayToHMI()
{
	switch (subsubTask[HMI])
	{
		case 0:
		switch (read_holding_registers(HMI, 801, 5, data_HMI))
		{
			case 2:
			BR_Position = data_HMI[0];
			sensorPosition = data_HMI[1];
			sensorEmul = sensorPosition;
			//  SerialUSB.println(BR_Position);

			clearHMIdata(0);
			break;

			case 3:

			data_HMI[0] = warningRelayMask[BR_Position][sensorPosition]; //EEPROM.read(flashOffset +750+BRSlice*(j-1)+i);      // маска второго порога реле
			data_HMI[1] = alarmRelayMask[BR_Position][sensorPosition]; //EEPROM.read(flashOffset +750+BRSlice*(j-1)+i);      // маска второго порога реле
			data_HMI[2] = warningSetpointH[sensorPosition];
			data_HMI[3] = alarmSetpointH[sensorPosition];
			data_HMI[4] = MLTPL[sensorPosition];
			data_HMI[5] = gasType[sensorPosition];
			data_HMI[6] = units[sensorPosition];

			preset_multiple_registers(HMI, 803, 7, data_HMI);
			subsubTask[HMI] = 1;
			break;
		}
		break;

		case 1:
		if (preset_multiple_registers(HMI, 803, 7, data_HMI) == portReady)
		{
			subTask_HMI = 20; // Reset comand buttons
			subsubTask[HMI] = 0;
		}
		break;
	}
}


void resetBtns()
{
	if (serialFlag[HMI] == taskComplete)
	{
		if (AlarmSoundOff == 1)
		data_HMI[0] = B10000000;
		else
		data_HMI[0] = 0;

		data_HMI[1] = 0;
	}


	if (preset_multiple_registers(HMI, 1802, 2, data_HMI) == portReady) // Preset registers
	subTask_HMI = 999;
}



void resetManualRelay()
{
	manualRelayMask = 0; // Test relay
	subTask_HMI = 20;
}


void setManualRelay()
{
	manualRelayMask = manualRelayMaskBuffer; // Test relay
	manualSlice = manualSliceBuffer;
	subTask_HMI = 20;
}


void toolBox()
{
	serviceMode = data_HMI[3] & B0000001;  // ToolBox radioButton Active
	subTask_HMI = 20;

	if (serviceMode == 1)
	{
		SerialUSB.begin(9600, SERIAL_8N1);     // Serial USB
	}
	else
	SerialUSB.end();     // Serial USB
}




void outProtocol()
{

	if (data_HMI[3] & B0000010)  // Activate IEC104
	Ethernet1Mode = IEC104mode;
	else
	Ethernet1Mode = TCPIP;

	outArray[1] = Ethernet1Mode;
	saveConfig("IEC104.txt", outArray, 1);

	softReset();

	subTask_HMI = 20;


}

void saveRelayToMemory()
{
	switch (read_holding_registers(HMI, 801, 6, data_HMI))
	{
		case 2:
		BR_Position = data_HMI[0];
		sensorPosition = data_HMI[1];

		warningRelayMask[BR_Position][sensorPosition]  = data_HMI[2];
		alarmRelayMask[BR_Position][sensorPosition]  = data_HMI[3];

		warningSetpointH[sensorPosition] = data_HMI[4];
		alarmSetpointH[sensorPosition] = data_HMI[5];


		switch (BR_Position)
		{
			case 1:
			saveConfig("YMask1.txt", warningRelayMask[1], totalSensors);
			saveConfig("RMask1.txt", alarmRelayMask[1], totalSensors);
			break;

			case 2:
			saveConfig("YMask2.txt", warningRelayMask[2], totalSensors);
			saveConfig("RMask2.txt", alarmRelayMask[2], totalSensors);
			break;

			case 3:
			saveConfig("YMask3.txt", warningRelayMask[3], totalSensors);
			saveConfig("RMask3.txt", alarmRelayMask[3], totalSensors);
			break;

			case 4:
			saveConfig("YMask4.txt", warningRelayMask[4], totalSensors);
			saveConfig("RMask4.txt", alarmRelayMask[4], totalSensors);
			break;
		}
		saveConfig("Yellow.txt", warningSetpointH, totalSensors);
		saveConfig("Red.txt", alarmSetpointH, totalSensors);

		break;

		case 3:
		subTask_HMI = 20;
		break;

	}
}


void makeGasNameList()
{
	clearHMIdata(-1);    // -1 - empty space, 0 - disconnected, 1 ... 100 - Gas name
	for (int i = 0; i < totalSensors; i++)
	{
		if (enabled[i+1] == 0)
		data_HMI[i] = 0;
		else
		data_HMI[i] = gasType[i+1];
	}
}


void gasTypesToHMI()
{
	if (serialFlag[HMI] == taskComplete)
	makeGasNameList(); // -1 - empty space, 0 - disconnected, 1 ... 100 - Gas name

	if (preset_multiple_registersX(HMI, 1101, hmiSendSize, data_HMI ) == portReady) // Preset registers
	subTask_HMI = 20;

}




void enableDisableDetector()   // Turn On and Off gas analyzer in system
{
	switch (read_holding_registers(HMI, 1098, 3, data_HMI))  // Ask gauge - send request
	{
		case 2:

		MBposition = data_HMI[0];
		enabled[MBposition] = data_HMI[1];

		if (data_HMI[1] == 0)
		regsDisabled(MBposition);

		saveConfig("Enbld.txt", enabled, totalSensors);

		data_HMI[0] = 0;
		break;

		case 3:
		subTask_HMI = 20;
		break;

	}
}


void serialPortsToHMI()   // Upload Serial Port number to HMI
{

	//   if (serialFlag[port_HMI] == taskComplete)
	//      {
	// SerialUSB.print("port_S1 -> ");
	// SerialUSB.println(port_S1);

	if (serialFlag[HMI] == taskComplete)
	{
		for (byte i = 0; i <= 3; i++)
		data_HMI[i] = B00000001 << (PORT[i + 3] - 3);
	}


	if (preset_multiple_registers(HMI, 791, 4, data_HMI) == portReady)      // Preset registers
	subTask_HMI = 20;
}





void saveSerialPortToSD()
{

	switch (read_holding_registers(HMI, 791, 4, data_HMI)) // Ask gauge - send request
	{
		case 2:

		for (byte i = 0; i <= 3; i++)
		switch (data_HMI[i])
		{
			case 1:
			PORT[i + 3] = 3;
			break;

			case 2:
			PORT[i + 3] = 4;
			break;

			case 4:
			PORT[i + 3] = 5;
			break;

			case 8:
			PORT[i + 3] = 6;
			break;
		}

		//    SerialUSB.println(port_S1);

		for (byte i = 1; i <= 4; i++)
		data_HMI[i] = PORT[i + 2];


		saveConfig("EIA.txt", data_HMI, EIA_Lines);
		break;

		case 3:
		subTask_HMI = 20;
		break;
	}
}



byte switchOffSound()
{
	if (AlarmSoundOff == 0)
	{
		AlarmSoundOff = 1;
		soundTimer = currentTime;
		//  SerialUSB.print("Start -> ");
		//  SerialUSB.println(soundTimer);
	}
	else if (currentTime - soundTimer >  BuzzerOffTime)
	{

		// SerialUSB.println("Finish -> ");
		// SerialUSB.println(currentTime - BuzzerOffTime);
		AlarmSoundOff = 0;
		subTask_HMI = 20;
		return 0;
	}
	subTask_HMI = 0;
}




void X2X_mb()
{
	switch (X2X_Data_Array[currentSlave[X2X]][TypeModule])
	{
		case LDO:
		if (serialFlag[X2X] == taskComplete)
		{
			// 			SerialUSB.print("LDO ");
			// 			SerialUSB.println(currentSlave[X2X]);
			// 			SerialUSB.print("TypeModule ");
			// 			SerialUSB.println(X2X_Data_Array[currentSlave[X2X]][TypeModule]);
			// 			SerialUSB.println();
			data_X2X[0] = X2X_Data_Array[currentSlave[X2X]][Data_0];
			
		}
		X2X_preset();
		break;

		case LDI8:
		if (serialFlag[X2X] == taskComplete)
		{
			// 			SerialUSB.print("LAI ");
			// 			SerialUSB.println(currentSlave[X2X]);
			// 			SerialUSB.print("TypeModule ");
			// 			SerialUSB.println(X2X_Data_Array[currentSlave[X2X]][TypeModule]);
			// 			SerialUSB.println();
		}
		X2X_read();
		break;
		
	}
}


enum
{
	RS_inProgress,
	RS_TimeOut,
	RS_Respond,
	RS_Finished,
};

byte const RSdebug = 0;

byte X2X_preset()
{
	switch (preset_multiple_registers(X2X, 0, 1, data_X2X))
	{
		case RS_TimeOut: // not responded
		brLost();   // BR communication lost. Proceed this event
		
		break;

		case RS_Respond:		   // Received Respond

		taskFlag[X2X] = standBy;  // Ready for new task

		BR_lostpacket[currentSlave[X2X]] = 0;
		if (logX2X[currentSlave[X2X]] == 1)
		{
			logX2X[currentSlave[X2X]] = 0;
			sdString = "Блок реле -> ";
			sdString += currentSlave[X2X];
			sdString += " снова на связи";
			printData();
		}
		// 		SerialUSB.print("Ok slave -> ");
		// 		SerialUSB.println(currentSlave[X2X]);
		BR_Fail[currentSlave[X2X]] = FALSE;

		break;


		case RS_Finished:		   // Next Task

		nextX2X();
		taskFlag[X2X] = standBy;  // Ready for new task
		subTask_X2X++;

		break;

		default:

		break;
	}
}



byte X2X_read()
{
	// General mode
	switch (read_holding_registers(X2X, 0, 1, data_X2X))
	{
		case RS_TimeOut: // Timeout respond
		//regsNoRespond(X2X);
		brLost();
		// 		SerialUSB.print("Lost slave -> ");
		// 		SerialUSB.println(currentSlave[X2X]);
		break;

		case RS_Respond: // Recieve respond
		//regsResponded(X2X, data_X2X);
		X2X_Data_Array[currentSlave[X2X]][Data_0] = data_X2X[0];


		BR_lostpacket[currentSlave[X2X]] = 0;
		if (logX2X[currentSlave[X2X]] == 1)
		{
			logX2X[currentSlave[X2X]] = 0;
			sdString = "Блок реле -> ";
			sdString += currentSlave[X2X];
			sdString += " снова на связи";
			printData();
		}
		// 		SerialUSB.print("Ok slave -> ");
		// 		SerialUSB.println(currentSlave[X2X]);
		BR_Fail[currentSlave[X2X]] = FALSE;
		// SerialUSB.println("LAI OK");

		break;

		case RS_Finished:  // Serial actions finished and poll time delay done. Ready for new subtask

		nextX2X();
		taskFlag[X2X] = standBy;  // Ready for new task
		subTask_X2X++;

		break;

		default:

		break;
	}
}



void setRTCdata()
{

	switch  (read_holding_registers(HMI, 401, 6, data_HMI)) // Ask gauge - send request
	{
		case 2:       // Data received
		/*
		SerialUSB.print("year -> ");
		SerialUSB.println(data_HMI[4]);

		SerialUSB.print("ucMonth -> ");
		SerialUSB.println(data_HMI[3]);

		SerialUSB.print("ucDay -> ");
		SerialUSB.println(data_HMI[2]);

		SerialUSB.print("ucHour -> ");
		SerialUSB.println(data_HMI[1]);

		SerialUSB.print("ucMinute -> ");
		SerialUSB.println( data_HMI[0]);

		SerialUSB.print("ucSecond -> ");
		SerialUSB.println(0);

		SerialUSB.println("******* HMI ********");
		SerialUSB.println();
		*/
		rtc_clock.set_time(data_HMI[1], data_HMI[0], 0);
		rtc_clock.set_date(data_HMI[2], data_HMI[3], data_HMI[4]);

		break;

		case 3:
		subTask_HMI = 20;
		break;
	}

}

int debugC = 0;

/*
byte getDetectorAlarmData(byte flow_, byte subTask_, int *data_)
{

// Signal Emulation Mode
if ((emulationActive == 1) && (sensorEmul == currentSlave[flow_]))  // if emulation mode active - update detector data and go next task
{
switch (emulStatus)
{
case 0:
data_[0] = emulConcentration;
data_[1] = 0;
//SerialUSB.println("Emilation - ");
//debugC++;
//SerialUSB.println(debugC);
regsResponded(flow_, data_);
break;

case 1:
regsNoRespond(flow_);
break;

case 2:
data_[0] = 0;
data_[1] = 1;
regsResponded(flow_, data_);
break;

case 3 ... 8:
data_[0] = 0;
data_[1] = 1 << (emulStatus - 3);
regsResponded(flow_, data_);
break;

}
subsubTask_S1 = 0;
taskFlag[flow_] = standBy;            // Ready for new task
serialFlag[flow_] = taskComplete;     // Serial Task Complete
nextSlave(flow_);               // Next slave - 0 - nothing to do. Else - go to next slave
subTask_++;
return subTask_;
}


if (gasType[currentSlave[flow_]] != 0)  // If detector enabled and serviceMode switched off
{
// General mode
switch (subsubTask_S1)
{
case 0:
switch (read_holding_registers(flow_, currentSlave[flow_], 504, 2, data_))
{
case 1: // Timeout respond
regsNoRespond(flow_);

return subTask_;
break;

case 2: // Recieve respond

alarmBuffer[1][0][1] = data_[0];
alarmBuffer[1][1][1] = data_[1];

return subTask_;
break;

case 3:  // Serial actions finished and poll time delay done. Ready for new subtask
taskFlag[flow_] = standBy;  // Ready for new task
nextSlave(flow_);      // Next slave - 0 - nothing to do. Else - go to next slave
subTask_;
subsubTask_S1++;
return subTask_;
break;

default:

return subTask_;
break;
}
break;


case 1:
switch (read_holding_registers(flow_, currentSlave[flow_], 0, 2, data_))
{
case 1: // Timeout respond
regsNoRespond(flow_);

return subTask_;
break;

case 2: // Recieve respond
regsResponded(flow_, data_);

return subTask_;
break;

case 3:  // Serial actions finished and poll time delay done. Ready for new subtask
taskFlag[flow_] = standBy;  // Ready for new task
nextSlave(flow_);      // Next slave - 0 - nothing to do. Else - go to next slave
subTask_++;
subsubTask_S1 = 0;
return subTask_;
break;

default:

return subTask_;
break;
}
break;
}

}
else  // If detector disabled or service Mode active
{
taskFlag[flow_] = standBy;  // Ready for new task
gaugeData[currentSlave[flow_] * 2 - 1] = hide;                // HMI: Датчик не ответил - надпись на поле датчика "не подключен"- код " - 1"
gaugeData[currentSlave[flow_] * 2] = ok;                     // HMI: Датчик не ответил - цвет поля красный - код 0
subsubTask_S1 = 0;
nextSlave(flow_);      // Next slave - 0 - nothing to do. Else - go to next slave
subTask_++;
return subTask_;
}
// subTask 0 Finish ***************************************
}

*/


uint8_t getRSdata(byte flow_, byte subTask_, int *data_)
{
	/*
	if (dmDebug)
	{
	SerialUSB.print("flow -> ");
	SerialUSB.println(flow_);

	SerialUSB.print("currentSlave -> ");
	SerialUSB.println(currentSlave[flow_]);

	SerialUSB.print("gasType[currentSlave[flow_]] -> ");
	SerialUSB.println(gasType[currentSlave[flow_]]);

	}
	*/

	switch(vendor[currentSlave[flow_]])
	{
		case IR_SF6:
		getDetectorData(flow_, subTask_, data_);

		break;

		case DM910:
		//	getDM910data(flow_, subTask_, data_);

		break;

		case DM910H:
		if (initSerialDebug)
		{
			SerialUSB.println("Ln 7453");
		}
		//	getDM910Hdata(flow_, subTask_, data_);

		break;

		case PVT100:

		//	getPVT100data(flow_, subTask_, data_);

		break;
		
		
		case ERIS210:

		getERISData(flow_, subTask_, data_);

		break;
		
		case fireDet:

		getFIREData(flow_, subTask_, data_);

		break;
		
		case binar:

		getBinarData(flow_, subTask_, data_);

		break;
		
		case proSense:

		getProSenseData(flow_, subTask_, data_);

		break;

		default:

		break;
	}
	if (initSerialDebug)
	{
		SerialUSB.println("Ln 7471");
	}
}


byte getDetectorData(byte flow_, byte subTask_, int *data_)
{

	if (emulateData(flow_, data_))
	{
		subTask_++;
		return subTask_;
	}
	
	

	if (enabled[currentSlave[flow_]] != 0)  // If detector enabled and serviceMode switched off
	{
		// General mode
		switch (read_holding_registers(flow_, 0, 2, data_))
		{
			case 1: // Timeout respond
			regsNoRespond(flow_);

			return subTask_;
			break;

			case 2: // Recieve respond
			regsResponded(flow_, data_);

			return subTask_;
			break;

			case 3:  // Serial actions finished and poll time delay done. Ready for new subtask
			taskFlag[flow_] = standBy;  // Ready for new task
			nextSlave(flow_);      // Next slave - 0 - nothing to do. Else - go to next slave
			subTask_++;
			return subTask_;
			break;

			default:

			return subTask_;
			break;
		}
	}
	else  // If detector disabled or service Mode active
	{
		taskFlag[flow_] = standBy;  // Ready for new task
		gaugeData[currentSlave[flow_] * 2 - 1] = hide;                // HMI: Датчик не ответил - надпись на поле датчика "не подключен"- код " - 1"
		gaugeData[currentSlave[flow_] * 2] = ok;                     // HMI: Датчик не ответил - цвет поля красный - код 0

		nextSlave(flow_);      // Next slave - 0 - nothing to do. Else - go to next slave
		subTask_++;
		return subTask_;
	}
	// subTask 0 Finish ***************************************
}



byte getFIREData(byte flow_, byte subTask_, int *data_)
{

	// Signal Emulation Mode
	if (emulateData(flow_, data_))
	{
		subTask_++;
		return subTask_;
	}


	if (enabled[currentSlave[flow_]] != 0)  // If detector enabled and serviceMode switched off
	{
		// General mode
		if (BR_Fail[3])
		{
			data_[0] = 0;
			data_[1] = BR_Fail[3];
			regsNoRespond(flow_);
		}
		else
		{
			data_[0] = bitRead(X2X_Data_Array[3][Data_0],(currentSlave[flow_]-24));
			data_[1] = BR_Fail[3];
			fireResponded(flow_, data_);
		}
	}
	else  // If detector disabled or service Mode active
	{
		gaugeData[currentSlave[flow_] * 2 - 1] = hide;                // HMI: Датчик не ответил - надпись на поле датчика "не подключен"- код " - 1"
		gaugeData[currentSlave[flow_] * 2] = ok;                     // HMI: Датчик не ответил - цвет поля красный - код 0
	}
	
	serialFlag[flow_] = taskComplete;     // Serial Task Complete
	taskFlag[flow_] = standBy;  // Ready for new task
	nextSlave(flow_);      // Next slave - 0 - nothing to do. Else - go to next slave
	subTask_++;
	return subTask_;
	
	// subTask 0 Finish ***************************************
}




byte getDataTask = 0;



byte getOwenData(byte flow_, byte subTask_, int *data_)
{

	// Signal Emulation Mode
	if ((emulationActive == 1) && (sensorEmul == currentSlave[flow_]))  // if emulation mode active - update detector data and go next task
	{
		switch (emulStatus)
		{
			case 0:
			data_[0] = emulConcentration;
			data_[1] = 0;
			//SerialUSB.println("Emilation - ");
			//debugC++;
			//SerialUSB.println(debugC);
			regsResponded(flow_, data_);

			break;

			case 1:
			regsNoRespond(flow_);
			break;

			case 2:
			data_[0] = 0;
			data_[1] = 1;
			regsResponded(flow_, data_);
			break;

			case 3 ... 8:
			data_[0] = 0;
			data_[1] = 1 << (emulStatus - 3);
			regsResponded(flow_, data_);
			break;

		}

		taskFlag[flow_] = standBy;            // Ready for new task
		serialFlag[flow_] = taskComplete;     // Serial Task Complete
		nextSlave(flow_);               // Next slave - 0 - nothing to do. Else - go to next slave
		subTask_++;
		return subTask_;
	}


	if (enabled[currentSlave[flow_]] != 0)  // If detector enabled and serviceMode switched off
	{
		// General mode
		switch (getDataTask)
		{
			case 0:   // get concentration // 255 + currentSlave[flow_]
			switch (read_holding_registers(flow_, 0, 2, data_))
			{
				case 1: // Timeout respond
				regsNoRespond(flow_);

				return subTask_;
				break;

				case 2: // Recieve respond
				//	regsResponded(flow_, data_);
				gaugeData[currentSlave[flow_] * 2 - 1] = data_[0]; // Показания датчика без показаний ошибок

				return subTask_;
				break;

				case 3:  // Serial actions finished and poll time delay done. Ready for new subtask
				taskFlag[flow_] = standBy;  // Ready for new task
				getDataTask++;    //
				return subTask_;
				break;

				default:

				return subTask_;
				break;
			}
			break;


			case 1: // get error data             // 279 + currentSlave[flow_]
			switch (read_holding_registers(flow_, 0, 2, data_))
			{
				case 1: // Timeout respond
				regsNoRespond(flow_);

				return subTask_;
				break;

				case 2: // Receive respond

				switch (data_[0])
				{
					case (0XF00B):
					data_[0] = 1;
					break;


					case (0XF00C):
					data_[0] = 2;
					break;


					case (0XF00D):
					data_[0] = 3;
					break;

					default:

					data_[0] = 0;
					break;
				}

				data_[1] = data_[0]; // Move error to proper second data byte
				data_[0] = gaugeData[currentSlave[flow_] * 2 - 1]; // move concentration to [0] from preveous read

				regsResponded(flow_, data_);

				return subTask_;
				break;

				case 3:  // Serial actions finished and poll time delay done. Ready for new subtask
				taskFlag[flow_] = standBy;  // Ready for new task
				nextSlave(flow_);      // Next slave - 0 - nothing to do. Else - go to next slave
				subTask_++;
				getDataTask++;    //
				return subTask_;
				break;

				default:

				return subTask_;
				break;
			}
			break;

			default:
			getDataTask = 0;
			break;
		}

	}
	else  // If detector disabled or service Mode active
	{
		taskFlag[flow_] = standBy;  // Ready for new task
		gaugeData[currentSlave[flow_] * 2 - 1] = hide;                // HMI: Датчик не ответил - надпись на поле датчика "не подключен"- код " - 1"
		gaugeData[currentSlave[flow_] * 2] = ok;                     // HMI: Датчик не ответил - цвет поля красный - код 0

		nextSlave(flow_);      // Next slave - 0 - nothing to do. Else - go to next slave
		subTask_++;
		return subTask_;
	}
	// subTask 0 Finish ***************************************
}





byte getESPData(byte flow_, byte subTask_, int *data_)
{
	float sgosConc;
	// Signal Emulation Mode


	if ((emulationActive == 1) && (sensorEmul == currentSlave[flow_]))  // if emulation mode active - update detector data and go next task
	{

		switch (emulStatus)
		{
			case 0:

			data_[0] = emulConcentration;
			data_[1] = 0;

			regsResponded(flow_, data_);
			break;

			case 1:
			regsNoRespond(flow_);
			break;

			case 2:
			data_[0] = 0;
			data_[1] = 1;
			regsResponded(flow_, data_);
			break;
		}
		subTask_++;
		return subTask_;
	}


	// General mode


	if (enabled[currentSlave[flow_]] != 0)  // If detector enabled and serviceMode switched off
	switch (read_holding_registers(flow_, 34, 2, data_))
	{
		case 1: // Timeout respond
		regsNoRespond(flow_);
		return subTask_;
		break;

		case 2: // Recieve respond

		sgosConc = 100.0 * v.val;
		data_[0] = sgosConc;
		data_[1] = 0;


		regsResponded(flow_, data_);
		return subTask_;
		break;

		case 3:  // Serial actions finished and poll time delay done. Ready for new subtask
		taskFlag[flow_] = standBy;  // Ready for new task
		nextSlave(flow_);      // Next slave - 0 - nothing to do. Else - go to next slave
		subTask_++;
		return subTask_;
		break;

		default:

		return subTask_;
		break;
	}
	else  // If detector disabled or service Mode active
	{
		//  SerialUSB.println(" 5 ");
		taskFlag[flow_] = standBy;  // Ready for new task
		nextSlave(flow_);      // Next slave - 0 - nothing to do. Else - go to next slave
		subTask_++;
		return subTask_;
	}
	//  SerialUSB.println(" 6 ");
	// subTask 0 Finish ***************************************
}




byte serviceModeCheck(byte flow_, byte subTask_)
{
	if (serviceMode == 1)
	{
		nextSlave(flow_);
		regsDisabled(currentSlave[flow_]);
		return  subTask_;
	}
	else
	{
		subTask_++;
		return subTask_;
	}
}



//*************************** Ответ  **********************************************

void emuleDetector()
{
	switch (read_holding_registers(HMI, 110, 4, data_HMI))
	{
		case 2:
		emulationActive = data_HMI[0];
		//sensorEmul = data_HMI[1];
		emulConcentration = data_HMI[2];
		emulStatus = data_HMI[3];
		/*
		SerialUSB.print("emulStatus ");
		SerialUSB.println(emulStatus);
		SerialUSB.print("emulationActive ");
		SerialUSB.println(emulationActive);
		SerialUSB.print("sensorEmul ");
		SerialUSB.println(sensorEmul);
		SerialUSB.print("emulConcentration ");
		SerialUSB.println(emulConcentration);
		*/

		clearHMIdata(0);
		break;

		case 3:
		subTask_HMI = 20; // Reset comand buttons
		break;
	}

}




void brLost()
{
	if (RSdebug)
	{
		SerialUSB.print("Lost slave -> ");
		SerialUSB.println(currentSlave[X2X]);
	}
	
	byte thisRelay;
	thisRelay = currentSlave[X2X];

	if (BR_lostpacket[thisRelay] > BR_lostpacketSP)
	{
		BR_Fail[thisRelay] = TRUE;
		if (logX2X[thisRelay] == 0)
		{
			logX2X[thisRelay] = 1;
			sdString = "Блок реле -> ";
			sdString += thisRelay;
			sdString += " не отвечает";
			printData();
		}
	}
	else
	BR_lostpacket[thisRelay]++;
}



// Cyclic redundant connection (echoCheck)

byte redundancyCheck(byte flow_)
{
	byte exitFault = 0;
	unsigned char echoString[65];
	for (byte j = 0; j < 10; j++)
	echoString[j] = 0;

	byte port_num = PORT[flow_] + 1;

	receive_response(port_num, echoString);

	/*
	SerialUSB.println("Check buffer");
	for (byte i = 0; i < 6; i++)
	{
	SerialUSB.print(echoString[i]);
	SerialUSB.print(" -> ");
	SerialUSB.print(redundantBuffer[port_num][i]);
	SerialUSB.println("");
	}
	*/

	for (byte i = 0; i < 6; i++)
	if (echoString[i] != redundantBuffer[flow_][i])
	{
		redundantFlag[flow_] = loopFail;  // Wire loop broken.
		/*
		SerialUSB.print("loop Fail -> ");
		SerialUSB.println(port_num);
		SerialUSB.println(echoString[i]);
		SerialUSB.println(redundantBuffer[port_num][i]);

		*/
		exitFault = 1;
		break;
		// TimeOut - not responded
	}


	if (exitFault)
	{
		//   SerialUSB.println("loop  fault");
		return 1;
	}
	redundantFlag[flow_] = loopOk;    // Redundant loop ok
	//  SerialUSB.println("loopOk");
	return 0;  //


}




int flushPort(byte flow_)
{
	switch (PORT[flow_])
	{
		/*
		case 1:  // PC

		while (UARTX.available(UART1_CS, CH_A))
		UARTX.read(UART1_CS, CH_A);

		break;
		*/
		case 2: // HMI

		while (UARTX.available(UART2_CS, CH_A))
		UARTX.read(UART2_CS, CH_A);

		break;


		case 3: // S1


		while (UARTX.available(UART2_CS, CH_B))
		UARTX.read(UART2_CS, CH_B);

		break;


		case 4: // S2

		while (Serial1.available())
		{
			Serial1.read();
		}
		break;


		case 5:

		while (Serial3.available())
		Serial3.read();

		break;


		case 6:

		while (Serial2.available())
		Serial2.read();

		break;
	}

}






void saveServiceRelay()
{


	switch (read_holding_registers(HMI, 1810, 3, data_HMI))
	{
		case 2:
		relayModule = data_HMI[0];
		relayPurpose = data_HMI[1];
		relayMask = data_HMI[2];

		switch (relayPurpose)
		{
			case 0:

			DisconnectRelayMask[relayModule] = relayMask;
			for (byte i = 1; i <= BRSlice; i++)
			outArray[i] = DisconnectRelayMask[i];

			saveConfig("detFlt.txt", outArray, BRSlice);

			break;

			case 1:

			systemFaultRelay[relayModule] = relayMask;
			for (byte i = 1; i <= BRSlice; i++)
			outArray[i] = systemFaultRelay[i];

			saveConfig("sysFlt.txt", outArray, BRSlice);

			break;

			case 2:
			BuzzerOffRelay[relayModule] = relayMask;
			for (byte i = 1; i <= BRSlice; i++)
			outArray[i] = BuzzerOffRelay[i];

			saveConfig("buzzer.txt", outArray, BRSlice);

			break;


		}

		break;

		case 3:
		subTask_HMI = 20;
		break;

	}
}



void loadServiceRelay()
{


	switch (subsubTask[HMI])
	{
		case 0:
		switch (read_holding_registers(HMI, 1810, 2, data_HMI))
		{

			case 2:

			relayModule = data_HMI[0];
			relayPurpose = data_HMI[1];
			clearHMIdata(0);


			break;


			case 3:

			switch (relayPurpose)
			{
				case 0:

				data_HMI[0] = DisconnectRelayMask[relayModule];

				break;

				case 1:

				data_HMI[0] = systemFaultRelay[relayModule];

				break;

				case 2:
				data_HMI[0] = BuzzerOffRelay[relayModule];

				break;

			}


			preset_multiple_registers(HMI, 1812, 1, data_HMI);
			subsubTask[HMI] = 1;

			break;
		}
		break;

		case 1:
		if (preset_multiple_registers(HMI, 1812, 2, data_HMI) == portReady)
		{
			subTask_HMI = 20; // Reset comand buttons
			subsubTask[HMI] = 0;
		}
		break;
	}
}



byte hmiLostPacket = 0;
int iecModeBuf[2];
int nextHmiTask = 0;





int stateRS = 0;

byte initHMI()
{
	byte statusHMI = 0;
	byte HMIdone = 0;

	switch (initSubTask)
	{
		case 0:    // Check HMI Connection
		{
			stateRS = read_holding_registers(HMI, 1802, 1, data_HMI);
			initHMIrespond(initSubTask, nextHmiTask, stateRS);
		}
		break;


		case 1:    // ********* Change HMI window to Loading screen while uploading settings
		{
			if (serialFlag[HMI] == taskComplete)
			{
				calcHMIscreenAndSize(totalSensors, &hmiSendSize);
				data_HMI[0] = 0;
				data_HMI[1] = 0;
				data_HMI[2] = 0;
			}
			stateRS = preset_multiple_registersX(HMI, 500, 3, data_HMI);
			initHMIrespond(initSubTask, nextHmiTask, stateRS);
		}
		break;


		case 2:     // Calculate and send Serial Port lines
		{
			if (serialFlag[HMI] == taskComplete)
			{
				clearHMIdata(0);
				// *********** Отправка номера портов в панель
				for (byte i = 0; i <= 3; i++)
				data_HMI[i] = B00000001 << (PORT[i + 3] - 3);
			}
			stateRS = preset_multiple_registersX(HMI, 791, 6, data_HMI );
			initHMIrespond(initSubTask, nextHmiTask, stateRS);
			
		}
		break;

		case 3: //  Preset X2X slices
		{
			if (serialFlag[HMI] == taskComplete)
			{
				clearHMIdata(0);

				data_HMI[0] = 1;
				byte buferByte = 1;

				for (byte i = 0; i < BRSlice; i++)
				{
					data_HMI[0] = data_HMI[0] | buferByte;
					buferByte = buferByte << 1;
				}
				data_HMI[1] = 0;

				if (redundantMode[4] == 1)
				data_HMI[1] = B00000001;

				if (redundantMode[5] == 1)
				data_HMI[1] = data_HMI[1] | B00000010;

			}

			stateRS = preset_multiple_registersX(HMI, 100, 2, data_HMI );
			initHMIrespond(initSubTask, nextHmiTask, stateRS);
		}
		break;


		case 4:    // **************** Reset all btns
		{
			if (serialFlag[HMI] == taskComplete)
			clearHMIdata(0);
			
			stateRS = preset_multiple_registersX(HMI, 1802, 5, data_HMI);
			initHMIrespond(initSubTask, nextHmiTask, stateRS);
		}
		break;



		case 5:   // ********* Preset Detector numbers to HMI main screen
		{
			if (serialFlag[HMI] == taskComplete)
			{
				clearHMIdata(0);
				//	SerialUSB.println("Номера -> ");
				for (byte i = 1; i <= totalSensors; i++)
				{
					data_HMI[i - 1] = i;
					//		SerialUSB.println(data_HMI[i - 1]);
				}
				//	SerialUSB.println("Fin ");
				
			}
			
			stateRS = preset_multiple_registersX(HMI, 1201, hmiSendSize, data_HMI);
			initHMIrespond(initSubTask, nextHmiTask, stateRS);
		}
		break;

		
		case 6:    // ********* Hide non used concentration windows on HMI main screen
		{
			if (serialFlag[HMI] == taskComplete)
			{
				clearHMIdata(-1);
				if (HMI_debug)
				SerialUSB.println("Enbld");

				for (byte i = 1; i <= totalSensors; i++)
				{
					if (enabled[i])
					data_HMI[2 * i - 1] = 0;
					
					if (HMI_debug)
					{
						SerialUSB.print(i);
						SerialUSB.print(" ");
						SerialUSB.println(data_HMI[2 * i - 1]);
					}
				}
			}
			data_HMI[0] = 0;

			stateRS = preset_multiple_registersX(HMI, 0, hmiSendSize*2, data_HMI);
			initHMIrespond(initSubTask, nextHmiTask, stateRS);
		}
		break;


		case 7:   // Preset Gas names
		{

			if (serialFlag[HMI] == taskComplete)
			makeGasNameList();

			stateRS = preset_multiple_registersX(HMI, 1101, hmiSendSize, data_HMI);
			initHMIrespond(initSubTask, nextHmiTask, stateRS);
		}
		break;



		case 8:   // Preset Gas names
		{
			if (serialFlag[HMI] == taskComplete)
			{
				iecModeBuf[0] = 0;
				iecModeBuf[1] = 0;

				if (Ethernet1Mode == IEC104mode)
				{
					iecModeBuf[0] = 2;
					iecModeBuf[1] = 2;
				}
			}

			stateRS = preset_multiple_registers(HMI, 1805, 1, iecModeBuf);
			initHMIrespond(initSubTask, nextHmiTask, stateRS);
		}
		break;
		
		
		case 9:   // Preset Gas names
		{
			if (serialFlag[HMI] == taskComplete)
			{
				clearHMIdata(0);
				loadConfig("units.txt", outArray);
				for (int i = 1; i <= outArray[0]; i++) // количество датчиков
				{
					units[i] = outArray[i];
					data_HMI[i-1] = outArray[i];
				}
			}
			stateRS = preset_multiple_registersX(HMI, 1301, totalSensors, data_HMI);
			initHMIrespond(initSubTask, nextHmiTask, stateRS);
		}
		break;
		
		
		case 10:   // Preset Gas names
		{
			if (serialFlag[HMI] == taskComplete)
			{
				clearHMIdata(0);
				
				
				
				for (int i = 1; i <= outArray[0]; i++) // множитель
				{
					data_HMI[i-1] = MLTPL[i];
					
				}
			}
			stateRS = preset_multiple_registersX(HMI, 1401, totalSensors, data_HMI);
			initHMIrespond(initSubTask, nextHmiTask, stateRS);
		}
		break;


		case 11:     // *************** Preset Start HMI Page
		{
			if (serialFlag[HMI] == taskComplete)
			{
				clearHMIdata(0);

				data_HMI[0] = calcHMIscreenAndSize(totalSensors, &hmiSendSize);
				// 			SerialUSB.print("data_HMI[0] -> ");
				// 			SerialUSB.println(data_HMI[0]);
				// 			SerialUSB.print("hmiSendSize -> ");
				// 			SerialUSB.println(hmiSendSize);
			}
			
			//     }

			stateRS =  preset_multiple_registers(HMI, 500, 1, data_HMI);
			initHMIrespond(initSubTask, nextHmiTask, stateRS);
			if (stateRS == RS_respond)
			{
				HMIstatus[currentSlave[HMI]] = ok;
				taskFlag[HMI] = taskComplete;
				initSubTask = 0;
				if (HMI_debug)
				{
					//SerialUSB.print(initSubTask);
					SerialUSB.print("HMI init finished ->  ");
					SerialUSB.println(currentSlave[HMI]);
					SerialUSB.println("************");
				}
			}
			
		}
		break;
		
		case 55:
		{
			if (serialFlag[HMI] == taskComplete)
			data_HMI[0] = nextHmiTask;

			if (preset_multiple_registersX(HMI, 605, 1, data_HMI) == portReady)
			{
				initSubTask = nextHmiTask;
				if (HMI_debug)
				{
					SerialUSB.println("initSubTask -> ");
					SerialUSB.println(initSubTask);
				}
				//delay(200);
			}
		}

		break;
		

		default:

		initSubTask = 0;
		break;

	}

}



uint8_t initHMIrespond(int &hmiTask_,int &nextHmiTask_, int state_)
{
	switch (state_)
	{
		case RS_timeOut:
		if (HMI_debug)
		{
			SerialUSB.print(hmiTask_);
			SerialUSB.print(".Lost ->  ");
			SerialUSB.println(currentSlave[HMI]);
		}
		hmiLostPacket = 1;

		return 99;  // No Connected to HMI - break preset settings procedure
		break;

		case RS_respond:   // Responded - goto next task

		if (HMI_debug)
		{
			SerialUSB.print(hmiTask_);
			SerialUSB.print(". Ok  -> ");
			SerialUSB.println(currentSlave[HMI]);
		}

		hmiLostPacket = 0;

		return 0;
		break;

		case RS_fin:  // Serial actions finished and poll time delay done. Ready for new subtask

		if (hmiLostPacket == 1)
		{
			HMIstatus[currentSlave[HMI]] = lost;
			hmiTask_ = 0;
			return 0;

		}
		nextHmiTask = hmiTask_+ 1;
		hmiTask_= 55;  // update progress bar

		break;

		default:

		return 0;
		break;
	}
	
};


byte calcHMIscreenAndSize(byte totalSensors_, byte *hmiSendSize_)
{
	switch (totalSensors_)
	{
		case 1 ... 5:
		// 10 detectors
		*hmiSendSize_ = 5;
		return detectors_5; // 5 detectors
		break;

		case 6 ... 10:
		
		*hmiSendSize_ = 10;
		return detectors_10; // 10 detectors
		break;

		case 11 ... 16:
		
		*hmiSendSize_ = 16;
		return detectors_16; // 16 detectors
		break;

		case 17 ... 20:

		*hmiSendSize_ = 20;
		return detectors_20; // 16 detectors
		break;

		case 21 ... 30:

		*hmiSendSize_ = 30;
		return detectors_30; // 16 detectors
		break;

		case 31 ... 44:

		*hmiSendSize_ = 44;
		return detectors_40; // 16 detectors
		break;

		default:
		return 49;  // Wrong Config

		break;

	}
}


void coilArrayWrite(int pos_, byte value_)
{
	int arrPos = pos_ * 0.125;
	byte bitPos = pos_ - arrPos * 8;
	bitWrite(RS485_Coil[arrPos], bitPos, value_);

}

byte coilArrayRead(int pos_)
{
	int arrPos = pos_ * 0.125;
	byte bitPos = pos_ - arrPos * 8;
	byte value_ = bitRead(RS485_Coil[arrPos], bitPos);
	return value_;
}


int8_t emulateData(byte flow_,int *data_)
{
	if ((emulationActive == 1) && (sensorEmul == currentSlave[flow_]))  // if emulation mode active - update detector data and go next task
	{
		switch (emulStatus)
		{
			case 0:
			data_[0] = emulConcentration;
			data_[1] = 0;


			regsResponded(flow_, data_);
			break;

			case 1:
			regsNoRespond(flow_);
			break;

			case 2:
			data_[0] = 0;
			data_[1] = 1;
			regsResponded(flow_, data_);
			break;

			case 3 ... 8:
			data_[0] = 0;
			data_[1] = 1 << (emulStatus - 3);
			regsResponded(flow_, data_);
			break;

		}

		taskFlag[flow_] = standBy;            // Ready for new task
		serialFlag[flow_] = taskComplete;     // Serial Task Complete
		nextSlave(flow_);               // Next slave - 0 - nothing to do. Else - go to next slave
		return 1;
	}
	return 0;
}


byte getBinarData(byte flow_, byte subTask_, int *data_)
{


	if (emulateData(flow_, data_))
	{
		subTask_++;
		return subTask_;
	}


	if (enabled[currentSlave[flow_]] != 0)  // If detector enabled and serviceMode switched off
	{
		
		switch (read_Input_registers(flow_, 3000, 3, data_))
		{
			case 1: // Timeout respond
			regsNoRespond(flow_);
			
			return subTask_;
			break;

			case 2: // Receive respond
			
			//	data_[0] = ((int32_t)data_[0] << 16) | (int32_t)data_[1];
			binarConcentration.bytes[1] = highByte(data_[1]);
			binarConcentration.bytes[0] = lowByte(data_[1]);
			binarConcentration.bytes[3] = highByte(data_[0]);
			binarConcentration.bytes[2] = lowByte(data_[0]);
			
			//	SerialUSB.println(binarConcentration.val*100);
			
			if (binarConcentration.val<0)
			{
				binarConcentration.val = 0;
			}
			
			
			data_[0] = MLTPL[currentSlave[flow_]]*binarConcentration.val;
			
			data_[1] = uint8_t(data_[2]) & B10001111;  // маскируем только биты с ошибками. остальные биты не отностятся к ошибкам
			
			//	SerialUSB.println(portArray_[0]);
			regsResponded(flow_, data_);

			return subTask_;
			break;

			case 3:  // Serial actions finished and poll time delay done. Ready for new subtask
			taskFlag[flow_] = standBy;  // Ready for new task
			nextSlave(flow_);      // Next slave - 0 - nothing to do. Else - go to next slave
			subTask_++;
			subsubTask[flow_] = 0;
			return subTask_;
			break;

			default:

			return subTask_;
			break;
		}



	}
	else  // If detector disabled or service Mode active
	{
		taskFlag[flow_] = standBy;  // Ready for new task
		gaugeData[currentSlave[flow_] * 2 - 1] = hide;                // HMI: Датчик не ответил - надпись на поле датчика "не подключен"- код " - 1"
		gaugeData[currentSlave[flow_] * 2] = ok;                     // HMI: Датчик не ответил - цвет поля красный - код 0

		nextSlave(flow_);      // Next slave - 0 - nothing to do. Else - go to next slave
		subTask_++;
		return subTask_;
	}
	// subTask 0 Finish ***************************************
}



byte getProSenseData(byte flow_, byte subTask_, int *data_)
{
	if (emulateData(flow_, data_))
	{
		subTask_++;
		return subTask_;
	}

	if (enabled[currentSlave[flow_]] != 0)  // If detector enabled and serviceMode switched off
	{
		
		switch (read_Input_registers(flow_, 5, 2, data_))
		{
			case 1: // Timeout respond
			regsNoRespond(flow_);
			
			return subTask_;
			break;

			case 2: // Receive respond
			
			data_[3] = data_[0];	// меняем местами данные. 0 - это ошибка, а 1 это концентрация
			data_[0] = data_[1];
			if (uint8_t(lowByte(data_[3])) == 3)
			data_[1] = 3;
			else
			data_[1] = 0;
			
			if (data_[0]<0)
			{
				data_[0] = 0;
			}
			
			
			//	data_[0] = data_[0]; //MLTPL[currentSlave[flow_]]*data_[0];
			
			
			//	SerialUSB.println(portArray_[0]);
			regsResponded(flow_, data_);

			return subTask_;
			break;

			case 3:  // Serial actions finished and poll time delay done. Ready for new subtask
			taskFlag[flow_] = standBy;  // Ready for new task
			nextSlave(flow_);      // Next slave - 0 - nothing to do. Else - go to next slave
			subTask_++;
			subsubTask[flow_] = 0;
			return subTask_;
			break;

			default:

			return subTask_;
			break;
		}



	}
	else  // If detector disabled or service Mode active
	{
		taskFlag[flow_] = standBy;  // Ready for new task
		gaugeData[currentSlave[flow_] * 2 - 1] = hide;                // HMI: Датчик не ответил - надпись на поле датчика "не подключен"- код " - 1"
		gaugeData[currentSlave[flow_] * 2] = ok;                     // HMI: Датчик не ответил - цвет поля красный - код 0

		nextSlave(flow_);      // Next slave - 0 - nothing to do. Else - go to next slave
		subTask_++;
		return subTask_;
	}
	// subTask 0 Finish ***************************************
}


int erisError = 0;

byte getERISData(byte flow_, byte subTask_, int *data_)
{

	if (emulateData(flow_, data_))
	{
		subTask_++;
		return subTask_;
	}




	if (enabled[currentSlave[flow_]] != 0)  // If detector enabled and serviceMode switched off
	{
		switch (subsubTask[flow_])
		{
			case 0: // Get error
			switch (read_Input_registers(flow_, 262, 1, data_))
			{
				case 1: // Timeout respond
				regsNoRespond(flow_);

				
				return subTask_;
				break;

				case 2: // Recieve respond
				//	SerialUSB.print("Error byte -> ");
				//	SerialUSB.println(portArray_[0]);
				erisError = B00000000 & highByte(data_[0]);
				
				//	SerialUSB.println(erisError);
				return subTask_;
				break;

				case 3:  // Serial actions finished and poll time delay done. Ready for new subtask
				subsubTask[flow_]++;
				return subTask_;
				break;

				default:

				return subTask_;
				break;
			}

			break;


			case 1: // Get concentration

			switch (read_Input_registers(flow_, 358, 1, data_))
			{
				case 1: // Timeout respond
				regsNoRespond(flow_);
				
				return subTask_;
				break;

				case 2: // Recieve respond
				data_[1] = erisError;
				
				//	SerialUSB.println(portArray_[0]);
				regsResponded(flow_, data_);

				return subTask_;
				break;

				case 3:  // Serial actions finished and poll time delay done. Ready for new subtask
				taskFlag[flow_] = standBy;  // Ready for new task
				nextSlave(flow_);      // Next slave - 0 - nothing to do. Else - go to next slave
				subTask_++;
				subsubTask[flow_] = 0;
				return subTask_;
				break;

				default:

				return subTask_;
				break;
			}

			break;
		}


	}
	else  // If detector disabled or service Mode active
	{
		taskFlag[flow_] = standBy;  // Ready for new task
		gaugeData[currentSlave[flow_] * 2 - 1] = hide;                // HMI: Датчик не ответил - надпись на поле датчика "не подключен"- код " - 1"
		gaugeData[currentSlave[flow_] * 2] = ok;                     // HMI: Датчик не ответил - цвет поля красный - код 0

		nextSlave(flow_);      // Next slave - 0 - nothing to do. Else - go to next slave
		subTask_++;
		return subTask_;
	}
	// subTask 0 Finish ***************************************
}




/*
byte erisResponded(byte flow_, int *data_)
{

byte currentSensor = currentSlave[flow_];
byte port_num = PORT[flow_];

data_[1] = erisError;
serialFlag[flow_] = msgDone;
startPollTimer[flow_] = millis();   // Записать текущее время и засечь интервал
lostPackets[currentSensor] = 0;
gaugeData[currentSensor * 2 - 1] = data_[0]; // Показания датчика
gaugeStatus[currentSensor] = data_[1]; // Ошибка датчика


//  IEC104data_M_SP[2 * currentSensor - 1] = ModbusCharBuffer[0];


if (logDisabled[currentSensor] == 1)
{
logDisabled[currentSensor] = 0;

sdString = "Газоанализатор -> ";
sdString += currentSensor;
sdString += " подключен к системе";

printData();
}


if (logDisconnect[currentSensor] == 1)
{
logDisconnect[currentSensor] = 0;
sdString = "Газоанализатор -> ";
sdString += currentSensor;
sdString += " на связи";

printData();
}



if (data_[1] != 0)
{
if (logError[currentSensor] == 0)
{
logError[currentSensor] = 1;
sdString = "Газоанализатор ->";
sdString += currentSensor;
sdString += " сообщает об ошибке -> ";
sdString += data_[1];

printData();
}


//  IEC104data_M_SP[2 * currentSensor] = fault;

// PC: Массив показаний датчиков для отправки на пульт PC
gaugeData[currentSensor * 2 - 1] = hide;                // HMI: Датчик не ответил - надпись на поле датчика "не подключен"- код " - 1"
gaugeData[currentSensor * 2] = fault;                     // HMI: Датчик не ответил - цвет поля красный - код 0                                                                    // HMI: Датчик не ответил - цвет общего фона красный - код B00000001
}



if (logError[currentSensor] == 1 &&  data_[1] == 0)
{
logError[currentSensor] = 0;
sdString = "Газоанализатор ->";
sdString += currentSensor;
sdString += ". Все ошибки сняты ";
sdString += data_[1];
}


if (data_[0] < warningSetpointH[currentSensor] && data_[1] == 0)                 // Показания не превышают первый порог?
{
ScreenState[currentSensor] = colorGrey;                                               // Массив цветов панели - для текущего датчика цвет ЗЕЛЕНЫЙ
gaugeData[currentSensor * 2] = ok;                                        // Цвет поля датчика на панели ЗЕЛЕНЫЙ, КОД 2


//  IEC104data_M_SP[2 * currentSensor] = ok;


if (logYellow[currentSensor] == 1)
{
logYellow[currentSensor] = 0;
sdString = "Газоанализатор -> ";
sdString += currentSensor;
sdString += " < ";
sdString += warningSetpointH[currentSensor];
sdString += ". Концентрация ниже первого порога";

printData();
}
}


if (data_[0] >= alarmSetpointH[currentSensor] && data_[1] == 0)          // Проверка на превышение второго порога  Alarm
{
gaugeData[currentSensor * 2] = alarm;                                       // Превышен второй порог, фон датчика красный


//  IEC104data_M_SP[2 * currentSensor] = alarm;



if (logRed[currentSensor] == 0)
{
logRed[currentSensor] = 1;
sdString = "Газоанализатор -> ";
sdString += currentSensor;
sdString += " > ";
sdString += alarmSetpointH[currentSensor];
sdString += ". Авария, превышен второй порог.";

printData();
}

}



if (data_[0] >= warningSetpointH[currentSensor] &&  data_[0] < alarmSetpointH[currentSensor] && data_[1] == 0 && gaugeData[currentSensor * 2] != alarm)                   // Если показания превысили первый порог но не привысели второй - ЖЕЛТАЯ ЗОНА
{
gaugeData[currentSensor * 2] = warning;                                        // Превышен первый порог, фон датчика желтый



//  IEC104data_M_SP[2 * currentSensor] = warning;


if (logYellow[currentSensor] == 0)
{
logYellow[currentSensor] = 1;
sdString = "Газоанализатор ->";
sdString += currentSensor;
sdString += " > ";
sdString += warningSetpointH[currentSensor];
sdString += ". Превышен первый порог.";

printData();
}

if (logRed[currentSensor] == 1)
{
logRed[currentSensor] = 0;
sdString = "Газоанализатор ->";
sdString += currentSensor;
sdString += " < ";
sdString += alarmSetpointH[currentSensor];
sdString += ".  Концентрация упала ниже второго порога";

printData();
}

}
// gaugeData[currentSlave_1 * 2] = gaugeData[currentSlave_1 * 2]; //| (SensorStatusByte[currentSlave_1] << 2);
}


*/


















byte read_Input_registers(byte flow_, int start_addr, byte count, int *data_)
{
	int function = 0x04;  /* Function: Read Holding Registers */
	int ret;
	byte slave = currentSlave[flow_];



	//  unsigned char packet[REQUEST_QUERY_SIZE + CHECKSUM_SIZE];
	byte port_num = PORT[flow_];

	switch (serialFlag[flow_])
	{

		case taskComplete:
		serialFlag[flow_] = sendMsg;
		return 0;
		break;



		case sendMsg:  // Send packet

		build_request_packet(slave, function, start_addr, count, packet[flow_]);

		if (redundantMode[flow_] == active)
		build_request_packet(slave, function, start_addr, count, redundantBuffer[flow_]);
		flushPort(flow_);
		send_query(packet[flow_], REQUEST_QUERY_SIZE, port_num);

		serialFlag[flow_] = sendingMsg;
		waitTillSent[flow_] = millis();
		packet_size[flow_] = count;
		return 0;  // do nothing

		break;


		case sendingMsg: // wait till packet left Serial buffer

		if (currentTime - waitTillSent[flow_] > (2 * (REQUEST_QUERY_SIZE + packet_size[flow_])))
		{
			serialFlag[flow_] = receiveMsg;   //
			startTimeOut[flow_] = currentTime;
		}
		return 0;  // do nothing

		break;




		case receiveMsg: // wait till packet left Serial buffer
		switch (receive_incoming_data_read(flow_, data_))   // Прокерка ответа - 0 - входной буффер пуст, сообщения нет.Проверяем пока не вышло время тайм аута, >0 - овтет получен, <0 - ошибка
		{
			case 0:                                                                           // Входной буфер пуст
			if (currentTime - startTimeOut[flow_] > WaitResponceGauge)                             // Входной буфер пуст - проверяем. как долго ожидался ответ?
			{
				if (redundantMode[flow_] == active && flow_ != PORT[HMI])
				{
					//  SerialUSB.println("Detector not responded, Check loop");
					redundancyCheck(flow_);

					if (redundantFlag[flow_] == loopFail)
					{
						//  SerialUSB.println("Loop fault, try connect detector another way");
						serialFlag[flow_] = reconnectLoop;
						return 0;
					}
					//  SerialUSB.println("Loop ok, detector lost");
					serialFlag[flow_] = msgDone;
					return 1;  // TimeOut - not responded

				}
				serialFlag[flow_] = msgDone;
				return 1;  // TimeOut - not responded

			}

			return 0;  // do nothing
			break;


			default: // Respond received

			serialFlag[flow_] = msgDone;


			if (redundantMode[flow_] == active && port_num != PORT[HMI])
			{
				redundancyCheck(flow_);
				//   SerialUSB.println("Line ok, clear buffer");
			}

			return 2;  // Respond received
			break;
		}

		break;






		case msgDone:  // respond received, serial job done

		startPollTimer[flow_] = currentTime;   // 30 - delay between polls. When poll time expired - system can send new message via RS485
		serialFlag[flow_] = pollWait;   // wait poll time before next Serial send

		return 0;
		break;



		case reconnectLoop:  // Send packet

		// build_request_packet(slave, function, start_addr, count, packet[flow_]);
		port_num = PORT[flow_] + 1;
		send_query(packet[flow_], REQUEST_QUERY_SIZE, port_num);

		serialFlag[flow_] = sendingLoop;
		waitTillSent[flow_] = millis();
		return 0;  // do nothing

		break;



		case sendingLoop: // wait till packet left Serial buffer


		if (currentTime - waitTillSent[flow_] > (2 * (REQUEST_QUERY_SIZE + count)))
		{
			serialFlag[flow_] = receiveLoop;   //
			startTimeOut[flow_] = currentTime;
		}
		return 0;  // do nothing

		break;




		case receiveLoop: // wait till packet left Serial buffer
		switch (receive_incoming_data_read(flow_, data_)) // Прокерка ответа - 0 - входной буффер пуст, сообщения нет.Проверяем пока не вышло время тайм аута, >0 - овтет получен, <0 - ошибка
		{
			case 0:                                                                           // Входной буфер пуст
			if (currentTime - startTimeOut[flow_] > WaitResponceGauge)                             // Входной буфер пуст - проверяем. как долго ожидался ответ?
			{
				serialFlag[flow_] = msgDone;
				//SerialUSB.println("Double check fail - detector lost");
				return 1;  // TimeOut - not responded
			}
			return 0;  // do nothing
			break;


			default:
			serialFlag[flow_] = msgDoneLoop;

			return 2;  // Respond received
			break;
		}

		break;




		case msgDoneLoop:  // respond recieved, serial job done

		startPollTimer[flow_] = currentTime;   // 30 - delay between polls. When poll time expired - system can send new message via RS485
		serialFlag[flow_] = pollWait;   // wait poll time before next Serial send

		return 0;
		break;


		case pollWait:  // wait poll time before next Serial send

		if (currentTime - startPollTimer[flow_] > pollDelay[flow_])
		{
			serialFlag[flow_] = taskComplete;   // Serial Port Ready for new subTask
			//  SerialUSB.print("Serial port -> ");
			//  SerialUSB.println(port_num);
			return 3;
		}
		return 0;
		break;
		// wait till packet left Serial buffer

		default:
		return 0;
		break;


	}

}



// Start NTP client



// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address)
{
	// set all bytes in the buffer to 0
	memset(packetBuffer, 0, NTP_PACKET_SIZE);
	// Initialize values needed to form NTP request
	// (see URL above for details on the packets)
	packetBuffer[0] = 0b11100011;   // LI, Version, Mode
	packetBuffer[1] = 0;     // Stratum, or type of clock
	packetBuffer[2] = 6;     // Polling Interval
	packetBuffer[3] = 0xEC;  // Peer Clock Precision
	// 8 bytes of zero for Root Delay & Root Dispersion
	packetBuffer[12]  = 49;
	packetBuffer[13]  = 0x4E;
	packetBuffer[14]  = 49;
	packetBuffer[15]  = 52;

	// all NTP fields have been given values, now
	// you can send a packet requesting a timestamp:
	Udp.beginPacket(address, 123); //NTP requests are to port 123
	Udp.write(packetBuffer,NTP_PACKET_SIZE);
	Udp.endPacket();
}


uint16_t ntpStepper = 600;


void getNTPtime()
{

	switch (ntpStepper++)
	{
		case 0:
		{
			sendNTPpacket(timeServer); // send an NTP packet to a time server
		}
		break;


		case 1:
		{
			getNTPreply();
		}
		break;

		case 600 ... 700:
		{
			ntpStepper = 0;
		}
		break;


	}


	// wait ten seconds before asking for the time again

};



byte getNTPreply()
{
	if ( Udp.parsePacket() )
	{
		// We've received a packet, read the data from it
		Udp.read(packetBuffer,NTP_PACKET_SIZE);  // read the packet into the buffer

		//the timestamp starts at byte 40 of the received packet and is four bytes,
		// or two words, long. First, esxtract the two words:

		unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
		unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
		// combine the four bytes (two words) into a long integer
		// this is NTP time (seconds since Jan 1 1900):
		unsigned long secsSince1900 = highWord << 16 | lowWord;
		SerialUSB.print("Seconds since Jan 1 1900 = " );
		SerialUSB.println(secsSince1900);

		// now convert NTP time into everyday time:
		SerialUSB.print("Unix time = ");
		// Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
		const unsigned long seventyYears = 2208988800UL;
		// subtract seventy years:
		unsigned long epoch = secsSince1900 - seventyYears;
		// print Unix time:
		SerialUSB.println(epoch);


		// print the hour, minute and second:
		SerialUSB.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
		SerialUSB.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
		SerialUSB.print(':');
		if ( ((epoch % 3600) / 60) < 10 ) {
			// In the first 10 minutes of each hour, we'll want a leading '0'
			SerialUSB.print('0');
		}
		SerialUSB.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
		SerialUSB.print(':');
		if ( (epoch % 60) < 10 ) {
			// In the first 10 seconds of each minute, we'll want a leading '0'
			SerialUSB.print('0');
		}
		SerialUSB.println(epoch %60); // print the second
		return 0;
	}
	else return 1;
}


// Finish NTP client







// IEC 104 Start


int calcMEsize()
{
	// START ************** Measured data calculations

	// 1. Lorentz X2X Modules Single Point
	//  Slices (with ME data type output)
	if (calcDebug)
	SerialUSB.println("Calculating ME UserSize .... ");
	
	
	int j = 0;  // Start from beginning
	
	for (byte i = 1; i <= BRSlice; i++) // save Main room detectors data
	{
		switch (X2X_Data_Array[i][TypeModule])
		{
			case LDO:
			
			break;

			case LAI:
			
			j = j+8;
			if (calcDebug)
			{
				SerialUSB.print("LAI -> ");
				SerialUSB.println(j);
			}
			
			break;
			
			case LDI8:
			
			
			break;


			case LCT:
			
			j = j + X2X_Data_Array[i][DataQnt]; //j = j+35; //Изменено! 10 резерва
			if (calcDebug)
			{
				SerialUSB.print("LCT -> ");
				SerialUSB.println(j);
			}
			
			
			break;
		}
	}


	// Detectors
	
	for (byte i = 1; i <= totalSensors; i++)			//
	switch(vendor[i])
	{
		case IR_SF6:
		j++;	// Concentration

		break;
		
		case DM910:

		break;
		
		case DM910H:
		
		break;
		
		case PVT100:
		j = j+2;
		break;
		
		case ERIS210:
		j++;
		break;
		
		case binar:
		j++;
		break;
		
		case proSense:
		j++;
		break;
		
	}
	if (calcDebug)
	{
		SerialUSB.print("Total ME Size-> ");
		SerialUSB.println(j);
	}
	return j;
	
}

/*
int calcTFsize()
{

if (calcDebug)
SerialUSB.println("TF UserSize -> ");

int j = 0;  // Start from beginning

for (byte i = 1; i <= BRSlice; i++) // save Main room detectors data
{
switch (X2X_Data_Array[i][TypeModule])
{
case LDO:

break;

case LAI:

break;

case LDI8:

break;

case LCT:

break;

}

}

// Detectors

for (byte i = 1; i <= totalSensors; i++)			//
switch(gasType[i])
{
case IR_SF6:

break;

case DM910:
j = j + 4;
if (calcDebug)
{
SerialUSB.print("DM910 -> ");
SerialUSB.println(j);
}
break;

case DM910H:

j = j + DM910DataSize; //- pressureDM;

if (calcDebug)
{
SerialUSB.print("DM910H -> ");
SerialUSB.println(j);
}

break;

case PVT100:

break;


}
if (calcDebug)
{
SerialUSB.print("Total TF Size-> ");
SerialUSB.println(j);
}
return j;
}
*/


int calcSPsize()
{
	
	if (calcDebug)
	SerialUSB.println("Calculating SP UserSize .... ");

	int j = 0;		// SP
	
	
	// Detectors
	for (byte i = 1; i <= totalSensors; i++)
	switch(vendor[i])
	{
		case IR_SF6:
		
		j=j+9;
		if (calcDebug)
		{
			SerialUSB.print("SF6 -> ");
			SerialUSB.println(j);
		}
		break;
		
		case DM910:
		//	getDM910data(flow_, subTask_, data_);
		j++;
		j++;
		if (calcDebug)
		{
			SerialUSB.print("DM910 -> ");
			SerialUSB.println(j);
		}
		break;
		
		case DM910H:
		
		j++;
		j++;
		if (calcDebug)
		{
			SerialUSB.print("DM910H -> ");
			SerialUSB.println(j);
		}
		
		break;
		
		case PVT100:
		j++;			// Lost Connected
		j++;			// Internal Failure
		if (calcDebug)
		{
			SerialUSB.print("PVT -> ");
			SerialUSB.println(j);
		}
		break;
		
		
		case ERIS210:
		
		j=j+4;
		if (calcDebug)
		{
			SerialUSB.print("ERIS210 -> ");
			SerialUSB.println(j);
		}
		break;
		
		case fireDet:

		j=j+2;
		if (calcDebug)
		{
			SerialUSB.print("Fire Det -> ");
			SerialUSB.println(j);
		}

		break;
		
		
		case binar:
		
		j=j+4;
		if (calcDebug)
		{
			SerialUSB.print("binar -> ");
			SerialUSB.println(j);
		}
		break;
		
		case proSense:
		
		j=j+4;
		if (calcDebug)
		{
			SerialUSB.print("binar -> ");
			SerialUSB.println(j);
		}
		break;
		
		
	}
	
	
	// START ************** Single point data calculations
	
	// 1. Lorentz X2X Modules Single Point
	// LCP no data at the moment
	j++; // sys fault
	// Another Slices

	if (calcDebug)
	{
		SerialUSB.print("System fault -> ");
		SerialUSB.println(j);
	}
	
	j++; // reserved
	
	if (calcDebug)
	{
		SerialUSB.print("System reserved -> ");
		SerialUSB.println(j);
	}
	
	j++; // reserved
	
	if (calcDebug)
	{
		SerialUSB.print("System reserved -> ");
		SerialUSB.println(j);
	}
	

	for (byte i = 1; i <= BRSlice; i++) // save Main room detectors data
	{
		switch (X2X_Data_Array[i][TypeModule])
		{
			case LDO:
			
			j++;
			if (calcDebug)
			{
				SerialUSB.print("LDO -> ");
				SerialUSB.println(j);
			}
			break;

			case LAI:
			
			j++;
			if (calcDebug)
			{
				SerialUSB.print("LAI -> ");
				SerialUSB.println(j);
			}
			
			break;
			
			case LDI8:
			
			j = j+9;
			if (calcDebug)
			{
				SerialUSB.print("LDI -> ");
				SerialUSB.println(j);
			}
			
			break;

			case LCT:
			
			j = j+10;
			if (calcDebug)
			{
				SerialUSB.print("LCT -> ");
				SerialUSB.println(j);
			}
			break;

		}
		
	}
	
	
	
	if (calcDebug)
	{
		SerialUSB.print("Total SP Size -> ");
		SerialUSB.println(j);
	}
	return j;
}

// IEC 104 Fin


// Modbus TCP  STart

/*
uint16_t coilSizeCalc()
{
if (calcDebug)
SerialUSB.println("Calculating SP UserSize .... ");

int j = 0;		// SP

// Detectors
for (byte i = 1; i <= totalSensors; i++)
switch(vendor[i])
{
case IR_SF6:

j=j+9;
if (calcDebug)
{
SerialUSB.print("SF6 -> ");
SerialUSB.println(j);
}
break;

case DM910:
//	getDM910data(flow_, subTask_, data_);
j++;
j++;
if (calcDebug)
{
SerialUSB.print("DM910 -> ");
SerialUSB.println(j);
}
break;

case DM910H:

j++;
j++;
if (calcDebug)
{
SerialUSB.print("DM910H -> ");
SerialUSB.println(j);
}

break;

case PVT100:
j++;			// Lost Connected
j++;			// Internal Failure
if (calcDebug)
{
SerialUSB.print("PVT -> ");
SerialUSB.println(j);
}
break;


case ERIS210:

j=j+4;
if (calcDebug)
{
SerialUSB.print("ERIS210 -> ");
SerialUSB.println(j);
}
break;


case binar:

j=j+4;
if (calcDebug)
{
SerialUSB.print("binar -> ");
SerialUSB.println(j);
}
break;

case proSense:

j=j+4;
if (calcDebug)
{
SerialUSB.print("binar -> ");
SerialUSB.println(j);
}
break;


}


// START ************** Single point data calculations

// 1. Lorentz X2X Modules Single Point
// LCP no data at the moment
j++; // sys fault
// Another Slices

if (calcDebug)
{
SerialUSB.print("System fault -> ");
SerialUSB.println(j);
}

j++; // reserved

if (calcDebug)
{
SerialUSB.print("System reserved -> ");
SerialUSB.println(j);
}

j++; // reserved

if (calcDebug)
{
SerialUSB.print("System reserved -> ");
SerialUSB.println(j);
}


for (byte i = 1; i <= BRSlice; i++) // save Main room detectors data
{
switch (X2X_Data_Array[i][TypeModule])
{
case LDO:

j++;
if (calcDebug)
{
SerialUSB.print("LDO -> ");
SerialUSB.println(j);
}
break;

case LAI:

j++;
if (calcDebug)
{
SerialUSB.print("LAI -> ");
SerialUSB.println(j);
}

break;

case LDI8:

j = j+9;
if (calcDebug)
{
SerialUSB.print("LDI -> ");
SerialUSB.println(j);
}

break;

case LCT:

j = j+10;
if (calcDebug)
{
SerialUSB.print("LCT -> ");
SerialUSB.println(j);
}
break;

}

}



if (calcDebug)
{
SerialUSB.print("Total SP Size -> ");
SerialUSB.println(j);
}
return j;
}
*/

uint16_t IGAS_Coils(uint8_t deviceNum,uint16_t j)
{
	switch (gaugeData[deviceNum * 2])  // Warning, alarm?
	{
		case warning:
		{
			mb1.Coil(mbCoilShift+j, 1);
		}
		break;

		case alarm:
		{
			mb1.Coil(mbCoilShift+j + 1, 1);
		}
		break;

		default:

		break;
	}
	
	if (gaugeStatus[deviceNum])
	{
		mb1.Coil(mbCoilShift+j + 2, 1);        // Ref PIR fault
	}
	j = j + 3;
	
	
	if (gaugeStatus[deviceNum] & B00000001)
	{
		mb1.Coil(mbCoilShift+j + 2, 1);        // Ref PIR fault
	}
	
	if (gaugeStatus[deviceNum] & B00000010)
	{
		mb1.Coil(mbCoilShift+j + 3, 1);        // Gas PIR fault
	}


	if (gaugeStatus[deviceNum] & B00000100)
	{
		mb1.Coil(mbCoilShift+j + 4, 1); // IR source fault
	}

	if (gaugeStatus[deviceNum] & B00001000)
	{
		mb1.Coil(mbCoilShift+j + 5, 1); // EEPROM fault
	}

	if (gaugeStatus[deviceNum] & B00010000)
	{
		mb1.Coil(mbCoilShift+j + 6, 1); // Optical pass contaminated
	}

	if (gaugeStatus[deviceNum] & B00100000)
	{
		mb1.Coil(mbCoilShift+j + 7, 1); // Heater fail
	}

	if  (gaugeData[deviceNum * 2] == lost)
	{
		mb1.Coil(mbCoilShift+j + 8, 1); // Heater fail
	}
	j = j + 9;
	
	
	if (gaugeData[deviceNum * 2] == lost || gaugeData[deviceNum * 2] == fault )
	{
		mb1.Coil(mbCoilShift+j, 1);
	}
	
	j = j + 1;
	
	return j;
	
}



uint16_t erisLevel1_Coils(uint8_t i,uint16_t j)
{
	switch (gaugeData[i * 2])  // Warning, alarm?
	{
		case warning:
		{
			mb1.Coil(mbCoilShift+j, 1);
			
			if (mbDebug)
			{
				SerialUSB.print(mbCoilShift+j);

				SerialUSB.print("-> detector warn ->  ");
				SerialUSB.println(i);
			}
		}
		break;

		case alarm:
		{
			if (mbDebug)
			{
				SerialUSB.print(mbCoilShift+j+1);
				SerialUSB.print("-> detector alarm ->  ");
				SerialUSB.println(i);
			}
			mb1.Coil(mbCoilShift+j + 1, 1);
		}
		break;

		default:

		break;
	}
	
	if (gaugeStatus[i])
	{
		mb1.Coil(mbCoilShift+j + 2, 1);        // Internal Fault
		if (mbDebug)
		{
			SerialUSB.print(mbCoilShift+j+2);
			SerialUSB.print("-> detector fault ->  ");
			SerialUSB.println(i);
		}
	}

	
	
	if (gaugeData[i * 2] == lost) // || gaugeData[i * 2] == fault )
	{
		mb1.Coil(mbCoilShift+j+3, 1);
		if (mbDebug)
		{
			SerialUSB.print(mbCoilShift+j+3);
			SerialUSB.print("-> detector lost ->  ");
			SerialUSB.println(i);
		}
	}
	j = j + 4;
	
	return j;
}




uint16_t fire_Coils(uint8_t i,uint16_t j)
{
	switch (gaugeData[i * 2])  // Warning, alarm?
	{

		case alarm:
		{
			mb1.Coil(mbCoilShift+j, 1);
			if (mbDebug)
			{
				SerialUSB.print(mbCoilShift+j);
				SerialUSB.print("-> fire alarm ->  ");
				SerialUSB.println(i);
			}
		}
		break;
		
		
		case lost:
		{
			mb1.Coil(mbCoilShift+j+1, 1);
			if (mbDebug)
			{
				SerialUSB.print(mbCoilShift+j+1);
				SerialUSB.print("-> fire lost ->  ");
				SerialUSB.println(i);
			}
		}
		break;

		default:

		break;
	}
	
	
	j = j + 2;
	
	return j;
}





uint16_t mbMoveCoils()
{
	
	uint16_t j = 0;

	for (uint8_t i = 1; i <= totalSensors; i++)
	{
		switch (gasType[i])
		{
			case disable:
			
			break;
			
			case gas_SF6:
			j = IGAS_Coils(i,j);
			break;
			
			case gas_H2:
			j = erisLevel1_Coils(i,j);
			break;
			
			case gas_O2:
			j = erisLevel1_Coils(i,j);
			break;
			
			case gas_AsH3:
			j = erisLevel1_Coils(i,j);
			break;
			
			case gas_PH3:
			j = erisLevel1_Coils(i,j);
			break;
			
			case gas_Fire:
			j = fire_Coils(i,j);
			break;
			
			default:
			
			
			break;
		}
	}
	
	
	
	mb1.Coil(mbCoilShift+j, 1); // System Alarm

	mb1.Coil(mbCoilShift+j+1, redundantFlag[PORT[RS1]]);
	mb1.Coil(mbCoilShift+j + 2, redundantFlag[PORT[RS2]]);

	j = j + 3;

	for (byte i = 1; i <= BRSlice; i++)
	{
		if (BR_Fail[i])
		{
			mb1.Coil(mbCoilShift+j, 1);
			if (mbDebug)
			{
				SerialUSB.print(mbCoilShift+j);
				SerialUSB.print("-> slise lost ->  ");
				SerialUSB.println(i);
			}
		}
		j++;
	}
	if (mbDebug)
	{

		SerialUSB.print(" Total j ->  ");
		SerialUSB.println(j);
	}
	
}


// Modbus TCP Fin




void initSDcard()
{

	digitalWrite(PLC_ok, LOW);

	while (1)
	{
		if (!SD.begin(microSD_CS))
		{
			delay(500);
			digitalWrite(SD_OK, LOW);
			SerialUSB.println("No SD ->");
			delay(500);
			digitalWrite(SD_OK, HIGH);
		}
		else
		return;
	}
}