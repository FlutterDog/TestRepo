/*
Gas Extraction System IGAS 52 PLC module
Sergey Zakrzhevski
IGAS Detection Ltd

*/

// Firm Version 198   - 30.09.2024

#include <SPI.h>
//#include <SC16IS750.h>
#include <SC16IS7xx.h>
#include <Modbus.h>
#include <ModbusIP.h>
#include <SD.h>
#include <IGAS_Eth.h>
#include <IGAS_RTC.h>
//#include <IGAS_104_ASDU.h>
#include <IGAS_104_SingleASDU.h>
//#include <IGAS_104_PackData.h>
#include <CircularBuffer.h>
#include <MB_LCP_Serv.h>
#include "IGAS_Processing.h"

// added in 22 ver
//#include <EthernetUdp2.h>
#include <IGAS_STRUCT_2.h>
#include "IGAS_ChannelPrinter.h"
#include "IGAS_ChannelNames.h"

const float SWver = 4.24;


CircularBuffer<byte, 40> slaveX2X;     // X2X task queue

CircularBuffer<byte, 20> HMIqueue;     // HMI task queue


MB_LCP_Serv IGAS_mb_X;

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

#define WDT_KEY (0xA5)



#define setQDS_invalid(value) ((value) |= (B10000000))
#define setQDS_valid(value) ((value) &= (B01111111))

#define QDS_Invalid  B10000000
#define QDS_Good  B00000000

byte QDS_;


// Declare the array and fill it with the selected structures

const int16_t system_Devices_size = 200;
void* system_Devices[system_Devices_size];
uint16_t totalNodes = 0;




char* daynames[] = {"Понедельник", "Вторник", "Среда", "Четверг", "Пятница", "Суббота", "Воскресенье"};


int startScreen;

#define setQDS_invalid(value) ((value) |= (B10000000))
#define setQDS_valid(value) ((value) &= (B01111111))

SpiUartDevice UARTX;
#define   CH_A    0x00
#define   CH_B    0x01

#define sc740 0
#define sc752 1


byte uartVer = sc740;
//byte const uartVer = sc740;


ModbusIP mb1;
ModbusIP mb2;

byte const ETH1_CS = 10;     // CS SPI pin
byte const ETH2_CS = 48;      // CS SPI pin
IEC_104 IEC104_E1(ETH1_CS);
IEC_104 IEC104_E2(ETH2_CS);

const byte oldVersion = 1; // For non automatic HMI


const byte LostPacketError = 5;
const byte BR_lostpacketSP = 5;


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
const unsigned int WaitResponceGauge = 500;
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

#define QDS_SP 8

#define sp_Invalid  B10000000
#define sp_Good  B00000000

//int16_t X2X_Data_Array[8][60];


enum
{
	COMM,
	FAULT,
};

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
};


enum
{
	detState,
	detError,
	gasConcentration,
	gasWarning,
	gasAlarm
};

byte const DMqnt = 50;
byte const DM910DataSize = 10;

float DM910Data[DMqnt][DM910DataSize];

#define PHASE_A				0
#define PHASE_B				1
#define PHASE_C				2
#define phase_quantity		3					// Total phase quantity
float	k_current[phase_quantity];


float elpaBuffer[5];

byte PVTlost = 0;
int16_t	PVThum	= 0;
int16_t	PVTtemp = 0;


uint16_t pollDelay[] = {60, 500, 500, 500, 500, 500, 500, 500};
unsigned long startTimeOut[] = {1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000}; // Array of timeout - waiting for respond on each channel

unsigned long x2xTimerCheck;
unsigned long x2xTimerRead;

byte totalTasks = 0;

//Used Pins

byte sensorState = 0;
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
byte const X2X_Transmit = 45;   // transmit enable pin X2X port o



enum           // IGAS 52 task sequence
{
	serviceCheck,
	openPipeValve,
	checkPipe,
	openPumpValve,
	pumpDown,
	closeTankValve,
	closePumpValve,
	stabilizing,
	gasMeasurements,
	finished
};

int const pumpingTime = 8000;
int const tankDelayTime = 600;
int const stabilizingTime = 30000;
int const idleTime = 10000;
int stepDelay = 3000;


byte const openValve = 1;
byte const closeValve = 0;
byte const update = 0;

byte updateSchematic = 0;



const byte spontaneosActivate = 1;
// EthernetClient client;
// EthernetServer iec104Server(2404);

// int alarmBuffer[40][2][50]; // slave, ref/gas, point



byte BR_lostpacket[20];


byte lowLim[8];
byte hiLim[8];


byte currentTask = 3;


byte relayModule;
byte relayPurpose;
byte relayMask;


byte serialFlag[9];  // Serial Flag array
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


byte redundantMode[8];
byte redundantBuffer[9][30];
byte redundantFlag[9];
byte taskFlag[] = {0, 0, 0, 0, 0, 0, 0, 0};

byte logYellow[30];
byte logRed[30];
byte logDisconnect[30];
byte logError[30];
byte logDisabled[30];
byte logHMI[7];

int parmaArray[15];

byte logX2X[20];


byte EIA_Lines;

byte BR_Position = 1;
byte sensorPosition;

byte nextCommand;


byte MBposition;
byte StatusByte;
byte serviceMode = 0;








unsigned long HMIupdTimer = 0;

enum        // HMI task sequence
{
	standByCase,
	updateInitHMI,
	updateFrontPage,
	updateSliceStatus,
	updateIPtable,
	updateDateTime,
	updateDetectorStatus,
	updateResetBeacon,

	updateResetBtns,
	updateButtons,
	// Start HMI Bit Buttons 1802.0

	updateRelayToHMI,
	updateSaveRelay,
	updateEnableDetLoad,
	updateEnableDetSave,
	updateSetRTC,
	updateSerialLoad,
	updateSerialSave,
	updateBeaconCommand,
	

	updateToolBox,
	updateRelayReset,
	updateEmulationCommand,
	updateTestRelayCommand,
	updateSrvRelayLoad,
	updateSrvRelaySave,
	updateRebootPLC,
	updateProtocol,

	// End Bit Commands

	// Queue tasks
	
};

const byte updated = 0;
byte btnState = 0;
byte HMITask = 0;




int resursLB_kA_A;
int resursHB_kA_A;

int resursLB_kA_B;
int resursHB_kA_B;

int resursLB_kA_C;
int resursHB_kA_C;


int16_t resursIntArray[50];
byte resursByteArray[20];

int current_A;
int current_B;
int current_C;

int counterLessA;
int counterLessB;
int counterLessC;

int counterMoreA;
int counterMoreB;
int counterMoreC;



const byte loopFail = 1;
const byte loopOk = 0;
const byte active = 1;

// External Serial flags
const byte portReady = 3;
byte Ethernet1Mode = 0;
byte Ethernet2Mode = 0;


uint8_t sporadicEn_1;
uint8_t sporadicEn_2;


byte relayMessage[7];

byte bridgePort;
byte bridgeByte;

unsigned long currentTime;
unsigned long lastTaskTime;
unsigned long lastMbSlaveTime;

unsigned long waitTillSent[8];

unsigned long X2XSend;

File dataFile;
String sdString = "";
char SDname[11] = "";
byte SD_lines = 0;

// On the Ethernet Shield, CS is pin 4. Note that even if it's not
// used as the CS pin, the hardware CS pin (10 on most Arduino boards,
// 53 on the Mega) must be left as an output or the SD library
// functions will not work.

byte initSubTask;
byte slavesOnChannel[8];

byte BuzzerOffRelay[7];

byte DisconnectRelayMask[7];

byte watchDogRelay;

static uint16_t g_x2xCount  = 0;   // кол-во X2X модулей (включая CPU addr=0)
static uint16_t g_sensBase  = 0;   // индекс начала датчиков в system_Devices[]
static uint16_t g_sensCount = 0;   // кол-во датчиков на RS485-линии



byte systemFaultRelay[7];

byte lostPackets[50];

const int baudrate_gauge = 9600;                                                   // Baudrate of 2 gauge channels
const unsigned long baudrate_HMI = 19200;                                                   // Baudrate of 2 gauge channels
const int baudrate_PC = 9600;                                                   // Baudrate of 2 gauge channels
unsigned long RelayProtectionTimer;


unsigned long startPollTimer[8];




// byte SensorStatusByte[12];



int16_t  lastBytesReceived[9];             // Array of quantity received in buffer after last check



unsigned long Interframe_Silence[8];  // Interframe silence - interframe delay without delay
unsigned long interframe_delay = 5;    /* Modbus t3.5 = 2 ms */
unsigned long interframe_delay_slave = 5;    /* Modbus t3.5 = 2 ms */
unsigned long soundTimer;


byte RelayWD;


byte totalSensors = 0;

int16_t           warningRelayMask[7][50];
int16_t           alarmRelayMask[7][50];
int16_t           warningSetpoint[50];
int16_t           alarmSetpoint[50];



byte RelayProtectionTrigger = 0;
byte AlarmSoundOff;




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

byte const standBy = 0;  //
byte const doTask = 1;

byte HMIRedScreen = B00000010;     // Фон красного цвета
byte HMIYellowScreen = B00000100;    // Фон желтого цвета
byte HMIRedAlarm = B00001000;    // Превышен второй порог - красный  фон
byte HMIYellowAlarm = B00010000;     // Превышен первый порог - желтый фон
byte sysFault = B00100000;       // Авария в системе автоматики
byte lostConnect = B01000000;      // Потеряна связь с детекторами



byte BR_Fail_Mask[] = {0, B00000001, B00000010, B00000100, B00001000, B00010000, B00100000, B01000000, B10000000}; // HMI diagnostic page
byte BR_Fail[8];

byte ScreenState[60];                   // Array of Desktop HMI color - Alarm-Warning
int16_t gaugeData[60];               // Array of bytes from gauge - channel 3
int16_t gaugeDataBuffer[60];               // Array of bytes from gauge - channel 3
int16_t gasType[50];
int16_t enabled[50];
byte gaugeErrorCode[50]; // Показания датчика

#define currScopeSize 210
enum     // HMI buttons state
{
	scopeA,   //
	scopeB,
	scopeC,
	encoder
};

int16_t scopeArray[4][currScopeSize];


int16_t processedData[60];

// HMI Status byte
int16_t hide = -1;



enum     // HMI buttons state
{
	idle,   //
	alarm,
	warning,
	ok,
	lost,
	fault,
	blockedPipe,
	disconnected,
};

const byte initFlag = 10;

byte HMIstatus[] = {initFlag, initFlag, initFlag, initFlag, initFlag, initFlag};

byte TRUE = 1;
byte FALSE = 0;

enum
{
	_OK,
	_FAULT
};

#define _VALID 0
#define _INVALID B10000000


byte colorGrey = 0;

int16_t outArray[70];


byte currentSlave[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};       // slave number that should be proceed next - channel 1

// byte slaveAddr[5][35];             // Array of HMI command regs

byte totalDisplays;

enum
{
	X2X,
	RS_Slave,
	HMI,
	RS1,
	RS2,
	RS3,
	RS4,
	portQnt
};




byte S_PORT[] = {0, 1, 2, 3, 4, 5, 6};


int16_t data_X2X[240];
int16_t data_PC[100];          // Array of data message contain command (x03, x10), slave number, data and CRC sum.
int16_t data_HMI[100];          // Array of data message contain command (x03, x10), slave number, data and CRC sum.
int16_t data_S1[70];        // Array of data message contain command (x03, x10), slave number, data and CRC sum.
int16_t data_S2[70];        // Array of data message contain command (x03, x10), slave number, data and CRC sum.
int16_t data_S3[70];        // Array of data message contain command (x03, x10), slave number, data and CRC sum.
int16_t data_S4[70];        // Array of data message contain command (x03, x10), slave number, data and CRC sum.


byte taskNum[9];

byte subTask_X2X;
byte subTask_PC;
byte subTask_S1;
byte subTask_S2;
byte subTask_S3;
byte subTask_S4;

byte subsubTask[8] = {0};
byte rtcTask = 0;

byte packet_size[8];

byte packet[7][100];        // Array of data message contain command (x03, x10), slave number, data and CRC sum.




String dataString;

byte manualRelayMaskBuffer;
byte manualRelayMask;
byte manualSlice;
byte manualSliceBuffer;

// Debug flags

byte const newASDU_Debug = 0;
byte const SerialUSB_Ok = 1;
byte const sdDebug = 1;
byte const X2X_Debug = 0;
byte const dmDebug =0;
byte const dmDebugSpont = 0;
byte const calcDebug = 0;
byte const lctDebug = 0;
byte const IEC_Data_Debug = 0;
byte const initSerialDebug = 0;
byte debugMB03 = 0;

byte  PLC_SlaveAdress;

const byte fulData = 0;
const byte changeData = 0;

// вместо int16_t IEC104data_M_SP[400]; ...
int16_t* IEC104data_M_SP   = nullptr;
int16_t* IEC104spont_M_SP  = nullptr;

int16_t* IEC104data_M_ME   = nullptr;
int16_t* IEC104spont_M_ME  = nullptr;
uint8_t* IEC104_M_ME_QDS   = nullptr;

float*   IEC104data_M_TF   = nullptr;
int16_t* IEC104spont_M_TF  = nullptr;
uint8_t* IEC104_M_TF_QDS   = nullptr;

// опционально тоже сделать динамикой (зависит от твоей карты Modbus)
int16_t* RS485_Holding     = nullptr;
uint8_t* RS485_Coil        = nullptr;

int16_t* changeBuffer      = nullptr;

uint16_t IEC104_SP_N = 0;
uint16_t IEC104_ME_N = 0;
uint16_t IEC104_TF_N = 0;

int16_t mbUserSize = 0;


enum
{
	compressor,
	airRelay,
	vacRelay,
	detectorState,
	airValve,
	vacValve,
	calibrValve,
	valve1,
	filter1,
	valve2,
	filter2,
	valve3,
	filter3,
	valve4,
	filter4,
	valve5,
	filter5,
	valve6,
	filter6,
	valve7,
	filter7,
	valve8,
	filter8,
	valve9,
	filter9,
	valve10,
	filter10,
	valve11,
	filter11,
	valve12,
	filter12,
	valve13,
	filter13,
	pneumoSize
};

byte startVlvEnum = valve1 - 2;


enum
{
	vlvNotInitialized,
	vlvClosed,
	vlvOpen,
	vlvWarning,
	vlvAlarm
};

enum
{
	state_1,
	state_2,
	state_Ok,
	state_3,
	state_Fault
};

int16_t pneumoData[pneumoSize];

File root;
uint8_t RS_startASDU = 0;

void watchdogSetup(void) {
	/*** watchdogDisable (); ***/
}

uint8_t SDconfigState = 1;


extern char _end;
extern "C" char *sbrk(int i);

int getFreeMemory() {
	char stack_dummy = 0;
	return &stack_dummy - sbrk(0);


}

void setup()
{
	delay(5000);
	SPI.begin();
	initPins();

	//
	// 	for (byte i = 0; i < pneumoSize; i++)
	// 	pneumoData[i] = vlvClosed;
	//
	// 	pneumoData[detectorState] = 0;

	if (!SD.begin(microSD_CS))
	{
		digitalWrite(PLC_ok, LOW);
		while (!SD.begin(microSD_CS))
		{
			delay(500);
			digitalWrite(SD_OK, LOW);
			if (SerialUSB_Ok)
			SerialUSB.println("No SD ->");
			delay(500);
			if (SerialUSB_Ok)
			digitalWrite(SD_OK, HIGH);
		}
	}
	
	SerialUSB.print("Single ASDU. FW ver. ");
	SerialUSB.println(SWver);
	SerialUSB.println();
	SerialUSB.println("********************");
	
	
	// Load redundant line
	loadConfig("SC16.txt", outArray);
	if (outArray[1] == 740)
	uartVer = sc740;
	else
	uartVer = sc752;


	initSerial();
	/*
	while (!SerialUSB)  {
	;  // wait USB to connect. Needed for Leonardo only
	}
	*/

	if (SerialUSB_Ok)
	SerialUSB.println("Карта microSD подключена к системе");
	digitalWrite(SD_OK, HIGH);
	delay(100);



	
	// redundantMode[3]=1;

	// Load BR config for special situation
	// 	loadConfig("buzzer.txt", outArray);
	// 	for (int i = 1; i <= outArray[0]; i++) // количество датчиков
	// 	BuzzerOffRelay[i] = outArray[i];
	//
	// 	// Load BR config for special situation
	// 	loadConfig("DetFlt.txt", outArray);
	// 	for (int i = 1; i <= outArray[0]; i++)
	// 	DisconnectRelayMask[i] = outArray[i];
	//
	//
	// 	// loadConfig("wtdg.txt", outArray);
	// 	//    watchDogRelay = outArray[1];    // Overrided for first relay BR1
	// 	watchDogRelay = B00000001;
	//
	//
	// 	// Load BR config for general system Fault
	// 	loadConfig("SysFlt.txt", outArray);
	// 	for (int i = 1; i <= outArray[0]; i++)
	// 	systemFaultRelay[i] = outArray[i];
	//

	//`
	// 	// Load  in system
	// 	loadConfig("HMIs.txt", outArray);
	// 	totalDisplays = outArray[1];
	//
	// 	/*
	// 	// Load Slave quantity on each line
	// 	loadConfig("slaves.txt", outArray);
	//
	// 	slavesOnChannel[RS1] = outArray[1];
	// 	slavesOnChannel[RS2] = outArray[2];
	// 	slavesOnChannel[RS3] = outArray[3];
	// 	slavesOnChannel[RS4] = outArray[4];
	// 	*/



	/*
	X2X_Data_Array
	//slaveX2X
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
	
	LCP - 0
	LDO - 1
	LAI - 2
	LDI - 3
	LCT - 4
	
	*/
	// Load BR modules quantity
	// loadConfig("BRX.txt", outArray);
	//g_x2xCount = outArray[1];
	
	loadConfig("RSpoll.txt", outArray);
	
	for (byte i=0; i<portQnt; i++)
	pollDelay[i] = outArray[i+1];
	
	int16_t ASDU_blank = 1;
	
	loadConfig("ASDU.txt", outArray);
	IEC104_E1.ASDU = outArray[1];

	loadConfig("ASDU2.txt", outArray);
	IEC104_E2.ASDU = outArray[1];




	
	
	// --------------------------
	// 1) X2X
	// --------------------------
	
	// 0) Всегда добавляем PLC первым
	totalNodes = 0;
	addNewASDU(LCP, ASDU_blank);   // system_Devices[0]

	if (!loadX2XModulesOrFail("X2X.txt", outArray))
	{
		SerialUSB.println("Stop: bad X2X.txt");
		delay(1000);
		while (1)
		{
			;
		}
	}

	// 2) База сенсоров = после PLC + X2X
	g_sensBase = 1 + g_x2xCount;


	// после построения X2X устройств
	normalizeX2XSlave();

	
	// 	for (byte i=1; i<=g_x2xCount; i++)
	// 	{
	// 		SerialUSB.print("Slave ");
	// 		SerialUSB.print(i);
	// 		SerialUSB.print(" ->ID ");
	// 		SerialUSB.println(((DeviceCommon*)system_Devices[i])->ID);
	// 	}
	
	// DELETE!!!!
	//addNewASDU(LCT);
	//	g_x2xCount = 3;
	// DELETE!!!!


	
	if (X2X_Debug)
	{
		SerialUSB.println("X2X modules");
	}
	//
	// 	for (byte i = 1; i < g_x2xCount; i++)               // X2X модули. 1 - блок реле. 2 - аналоговый вход
	// 	{
	// 		X2X_Data_Array[i][TypeModule] = outArray[i+1];
	//
	// 		switch (outArray[i])
	// 		{
	// 			case LDO:
	// 			X2X_Data_Array[i][DataQnt] = 1;
	// 			break;
	//
	// 			case LAI:
	// 			X2X_Data_Array[i][DataQnt] = 8;
	// 			break;
	//
	// 			case LDI8:
	// 			X2X_Data_Array[i][DataQnt] = 1;
	// 			break;
	//
	// 			case LCT:
	// 			X2X_Data_Array[i][DataQnt] = 45;
	// 			break;
	//
	// 		}
	//
	// 		if (X2X_Debug)
	// 		{
	// 			SerialUSB.print("Slave -> ");
	// 			SerialUSB.print(i);
	// 			SerialUSB.print("Type -> ");
	// 			SerialUSB.print(X2X_Data_Array[i][TypeModule]);
	// 			SerialUSB.print(" Data qnt -> ");
	// 			SerialUSB.println(X2X_Data_Array[i][DataQnt]);
	// 		}
	//
	// 	}
	
	// Load Slave adresses on line 1
	loadConfig("ch1.txt", outArray);

	slavesOnChannel[RS1] = outArray[1];

	
	
	//

	/*
	if (outArray[0] != 0)
	for (int i = 1; i <= slavesOnChannel[RS1]; i++)
	slaveAddr[1][i] = outArray[i];
	*/



	// Load Slave addresses on line 2
	loadConfig("ch2.txt", outArray);
	slavesOnChannel[RS2] = outArray[1];
	//

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




	/*
	if (outArray[0] != 0)
	for (int i = 1; i <= slavesOnChannel[RS4]; i++)
	slaveAddr[4][i] = outArray[i];
	*/

	// ***** Отправка настроек в панель *****


	for (byte i = RS1; i <= RS4 ; i++)
	{
		totalSensors += slavesOnChannel[i];
		if (SerialUSB_Ok)
		{
			SerialUSB.print("Port ");
			SerialUSB.print(i);
			SerialUSB.print(" -> ");
			SerialUSB.println(totalSensors);
		}
	}

	lowLim[RS1] = 1;
	hiLim[RS1] = slavesOnChannel[RS1];
	currentSlave[RS1] = lowLim[RS1];

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
			lowLim[i] = hiLim[i - 1] + 1;
			hiLim[i] = lowLim[i] + slavesOnChannel[i]-1;
		}
		else
		{
			lowLim[i] = hiLim[i - 1] + 1;
			hiLim[i] = lowLim[i];
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

	


	// Load Yellow Warning set point
	//
	loadConfig("Yellow.txt", outArray);
	//
	for (int i = 1; i <= totalSensors + 1; i++)               // количество датчиков
	warningSetpoint[i] = outArray[i];
	//
	//
	// 	// Load Red Alarm set point
	//
	loadConfig("Red.txt", outArray);
	//
	for (int i = 1; i <= totalSensors + 1; i++) // количество датчиков
	alarmSetpoint[i] = outArray[i];
	//




	// 	// Relay mask - BR1
	// 	if (g_x2xCount > 0)
	// 	{
	loadConfig("RMask1.txt", outArray);
	for (int i = 1; i <= totalSensors + 1; i++) // количество датчиков
	alarmRelayMask[1][i] = outArray[i];
	//
	loadConfig("YMask1.txt", outArray);
	for (int i = 1; i <= totalSensors + 1; i++) // количество датчиков
	warningRelayMask[1][i] = outArray[i];
	// 	}
	//
	// 	// Relay mask - BR2
	//
	// 	if (g_x2xCount > 1)
	// 	{
	// 		loadConfig("RMask2.txt", outArray);
	// 		for (int i = 1; i <= totalSensors + 1; i++) // количество датчиков
	// 		alarmRelayMask[2][i] = outArray[i];
	//
	//
	// 		loadConfig("YMask2.txt", outArray);
	// 		for (int i = 1; i <= totalSensors + 1; i++) // количество датчиков
	// 		warningRelayMask[2][i] = outArray[i];
	// 	}
	//
	//
	// 	// Relay mask - BR3
	// 	if (g_x2xCount > 2)
	// 	{
	// 		loadConfig("RMask3.txt", outArray);
	// 		for (int i = 1; i <= totalSensors + 1; i++) // количество датчиков
	// 		alarmRelayMask[3][i] = outArray[i];
	//
	//
	// 		loadConfig("YMask3.txt", outArray);
	// 		for (int i = 1; i <= totalSensors + 1; i++) // количество датчиков
	// 		warningRelayMask[3][i] = outArray[i];
	// 	}
	//
	// 	// Relay mask - BR3
	// 	if (g_x2xCount > 3)
	// 	{
	// 		loadConfig("RMask4.txt", outArray);
	// 		for (int i = 1; i <= totalSensors + 1; i++) // количество датчиков
	// 		alarmRelayMask[4][i] = outArray[i];
	//
	//
	// 		loadConfig("YMask4.txt", outArray);
	// 		for (int i = 1; i <= totalSensors + 1; i++) // количество датчиков
	// 		warningRelayMask[4][i] = outArray[i];
	// 	}

	// *************** Serial ***************************

	loadConfig("EIA.txt", outArray);
	EIA_Lines = outArray[0];

	for (byte i = 1; i <= 4; i++)
	S_PORT[i + 2] = outArray[i];


	totalTasks = EIA_Lines;

	if (SerialUSB_Ok)
	{
		SerialUSB.print("totalTasks -> ");
		SerialUSB.println(totalTasks);
	}
	// *************** Sensor ***************************
	if (SerialUSB_Ok)
	{
		SerialUSB.print("EIA_Lines -> ");
		SerialUSB.println(EIA_Lines);
	}
	
	loadConfig("Sens.txt", outArray);
	g_sensCount = outArray[0];
	totalSensors = g_sensCount;   // если totalSensors используется дальше

	// slave=1..N, в дереве = g_sensBase + (slave-1)
	for (uint16_t slave = 1; slave <= g_sensCount; ++slave)
	{
		uint16_t id = outArray[slave];
		gasType[slave] = id;          // если gasType[] индексируется с 1
		addNewASDU(id, ASDU_blank);
	}

	allocIec104Buffers(15, 8);// выделение памяти в динамических массивах

	
	// 	IEC104_E1.asduTail = totalNodes;
	// 	IEC104_E2.asduTail = totalNodes;
	
	// 	loadConfig("Enbld.txt", outArray);
	//
	//
	// 	for (uint8_t i = 1; i <= totalSensors; i++)
	// 	{
	// 		enabled[i] = outArray[i];
	// 		//  IEC104data_M_SP[i] = 0;
	// 		//  IEC104data_M_SP[2*i] = ok;
	// 	}
	
	// 	SerialUSB.println(" + 5 ");
	// 	for (int i = totalSensors; i <= totalSensors + 5; i++)
	// 	{
	// 		SerialUSB.print(" -> ");
	// 		SerialUSB.print(enabled[i]);
	// 		//  IEC104data_M_SP[i] = 0;
	// 		//  IEC104data_M_SP[2*i] = ok;
	// 	}
	// 	SerialUSB.println(" + 5 ");
	// Added 4.0.3_1 -> Start
	

	delay(300);
	//SerialUSB.print("E1");
	//SerialUSB.println(IEC104_E1.ASDU);

	//SerialUSB.print("E2");
	//SerialUSB.println(IEC104_E2.ASDU);

	//  IEC104data_M_SP[0]=0;



	initEthernet();

	/*
	if (Ethernet1Mode == TCPIP)
	{
	mbUserSize = 20; //totalSensors * 3 + g_x2xCount + 3 + 7;

	for (byte i = 0; i < 40; i++)  // add concentration data
	mb1.addIreg(i, 0);

	for (byte j = 1; j < 40; j++)  // Add status bit data
	mb1.addCoil(j,1);
	}
	*/
	// *************** Reset Screen State ***************************

	/*
	for (int j = 1; j <= totalSensors; j++) // Reset all HMI's gauges fields to normal
	{
	ScreenState[j] = 0;
	}

	for (int j = 1; j <= totalSensors; j++) // Reset all HMI's gauges fields to normal
	{
	gaugeData[2 * j - 1] = 0;
	gaugeData[2 * j] = ok;
	}
	*/


	// ***** Preset HMI settings

	//  while (initHMI() != 99 || HMIstatus != ok);


	currentSlave[HMI] = 1;

	RelayProtectionTimer = millis();

	// Set user Array 104 size

	//mbUserSize = totalSensors * 3 + g_x2xCount + 3 + 7;






	// 	IEC104_E1.size_M_SP = calcSPsize();//mbUserSize;
	// 	IEC104_E1.size_M_ME = calcMEsize();//totalSensors;
	// 	IEC104_E1.size_M_TF = calcTFsize();//totalSensors;
	//
	// 	IEC104_E2.size_M_SP = calcSPsize();  //mbUserSize;
	// 	IEC104_E2.size_M_ME = calcMEsize(); //totalSensors;
	// 	IEC104_E2.size_M_TF = calcTFsize();

	// 	if (SerialUSB_Ok)
	// 	{
	// 		SerialUSB.print("Output data size ->  ");
	// 		SerialUSB.println(mbUserSize);
	// 	}
	sdString = "IGAS. Включение питания.";
	printData();

	digitalWrite(PLC_ok, HIGH);

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

		SerialUSB.println("***************"); //Printing for debugging purpose

	}
	currentSlave[X2X] = 1;

	for (byte i = 1; i < g_x2xCount; i++)
	{
		slaveX2X.push(i);
	}

	//
	// 	IEC104_E1.totalASDU = totalNodes;
	// 	IEC104_E2.totalASDU = totalNodes;



	//	IEC104_E1.ASDU = outArray[1];


	uint16_t outArrayU[70];

	loadConfig("IECreg.txt", outArrayU);
	
	IEC104_E1.spStartAddr = outArrayU[1];
	IEC104_E1.meStartAddr = outArrayU[2];
	IEC104_E1.tfStartAddr = outArrayU[3];

	
	IEC104_E2.spStartAddr = outArrayU[1];
	IEC104_E2.meStartAddr = outArrayU[2];
	IEC104_E2.tfStartAddr = outArrayU[3];

	

	digitalWrite(X2X_LED, HIGH);

	RTC->RTC_CR &= (uint32_t)(~RTC_CR_UPDTIM) ;
	RTC->RTC_CR &= (uint32_t)(~RTC_CR_UPDCAL) ;
	RTC->RTC_SCCR |= RTC_SCCR_SECCLR ;
	
	
	
	IGAS_mb_X.initMB(1001);
	IGAS_mb_X.setDelegate(modbusProceed);        // To run library function from main loop
	IGAS_mb_X.slave = 1;
	//
	// 	outArray[0] = 1;
	// 	outArray[1] = 2;
	// 	outArray[2] = 3;
	// 	outArray[3] = 4;
	// 	outArray[4] = 5;



	//	saveConfig("X2X.txt", outArray, 4);

	//	root = SD.open("/");
	//	printDirectory(root, 0);

	//	loadConfig("IIAS.TXT", outArray);
	
	
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





int const pollRTC = 100;
unsigned long RTCLast;


int const pollTimeRTC = 1000;
unsigned long RTCLastTime;

unsigned long debugPrinter = 0;

unsigned long updIecData = 0;




floatval pressureDM_val;
floatval pressure20DM_val;
floatval themperatureDM_val;
floatval densityDM_val;
floatval lctFloat;
floatval laiFloat;

floatval dewPointDM_val;
floatval PPMvDM_val;
floatval RH_DM_val;

byte const RSDebug = 0;
unsigned long makeIecArrayTimer = 0;

void loop()
{
	currentTime = millis();
	
	
	//Restart watchdog
	WDT->WDT_CR = WDT_CR_KEY(WDT_KEY)
	| WDT_CR_WDRSTT;
	
	
	//	IGAS_52();

	if ( (rtc_clock.rtcFlag != RTC_FLAG_IDLE) && (currentTime - RTCLast > pollRTC))
	{
		RTCLast = currentTime;
		rtc_clock.rtcLOOP();
	}
	
	

	pollUsbConsole();


	
	// 	if (Ethernet1Mode == TCPIP)
	// 	{
	// 		Ethernet.select(ETH1_CS);
	// 		mb1.task();    // Slave output to customer PC via Ethernet Modbus TCP line
	//
	// 		Ethernet.select(ETH2_CS);
	// 		mb1.task();    // Slave output to customer PC via Ethernet Modbus TCP line
	// 	}
	//
	// 	else
	// 	{
	if (initSerialDebug)
	{
		SerialUSB.println("Ln 1396");
	}
	
	
	if (currentTime - makeIecArrayTimer > 2000)
	{
		// 		int freeMem = getFreeMemory();
		// 		SerialUSB.print("Free RAM: ");
		// 		SerialUSB.print(freeMem);
		// 		SerialUSB.println(" bytes");
		moveSingleIEC();  // Enable for IEC104
		makeIecArrayTimer = currentTime;
	}
	

	if (currentTime - updIecData > 20)
	{
		// 		IEC104_E1.LOOP(system_Devices, IEC104data_M_SP, IEC104spont_M_SP, IEC104data_M_ME, IEC104spont_M_ME, IEC104data_M_TF, IEC104spont_M_TF);
		// 		//delay(100);
		// 		IEC104_E2.LOOP(system_Devices, IEC104data_M_SP, IEC104spont_M_SP, IEC104data_M_ME, IEC104spont_M_ME, IEC104data_M_TF, IEC104spont_M_TF);

		IEC104_E1.LOOP(IEC104data_M_SP, IEC104spont_M_SP, IEC104data_M_ME, IEC104spont_M_ME, IEC104_M_ME_QDS, IEC104data_M_TF, IEC104spont_M_TF, IEC104_M_TF_QDS);
		IEC104_E2.LOOP(IEC104data_M_SP, IEC104spont_M_SP, IEC104data_M_ME, IEC104spont_M_ME, IEC104_M_ME_QDS, IEC104data_M_TF, IEC104spont_M_TF, IEC104_M_TF_QDS);
		
		updIecData = currentTime;
	}
	
	
	
	if (initSerialDebug)
	{
		SerialUSB.println("Ln 1408");
	}
	// Added NTP ver 22
	//	}
	
	//delay(100);
	
	//}


	taskTimer();  //  Run all system task every poll time ms.


	//
	// 	// serviceMode = 1;
	// 	if (serviceMode == 1)      // Activate USB-RS485 bridge
	// 	mbBridge();

	/*
	if (currentTime - debugPrinter > 2000)    // X2X active bus LED watchDog
	{
	for (byte i = 1; i <= 8; i++)
	{
	SerialUSB.print(X2X_Data_Array[5][i+2]);
	SerialUSB.print("  ");
	}
	SerialUSB.println();
	debugPrinter = currentTime;
	}
	*/

	
	//	if (taskFlag[RS1] == doTask)   // Task timer triggers new subTask on branch 1
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
	if (initSerialDebug)
	{
		SerialUSB.println("Ln 1462");
	}
	
	
	
	//*******************************************************************
	//             Gas Concentration Request - Channel 2
	//*******************************************************************
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
	
	
	//	if (taskFlag[RS1] == doTask)   // Task timer triggers new subTask on branch 1
	

	
	switch (subTask_S3)
	{

		case 0:   // Get gas Analyzer data  ***************************************

		subTask_S3 = getRSdata(RS3, subTask_S3, data_S3);

		break;

		case 1:   // If Service Mode activated ***************************************

		subTask_S3 = serviceModeCheck(RS3, subTask_S3);
		
		break;

		default: // Start from beginning **********************************
		subTask_S3 = 0;
		break;
	}
	
	
	
	//*******************************************************************
	//             Gas Concentration Request - Channel 3
	//*******************************************************************
	
	//	if (taskFlag[RS1] == doTask)   // Task timer triggers new subTask on branch 1
	switch (subTask_S4)
	{

		case 0:   // Get gas Analyzer data  ***************************************

		subTask_S4 = getRSdata(RS4, subTask_S4, data_S4);

		break;

		case 1:   // If Service Mode activated ***************************************

		subTask_S4 = serviceModeCheck(RS4, subTask_S4);
		
		
		break;

		default: // Start from beginning **********************************
		subTask_S4 = 0;
		break;
	}
	
	
	//	X2X_read();

	
	X2X_mb(); // New queue mechanism
	if (initSerialDebug)
	{
		SerialUSB.println("Ln 1551");
	}
	//update_mb_slave();
	// 	if (serviceMode != 1)
	//	IGAS_mb_X.update();

}











void moveSingleIEC()
{
	uint16_t spOffset = 0;
	uint16_t meOffset = 0;
	uint16_t tfOffset = 0;
	
	uint16_t dataChng_SP = 0;
	uint16_t dataChng_ME = 0;
	uint16_t dataChng_TF = 0;

	uint16_t size_M_SP = 0;
	uint16_t size_M_ME = 0;
	uint16_t size_M_TF = 0;
	
	
	for (uint8_t k = 0; k < totalNodes; ++k)
	{
		auto* hdr = static_cast<DeviceHeader*>(system_Devices[k]);
		switch (hdr->ID)
		{
			case LCP:
			{
				auto* dev = static_cast<LCP1116*>(hdr);
				//SP
				processSP(dev, spOffset, dataChng_SP);
				
			}
			break;
			
			case LDO:
			{
				auto* dev = static_cast<LDO1118*>(hdr);
				//SP
				
				processSP(dev, spOffset, dataChng_SP);

			}
			break;

			case LAI:
			{
				
				auto* dev = static_cast<LAI1118*>(hdr);
				//SP
				processSP(dev, spOffset, dataChng_SP);
				//TF
				processTF(dev, tfOffset, dataChng_TF);

				
			}
			break;
			
			

			case LDI8:
			{
				auto* dev = static_cast<LDI1118*>(hdr);
				processSP(dev, spOffset, dataChng_SP);
				
			}
			break;
			
			case LDI16:
			{
				auto* dev = static_cast<LDI1116*>(hdr);
				processSP(dev, spOffset, dataChng_SP);
				
			}
			break;
			

			
			
			case LCT:
			{
				// 1) приводим к конкретному устройству и его заголовку
				auto* dev = static_cast<LCT1114*>(hdr);
				

				// 2) отладка до SP

				// 3) SP-обработка
				processSP< LCT1114 >(dev, spOffset, dataChng_SP);

				// 4) отладка после SP

				// 5) TF-обработка
				processTF< LCT1114 >(dev, tfOffset, dataChng_TF);
			}
			break;
			
			

			case LCT_2:
			{
				// 1) приводим к конкретному устройству и его заголовку
				
				auto* dev = static_cast<LCT1114_2*>(hdr);

				// 2) отладка до SP
				

				// 3) SP-обработка
				processSP< LCT1114_2 >(dev, spOffset, dataChng_SP);

				// 4) отладка после SP

				// 5) TF-обработка
				processTF< LCT1114_2 >(dev, tfOffset, dataChng_TF);
			}
			break;
			
			
			
			case DM910:
			{
				auto* dev = static_cast<DM91_0*>(hdr);
				//SP
				processSP(dev, spOffset, dataChng_SP);
				//TF
				processTF(dev, tfOffset, dataChng_TF);
				
			}
			
			break;
			
			
			case DM910H:
			{
				auto* dev = static_cast<DM91_H*>(hdr);
				//SP
				processSP(dev, spOffset, dataChng_SP);
				//TF
				processTF(dev, tfOffset, dataChng_TF);
				
			}
			break;
			
			
			
			case PVT100:
			{
				auto* dev = static_cast<PVT_100*>(hdr);

				// SP-теги (связь + авария)
				processSP(dev, spOffset, dataChng_SP);

				// ME-каналы (PVTtemp, PVThum)
				processME(dev, meOffset, dataChng_ME);
			}
			break;
			
		}
		
		
		
	}
	IEC104_E1.size_M_SP = spOffset;//mbUserSize;
	IEC104_E1.size_M_ME = meOffset;//totalSensors;
	IEC104_E1.size_M_TF = tfOffset;//totalSensors;

	IEC104_E2.size_M_SP = spOffset;//mbUserSize;
	IEC104_E2.size_M_ME = meOffset;//totalSensors;
	IEC104_E2.size_M_TF = tfOffset;//totalSensors;


	if (dataChng_SP + dataChng_ME + dataChng_TF)
	{

		//	if (sporadicEn_1 == 1)
		IEC104_E1.spontaneousSend = spontaneosActivate;

		IEC104_E1.dataChng_M_SP_ = dataChng_SP;
		IEC104_E1.dataChng_M_ME_ = dataChng_ME;
		IEC104_E1.dataChng_M_TF_ = dataChng_TF;
		
		//	if (sporadicEn_2 == 1)
		IEC104_E2.spontaneousSend = spontaneosActivate;

		IEC104_E2.dataChng_M_SP_ = dataChng_SP;
		IEC104_E2.dataChng_M_ME_ = dataChng_ME;
		IEC104_E2.dataChng_M_TF_ = dataChng_TF;


		dataChng_ME = 0;  // Reset change counter
		dataChng_TF = 0;
	}
}





/****************************************************************************
BEGIN MODBUS RTU SLAVE FUNCTIONS
****************************************************************************/

/* global variables */



/* enum of supported modbus function codes. If you implement a new one, put its function code here ! */
// enum
// {
// 	FC_READ_COILS  = 0x01,   //Read contiguous block of holding register
// 	FC_READ_DISCRETE  = 0x02,   //Read contiguous block of holding register
// 	FC_READ_REGS  = 0x03,   //Read contiguous block of holding register
// 	FC_WRITE_REG  = 0x06,   //Write single holding register
// 	FC_WRITE_REGS = 0x10    //Write block of contiguous registers
// };

/* supported functions. If you implement a new one, put its function code into this array! */
//const unsigned char fsupported[] = {FC_READ_COILS, FC_READ_REGS, FC_WRITE_REG, FC_WRITE_REGS };

/* constants */
// enum
// {
// 	MAX_READ_REGS = 0x20,
// 	MAX_WRITE_REGS = 0x20,
// 	MAX_MESSAGE_LENGTH = 120
// };

//
// enum
// {
// 	RESPONSE_SIZE = 6,
// 	EXCEPTION_SIZE = 3,
// 	CHECKSUM_SIZE = 2
// };

/* exceptions code */
// enum
// {
// 	NO_REPLY = -1,
// 	EXC_FUNC_CODE = 1,
// 	EXC_ADDR_RANGE = 2,
// 	EXC_REGS_QUANT = 3,
// 	EXC_EXECUTE = 4
// };

/* positions inside the query/response array */
// enum
// {
// 	SLAVE = 0,
// 	FUNC,
// 	START_H,
// 	START_L,
// 	REGS_H,
// 	REGS_L,
// 	BYTE_CNT
// };















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

unsigned int crc(unsigned char *buf, int16_t start, int16_t cnt)
{
	int16_t i, j;
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
uint16_t start_addr,
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
uint16_t write_addr, uint16_t reg_val, unsigned char* packet)
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

void build_request_packet(int16_t slave, int16_t function, int16_t start_addr,
int16_t count, unsigned char *packet)
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

int write_regs(unsigned int start_addr, unsigned char *query, int16_t *regs)
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


byte currentCycle[] = {0,0,0,0,0,0,0,0,0,0,0,0};

byte fullMBPackageNumber[portQnt];
byte lastMBPackageLength[portQnt];									// Calc packets to transfer full data
byte addrShift_[portQnt];

unsigned int start_addr_[portQnt];
byte reg_count_[portQnt];

byte preset_multiple_registersX(byte flow_, int16_t start_addr, int16_t reg_count,  int16_t *data_)
{
	int16_t function = 0x10;  /* Function 16: Write Multiple Registers */
	int16_t byte_count, i;
	//int ret;
	byte byteCount = 6;
	byte port_num = S_PORT[flow_];
	byte slave_ = currentSlave[flow_];
	

	switch (serialFlag[flow_])
	{

		case taskComplete:
		
		if (currentCycle[flow_] == 0)
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
			if (currentCycle[flow_] != fullMBPackageNumber[flow_])   // Check if current cycle is a full 25 regs send or last regs send
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
			waitTillSent[flow_] = currentTime;
			packet_size[flow_] = byteCount;

			return 0;  // do nothing
		}
		break;



		case sendingMsg: // wait till packet left Serial buffer



		if ((unsigned)(currentTime - waitTillSent[flow_]) > (unsigned)(2 * (REQUEST_QUERY_SIZE + packet_size[flow_])))
		{
			// SerialUSB.println(REQUEST_QUERY_SIZE);
			// SerialUSB.println(packet_size[flow_]);
			serialFlag[flow_] = receiveMsg;   //
			startTimeOut[flow_] = currentTime;

		}
		return 0;  // do nothing

		break;


		case receiveMsg: // wait till packet left Serial buffer
		switch (receive_incoming_data_preset(flow_, data_))   // Проверка ответа - 0 - входной буффер пуст, сообщения нет.Проверяем пока не вышло время тайм аута, >0 - овтет получен, <0 - ошибка
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
			
			if (currentCycle[flow_] == fullMBPackageNumber[flow_] )
			{
				currentCycle[flow_] = 0;
				return 3;
			}
			currentCycle[flow_]++;
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





/************************************************************************

preset_multiple_registers(slave_id, first_register, number_of_registers,
data_array, registers_array)

Write the data from an array into the holding registers of the slave.

*************************************************************************/

int preset_multiple_registers_Slave(unsigned char slave,
unsigned int start_addr,
unsigned char count,
unsigned char *query,
int16_t *regs)
{
	unsigned char function = FC_WRITE_REGS; /* Preset Multiple Registers */
	int16_t status = 0;
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
unsigned int write_addr, unsigned char *query, int16_t *regs)
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
		UARTX.write(query[i], UART1_CS);

		break;
		*/

		case 2:  // HMI


		for (i = 0; i < string_length; i++)
		UARTX.write(query[i], UART2_CS, CH_A);



		break;

		case 3: // S1

		if (RSDebug)
		SerialUSB.println("Send");
		if (initSerialDebug)
		{
			SerialUSB.println("Ln 2372");
		}
		
		for (i = 0; i < string_length; i++)
		{
			if (RSDebug)
			{
				SerialUSB.print(query[i]);
				SerialUSB.print(" ");
			}
			
			if (uartVer == sc740)
			UARTX.write(query[i], UART3_CS, CH_A);
			else
			UARTX.write(query[i], UART2_CS, CH_B);
			
			if (initSerialDebug)
			{
				SerialUSB.println("Ln 2390");
			}
		}
		
		//	SerialUSB.println();

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


		while (Serial.available())
		{
			received_string[bytes_received] = Serial.read();
			//	SerialUSB.print(received_string[bytes_received]);
			//	SerialUSB.print(" ");

			bytes_received++;
			if (bytes_received >= MAX_RESPONSE_LENGTH)
			return PORT_ERROR;
		}
		// SerialUSB.println();


		break;

		/*
		case 1:  // PC

		while ( UARTX.available(UART1_CS) == 0)
		{
		delay(1);
		if (i++ > TIMEOUT)
		return bytes_received;
		}

		while (UARTX.available(UART1_CS))
		{
		received_string[bytes_received] = UARTX.read(UART1_CS);
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

		//		SerialUSB.println();
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

	byte portNum = S_PORT[flow_];





	//  do {        // repeat if unexpected slave replied
	response_length = receive_response(portNum, recievedBytes);
	//  } while ((response_length > 0) && (data[0] != portArray[0]));

	// exceptions code
	if (recievedBytes[FUNC] > 0x80)
	return 0;


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
			return 0;
		}

		/********** check for exception response *****/
		byte slave_;

		slave_ = currentSlave[flow_];

		if (slave_ != recievedBytes[SLAVE])
		{
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



int read_Discrete_response(byte flow_, int16_t* data_)
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




int read_reg_response(byte flow_, int16_t* data_)
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
	// 	SerialUSB.print("raw_response_length  ");
	// 	SerialUSB.print(currentSlave[flow_]);
	// 	SerialUSB.print(" ->  ");
	// 	SerialUSB.println(raw_response_length);
	
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




byte read_holding_registersX(byte flow_, int16_t start_addr, byte reg_count, int16_t *data_ )
{
	int16_t function = 0x03;  /* Function: Read Holding Registers */
	int16_t ret;
	byte slave_;

	slave_ = currentSlave[flow_];
	byte port_num = S_PORT[flow_];
	int16_t replyBufferX[40];
	byte const maxRegs_ = 10;

	switch (serialFlag[flow_])
	{

		case taskComplete:
		
		if (currentCycle[flow_] == 0)
		{
			fullMBPackageNumber[flow_] = reg_count * 0.1;
			lastMBPackageLength[flow_] = reg_count - fullMBPackageNumber[flow_] * maxRegs_;
			
			if (fullMBPackageNumber[flow_])
			reg_count_[flow_] = 10;
			else
			reg_count_[flow_] = reg_count;

			start_addr_[flow_] = start_addr;
			addrShift_[flow_] = 0;
			
			if (debugMB03)
			{
				SerialUSB.print("fullMBPackageNumber -> ");
				SerialUSB.println(fullMBPackageNumber[flow_]);
				SerialUSB.print("lastMBPackageLength -> ");
				SerialUSB.println(lastMBPackageLength[flow_]);
				
				SerialUSB.print(start_addr_[flow_]);
				SerialUSB.print("  ");
				SerialUSB.print(addrShift_[flow_]);
				SerialUSB.print(" ");
				SerialUSB.print(reg_count_[flow_]);
				SerialUSB.println();
			}
		}
		else
		{
			if (currentCycle[flow_] != fullMBPackageNumber[flow_])   // Check if current cycle is a full 25 regs send or last regs send
			{
				start_addr_[flow_] = start_addr_[flow_] + maxRegs_;    // if current send cycle == 0 -> start addr = start addr
			}
			else
			{
				start_addr_[flow_] = start_addr_[flow_] + maxRegs_;
				reg_count_[flow_] = lastMBPackageLength[flow_];
				
			}
			addrShift_[flow_] = start_addr_[flow_] - start_addr;
			
			
			if (debugMB03)
			{
				SerialUSB.print(start_addr_[flow_]);
				SerialUSB.print("  ");
				SerialUSB.print(addrShift_[flow_]);
				SerialUSB.print(" ");
				SerialUSB.print(reg_count_[flow_]);
				SerialUSB.println();
			}
		}
		

		serialFlag[flow_] = sendMsg;
		return 0;
		break;

		case sendMsg:  // Send packet
		{
			build_request_packet(slave_, function, start_addr_[flow_], reg_count_[flow_], packet[flow_]);

			if (redundantMode[flow_] == active && port_num != S_PORT[HMI])
			{
				build_request_packet(slave_, function, start_addr_[flow_], reg_count_[flow_], packet[flow_]);
				flushPort(port_num);
			}
			
			send_query(packet[flow_], REQUEST_QUERY_SIZE, port_num);

			serialFlag[flow_] = sendingMsg;
			waitTillSent[flow_] = currentTime;
			packet_size[flow_] = reg_count_[flow_];
			return 0;  // do nothing
		}
		break;



		case sendingMsg: // wait till packet left Serial buffer

		if ((unsigned)(currentTime - waitTillSent[flow_]) > (unsigned)(2 * REQUEST_QUERY_SIZE))
		{
			// SerialUSB.println(REQUEST_QUERY_SIZE);
			// SerialUSB.println(packet_size[flow_]);
			serialFlag[flow_] = receiveMsg;   //
			startTimeOut[flow_] = currentTime;

		}
		return 0;  // do nothing

		break;



		case receiveMsg: // wait till packet left Serial buffer
		{
			switch (receive_incoming_data_read(flow_, replyBufferX))   // Прокерка ответа - 0 - входной буффер пуст, сообщения нет.Проверяем пока не вышло время тайм аута, >0 - овтет получен, <0 - ошибка
			{
				case 0:                                                                           // Входной буфер пуст
				if (currentTime - startTimeOut[flow_] > WaitResponceGauge)                             // Входной буфер пуст - проверяем. как долго ожидался ответ?
				{
					// 					if (redundantMode[flow_] == active && flow_ != S_PORT[HMI])
					// 					{
					// 						if (debugMB03)
					// 						SerialUSB.println("Slave not responded, Check loop");
					//
					// 						redundancyCheck(flow_);
					//
					// 						if (redundantFlag[flow_] == loopFail)
					// 						{
					// 							if (debugMB03)
					// 							SerialUSB.println("Loop fault, try connect detector another way");
					//
					// 							serialFlag[flow_] = reconnectLoop;
					// 							return 0;
					// 						}
					//
					// 						if (debugMB03)
					// 						SerialUSB.println("Loop ok, detector lost");
					// 						serialFlag[flow_] = msgDone;
					// 						return 1;  // TimeOut - not responded
					//
					// 					}
					// 					SerialUSB.print("*** LCT Lost -> ");
					// 					SerialUSB.println(currentCycle[flow_]);
					
					if (debugMB03)
					SerialUSB.println("X2X lost");
					currentCycle[flow_] = fullMBPackageNumber[flow_];
					serialFlag[flow_] = msgDone;
					return 1;  // TimeOut - not responded

				}

				return 0;  // do nothing
				break;


				default: // Respond received

				serialFlag[flow_] = msgDone;

				for (byte i = 0; i < reg_count_[flow_]; i++)
				data_[i+addrShift_[flow_]] = replyBufferX[i];



				if (redundantMode[flow_] == active && port_num != S_PORT[HMI])
				{
					redundancyCheck(flow_);
					if (debugMB03)
					SerialUSB.println("Line ok, clear buffer");
				}

				if (currentCycle[flow_] == fullMBPackageNumber[flow_] )
				{
					if (debugMB03)
					SerialUSB.println("We got data");
					
					return 2;
				}
				return 0;//2;  // Respond received
				break;
				
				
			}
			
		}
		break;


		case msgDone:  // respond received, serial job done
		{
			startPollTimer[flow_] = currentTime;   // 30 - delay between polls. When poll time expired - system can send new message via RS485
			serialFlag[flow_] = pollWait;   // wait poll time before next Serial send
			return 0;
		}
		break;

		case pollWait:  // wait poll time before next Serial send

		if (currentTime - startPollTimer[flow_] > pollDelay[flow_])
		{
			serialFlag[flow_] = taskComplete;   // Serial Port Ready for new subTask
			
			if (debugMB03)
			{
				SerialUSB.print("serialFlag -> ");
				SerialUSB.print(serialFlag[flow_]);
				SerialUSB.println();
			}
			
			if (currentCycle[flow_] == fullMBPackageNumber[flow_] )
			{
				currentCycle[flow_] = 0;
				if (debugMB03)
				{
					SerialUSB.print("Finished -> ");
					SerialUSB.print(currentSlave[flow_]);
					SerialUSB.println();
				}
				return 3;
			}
			currentCycle[flow_]++;
			
			if (debugMB03)
			{
				SerialUSB.print("currentCycle ");
				SerialUSB.print(currentCycle[flow_]);
				SerialUSB.println();
			}
			
			
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




byte read_holding_registers(byte flow_, int16_t start_addr, byte count, int16_t *data_ )
{
	int function = 0x03;  /* Function: Read Holding Registers */
	int ret;
	byte slave_;

	slave_ = currentSlave[flow_];

	//  unsigned char packet[REQUEST_QUERY_SIZE + CHECKSUM_SIZE];

	byte port_num = S_PORT[flow_];
	
	switch (serialFlag[flow_])
	{

		case taskComplete:
		serialFlag[flow_] = sendMsg;
		return 0;
		break;



		case sendMsg:  // Send packet

		build_request_packet(slave_, function, start_addr, count, packet[flow_]);

		if (redundantMode[flow_] == active && port_num != S_PORT[HMI])
		{
			build_request_packet(slave_, function, start_addr, count, redundantBuffer[flow_]);
			flushPort(port_num);
		}
		if (initSerialDebug)
		{
			SerialUSB.println("Ln 3147");
		}
		

		send_query(packet[flow_], REQUEST_QUERY_SIZE, port_num);

		serialFlag[flow_] = sendingMsg;
		waitTillSent[flow_] = millis();
		packet_size[flow_] = count;
		
		if (initSerialDebug)
		{
			SerialUSB.println("Ln 3159");
		}
		
		return 0;  // do nothing

		break;



		case sendingMsg: // wait till packet left Serial buffer

		if (currentTime - waitTillSent[flow_] > (REQUEST_QUERY_SIZE + packet_size[flow_]))
		{
			serialFlag[flow_] = receiveMsg;   //
			startTimeOut[flow_] = currentTime;
		}
		return 0;  // do nothing

		break;



		case receiveMsg: // wait till packet left Serial buffer
		
		if (initSerialDebug)
		{
			SerialUSB.println("Ln 3185");
		}
		
		switch (receive_incoming_data_read(flow_, data_))   // Прокерка ответа - 0 - входной буффер пуст, сообщения нет.Проверяем пока не вышло время тайм аута, >0 - овтет получен, <0 - ошибка
		{
			case 0:                                                                           // Входной буфер пуст
			if (currentTime - startTimeOut[flow_] > WaitResponceGauge)                             // Входной буфер пуст - проверяем. как долго ожидался ответ?
			{
				if (redundantMode[flow_] == active && flow_ != S_PORT[HMI])
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
			if (initSerialDebug)
			{
				SerialUSB.println("Ln 3223");
			}

			if (redundantMode[flow_] == active && port_num != S_PORT[HMI])
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
		serialFlag[port_num] = reconnectLoop;   // try to get Connected via redundant line
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
		port_num = S_PORT[flow_] + 1;
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






byte read_Discrete_Input(byte flow_, int16_t start_addr, byte count, int16_t *data_)
{
	int16_t function = FC_READ_DISCRETE;  /* Function: Read Holding Registers */
	int16_t ret;
	
	byte slave = currentSlave[flow_];



	//  unsigned char packet[REQUEST_QUERY_SIZE + CHECKSUM_SIZE];
	byte port_num = S_PORT[flow_];

	switch (serialFlag[flow_])
	{

		case taskComplete:
		serialFlag[flow_] = sendMsg;
		return 0;
		break;



		case sendMsg:  // Send packet

		build_request_packet(slave, function, start_addr, count, packet[flow_]);

		//	flushPort(flow_);
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
		switch (receive_incoming_Discrete_read(flow_, data_))   // Прокерка ответа - 0 - входной буффер пуст, сообщения нет.Проверяем пока не вышло время тайм аута, >0 - овтет получен, <0 - ошибка
		{
			case 0:                                                                           // Входной буфер пуст
			if (currentTime - startTimeOut[flow_] > WaitResponceGauge)                             // Входной буфер пуст - проверяем. как долго ожидался ответ?
			{
				
				serialFlag[flow_] = msgDone;
				return 1;  // TimeOut - not responded

			}

			return 0;  // do nothing
			break;


			default: // Respond received

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

int read_holding_registers_Slave(unsigned char slave, uint16_t start_addr,

unsigned char reg_count, int16_t *regs)
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





/************************************************************************

check_warning




*************************************************************************/


/************************************************************************

preset_multiple_registers

Write the data from an array into the holding registers of a
slave.

*************************************************************************/


byte preset_multiple_registers(byte flow_, int16_t start_addr, int16_t reg_count,  int16_t *data_)
{
	int function = 0x10;  /* Function 16: Write Multiple Registers */
	int byte_count, i;
	int ret;
	byte byteCount = 6;
	byte port_num = S_PORT[flow_];
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



		if (currentTime - waitTillSent[flow_] > (REQUEST_QUERY_SIZE + packet_size[flow_]))
		{
			// SerialUSB.println(REQUEST_QUERY_SIZE);
			// SerialUSB.println(packet_size[flow_]);
			serialFlag[flow_] = receiveMsg;   //
			startTimeOut[flow_] = currentTime;

		}
		return 0;  // do nothing

		break;


		case receiveMsg: // wait till packet left Serial buffer
		switch (receive_incoming_data_preset(flow_, data_))   // Проверка ответа - 0 - входной буффер пуст, сообщения нет.Проверяем пока не вышло время тайм аута, >0 - овтет получен, <0 - ошибка
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
			serialFlag[flow_] = taskComplete;   // Serial Port Ready for new subTask
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



byte receive_incoming_data_read(byte flow_, int16_t* data_)
{
	int length;
	unsigned long now = millis();
	byte port_num = S_PORT[flow_];


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
		
		if (RSDebug)
		{
			SerialUSB.print("Incoming ");
			SerialUSB.println(length);
		}

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

	length = read_reg_response(flow_, data_);

	// 	SerialUSB.print("read_reg_response  ");
	// 	SerialUSB.println(length);
	

	return (length);

}




byte receive_incoming_Discrete_read(byte flow_, int16_t* data_)
{
	int length;
	unsigned long now = millis();
	byte port_num = S_PORT[flow_];


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




byte checkIncoming(byte port_num)
{
	int length;
	unsigned long now = millis();

	switch (port_num)
	{
		/*
		case 1:
		length = UARTX.available(UART1_CS);

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


byte receive_incoming_data_preset(byte flow_, int16_t* data_)
{
	byte length;
	unsigned long now = millis();
	byte packet[REQUEST_QUERY_SIZE + CHECKSUM_SIZE];

	byte port_num = S_PORT[flow_];

	switch (port_num)
	{

		case 4:
		length = Serial1.available();
		break;

		case 3:
		
		if (uartVer == sc740)
		length = UARTX.available(UART3_CS, CH_A);
		else
		length = UARTX.available(UART2_CS, CH_B);
		
		break;

		case 2:
		length =  UARTX.available(UART2_CS, CH_A);
		break;
		/*
		case 1:
		length =  UARTX.available(UART1_CS);
		break;
		*/
		case 0:
		length =  Serial.available();
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
		Interframe_Silence[port_num] = now ;
		return 0;
	}



	if (now - Interframe_Silence[port_num] < interframe_delay)
	{
		return 0;
	}



	lastBytesReceived[port_num] = 0;

	int16_t respondBuffer[100];
	length = read_reg_response(flow_, respondBuffer);


	return (length);

}


















































/*
update_mb_slave(slave_id, holding_regs_array, number_of_regs)

checks if there is any valid request from the modbus master. If there is,
performs the action requested
*/

unsigned long Nowdt = 0;
unsigned int lastBytesReceivedSlave;


int update_mb_slave(byte slave, int16_t *regsH, byte *regsC,
uint16_t regs_size, uint16_t coil_size)
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



/*

slaveX2X queue

X2X_Data_Array
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
}
*/




//
//
// void UpdateRelays()
// {
// 	//******** Проверить отключенные датчики ***********
// 	byte StatusUpd_ = 0;
//
//
// 	for (byte j = 1; j < g_x2xCount; j++)
// 	if (X2X_Data_Array[j][TypeModule] == LDO)
// 	{
// 		StatusUpd_ = X2X_Module(j);
//
//
// 		if (X2X_Data_Array[j][Data_0] != StatusUpd_)
// 		{
// 			X2X_Data_Array[j][Data_0] = StatusUpd_;
// 			slaveX2X.push(j);
// 		}
// 	}
//
// }




byte X2X_Module(byte slice_)
{
	byte DoItRelay = B00000000;


	if (manualRelayMask != 0 && slice_ == manualSlice)      // If slice in manual mode
	{
		return manualRelayMask;
	}
	

	/*
	if (pneumoData[airRelay] == state_Fault)  // Check compressed air pressure
	{
	DoItRelay = DoItRelay |systemFaultRelay [slice_];
	
	// SerialUSB.println("Low CA Pressure");
	}
	*/
	

	for (int j = 1; j <= totalSensors; j++)
	{
		switch (processedData[j * 2])
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
			
			case blockedPipe:
			DoItRelay = DoItRelay | DisconnectRelayMask [slice_];
			break;
			

			case disconnected:
			// do nothing
			break;


		}
	}





	if (BR_Fail[0] || (HMIstatus[currentSlave[HMI]] == lost))                     // If any slice fault
	{
		DoItRelay = DoItRelay | systemFaultRelay [slice_];
	}

	if (AlarmSoundOff == 1)                 // If Switch off beacon btn pressed
	{
		//	SerialUSB.print("BuzzerOffRelay -> ");
		//	SerialUSB.println(BuzzerOffRelay[slice_] );
		DoItRelay = DoItRelay & BuzzerOffRelay[slice_];
	}


	if (slice_ == 1)
	{
		if (DoItRelay & watchDogRelay)
		DoItRelay = DoItRelay & (~watchDogRelay);   // WatchDog relay is True when PLC ok. And goes False in case of fault
		else
		DoItRelay = DoItRelay | watchDogRelay;
	}


	return DoItRelay;


}

















int check_HMI_command_regs(int respond)
{
	byte mask_HMI = 0;
	int HMI_i = 1;
	bool action;


	for (mask_HMI = 00000001; mask_HMI > 0; mask_HMI <<= 1)        // iterate through bit mask
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
		while (UARTX.available(UART1_CS))
		{
		DirtInBuffer = 1;
		if (millis() - now <= BufCheckTimeOut)                             // Check for Timeout                                                                             // Data was not received - Timeout
		{
		UARTX.read(UART1_CS);
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
	for (int i = 0; i <= 98; i++)
	data_HMI[i] = clearByte_;
}



byte mbBridge()
{
	//	if (!SerialUSB)
	//	return 0;

	if (checkIncoming(7) == 1)
	{
		while (SerialUSB.available())
		{
			if (uartVer == sc740)
			UARTX.write(SerialUSB.read(), UART3_CS, CH_A);
			else
			UARTX.write(SerialUSB.read(), UART2_CS, CH_B);
		}
	}

	
	if (checkIncoming(RS1) == 1)
	{
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
	}

}




byte firmIncoming(byte port_num)
{
	int length = 0;
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








byte HMItaskerDebug = 0;



void initSerial()
{
	if (initSerialDebug)
	{
		SerialUSB.println();
		SerialUSB.println("Serial INIT *************************************");
	}
	//	if (SerialUSB_Ok || HMItaskerDebug)  // Activate USB -> Service text mode
	SerialUSB.begin(115200, SERIAL_8N1);            // Serial USB

	Serial.begin(9600, SERIAL_8N1);   // X2X line
	
	if (uartVer==sc740)
	UARTX.begin(9600, 0, UART1_CS, CH_A);   // PC - RS485 output
	
	UARTX.begin(19200, 0, UART2_CS, CH_A);  // HMI line
	
	if (initSerialDebug)
	{
		SerialUSB.println("HMI init");
	}

	// Load Parity Config
	// Load Parity Config
	loadConfig("Parity.txt", outArray);

	// added 752 port
	if (uartVer == sc740)
	UARTX.begin(9600, outArray[1], UART3_CS, CH_A); // S1
	else
	UARTX.begin(9600, outArray[1], UART2_CS, CH_B); // S1 in 752 NXP


	if (initSerialDebug)
	{
		SerialUSB.println("S1 init");
	}

	switch (outArray[2])   // Serial 2
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


	switch (outArray[3])  // Serial 3
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

	switch (outArray[4])  // Serial 4
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
	
	if (initSerialDebug)
	{
		SerialUSB.println();
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
	pinMode(A9, OUTPUT);

	digitalWrite(X2X_LED, LOW);
	digitalWrite(UART1_CS, HIGH);
	digitalWrite(UART2_CS, HIGH);
	digitalWrite(UART3_CS, HIGH);
	digitalWrite(microSD_CS, HIGH);
	digitalWrite(ETH1_CS, HIGH);
	digitalWrite(ETH2_CS, HIGH);
	digitalWrite(microSD_CS, HIGH);

	digitalWrite(X2X_Transmit, LOW);
	digitalWrite(A7, LOW);
	digitalWrite(A9, LOW);

	pinMode(SSerialTxControl_0, OUTPUT);             // Initialize receive-transmit enable PIN
	pinMode(SSerialTxControl_1, OUTPUT);             // Initialize receive-transmit enable PIN
	pinMode(SSerialTxControl_2, OUTPUT);             // Initialize receive-transmit enable PIN
	pinMode(SSerialTxControl_3, OUTPUT);             // Initialize receive-transmit enable PIN

	digitalWrite(SSerialTxControl_0, RS485Receive);   // Set default state of Enable Pin
	digitalWrite(SSerialTxControl_1, RS485Receive);   // Set default state of Enable Pin
	digitalWrite(SSerialTxControl_2, RS485Receive);   // Set default state of Enable Pin
	digitalWrite(SSerialTxControl_3, RS485Receive);   // Set default state of Enable Pin

	delay(300);
	digitalWrite(SSerialTxControl_0, RS485Transmit);   // Set default state of Enable Pin
	delay(300);
	digitalWrite(SSerialTxControl_1, RS485Transmit);   // Set default state of Enable Pin
	delay(300);
	digitalWrite(SSerialTxControl_2, RS485Transmit);   // Set default state of Enable Pin
	delay(300);
	digitalWrite(SSerialTxControl_3, RS485Transmit);   // Set default state of Enable Pin
	delay(300);

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


	// 	loadConfig("COT1.txt", outArray);
	// 	sporadicEn_1 = outArray[1];
	sporadicEn_1 = 0;

	// 	loadConfig("COT2.txt", outArray);
	// 	sporadicEn_2 = outArray[1];
	sporadicEn_2 = 0;

	if (Ethernet1Mode == TCPIP) // Modbus TCP Connected
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

	if (Ethernet2Mode == TCPIP) // Modbus TCP Connected
	{
		Ethernet.select(ETH2_CS);
		mb2.config(mac, ip);  //Config Modbus IP
	}
	else
	{
		Ethernet.select(ETH2_CS);
		Ethernet.begin(mac, ip, gateway, subnet);
	}


	// NTP added v22
	// Udp.begin(localPort);

}



byte loadConfig(const char* fileName, int16_t* output_)
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
		SDconfigState = 0;
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

byte loadConfig(const char* fileName, uint16_t* output_)
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
		SDconfigState = 0;
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

// byte loadConfig(char* fileName, int16_t* output_)
// {
// 	byte i = 1;
// 	String sdLine;
// 	byte totalStrings_ = 255;
//
// 	digitalWrite(SD_OK, LOW);
//
// 	// 	if (SD.exists(fileName))
// 	// 	SD.remove(fileName);
// 	// 	else return 0;
//
// 	if (!SD.exists(fileName))
// 	return 0;
//
// 	File file_ = SD.open(fileName);
//
//
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
// 			SerialUSB.println(" -> файл ");
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
// 	output_[0] = totalStrings_;
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
// 		output_[i] = file_.parseInt();
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
// 	return (1);
// }



byte saveConfig(char* fileName, int16_t* input_, byte size_)
{
	String sdLine;


	if (SD.exists(fileName))
	{
		SD.remove(fileName);
	}

	//	else return (0);


	File file_ = SD.open(fileName, FILE_WRITE);


	if (!file_)
	{
		if (sdDebug)
		{
			//SerialUSB.print(fileName);
			SerialUSB.println(" -> file cannot be opened");
		}
		return (0);
	}

	if (sdDebug)
	{
		SerialUSB.print(fileName);
		SerialUSB.println(" -> Saving ....");
	}

	digitalWrite(SD_OK, HIGH);
	
	file_.println(size_);            // First line - total number of strings

	for (byte i = 1; i <= size_; i++)
	{
		dataString = String(input_[i]);
		file_.println(dataString);
	}

	file_.println("Fin");   // Last String - "Fin"
	
	digitalWrite(SD_OK, LOW);

	file_.close();

	if (sdDebug)
	SerialUSB.println(" -> Saved");

	

	return (1);
}













byte regsDetNoRespond(byte flow_)
{

	byte port_num = S_PORT[flow_];
	serialFlag[flow_] = msgDone;

	byte currentSensor = currentSlave[flow_];

	//if (flow_ != RS1)
	if (lostPackets[currentSensor] < LostPacketError)
	{
		lostPackets[currentSensor]++;
		return (0);
	}

	startPollTimer[flow_] = millis();

	gaugeData[currentSensor * 2 - 1] = hide;                // HMI: Датчик не ответил - надпись на поле датчика "не подключен"- код " - 1"
	gaugeData[currentSensor * 2] = lost;                     // HMI: Датчик не ответил - цвет поля красный - код 0                                                                    // HMI: Датчик не ответил - цвет общего фона красный - код B00000001
	gaugeErrorCode[currentSensor] = 0;

	DM910Data[currentSensor][detState] = TRUE;



	// SerialUSB.println("detector lost");
	//   if (Ethernet1Mode == TCPIP)
	//     mb1.Ireg(currentSensor-1, 0);

	// 	for (byte i = 0; i < 10; i++)
	// 	X2X_Data_Array[currentSlave[X2X]][i + Data_0];



	//  IEC104data_M_SP[2 * currentSensor - 1] = 0;
	//  IEC104data_M_SP[2 * currentSensor] = lost;



	if (logDisconnect[currentSensor] == 0)
	{
		logDisconnect[currentSensor] = 1;
		sdString = "Газоанализатор ->";
		sdString += currentSensor;
		sdString += " не отвечает";
		printData();
		HMIqueue.push(updateDetectorStatus);
	}

}



byte regsDetResponded(byte flow_, int *data_)
{

	byte currentSensor = currentSlave[flow_];
	byte port_num = S_PORT[flow_];

	serialFlag[flow_] = msgDone;
	startPollTimer[flow_] = millis();   // Записать текущее время и засечь интервал
	lostPackets[currentSensor] = 0;
	gaugeData[currentSensor * 2 - 1] = data_[0]; // Показания датчика
	gaugeErrorCode[currentSensor] = data_[1]; // Ошибка датчика


	DM910Data[currentSensor][detState] = FALSE;  // Ответил
	DM910Data[currentSensor][detError] = data_[1];
	DM910Data[currentSensor][gasConcentration] = data_[0]; // Показания датчика
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



	if (gaugeErrorCode[currentSensor] != 0)
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



	if (logError[currentSensor] == 1 &&  gaugeErrorCode[currentSensor] == 0)
	{
		logError[currentSensor] = 0;
		sdString = "Газоанализатор ->";
		sdString += currentSensor;
		sdString += ". Все ошибки сняты ";
		sdString += data_[1];
		
	}



	if (data_[0] < warningSetpoint[currentSensor] && data_[1] == 0)                 // Показания не превышают первый порог?
	{
		ScreenState[currentSensor] = colorGrey;                                               // Массив цветов панели - для текущего датчика цвет ЗЕЛЕНЫЙ
		gaugeData[currentSensor * 2] = ok;                                        // Цвет поля датчика на панели ЗЕЛЕНЫЙ, КОД 2

		DM910Data[currentSensor][gasWarning] = FALSE; // Порог предупреждения не превышен
		DM910Data[currentSensor][gasAlarm] = FALSE; // Порог аварии не превышен
		//  IEC104data_M_SP[2 * currentSensor] = ok;



		if (logYellow[currentSensor] == 1)
		{
			logYellow[currentSensor] = 0;
			sdString = "Газоанализатор -> ";
			sdString += currentSensor;
			sdString += " < ";
			sdString += warningSetpoint[currentSensor];
			sdString += ". Концентрация ниже первого порога";

			printData();
		}
	}


	if (data_[0] >= alarmSetpoint[currentSensor] && data_[1] == 0)          // Проверка на превышение второго порога  Alarm
	{
		gaugeData[currentSensor * 2] = alarm;                                       // Превышен второй порог, фон датчика красный

		DM910Data[currentSensor][gasWarning] = FALSE; // Порог предупреждения не превышен
		DM910Data[currentSensor][gasAlarm] = TRUE; // Порог аварии превышен


		//  IEC104data_M_SP[2 * currentSensor] = alarm;



		if (logRed[currentSensor] == 0)
		{
			logRed[currentSensor] = 1;
			sdString = "Газоанализатор -> ";
			sdString += currentSensor;
			sdString += " > ";
			sdString += alarmSetpoint[currentSensor];
			sdString += ". Авария, превышен второй порог.";

			printData();
		}

	}



	if (data_[0] >= warningSetpoint[currentSensor] &&  data_[0] < alarmSetpoint[currentSensor] && data_[1] == 0 && gaugeData[currentSensor * 2] != alarm)                   // Если показания превысили первый порог но не привысели второй - ЖЕЛТАЯ ЗОНА
	{
		gaugeData[currentSensor * 2] = warning;                                        // Превышен первый порог, фон датчика желтый

		DM910Data[currentSensor][gasWarning] = TRUE; // Порог предупреждения превышен
		DM910Data[currentSensor][gasAlarm] = FALSE; // Порог аварии не превышен

		//  IEC104data_M_SP[2 * currentSensor] = warning;


		if (logYellow[currentSensor] == 0)
		{
			logYellow[currentSensor] = 1;
			sdString = "Газоанализатор ->";
			sdString += currentSensor;
			sdString += " > ";
			sdString += warningSetpoint[currentSensor];
			sdString += ". Превышен первый порог.";

			printData();
		}

		if (logRed[currentSensor] == 1)
		{
			logRed[currentSensor] = 0;
			sdString = "Газоанализатор ->";
			sdString += currentSensor;
			sdString += " < ";
			sdString += alarmSetpoint[currentSensor];
			sdString += ".  Концентрация упала ниже второго порога";

			printData();
		}

	}
	
	// 	SerialUSB.print(DM910Data[currentSensor][gasWarning]);
	// 	SerialUSB.print(" ");
	// 	SerialUSB.print(DM910Data[currentSensor][gasAlarm]);
	// 	SerialUSB.println(" ");
	//
	// gaugeData[currentSlave_1 * 2] = gaugeData[currentSlave_1 * 2]; //| (SensorStatusByte[currentSlave_1] << 2);
}






void regsDisabled(byte currentSensor)
{
	gaugeData[currentSensor * 2 - 1] = hide;
	gaugeData[currentSensor * 2] = ok;                                        // Цвет поля датчика на панели ЗЕЛЕНЫЙ, КОД 2

	processedData[currentSensor * 2 - 1] = hide;
	processedData[currentSensor * 2] = ok;                                        // Цвет поля датчика на панели ЗЕЛЕНЫЙ, КОД 2
	HMIqueue.push(updateFrontPage);
	
	if (currentSensor < 13)
	pneumoData[startVlvEnum + currentSensor * 2 + 1] = state_Ok;

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






//
//
// void updateColor()
// {
//
// 	byte color = B00000000;
// 	byte sensFlt = 0;
//
// 	color = gaugeColor();  // Get common gauge color
//
// 	color = color | systemColor();  // Get common gauge color
//
// 	//	if (sensorState == fault || sensorState == lost)
// 	//	sensFlt = HMIRedScreen;
//
// 	processedData[0] = color;// | sensFlt;           // Common system status screen
//
// 	//	SerialUSB.print("gaugeData[0] -> ");
// 	//	SerialUSB.println(gaugeData[0]);
//
//
// }
//




/*
int calcMEsize()
{
// START ************** Measured data calculations

// 1. Lorentz X2X Modules Single Point
//  Slices (with ME data type output)
if (calcDebug)
SerialUSB.println("ME UserSize -> ");


int j = 0;  // Start from beginning

for (byte i = 1; i <= g_x2xCount; i++) // save Main room detectors data
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

// 			j = j + X2X_Data_Array[i][DataQnt]; //j = j+35; //Изменено! 10 резерва
// 			if (calcDebug)
// 			{
// 				SerialUSB.print("LCT -> ");
// 				SerialUSB.println(j);
// 			}


break;
}
}


// Detectors

for (byte i = 1; i <= totalSensors; i++)			//
switch(gasType[i])
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


}
if (calcDebug)
{
SerialUSB.print("Total ME Size-> ");
SerialUSB.println(j);
}
return j;

}


int calcTFsize()
{

if (calcDebug)
SerialUSB.println("TF UserSize -> ");

int j = 0;  // Start from beginning

for (byte i = 1; i <= g_x2xCount; i++) // save Main room detectors data
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

int calcSPsize()
{

if (calcDebug)
SerialUSB.println("Calc SP UserSize -> ");

int j = 0;		// SP

// START ************** Single point data calculations

// 1. Lorentz X2X Modules Single Point
// LCP no data at the moment
j++;
// Another Slices

if (calcDebug)
{
SerialUSB.print("PLC -> ");
SerialUSB.println(j);
}

for (byte i = 1; i <= g_x2xCount; i++) // save Main room detectors data
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


// Detectors
for (byte i = 1; i <= totalSensors; i++)
switch(gasType[i])
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


}
if (calcDebug)
{
SerialUSB.print("Total SP Size -> ");
SerialUSB.println(j);
}
return j;
}

*/

void checkSP()
{



}


void moveSP(int newValue_)
{

	/*
	IEC104spont_M_SP[dataChng_M_SP_] = 0;

	// 		SerialUSB.print(i);
	// 		SerialUSB.print(" changeBuffer[i] -> ");
	// 		SerialUSB.print(changeBuffer[i]);
	// 		SerialUSB.print(" IEC104data_M_SP[i] -> ");
	// 		SerialUSB.println(IEC104data_M_SP[i]);

	if (IEC104data_M_SP[i] != changeBuffer[i])
	{
	IEC104spont_M_SP[dataChng_M_SP_] = i;
	dataChng_M_SP_++;
	}

	IEC104data_M_SP[i] = changeBuffer[i];
	changeBuffer[i] = 0;
	*/
}



//
// void checkME(int16_t index_, int16_t currentMe_, byte qds_)  // отслеживание изменений величины для отправки спорадически
// {
// 	byte change = 0;
// 	//	IEC104spont_M_ME[dataChng_M_ME_] = 0;
//
// 	//	if (IEC104data_M_ME[index_] != currentMe_)  // Если изменилось значение сигнала - то
// 	if (dataDifferME(IEC104data_M_ME[index_],currentMe_))
// 	{
// 		// IEC104.spontaneousSend = spontaneosActivate;
// 		IEC104data_M_ME[index_] = currentMe_;
// 		change = 1;
//
// 	}
//
// 	if (IEC104_M_ME_QDS[index_] != qds_)  // Если изменилось качество сигнала - то
// 	{
// 		IEC104_M_ME_QDS[index_] = qds_;
// 		change = 1;
// 	}
//
// 	if (change)
// 	{
// 		IEC104spont_M_ME[dataChng_M_ME_] = index_;
// 		dataChng_M_ME_++;
// 	}
//
//
// }


//
// void checkTF(uint16_t index_, float currentMe_, byte qds_)  // отслеживание изменений величины для отправки спорадически
// {
// 	byte change = 0;
// 	//	IEC104spont_M_ME[dataChng_M_ME_] = 0;
//
// 	//	if (IEC104data_M_TF[index_] != currentMe_)  // Если изменилось значение сигнала - то
//
// 	if (dataDifferTF(IEC104data_M_TF[index_], currentMe_))
// 	{
// 		// IEC104.spontaneousSend = spontaneosActivate;
// 		IEC104data_M_TF[index_] = currentMe_;
// 		change = 1;
// 	}
//
// 	if (IEC104_M_TF_QDS[index_] != qds_)  // Если изменилось качество сигнала - то
// 	{
// 		IEC104_M_TF_QDS[index_] = qds_;
// 		change = 1;
// 	}
//
// 	if (change)
// 	{
// 		IEC104spont_M_TF[dataChng_M_TF_] = index_;
// 		dataChng_M_TF_++;
// 	}
//
// }




void checkAndMove(int *changeBuffer_, int register_, int newValue_)
{
	changeBuffer_[register_] = newValue_;
}


uint8_t dataDifferME(int16_t prev_, int16_t now_)
{
	if (now_ == 0 && prev_ != 0)  // деление на ноль и не равно старому значению - обновляем
	{
		return 1;
	}


	if (now_ == 0 && prev_ == 0) // оба нуля - не  обновляем
	{
		return 0;
	}

	if (now_ == prev_)			// значения равны - не обновляем
	return 0;

	float buffer = 1.0*(prev_ - now_)/now_;

	buffer = abs(buffer);

	// 	SerialUSB.print(prev_);
	// 	SerialUSB.print(" ");
	// 	SerialUSB.print(now_);
	// 	SerialUSB.print(" ");
	// 	SerialUSB.print(buffer);

	if (buffer > 0.002)
	{
		//	SerialUSB.println(" change! ");
		return 1;
	}
	else
	{
		//SerialUSB.println(" not change ");
		return 0;
	}
}

uint8_t dataDifferTF(float prev_, float now_)
{
	if (abs((prev_ - now_)/now_) > 0.002)
	return 1;
	else return 0;

}




byte gaugeColor()
{
	byte color = B00000000;
	byte pipeColor = B00000000;


	byte updateChanges = 0;
	// Check detector data

	for (int j = 1; j <= 12; j++)
	{
		pipeColor = B00000000;
		switch (gaugeData[j * 2])
		{

			case alarm:
			color = color | HMIRedScreen | HMIRedAlarm;
			pipeColor = color | HMIRedScreen;


			break;

			case warning:
			color = color | HMIYellowScreen | HMIYellowAlarm;

			break;

			case ok:
			color = color | colorGrey;


			break;

			case lost:
			color = color | HMIRedScreen;
			pipeColor = color | HMIRedScreen;

			break;

			case fault:
			color = color | HMIRedScreen;
			pipeColor = color | HMIRedScreen;

			break;

			case disconnected:
			color = color | colorGrey;

			break;


			default:
			color = color | colorGrey;

			break;
		}

		// Added pipe blocked code


		if (processedData[j * 2] != gaugeData[j * 2])
		updateChanges = 1;

		if (processedData[j * 2-1] != gaugeData[j * 2-1])
		updateChanges = 1;

		processedData[j * 2] = gaugeData[j * 2];
		processedData[j * 2-1] = gaugeData[j * 2-1];


		if (pneumoData[startVlvEnum + j * 2 + 1] == state_Fault)  // Pipe blocked - red screen
		{

			if ((pipeColor & HMIRedScreen) == 0)  // Red screen not detected
			{
				processedData[j * 2] = blockedPipe;					// Alarm status
				color = color | HMIRedScreen;					// HMI Red screen
			}
		}
		/*
		SerialUSB.print(j);
		SerialUSB.print(" -> ");
		SerialUSB.print(processedData[j * 2]);
		SerialUSB.print(" -> ");
		SerialUSB.println(processedData[j * 2 - 1]);
		*/
	}

	for (int j = 13; j <= totalSensors; j++)
	{

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
			color = color | colorGrey;

			break;
		}

		// Added pipe blocked code
		if (processedData[j * 2] != gaugeData[j * 2])
		updateChanges = 1;

		if (processedData[j * 2-1] != gaugeData[j * 2-1])
		updateChanges = 1;

		processedData[j * 2] = gaugeData[j * 2];
		processedData[j * 2-1] = gaugeData[j * 2-1];
	}

	//SerialUSB.println("***************************");

	if (updateChanges)
	HMIqueue.push(updateFrontPage);
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
//
// byte systemColor()
// {
// 	byte color = B00000000;
//
// 	if (pneumoData[airRelay] == state_Fault)  // Check compressed air pressure
// 	{
// 		color = color | HMIRedScreen;
//
// 		// SerialUSB.println("Low CA Pressure");
// 	}
//
//
// 	BR_Fail[0] = 0;
//
// 	for (byte i = 1; i < g_x2xCount; i++)        // Check slice Status
// 	if (X2X_Data_Array[i][Status] == TRUE)
// 	{
// 		//SerialUSB.print("BR fail -> ");
// 		//SerialUSB.println(i);
// 		BR_Fail[0] = BR_Fail[0] | BR_Fail_Mask[i];
// 		color = color | HMIRedScreen | sysFault;
// 		//SerialUSB.print("color -> ");
// 		//SerialUSB.println(color);
// 	}
//
// 	if (redundantFlag[S_PORT[RS1]] == loopFail)         // Check Loop status
// 	{
// 		BR_Fail[0] = BR_Fail[0] | BR_Fail_Mask[7];
// 		color = color | HMIRedScreen | lostConnect;
// 	}
//
// 	if (redundantFlag[S_PORT[RS2]] == loopFail)         // Check Loop status
// 	{
// 		BR_Fail[0] = BR_Fail[0] | BR_Fail_Mask[8];
// 		color = color | HMIRedScreen | lostConnect;
// 	}
// 	//  SerialUSB.print("Color -> ");
// 	//  SerialUSB.println(BR_Fail[0]);
// 	return color;
//
//
// }




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
		// 		dataFile.print(" ");
		// 		dataFile.print(daynames[rtc_clock.get_day_of_week() - 1]);
		// 		dataFile.print(": ");
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


void nextSlave(byte flow_)
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




byte getPortNum(byte bridgeByte_)
{

	if (lowLim[RS1] <= bridgeByte_ && bridgeByte_ <= hiLim[RS1])
	return S_PORT[RS1];


	if (lowLim[RS2] <= bridgeByte_ && bridgeByte_ <= hiLim[RS2])
	return S_PORT[RS2];

	if (lowLim[RS3] <= bridgeByte_ && bridgeByte_ <= hiLim[RS3])
	return S_PORT[RS3];

	if (lowLim[RS4] <= bridgeByte_)
	return S_PORT[RS4];

}



byte nextTask()
{
	currentTask++;

	if (currentTask > totalTasks)
	currentTask = 3;
}




byte what_to_do()
{
	byte command_;
	//	SerialUSB.println("what_to_do **** ");

	manualRelayMaskBuffer = data_HMI[2];
	manualSliceBuffer = data_HMI[4];



	for (byte i = 0; i <= 1; i++)
	if (command_ = check_HMI_command_regs(data_HMI[i]))
	{
		return (command_ + 8 * i);
	}


	// SerialUSB.println("Nothing todo ***");
	return 0;
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

		//	SerialUSB.print("nextCommand -> ");
		//	SerialUSB.println(nextCommand);
		HMITask = nextCommand + updateButtons; // updateButtons - Starting position


		if (nextCommand == 0)
		{
			//SerialUSB.println("Finished Task");
			btnState = updated;
			taskFlag[HMI] = taskComplete; // No action needed.
		}


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
			//  SerialUSB.println(BR_Position);

			clearHMIdata(0);
			break;

			case 3:

			data_HMI[0] = warningRelayMask[BR_Position][sensorPosition]; //EEPROM.read(flashOffset +750+g_x2xCount*(j-1)+i);      // маска второго порога реле
			data_HMI[1] = alarmRelayMask[BR_Position][sensorPosition]; //EEPROM.read(flashOffset +750+g_x2xCount*(j-1)+i);      // маска второго порога реле
			data_HMI[2] = warningSetpoint[sensorPosition];
			data_HMI[3] = alarmSetpoint[sensorPosition];

			preset_multiple_registers(HMI, 803, 6, data_HMI);
			subsubTask[HMI] = 1;
			break;
		}
		break;

		case 1:
		if (preset_multiple_registers(HMI, 803, 6, data_HMI) == portReady)
		{
			HMITask = updateResetBtns; // Reset comand buttons
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
	{
		//SerialUSB.println("All btns Reset");
		taskFlag[HMI] = taskComplete;
		btnState = updated;
	}
}



void resetManualRelay()
{
	SerialUSB.println("Reset Relay");
	manualRelayMask = 0; // Test relay
	HMITask = updateResetBtns;
}


void setManualRelay()
{
	SerialUSB.print("Set Relay - > ");
	manualSlice = manualSliceBuffer;
	SerialUSB.print(manualSlice);
	SerialUSB.print(" - > ");
	manualRelayMask = manualRelayMaskBuffer; // Test relay
	SerialUSB.println(manualRelayMask);


	HMITask = updateResetBtns;
}


void toolBox()
{
	serviceMode = data_HMI[3] & B0000001;  // ToolBox radioButton Active
	HMITask = 20;

	/*
	if (serviceMode == 1)
	{
	SerialUSB.begin(9600, SERIAL_8N1);     // Serial USB
	}
	else
	SerialUSB.end();     // Serial USB
	*/

}





void outProtocol()
{

	if (data_HMI[5] & B0000010)  // Activate IEC104
	Ethernet1Mode = IEC104mode;
	else
	Ethernet1Mode = TCPIP;

	outArray[1] = Ethernet1Mode;
	saveConfig("DCP.txt", outArray, 1);

	softReset();

	HMITask = updateResetBtns;


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

		warningSetpoint[sensorPosition] = data_HMI[4];
		alarmSetpoint[sensorPosition] = data_HMI[5];


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
		saveConfig("Yellow.txt", warningSetpoint, totalSensors);
		saveConfig("Red.txt", alarmSetpoint, totalSensors);

		break;

		case 3:
		HMITask = updateResetBtns;
		break;

	}
}


void makeGasNameList()
{
	clearHMIdata(-1);    // -1 - empty space, 0 - disconnected, 1 ... 100 - Gas name
	for (int i = 1; i <= totalSensors; i++)
	{
		if (enabled[i] == 0)
		data_HMI[i] = 0;
		else
		data_HMI[i] = gasType[i];
	}
	data_HMI[0] = 0;
}


void gasTypesToHMI()
{
	if (serialFlag[HMI] == taskComplete)
	makeGasNameList(); // -1 - empty space, 0 - disconnected, 1 ... 100 - Gas name

	if (preset_multiple_registers(HMI, 1100, 40, data_HMI ) == portReady) // Preset registers
	{
		HMITask = updateResetBtns;
		HMIqueue.push(updateFrontPage);
		//	SerialUSB.println("****  Push gasTypesToHMI ******* ");
	}



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
		HMITask = updateResetBtns;
		//	SerialUSB.println("****  Push enableDisableDetector ******* ");
		HMIqueue.push(updateFrontPage);
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
		data_HMI[i] = B00000001 << (S_PORT[i + 3] - 3);
	}


	if (preset_multiple_registers(HMI, 791, 4, data_HMI) == portReady)      // Preset registers
	HMITask = updateResetBtns;
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
			S_PORT[i + 3] = 3;
			break;

			case 2:
			S_PORT[i + 3] = 4;
			break;

			case 4:
			S_PORT[i + 3] = 5;
			break;

			case 8:
			S_PORT[i + 3] = 6;
			break;
		}

		//    SerialUSB.println(port_S1);

		for (byte i = 1; i <= 4; i++)
		data_HMI[i] = S_PORT[i + 2];


		saveConfig("EIA.txt", data_HMI, EIA_Lines);
		break;

		case 3:
		HMITask = updateResetBtns;
		break;
	}
}



byte switchOffSound()
{
	if (AlarmSoundOff == 0)
	{
		AlarmSoundOff = 1;

		//	soundTimer = currentTime;
	}
	else
	{
		AlarmSoundOff = 0;
	}

	HMITask = updateResetBtns;
	// UpdateRelays();
	// slaveX2X.push(2);
	// HMIqueue.push(updateResetBeacon);

}


byte const setData = 1;
byte const getData = 2;



/*
X2X_Data_Array
//slaveX2X
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
*/


/*
case LDO:
{
if (newASDU_Debug)
{
SerialUSB.print("Add LDO -> ");
SerialUSB.println(asduTail);
}

LDO1118* device = new LDO1118;
device->ID = selection;
device->deviceStatus = 0;
system_Devices[asduTail++] = (void*)device;
break;
}
*/

unsigned long x2xDebTimer = 0;
byte X2X_mb()
{
	normalizeX2XSlave();

	DeviceHeader* hdr = getX2XBySlave(currentSlave[X2X]);
	if (!hdr) {
		nextX2X();
		taskFlag[X2X] = standBy;
		return 0;
	}

	switch (hdr->ID)
	{
		case LDO:   X2X_LDO();    break;
		case LAI:   X2X_LAI();    break;
		case LDI8:  X2X_LDI_8();  break;
		case LDI16: X2X_LDI_16(); break;
		case LCT:   X2X_LCT();    break;
		case LCT_2: X2X_LCT_2();  break;

		default:
		nextX2X();
		taskFlag[X2X] = standBy;
		break;
	}

	return hdr->ID;
}



// byte X2X_mb()
// {
//
// 	DeviceHeader* hdr = getX2XBySlave(currentSlave[X2X]);
// 	if (!hdr) {
// 		nextX2X();
// 		taskFlag[X2X] = standBy;
// 		return 0;
// 	}
// 	// получаем указатель на заголовок текущего устройства
// 	auto* hdr = static_cast<DeviceHeader*>(system_Devices[currentSlave[X2X]]);
//
// 	switch (hdr->ID)
// 	{
// 		case LDO:
// 		X2X_LDO();
// 		break;
//
// 		case LAI:
// 		X2X_LAI();
// 		break;
//
// 		case LDI8:
// 		X2X_LDI_8();
// 		break;
//
// 		case LDI16:
// 		X2X_LDI_16();
// 		break;
//
// 		case LCT:
// 		X2X_LCT();
// 		break;
//
// 		case LCT_2:
// 		X2X_LCT_2();
// 		break;
//
// 		default:
// 		nextX2X();
// 		taskFlag[X2X] = standBy;
// 		break;
// 	}
//
// 	return hdr->ID; // или другой статус, если нужно
// }

// byte X2X_mb()
// {
// 	switch (((DeviceCommon*)system_Devices[currentSlave[X2X]])->ID)
// 	{
// 		case LDO:
// 		X2X_LDO();
//
// 		break;
//
// 		case LAI:
// 		X2X_LAI();
//
// 		break;
//
// 		case LDI8:
// 		X2X_LDI_8();
// 		break;
//
// 		case LDI16:
// 		X2X_LDI_16();
// 		break;
//
// 		case LCT:
// 		X2X_LCT();
// 		break;
//
// 		case LCT_2:
// 		X2X_LCT_2();
// 		break;
//
// 		default:
// 		nextX2X();
// 		taskFlag[X2X] = standBy;
//
// 		break;
//
// 	}
// }


byte X2X_LDI_8()
{
	normalizeX2XSlave();
	const uint8_t slave = currentSlave[X2X];

	DeviceHeader* hdr = getX2XBySlave(slave);
	if (!hdr || hdr->ID != LDI8) {
		nextX2X();
		taskFlag[X2X] = standBy;
		return 0;
	}

	auto* dev = static_cast<LDI1118*>(hdr);

	// читаем 1 регистр DI bitmap
	switch (read_holding_registers(X2X, 0, 1, data_X2X))
	{
		case 1: // timeout
		{
			brLost();
			checkConnection(dev, TRUE);
		}
		break;

		case 2: // received
		{
			checkConnection(dev, FALSE);

			// bitmap входов
			dev->DigitalInput = (uint32_t)(uint16_t)data_X2X[0];

			brOk();
		}
		break;

		case 3: // ready for next
		{
			nextX2X();
		}
		break;

		default:
		break;
	}

	return LDI8;
}


byte X2X_LDI_16()
{
	normalizeX2XSlave();
	const uint8_t slave = currentSlave[X2X];

	DeviceHeader* hdr = getX2XBySlave(slave);
	if (!hdr || hdr->ID != LDI16) {
		nextX2X();
		taskFlag[X2X] = standBy;
		return 0;
	}

	auto* dev = static_cast<LDI1116*>(hdr);

	// читаем 1 регистр DI bitmap
	switch (read_holding_registers(X2X, 0, 1, data_X2X))
	{
		case 1: // timeout
		{
			brLost();
			checkConnection(dev, TRUE);
		}
		break;

		case 2: // received
		{
			checkConnection(dev, FALSE);

			dev->DigitalInput = (uint32_t)(uint16_t)data_X2X[0];

			brOk();
		}
		break;

		case 3: // ready for next
		{
			nextX2X();
		}
		break;

		default:
		break;
	}

	return LDI16;
}


byte X2X_LAI()
{
	normalizeX2XSlave();
	const uint8_t slave = currentSlave[X2X];

	DeviceHeader* hdr = getX2XBySlave(slave);
	if (!hdr || hdr->ID != LAI) {
		nextX2X();
		taskFlag[X2X] = standBy;
		return 0;
	}

	auto* dev = static_cast<LAI1118*>(hdr);

	switch (read_holding_registersX(X2X, 0, 17, data_X2X))
	{
		case 1: // Timeout respond
		{
			brLost();
			checkConnection(dev, TRUE);
		}
		break;

		case 2: // Receive respond
		{
			checkConnection(dev, FALSE);

			// DI
			dev->DigitalInput = (uint16_t)data_X2X[0];

			// TF floats from reg 1
			uint8_t Modbus_Floats_Addr = 1;
			X2X_to_Float(dev, data_X2X, Modbus_Floats_Addr);

			brOk();
		}
		break;

		case 3: // Serial actions finished / ready for new task
		{
			nextX2X();
		}
		break;

		default:
		break;
	}

	return LAI;
}


//
// byte X2X_LAI()
// {
// 	// General mode
// 	switch (read_holding_registersX(X2X, 0, 17, data_X2X))
// 	{
// 		case 1: // Timeout respond
// 		//regsNoRespond(X2X);
// 		{
// 			brLost();
// 			auto* dev = static_cast<LAI1118*>(system_Devices[currentSlave[X2X]]);
// 			checkConnection(dev, TRUE);
//
// 		}
// 		break;
//
// 		case 2: // Receive respond
// 		{
// 			// сохраняем первый регистр
// 			//	X2X_Data_Array[currentSlave[X2X]][Data_0] = data_X2X[0];
//
// 			// приводим к вашему типу
// 			auto* dev = static_cast<LAI1118*>(system_Devices[currentSlave[X2X]]);
//
// 			// 1) проверка связи
// 			checkConnection(dev, FALSE);
//
// 			// 2) цифровые входы
// 			dev->DigitalInput = uint16_t(data_X2X[0]);
//
// 			uint8_t Modbus_Floats_Addr = 1; // Start Floats register
// 			X2X_to_Float(dev,data_X2X,Modbus_Floats_Addr);
//
// 			// 4) отвечаем «OK»
// 			brOk();
// 		}
// 		break;
//
// 		case 3:  // Serial actions finished and poll time delay done. Ready for new subtask
// 		{
// 			// 			SerialUSB.print("LDI -> ");
// 			// 			SerialUSB.println(currentSlave[X2X]);
//
// 			nextX2X();
//
// 		}
// 		break;
//
// 		default:
//
// 		break;
// 	}
// }
//


uint8_t const newOscillograms = 1;
byte X2X_LCT()
{
	normalizeX2XSlave();
	const uint8_t slave = currentSlave[X2X];

	DeviceHeader* hdr = getX2XBySlave(slave);
	if (!hdr || hdr->ID != LCT) {
		nextX2X();
		subsubTask[X2X] = 0;
		taskFlag[X2X] = standBy;
		return 0;
	}

	auto* dev = static_cast<LCT1114*>(hdr);

	switch (subsubTask[X2X])
	{
		case 0: // Опрос измерений
		{
			switch (read_holding_registersX(X2X, 0, 78, data_X2X))
			{
				case 1: // Timeout
				{
					brLost();
					checkConnection(dev, TRUE);

					if (lctDebug) {
						SerialUSB.print("*** LCT 1 Lost -> ");
						SerialUSB.println(slave);
					}
				}
				break;

				case 2: // Receive
				{
					brOk();
					checkConnection(dev, FALSE);

					if (lctDebug) {
						SerialUSB.print("*** LCT 1 Ok -> ");
						SerialUSB.println(slave);
						SerialUSB.print("Connected -> ");
						SerialUSB.println(dev->ConnectLost);
					}

					// DigitalInput packing (как у тебя было)
					uint32_t di = uint32_t(data_X2X[0] & 0x3F);
					di |= (uint32_t(data_X2X[1] & 0x1FFF) << 6);
					dev->DigitalInput = di;

					// TF floats from reg 2
					uint8_t Modbus_Floats_Addr = 2;
					X2X_to_Float(dev, data_X2X, Modbus_Floats_Addr);

					if (lctDebug) {
						SerialUSB.print("Post-update ConnectLost -> ");
						SerialUSB.println(dev->ConnectLost);
					}
				}
				break;

				case 3: // Ready for next subtask
				{
					if (dev->ConnectLost == TRUE) {
						nextX2X();
						subsubTask[X2X] = 0;
						} else {
						subsubTask[X2X]++;
					}
				}
				break;

				default:
				break;
			}
		}
		break;

		case 1: // Пришли ли новые осциллограммы?
		{
			switch (read_holding_registersX(X2X, 850, 1, data_X2X))
			{
				case 1: brLost(); break;
				case 2: brOk();   break;

				case 3:
				{
					if (data_X2X[0] != newOscillograms) {
						nextX2X();
						subsubTask[X2X] = 0;
						} else {
						if (lctDebug) SerialUSB.println("New LCT Data");
						subsubTask[X2X]++;
					}
				}
				break;

				default: break;
			}
		}
		break;

		case 2: // scope A
		{
			switch (read_holding_registersX(X2X, 200, 200, data_X2X))
			{
				case 1: brLost(); break;
				case 2: brOk(); memcpy(scopeArray[scopeA], data_X2X, sizeof(scopeArray[scopeA])); break;
				case 3: if (lctDebug) SerialUSB.println("Current Scope phA"); subsubTask[X2X]++; break;
				default: break;
			}
		}
		break;

		case 3: // scope B
		{
			switch (read_holding_registersX(X2X, 400, 200, data_X2X))
			{
				case 1: brLost(); break;
				case 2: brOk(); memcpy(scopeArray[scopeB], data_X2X, sizeof(scopeArray[scopeB])); break;
				case 3: if (lctDebug) SerialUSB.println("Current Scope phB"); subsubTask[X2X]++; break;
				default: break;
			}
		}
		break;

		case 4: // scope C
		{
			switch (read_holding_registersX(X2X, 600, 200, data_X2X))
			{
				case 1: brLost(); break;
				case 2: brOk(); memcpy(scopeArray[scopeC], data_X2X, sizeof(scopeArray[scopeC])); break;
				case 3: if (lctDebug) SerialUSB.println("Current Scope phC"); subsubTask[X2X]++; break;
				default: break;
			}
		}
		break;

		case 5: // Сбросить флаг новых данных в модуле LCT + save
		{
			if (serialFlag[X2X] == taskComplete) {
				data_X2X[0] = 0;
				if (lctDebug) SerialUSB.println("Send reset to LCT");
			}

			switch (preset_multiple_registers(X2X, 850, 1, data_X2X))
			{
				case 1: brLost(); break;
				case 2: brOk();   break;

				case 3:
				{
					unsigned long startSaveSD = millis();
					saveScope(slave);

					if (lctDebug) {
						SerialUSB.print("Sd save time -> ");
						SerialUSB.println(millis() - startSaveSD);
					}

					nextX2X();
					subsubTask[X2X] = 0;
				}
				break;

				default: break;
			}
		}
		break;

		default:
		break;
	}

	return LCT;
}



unsigned long newLCTdataCheck[7] = {0};


byte X2X_LCT_2()   // Read Data from LCT1114_2 module (X2X)
{
	normalizeX2XSlave();
	const uint8_t slave = currentSlave[X2X];

	DeviceHeader* hdr = getX2XBySlave(slave);
	if (!hdr || hdr->ID != LCT_2) {
		nextX2X();
		subsubTask[X2X] = 0;
		taskFlag[X2X] = standBy;
		return 0;
	}

	auto* dev = static_cast<LCT1114_2*>(hdr);

	switch (subsubTask[X2X])
	{
		case 0: // Опрос измерений с модуля LCT_2
		{
			switch (read_holding_registersX(X2X, 0, 94, data_X2X))
			{
				case 1: // Timeout respond
				{
					checkConnection(dev, TRUE);

					if (lctDebug)
					{
						
						SerialUSB.print("*** LCT_2 read 1 Lost. Slave -> ");
						SerialUSB.println(slave);
					}
				}
				break;

				case 2: // Receive respond
				{
					brOk();

					if (lctDebug) {
						SerialUSB.print("*** LCT_2 1 Ok. Slave -> ");
						SerialUSB.println(slave);
					}

					checkConnection(dev, FALSE);


					// DI packing
					uint32_t di = uint32_t(data_X2X[0] & 0x3F);
					di |= (uint32_t(data_X2X[1] & 0x1FFF) << 6);
					dev->DigitalInput = di;

					// TF floats start at reg 2
					uint8_t Modbus_Floats_Addr = 2;
					X2X_to_Float(dev, data_X2X, Modbus_Floats_Addr);
				}
				break;

				case 3: // Ready for new subtask
				{
					if (dev->ConnectLost == TRUE)
					{

						nextX2X();
						subsubTask[X2X] = 0;
					}
					else
					{
						subsubTask[X2X]++;
					}
				}
				break;

				default:
				break;
			}
		}
		break;

		case 1: // Проверка тайм-окна на чтение новых данных
		{
			if (millis() - newLCTdataCheck[slave] < 1000)
			{
				nextX2X();
				if (lctDebug) SerialUSB.println("Skip check ");
				subsubTask[X2X] = 0;
			}
			else
			{
				if (lctDebug) SerialUSB.println("Check New LCT Data");
				subsubTask[X2X]++; // -> case 2
			}
		}
		break;

		case 2: // Пришли ли новые осциллограммы? (reg 850)
		{
			switch (read_holding_registersX(X2X, 850, 1, data_X2X))
			{
				case 1: // Timeout respond
				{
					brLost();
					if (lctDebug)
					{
						SerialUSB.print("*** LCT_2. read 2 Lost. Slave -> ");
						SerialUSB.println(slave);
					}
				}
				break;

				case 2: // Receive respond
				{
					brOk();
					if (lctDebug)
					{
						SerialUSB.print("*** LCT_2 2 Ok. Slave -> ");
						SerialUSB.println(slave);
					}
				}
				break;

				case 3: // Ready for new subtask
				{
					if (data_X2X[0] != newOscillograms)
					{
						nextX2X();
						if (lctDebug) SerialUSB.println("Next slave ");
						subsubTask[X2X] = 0;
					}
					else
					{
						if (lctDebug) SerialUSB.println("New LCT Data");
						subsubTask[X2X]++; // -> case 3
					}
				}
				break;

				default:
				break;
			}
		}
		break;

		case 3: // scope A
		{
			switch (read_holding_registersX(X2X, 201, 195, data_X2X))
			{
				case 1: brLost(); break;
				case 2:
				brOk();
				memcpy(scopeArray[scopeA], data_X2X, sizeof(scopeArray[scopeA]));
				break;
				case 3:
				if (lctDebug) SerialUSB.println("Current Scope phA");
				subsubTask[X2X]++;
				break;
				default: break;
			}
		}
		break;

		case 4: // scope B
		{
			switch (read_holding_registersX(X2X, 401, 195, data_X2X))
			{
				case 1: brLost(); break;
				case 2:
				brOk();
				memcpy(scopeArray[scopeB], data_X2X, sizeof(scopeArray[scopeB]));
				break;
				case 3:
				if (lctDebug) SerialUSB.println("Current Scope phB");
				subsubTask[X2X]++;
				break;
				default: break;
			}
		}
		break;

		case 5: // scope C
		{
			switch (read_holding_registersX(X2X, 601, 195, data_X2X))
			{
				case 1: brLost(); break;
				case 2:
				brOk();
				memcpy(scopeArray[scopeC], data_X2X, sizeof(scopeArray[scopeC]));
				break;
				case 3:
				if (lctDebug) SerialUSB.println("Current Scope phC");
				subsubTask[X2X]++;
				break;
				default: break;
			}
		}
		break;

		case 6: // Сбросить флаг новых данных + saveScope
		{
			if (serialFlag[X2X] == taskComplete)
			{
				data_X2X[0] = 0;
				if (lctDebug) SerialUSB.println("Send reset to LCT_2");
			}

			switch (preset_multiple_registers(X2X, 850, 1, data_X2X))
			{
				case 1: brLost(); break;
				case 2: brOk();   break;

				case 3:
				{
					unsigned long startSaveSD = millis();
					saveScope(slave);

					if (lctDebug)
					{
						SerialUSB.print("Sd save time -> ");
						SerialUSB.println(millis() - startSaveSD);
					}

					newLCTdataCheck[slave] = millis();

					nextX2X();
					subsubTask[X2X] = 0;
				}
				break;

				default:
				break;
			}
		}
		break;

		default:
		break;
	}

	return LCT_2;
}

void nextX2X()
{
	uint8_t& slave = currentSlave[X2X];

	normalizeX2XSlave();
	if (g_x2xCount == 0) {
		taskFlag[X2X] = standBy;
		return;
	}

	// циклически 1..g_x2xCount
	if (++slave > g_x2xCount) slave = 1;

	taskFlag[X2X] = standBy;

	if (X2X_Debug)
	{
		auto* hdr = getX2XBySlave(slave);
		SerialUSB.print("*** X2X Next Slave -> ");
		SerialUSB.println(slave);
		SerialUSB.print("*** Device ID       -> ");
		SerialUSB.println(hdr ? hdr->ID : 255);
	}
}



//
// void nextX2X()
// {
// 	// локальная ссылка на индекс текущего «slave»
// 	uint8_t& slave = currentSlave[X2X];
//
// 	if (X2X_Debug)
// 	SerialUSB.println(slave);
//
// 	// переходим к следующему, с циклическим сбросом
// 	if (++slave >= g_x2xCount)
// 	slave = 1;
//
// 	taskFlag[X2X] = standBy;  // готов к следующей задаче
//
// 	if (X2X_Debug)
// 	{
// 		// приводим буфер к заголовку, чтобы достать ID
// 		auto* hdr = reinterpret_cast<DeviceHeader*>(system_Devices[slave]);
//
// 		SerialUSB.print("*** X2X Next Slave -> ");
// 		SerialUSB.println(slave);
// 		SerialUSB.print("*** Device ID       -> ");
// 		SerialUSB.println(hdr->ID);
// 	}
// }


// void nextX2X()
// {
// 	if (X2X_Debug)
// 	SerialUSB.println(currentSlave[X2X]);
//
// 	currentSlave[X2X]++;
//
//
// 	if (currentSlave[X2X] > g_x2xCount)
// 	currentSlave[X2X] = 1;
//
// 	taskFlag[X2X] = standBy;  // Ready for new task
//
// 	if (X2X_Debug)
// 	{
// 		SerialUSB.print("*** X2X Next Slave -> ");
// 		SerialUSB.println(currentSlave[X2X]);
// 		SerialUSB.print("***Device ID -> ");
// 		SerialUSB.println((((DeviceCommon*)system_Devices[currentSlave[X2X]])->ID));
// 	}
// }

byte X2X_LDO()
{
	DeviceHeader* hdr = getX2XBySlave(currentSlave[X2X]);
	if (!hdr || hdr->ID != LDO) {
		// кто-то сбил индекс или конфиг
		nextX2X();
		taskFlag[X2X] = standBy;
		return 0;
	}

	auto* dev = static_cast<LDO1118*>(hdr);

	switch (preset_multiple_registers(X2X, 0, 1, data_X2X))
	{
		case 1: // not responded
		{
			brLost();
			checkConnection(dev, TRUE);
		}
		break;

		case 2: // received respond
		{
			brOk();
			checkConnection(dev, FALSE);
			taskFlag[X2X] = standBy;
		}
		break;

		case portReady: // next task
		{
			nextX2X();
		}
		break;

		default:
		break;
	}

	return LDO;
}

//
// byte X2X_LDO()
// {
//
//
// 	switch (preset_multiple_registers(X2X, 0, 1, data_X2X))
// 	{
// 		case 1: // not responded
// 		{
// 			brLost();   // BR communication lost. Proceed this event
// 			auto* dev = static_cast<LDO1118*>(system_Devices[currentSlave[X2X]]);
// 			checkConnection(dev, TRUE);
//
// 		}
// 		break;
//
// 		case 2:		   // Received Respond
// 		{
// 			brOk();
//
// 			auto* dev = static_cast<LDO1118*>(system_Devices[currentSlave[X2X]]);
// 			checkConnection(dev, FALSE);
//
// 			taskFlag[X2X] = standBy;  // Ready for new task
// 		}
//
// 		break;
//
//
// 		case portReady:		   // Next Task
//
// 		nextX2X();
//
// 		break;
//
// 		default:
//
// 		break;
// 	}
// }


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
		HMITask = updateResetBtns;
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


byte getDetectorData(byte flow_, byte subTask_, int16_t *data_)
{
	// Нормализация slave по диапазону линии
	if (currentSlave[flow_] < lowLim[flow_] || currentSlave[flow_] > hiLim[flow_]) {
		currentSlave[flow_] = lowLim[flow_];
	}

	const uint8_t slave = currentSlave[flow_];

	DeviceHeader* hdr = getSensBySlave(slave);
	if (!hdr || hdr->ID != IR_SF6) {
		taskFlag[flow_]   = standBy;
		subsubTask[flow_] = 0;
		nextSlave(flow_);
		return subTask_;
	}

	// Если выключен датчик / сервисный режим — пропускаем и помечаем как "нет"
	if (enabled[slave] == 0)
	{
		taskFlag[flow_] = standBy;

		gaugeData[slave * 2 - 1] = hide;
		gaugeData[slave * 2]     = ok;

		// Чтобы dataset/печать тоже понимали, что связи "нет"
		checkConnection(hdr, true);
		// Можно сбросить счётчик потерь, чтобы при включении стартовать с 0
		lostPackets[slave] = 0;

		nextSlave(flow_);
		++subTask_;
		return subTask_;
	}

	// General mode
	switch (read_holding_registers(flow_, 0, 2, data_))
	{
		case 1: // Timeout
		{
			regsNoRespond(flow_);

			// мягкая потеря связи через счётчик
			if (++lostPackets[slave] > LostPacketError)
			checkConnection(hdr, true);

			return subTask_;
		}

		case 2: // Receive
		{
			regsResponded(flow_, data_);

			lostPackets[slave] = 0;
			checkConnection(hdr, false);

			return subTask_;
		}

		case 3: // цикл завершён
		{
			taskFlag[flow_] = standBy;
			nextSlave(flow_);
			++subTask_;
			return subTask_;
		}

		default:
		return subTask_;
	}
}





uint8_t getRSdata(byte flow_, byte subTask_, int16_t *data_)
{
	// Нормализация slave по диапазону на этой линии
	if (currentSlave[flow_] < lowLim[flow_] || currentSlave[flow_] > hiLim[flow_]) {
		currentSlave[flow_] = lowLim[flow_];
	}

	const uint8_t slave = currentSlave[flow_];

	DeviceHeader* hdr = getSensBySlave(slave);   // <-- твой хелпер
	if (!hdr) {
		// конфиг/индексы не совпали с деревом
		taskFlag[flow_]   = standBy;
		subsubTask[flow_] = 0;
		nextSlave(flow_);
		return 0;
	}

	switch (hdr->ID)
	{
		case IR_SF6:
		getDetectorData(flow_, subTask_, data_);
		break;

		case DM910:
		getDM910data(flow_, subTask_, data_);
		break;

		case DM910H:
		getDM910Hdata(flow_, subTask_, data_);
		break;

		case PVT100:
		getPVT100data(flow_, subTask_, data_);
		break;

		default:
		// неизвестный тип в дереве — просто идём дальше
		taskFlag[flow_]   = standBy;
		subsubTask[flow_] = 0;
		nextSlave(flow_);
		return 0;
	}

	return hdr->ID;
}


//
// uint8_t getRSdata(byte flow_, byte subTask_, int16_t *data_)
// {
// 	/*
// 	if (dmDebug)
// 	{
// 	SerialUSB.print("flow -> ");
// 	SerialUSB.println(flow_);
//
// 	SerialUSB.print("currentSlave -> ");
// 	SerialUSB.println(currentSlave[flow_]);
//
// 	SerialUSB.print("gasType[currentSlave[flow_]] -> ");
// 	SerialUSB.println(gasType[currentSlave[flow_]]);
//
// 	}
// 	*/
//
// 	switch(gasType[currentSlave[flow_]])
// 	{
// 		case IR_SF6:
// 		getDetectorData(flow_, subTask_, data_);
//
// 		break;
//
// 		case DM910:
// 		getDM910data(flow_, subTask_, data_);
//
// 		break;
//
// 		case DM910H:
// 		if (initSerialDebug)
// 		{
// 			SerialUSB.println("Ln 7453");
// 		}
// 		getDM910Hdata(flow_, subTask_, data_);
//
// 		break;
//
// 		case PVT100:
//
// 		getPVT100data(flow_, subTask_, data_);
//
// 		break;
//
// 		default:
//
// 		break;
// 	}
// 	if (initSerialDebug)
// 	{
// 		SerialUSB.println("Ln 7471");
// 	}
// }

byte const pvtDebug = 0;
byte getPVT100data(byte flow_, byte subTask_, int16_t *data_)
{
	// нормализуем slave по диапазону линии RS485 (lowLim/hiLim)
	if (currentSlave[flow_] < lowLim[flow_] || currentSlave[flow_] > hiLim[flow_]) {
		currentSlave[flow_] = lowLim[flow_];
	}

	const uint8_t slave = currentSlave[flow_];

	DeviceHeader* hdr = getSensBySlave(slave);
	if (!hdr || hdr->ID != PVT100) {
		// конфиг/индексация сломаны — не трогаем память, переходим дальше
		taskFlag[flow_]   = standBy;
		subsubTask[flow_] = 0;
		nextSlave(flow_);
		return subTask_;
	}

	auto* dev = static_cast<PVT_100*>(hdr);

	// читаем 2 регистра: [0]=температура, [1]=влажность
	switch (read_holding_registers(flow_, 258, 2, data_))
	{
		case 1: // timeout
		{
			brLost();

			if (++lostPackets[slave] > LostPacketError) {
				checkConnection(dev, true);
			}

			if (pvtDebug) {
				SerialUSB.print("PVT slave=");
				SerialUSB.print(slave);
				SerialUSB.println(" timeout");
			}

			return subTask_;
		}

		case 2: // receive
		{
			brOk();

			lostPackets[slave] = 0;
			checkConnection(dev, false);

			dev->PVTtemp = data_[0];
			dev->PVThum  = data_[1];

			if (pvtDebug) {
				SerialUSB.print("PVT slave=");
				SerialUSB.print(slave);
				SerialUSB.print(" Temp=");
				SerialUSB.print(dev->PVTtemp);
				SerialUSB.print(" Hum=");
				SerialUSB.println(dev->PVThum);
			}

			return subTask_;
		}

		case 3: // цикл завершён
		{
			taskFlag[flow_]   = standBy;
			subsubTask[flow_] = 0;      // на всякий случай
			nextSlave(flow_);           // ВАЖНО: у тебя этого не было
			++subTask_;
			return subTask_;
		}

		default:
		return subTask_;
	}
}



/*
byte getDM910data(byte flow_, byte subTask_, int16_t *data_)
{

//currentSlave[RS3] = 1;
switch (subsubTask[flow_])
{
case 0:
{
switch (read_holding_registers(flow_, 0, 2, data_))
{
case 1: // Timeout respond
//	regsNoRespond(flow_);
if (dmDebug)
{
SerialUSB.print("DM910 -> ");
SerialUSB.print(currentSlave[flow_]);
SerialUSB.println(" -> not responded s1");
}


//	DM910Data[currentSlave[flow_]][pressureDM] = -100;
//DM910Data[currentSlave[flow_]][state] = TRUE;

return subTask_;
break;

case 2: // Receive respond
// regsResponded(flow_, data_);

if (dmDebug)
{
SerialUSB.print("DM910 pressure -> ");
SerialUSB.print(currentSlave[flow_]);
SerialUSB.println(" -> OK");
}

pressureDM_val.bytes[3] = highByte(data_[1]);
pressureDM_val.bytes[2] = lowByte(data_[1]);
pressureDM_val.bytes[1] = highByte(data_[0]);
pressureDM_val.bytes[0] = lowByte(data_[0]);

//DM910Data[currentSlave[flow_]][pressureDM] = pressureDM_val.val;
//DM910Data[currentSlave[flow_]][state] = FALSE;

// 				SerialUSB.print(data_[0]);
// 				SerialUSB.println(data_[1]);
return subTask_;
break;

case 3:  // Serial actions finished and poll time delay done. Ready for new subtask
taskFlag[flow_] = standBy;  // Ready for new task
//	nextSlave(flow_);      // Next slave - 0 - nothing to do. Else - go to next slave

subsubTask[flow_]++;
subTask_++;

if (dmDebug)
{
SerialUSB.print("DM910 -> ");
SerialUSB.print(currentSlave[flow_]);
SerialUSB.println(" -> next step");
}

// 				SerialUSB.print(currentSlave[flow_]);
// 				SerialUSB.println(" DM910");
return subTask_;
break;

default:

return subTask_;
break;
}
}

break;




case 1:
{
switch (read_holding_registers(flow_, 12, 12, data_))
{
case 1: // Timeout respond
//	regsNoRespond(flow_);
//	DM910Data[currentSlave[flow_]][state] = TRUE;
if (dmDebug)
{
SerialUSB.print("DM910 -> ");
SerialUSB.print(currentSlave[flow_]);
SerialUSB.println(" -> not responded s2");
}
return subTask_;
break;

case 2: // Receive respond
//	regsResponded(flow_, data_);
//	DM910Data[currentSlave[flow_]][state] = FALSE;

themperatureDM_val.bytes[3] = highByte(data_[1]);
themperatureDM_val.bytes[2] = lowByte(data_[1]);
themperatureDM_val.bytes[1] = highByte(data_[0]);
themperatureDM_val.bytes[0] = lowByte(data_[0]);


//	DM910Data[currentSlave[flow_]][themperatureDM] = themperatureDM_val.val;


densityDM_val.bytes[3] = highByte(data_[7]);
densityDM_val.bytes[2] = lowByte(data_[7]);
densityDM_val.bytes[1] = highByte(data_[6]);
densityDM_val.bytes[0] = lowByte(data_[6]);

//	DM910Data[currentSlave[flow_]][densityDM] = densityDM_val.val;





pressure20DM_val.bytes[3] = highByte(data_[11]);
pressure20DM_val.bytes[2] = lowByte(data_[11]);
pressure20DM_val.bytes[1] = highByte(data_[10]);
pressure20DM_val.bytes[0] = lowByte(data_[10]);

//DM910Data[currentSlave[flow_]][pressure20DM] = pressure20DM_val.val;

if (dmDebug)
{
SerialUSB.print("DM910 pressure, temp, dens-> ");
SerialUSB.print(currentSlave[flow_]);
SerialUSB.println(" -> OK");
}
return subTask_;
break;

case 3:  // Serial actions finished and poll time delay done. Ready for new subtask
taskFlag[flow_] = standBy;  // Ready for new task
nextSlave(flow_);      // Next slave - 0 - nothing to do. Else - go to next slave
if (dmDebug)
{
SerialUSB.print("DM910 -> ");
SerialUSB.print(currentSlave[flow_]);
SerialUSB.println(" -> next slave");
}
subsubTask[flow_]=0;
subTask_++;
return subTask_;
break;

default:

return subTask_;
break;
}

}
break;

}

// subTask 0 Finish ***************************************

// subTask 0 Finish ***************************************
}
*/

byte getDM910data(byte flow_, byte subTask_, int16_t *data_)
{
	// Нормализация slave по диапазону на этой линии
	if (currentSlave[flow_] < lowLim[flow_] || currentSlave[flow_] > hiLim[flow_]) {
		currentSlave[flow_] = lowLim[flow_];
	}

	const uint8_t slave = currentSlave[flow_];

	DeviceHeader* hdr = getSensBySlave(slave);
	if (!hdr || hdr->ID != DM910) {
		// дерево/конфиг сломан или не тот тип
		taskFlag[flow_]   = standBy;
		subsubTask[flow_] = 0;
		nextSlave(flow_);
		return subTask_;
	}

	auto* dev = static_cast<DM91_0*>(hdr);

	switch (subsubTask[flow_])
	{
		// ---------------------------
		// Шаг 0: читаем pressureDM (0..1)
		case 0:
		{
			switch (read_holding_registers(flow_, 0, 2, data_))
			{
				case 1: // timeout
				{
					brLost();
					if (dmDebug) SerialUSB.println("DM sensor timeout on first read");

					if (++lostPackets[slave] > LostPacketError)
					checkConnection(dev, true);

					return subTask_;
				}

				case 2: // ok
				{
					checkConnection(dev, false);
					lostPackets[slave] = 0;

					dev->pressureDM = regsToFloat(data_[0], data_[1]);

					brOk();
					return subTask_;
				}

				case 3: // cycle done
				{
					if (dev->ConnectLost)
					{
						if (dmDebug) SerialUSB.println("DM sensor lost. next");

						taskFlag[flow_]    = standBy;
						nextSlave(flow_);
						subsubTask[flow_]  = 0;
						++subTask_;
					}
					else
					{
						++subsubTask[flow_];
						++subTask_;
					}
					return subTask_;
				}

				default:
				return subTask_;
			}
		}

		// ---------------------------
		// Шаг 1: читаем остальные float (12..23 или как у тебя)
		case 1:
		{
			switch (read_holding_registers(flow_, 12, 12, data_))
			{
				case 1: // timeout
				{
					if (dmDebug) SerialUSB.println("DM sensor timeout on secondary read");
					checkConnection(dev, true);
					return subTask_;
				}

				case 2: // ok
				{
					checkConnection(dev, false);

					dev->themperatureDM = regsToFloat(data_[0],  data_[1]);
					dev->densityDM      = regsToFloat(data_[6],  data_[7]);
					dev->pressure20DM   = regsToFloat(data_[10], data_[11]);

					brOk();   // <- добавил
					return subTask_;
				}

				case 3:
				{
					taskFlag[flow_]    = standBy;
					nextSlave(flow_);
					subsubTask[flow_]  = 0;
					++subTask_;
					return subTask_;
				}

				default:
				return subTask_;
			}
		}

		default:
		subsubTask[flow_] = 0;
		return subTask_;
	}
}
byte getDM910Hdata(byte flow_, byte subTask_, int16_t *data_)
{
	// Нормализация slave по диапазону линии
	if (currentSlave[flow_] < lowLim[flow_] || currentSlave[flow_] > hiLim[flow_]) {
		currentSlave[flow_] = lowLim[flow_];
	}

	const uint8_t slave = currentSlave[flow_];

	DeviceHeader* hdr = getSensBySlave(slave);
	if (!hdr || hdr->ID != DM910H) {
		taskFlag[flow_]   = standBy;
		subsubTask[flow_] = 0;
		nextSlave(flow_);
		return subTask_;
	}

	auto* dev = static_cast<DM91_H*>(hdr);

	switch (subsubTask[flow_])
	{
		// -----------------------------------
		// Шаг 0: pressureDM (0–1)
		case 0:
		{
			switch (read_holding_registers(flow_, 0, 2, data_))
			{
				case 1: // timeout
				{
					brLost();

					if (++lostPackets[slave] > LostPacketError)
					checkConnection(dev, true);

					if (dmDebug) SerialUSB.println("DM910H pressureDM timeout");
					return subTask_;
				}

				case 2: // ok
				{
					lostPackets[slave] = 0;
					checkConnection(dev, false);

					dev->pressureDM = regsToFloat(data_[0], data_[1]);

					if (dmDebug) { SerialUSB.print("dev->pressureDM -> "); SerialUSB.println(dev->pressureDM); }

					brOk();
					return subTask_;
				}

				case 3: // завершение шага
				{
					if (dev->ConnectLost)
					{
						taskFlag[flow_]   = standBy;
						nextSlave(flow_);
						subsubTask[flow_] = 0;
						++subTask_;
					}
					else
					{
						++subsubTask[flow_];
						++subTask_;
					}
					return subTask_;
				}

				default:
				return subTask_;
			}
		}

		// -----------------------------------
		// Шаг 1: dewPointDM (30–31)
		case 1:
		{
			switch (read_holding_registers(flow_, 30, 2, data_))
			{
				case 1:
				{
					brLost();

					if (++lostPackets[slave] > LostPacketError)
					checkConnection(dev, true);

					if (dmDebug) SerialUSB.println("DM910H dewPoint timeout");
					return subTask_;
				}

				case 2:
				{
					lostPackets[slave] = 0;
					checkConnection(dev, false);

					dev->dewPointDM = regsToFloat(data_[0], data_[1]);

					if (dmDebug) { SerialUSB.print("dev->dewPointDM -> "); SerialUSB.println(dev->dewPointDM); }

					brOk();
					return subTask_;
				}

				case 3:
				{
					if (dev->ConnectLost)
					{
						taskFlag[flow_]   = standBy;
						nextSlave(flow_);
						subsubTask[flow_] = 0;
						++subTask_;
					}
					else
					{
						++subsubTask[flow_];
						++subTask_;
					}
					return subTask_;
				}

				default:
				return subTask_;
			}
		}

		// -----------------------------------
		// Шаг 2: PPMvDM (40–41)
		case 2:
		{
			switch (read_holding_registers(flow_, 40, 2, data_))
			{
				case 1:
				{
					brLost();

					if (++lostPackets[slave] > LostPacketError)
					checkConnection(dev, true);

					if (dmDebug) SerialUSB.println("DM910H PPMv timeout");
					return subTask_;
				}

				case 2:
				{
					lostPackets[slave] = 0;
					checkConnection(dev, false);

					dev->PPMvDM = regsToFloat(data_[0], data_[1]);

					if (dmDebug) { SerialUSB.print("dev->PPMvDM -> "); SerialUSB.println(dev->PPMvDM); }

					brOk();
					return subTask_;
				}

				case 3:
				{
					if (dev->ConnectLost)
					{
						taskFlag[flow_]   = standBy;
						nextSlave(flow_);
						subsubTask[flow_] = 0;
						++subTask_;
					}
					else
					{
						++subsubTask[flow_];
						++subTask_;
					}
					return subTask_;
				}

				default:
				return subTask_;
			}
		}

		// -----------------------------------
		// Шаг 3: RH_DM (48–49)
		case 3:
		{
			switch (read_holding_registers(flow_, 48, 2, data_))
			{
				case 1:
				{
					brLost();

					if (++lostPackets[slave] > LostPacketError)
					checkConnection(dev, true);

					if (dmDebug) SerialUSB.println("DM910H RH timeout");
					return subTask_;
				}

				case 2:
				{
					lostPackets[slave] = 0;
					checkConnection(dev, false);

					dev->RH_DM = regsToFloat(data_[0], data_[1]);

					if (dmDebug) { SerialUSB.print("dev->RH_DM -> "); SerialUSB.println(dev->RH_DM); }

					brOk();
					return subTask_;
				}

				case 3:
				{
					if (dev->ConnectLost)
					{
						taskFlag[flow_]   = standBy;
						nextSlave(flow_);
						subsubTask[flow_] = 0;
						++subTask_;
					}
					else
					{
						++subsubTask[flow_];
						++subTask_;
					}
					return subTask_;
				}

				default:
				return subTask_;
			}
		}

		// -----------------------------------
		// Шаг 4: дополнительные float (12..23)
		// (у тебя чтение 12 регистров, используешь [0..11])
		case 4:
		{
			switch (read_holding_registers(flow_, 12, 12, data_))
			{
				case 1:
				{
					brLost();

					if (++lostPackets[slave] > LostPacketError)
					checkConnection(dev, true);

					if (dmDebug) SerialUSB.println("DM910H additional float timeout");
					return subTask_;
				}

				case 2:
				{
					lostPackets[slave] = 0;
					checkConnection(dev, false);

					// по твоей раскладке
					dev->themperatureDM = regsToFloat(data_[0],  data_[1]);
					dev->densityDM      = regsToFloat(data_[6],  data_[7]);
					dev->pressure20DM   = regsToFloat(data_[10], data_[11]);

					if (dmDebug) {
						SerialUSB.print("dev->themperatureDM "); SerialUSB.println(dev->themperatureDM);
						SerialUSB.print("dev->densityDM      "); SerialUSB.println(dev->densityDM);
						SerialUSB.print("dev->pressure20DM   "); SerialUSB.println(dev->pressure20DM);
						SerialUSB.println();
					}

					brOk();
					return subTask_;
				}

				case 3:
				{
					taskFlag[flow_]   = standBy;
					nextSlave(flow_);
					subsubTask[flow_] = 0;
					++subTask_;
					return subTask_;
				}

				default:
				return subTask_;
			}
		}

		default:
		subsubTask[flow_] = 0;
		return subTask_;
	}
}




byte getResursData(byte flow_, byte subTask_, int16_t *data_)
{

	currentSlave[RS2] = 1;

	switch (subsubTask[flow_])
	{
		case 0:
		{
			switch (read_Input_registers(flow_, 0, 16, data_))
			{
				case 1: // Timeout respond
				regsNoRespond(flow_);
				//		SerialUSB.println("DM not responded")
				for (byte i=0; i<17; i++)
				resursIntArray[i] = -1;
				return subTask_;
				break;

				case 2: // Receive respond


				for (byte i=0; i<17; i++)
				resursIntArray[i] = data_[i];

				// 		SerialUSB.print(data_[0]);
				// 		SerialUSB.println(data_[1]);
				return subTask_;
				break;

				case 3:  // Serial actions finished and poll time delay done. Ready for new subtask
				taskFlag[flow_] = standBy;  // Ready for new task
				//	nextSlave(flow_);      // Next slave - 0 - nothing to do. Else - go to next slave

				subsubTask[flow_]++;
				subTask_++;
				return subTask_;
				break;

				default:

				return subTask_;
				break;
			}
		}

		break;



		case 1:
		{
			switch (read_Input_registers(flow_, 16, 17, data_))
			{
				case 1: // Timeout respond
				regsNoRespond(flow_);

				for (byte i=0; i<17; i++)
				resursIntArray[i+16] = -1;

				//		SerialUSB.println("DM not responded");

				//	SerialUSB.println("res no rep");


				return subTask_;
				break;

				case 2: // Receive respond

				for (byte i=0; i<17; i++)
				resursIntArray[i+16] = data_[i];


				return subTask_;
				break;

				case 3:  // Serial actions finished and poll time delay done. Ready for new subtask
				taskFlag[flow_] = standBy;  // Ready for new task
				//	nextSlave(flow_);      // Next slave - 0 - nothing to do. Else - go to next slave

				subsubTask[flow_]++;
				subTask_++;

				return subTask_;
				break;

				default:

				return subTask_;
				break;
			}
		}

		break;


		case 2:
		{
			switch (read_Discrete_Input(flow_, 0, 15, data_))
			{
				case 1: // Timeout respond
				regsNoRespond(flow_);

				for (byte i=0; i<15; i++)
				resursByteArray[i] = -1;

				//		SerialUSB.println("DM not responded");

				//	SerialUSB.println("res no rep");


				return subTask_;
				break;

				case 2: // Receive respond

				for (byte i=0; i<15; i++)
				resursByteArray[i] = data_[i];

				// 				for (byte i=0; i<15; i++)
				// 				SerialUSB.println(resursByteArray[i]);
				//

				return subTask_;
				break;

				case 3:  // Serial actions finished and poll time delay done. Ready for new subtask
				taskFlag[flow_] = standBy;  // Ready for new task
				//	nextSlave(flow_);      // Next slave - 0 - nothing to do. Else - go to next slave

				subsubTask[flow_]++;
				subTask_++;

				return subTask_;
				break;

				default:

				return subTask_;
				break;
			}
		}

		break;



		case 3:
		{
			switch (read_Input_registers(flow_, 36, 12, data_))
			{
				case 1: // Timeout respond
				regsNoRespond(flow_);
				//	SerialUSB.println("Res not responded");

				for (byte i=0; i<12; i++)
				resursIntArray[i+36] = -1;
				return subTask_;
				break;

				case 2: // Receive respond


				// 				for (byte i=0; i<12; i++)
				// 				SerialUSB.println(data_[i]);
				//
				// 				SerialUSB.println();
				//
				for (byte i=0; i<12; i++)
				resursIntArray[i+36] = data_[i];

				// 				for (byte i=0; i<48; i++)
				// 				SerialUSB.println(resursIntArray[i]);



				// 		SerialUSB.print(data_[0]);
				// 		SerialUSB.println(data_[1]);
				return subTask_;
				break;

				case 3:  // Serial actions finished and poll time delay done. Ready for new subtask
				taskFlag[flow_] = standBy;  // Ready for new task
				//	nextSlave(flow_);      // Next slave - 0 - nothing to do. Else - go to next slave

				subsubTask[flow_]=0;
				subTask_++;
				return subTask_;
				break;

				default:

				return subTask_;
				break;
			}

		}
		break;

	}
	/*	}
	else  // If detector disabled or service Mode active
	{
	taskFlag[flow_] = standBy;  // Ready for new task
	gaugeData[currentSlave[flow_] * 2 - 1] = hide;                // HMI: Датчик не ответил - надпись на поле датчика "не подключен"- код " - 1"
	gaugeData[currentSlave[flow_] * 2] = ok;                     // HMI: Датчик не ответил - цвет поля красный - код 0

	nextSlave(flow_);      // Next slave - 0 - nothing to do. Else - go to next slave
	subTask_++;
	return subTask_;
	}
	*/
	// subTask 0 Finish ***************************************

	// subTask 0 Finish ***************************************
}





byte getParmaData(byte flow_, byte subTask_, int16_t *data_)
{

	// Signal Emulation Mode Start
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
		subTask_++;
		return subTask_;
	}

	// Signal Emulation Mode Finish




	// General mode
	//	if (enabled[currentSlave[flow_]] != 0)  // If detector enabled and serviceMode switched off
	//	{
	switch (read_Input_registers(flow_, 7, 4, data_))
	{
		case 1: // Timeout respond
		//	regsNoRespond(flow_);

		//
		// 		 		SerialUSB.print("Parma Lost-> ");
		// 		 		SerialUSB.println(currentSlave[flow_]);


		for (byte i = 0; i <= 3; i++)
		{
			mb1.Ireg(6 + i + 4*currentSlave[flow_], -100);
			parmaArray[i + 4*(currentSlave[flow_]-1)] = -100;
		}


		return subTask_;
		break;

		case 2: // Receive respond
		//	regsResponded(flow_, data_);


		// 		 		SerialUSB.print("Parma -> ");
		// 		 		SerialUSB.println(currentSlave[flow_]);
		//
		// 		SerialUSB.println(data_[0]);
		// 		SerialUSB.println(data_[1]);
		// 		SerialUSB.println(data_[2]);
		// 		SerialUSB.println(data_[3]);
		//
		for (byte i = 0; i <= 3; i++)
		{
			mb1.Ireg(6 + i + 4*currentSlave[flow_], data_[i]);
			parmaArray[i + 4*(currentSlave[flow_]-1)] = data_[i];
		}

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
	// 	}
	// 	else  // If detector disabled or service Mode active
	// 	{
	// 		taskFlag[flow_] = standBy;  // Ready for new task
	// 		gaugeData[currentSlave[flow_] * 2 - 1] = hide;                // HMI: Датчик не ответил - надпись на поле датчика "не подключен"- код " - 1"
	// 		gaugeData[currentSlave[flow_] * 2] = ok;                     // HMI: Датчик не ответил - цвет поля красный - код 0
	//
	// 		//	nextSlave(flow_);      // Next slave - 0 - nothing to do. Else - go to next slave
	// 		subTask_++;
	// 		return subTask_;
	// 	}
	// subTask 0 Finish ***************************************

	// subTask 0 Finish ***************************************
}













byte getDataTask = 0;



byte getOwenData(byte flow_, byte subTask_, int16_t *data_)
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





byte getESPData(byte flow_, byte subTask_, int16_t *data_)
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
		sensorEmul = data_HMI[1];
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
		HMITask = updateResetBtns; // Reset comand buttons
		break;
	}

}



void brOk()
{
	BR_lostpacket[currentSlave[X2X]] = 0;
	if (logX2X[currentSlave[X2X]] == 1)
	{
		logX2X[currentSlave[X2X]] = 0;
		sdString = "Блок реле -> ";
		sdString += currentSlave[X2X];
		sdString += " снова на связи";
		printData();
		//		X2X_Data_Array[currentSlave[X2X]][Status] = FALSE;
		if (X2X_Debug)
		{
			SerialUSB.print("X2X_");
			SerialUSB.print(currentSlave[X2X]);
			SerialUSB.print("->");
			//	SerialUSB.println(X2X_Data_Array[currentSlave[X2X]][Status]);
		}
	}

}


uint8_t brLost()
{
	byte thisRelay;
	thisRelay = currentSlave[X2X];

	if (X2X_Debug)
	{
		SerialUSB.print("Lost BR -> ");
		SerialUSB.println(thisRelay);

	}
	if (BR_lostpacket[thisRelay] > BR_lostpacketSP)
	{

		if (logX2X[thisRelay] == 0)
		{
			
			logX2X[thisRelay] = 1;
			//	X2X_Data_Array[thisRelay][Status] = TRUE;
			sdString = "Блок реле -> ";
			sdString += thisRelay;
			sdString += " не отвечает";
			printData();
			if (X2X_Debug)
			{
				SerialUSB.print("X2X_");
				SerialUSB.print(currentSlave[X2X]);
				SerialUSB.print("->");
				//	SerialUSB.println(X2X_Data_Array[currentSlave[X2X]][Status]);
			}
		}
		return 1;
	}
	else
	BR_lostpacket[thisRelay]++;
	
	return 0;
	
}



// Cyclic redundant Connected (echoCheck)

byte redundancyCheck(byte flow_)
{
	byte exitFault = 0;
	unsigned char echoString[65];
	for (byte j = 0; j < 10; j++)
	echoString[j] = 0;

	byte port_num = S_PORT[flow_] + 1;

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
	switch (S_PORT[flow_])
	{
		/*
		case 1:  // PC

		while (UARTX.available(UART1_CS))
		UARTX.read(UART1_CS);

		break;
		*/
		case 2: // HMI

		while (UARTX.available(UART2_CS, CH_A))
		UARTX.read(UART2_CS, CH_A);

		break;


		case 3: // S1

		if (initSerialDebug)
		{
			SerialUSB.println("Flush 0");
		}

		if (uartVer == sc740)
		{
			while (UARTX.available(UART3_CS, CH_A))
			UARTX.read(UART3_CS, CH_A);
		}
		else
		{
			while (UARTX.available(UART2_CS, CH_B))
			UARTX.read(UART2_CS, CH_B);
		}

		if (initSerialDebug)
		{
			SerialUSB.println("Flush 1");
		}

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
			for (byte i = 1; i < g_x2xCount; i++)
			outArray[i] = DisconnectRelayMask[i];

			saveConfig("detFlt.txt", outArray, g_x2xCount);

			break;

			case 1:

			systemFaultRelay[relayModule] = relayMask;
			for (byte i = 1; i < g_x2xCount; i++)
			outArray[i] = systemFaultRelay[i];

			saveConfig("sysFlt.txt", outArray, g_x2xCount);

			break;

			case 2:
			BuzzerOffRelay[relayModule] = relayMask;
			for (byte i = 1; i < g_x2xCount; i++)
			outArray[i] = BuzzerOffRelay[i];

			saveConfig("buzzer.txt", outArray, g_x2xCount);

			break;


		}

		break;

		case 3:
		HMITask = updateResetBtns;
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
			HMITask = updateResetBtns; // Reset command buttons
			subsubTask[HMI] = 0;
		}
		break;
	}
}



byte hmiLostPacket = 0;
int16_t iecModeBuf[2];

byte initHMI()
{


	switch (initSubTask)
	{
		case 0:    // Check HMI Connected

		switch (read_holding_registers(HMI, 1802, 1, data_HMI))
		{
			case 1:
			if (SerialUSB_Ok)
			{
				SerialUSB.print("Нет связи с HMI ... ->  ");
				SerialUSB.println(currentSlave[HMI]);
			}

			hmiLostPacket = 1;

			return 99;  // No Connected to HMI - break preset settings procedure
			break;

			case 2:   // Responded - goto next task

			if (SerialUSB_Ok)
			{
				SerialUSB.print("Связь с панелью установлена  -> ");
				SerialUSB.println(currentSlave[HMI]);
			}

			hmiLostPacket = 0;

			return 0;
			break;

			case 3:  // Serial actions finished and poll time delay done. Ready for new subtask

			if (hmiLostPacket == 1)
			{
				if (SerialUSB_Ok)
				{
					SerialUSB.print("Порт свободен, связь не установлена ...  -> ");
					SerialUSB.println(currentSlave[HMI]);
				}
				HMIstatus[currentSlave[HMI]] = lost;
				initSubTask = 0;
				return 0;

			}
			initSubTask++;
			if (SerialUSB_Ok)
			{

				SerialUSB.print("Порт свободен, связь установлена -> ");
				SerialUSB.println(currentSlave[HMI]);
			}


			break;

			default:

			return 0;
			break;

		}
		break;


		case 1:    // ********* Change HMI window to Loading screen while uploading settings

		if (serialFlag[HMI] == taskComplete)
		{
			data_HMI[0] = 0;
			data_HMI[1] = 0;
			data_HMI[2] = 0;
		}

		switch (preset_multiple_registers(HMI, 500, 3, data_HMI))
		{
			case 1: // not responded
			if (SerialUSB_Ok)
			{

				SerialUSB.print("Связи нет -> ");
				SerialUSB.println(currentSlave[HMI]);
			}

			initSubTask = 0;
			HMIstatus[currentSlave[HMI]] = lost;
			return 99;

			break;

			case portReady:
			if (SerialUSB_Ok)
			{

				SerialUSB.print("Экран загрузки -> ");
				SerialUSB.println(currentSlave[HMI]);
			}


			initSubTask++;
			break;

			default:
			return 0;
			break;
		}
		break;


		case 2:     // Calculate and send Serial Port lines

		//  if (serialFlag[port_HMI] == taskComplete)
		//     {
		if (serialFlag[HMI] == taskComplete)
		{
			clearHMIdata(0);
			// *********** Отправка номера портов в панель
			for (byte i = 0; i <= 3; i++)
			data_HMI[i] = B00000001 << (S_PORT[i + 3] - 3);
		}

		switch (preset_multiple_registers(HMI, 791, 6, data_HMI ))
		{
			case 1: // not responded
			initSubTask = 0;
			HMIstatus[currentSlave[HMI]] = lost;
			if (SerialUSB_Ok)
			{

				SerialUSB.print("Связи нет -> ");
				SerialUSB.println(currentSlave[HMI]);
			}
			return 99;

			break;

			case portReady:
			if (SerialUSB_Ok)
			{
				SerialUSB.print("Адреса портов -> ");
				SerialUSB.println(currentSlave[HMI]);
			}

			initSubTask++;
			break;

			default:
			return 0;
			break;
		}
		break;

		case 3: //  Preset X2X slices
		{
			if (serialFlag[HMI] == taskComplete)
			{
				clearHMIdata(0);

				data_HMI[0] = 1;
				byte buferByte = 1;

				for (byte i = 0; i < g_x2xCount; i++)
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

			switch (preset_multiple_registers(HMI, 100, 2, data_HMI ))
			{
				case 1: // not responded
				if (SerialUSB_Ok)
				{
					SerialUSB.print("Связи нет -> ");
					SerialUSB.println(currentSlave[HMI]);
				}
				initSubTask = 0;
				HMIstatus[currentSlave[HMI]] = lost;
				return 99;

				break;

				case portReady:
				if (SerialUSB_Ok)
				{
					SerialUSB.print("Состав системы -> ");
					SerialUSB.println(currentSlave[HMI]);
				}


				initSubTask++;
				break;

				default:
				return 0;
				break;
			}
		}

		break;


		case 4:    // **************** Reset all btns


		if (serialFlag[HMI] == taskComplete)
		{

			clearHMIdata(0);

			if (Ethernet1Mode == IEC104mode)
			{
				data_HMI[5] = B00000010;
				SerialUSB.println("IEC");
			}


		}


		switch (preset_multiple_registers(HMI, 1802, 5, data_HMI))
		{
			case 1: // not responded
			if (SerialUSB_Ok)
			{
				SerialUSB.print("Связи нет -> ");
				SerialUSB.println(currentSlave[HMI]);
			}

			initSubTask = 0;
			HMIstatus[currentSlave[HMI]] = lost;
			return 99;

			break;

			case portReady:
			if (SerialUSB_Ok)
			{
				SerialUSB.print("Сброс кнопок -> ");
				SerialUSB.println(currentSlave[HMI]);
			}


			initSubTask++;
			break;

			default:
			return 0;
			break;
		}
		break;



		case 5:   // ********* Preset Detector numbers to HMI main screen

		if (serialFlag[HMI] == taskComplete)
		{
			clearHMIdata(0);
			for (byte i = 1; i <= totalSensors; i++)
			data_HMI[i - 1] = i;
		}

		switch (preset_multiple_registers(HMI, 1201, 40, data_HMI ))
		{
			case 1: // not responded
			if (SerialUSB_Ok)
			{
				SerialUSB.print("Связи нет -> ");
				SerialUSB.println(currentSlave[HMI]);
			}
			initSubTask = 0;
			HMIstatus[currentSlave[HMI]] = lost;
			return 99;

			break;

			case portReady:
			if (SerialUSB_Ok)
			{
				SerialUSB.print("Отрисовка номеров датчиков -> ");
				SerialUSB.println(currentSlave[HMI]);
			}


			initSubTask++;
			break;

			default:
			return 0;
			break;
		}


		break;

		/*
		case 6:    // ********* Preset Detector GAS numbers to HMI main screen

		if (serialFlag[port_HMI] == taskComplete)
		{
		clearHMIdata(-1);
		for (byte i=1; i<=totalSensors; i++)
		{
		ModbusCharBuffer[i-1]=gasType[i];
		}
		}

		switch (preset_multiple_registers(port_HMI, HMI_Slave, 1101, 40, ModbusCharBuffer ))
		{
		case 1: // not responded
		if (SerialUSB_Ok)
		SerialUSB.print("5 Связи нет");
		initSubTask = 0;
		HMIstatus = lost;
		return 99;

		break;

		case portReady:
		if (SerialUSB_Ok)
		SerialUSB.println("5 Следующий ");
		taskFlag[HMI] = standBy;  // Ready for new task
		initSubTask++;
		break;

		default:
		return 0;
		break;
		}
		break;

		*/
		case 6:    // ********* Hide non used concentration windows on HMI main screen

		if (serialFlag[HMI] == taskComplete)
		{
			clearHMIdata(-1);

			gaugeData[0] = idle;
			data_HMI[0] = idle;

			for (byte i = 1; i <= totalSensors; i++)
			{
				gaugeData[i * 2 - 1] = hide;  // Hide zero before valid data occur
				data_HMI[i * 2 - 1] = hide;

			}

		}

		switch (preset_multiple_registers(HMI, 0, 81, data_HMI))
		{
			case 1: // not responded
			if (SerialUSB_Ok)
			{
				SerialUSB.print("Связи нет -> ");
				SerialUSB.println(currentSlave[HMI]);
			}
			initSubTask = 0;
			HMIstatus[currentSlave[HMI]] = lost;
			return 99;

			break;

			case portReady:
			if (SerialUSB_Ok)
			{
				SerialUSB.print("Отрисовка газов -> ");
				SerialUSB.println(currentSlave[HMI]);
			}


			initSubTask++;
			break;

			default:
			return 0;
			break;
		}
		break;


		case 7:   // Preset Gas names

		if (serialFlag[HMI] == taskComplete)
		makeGasNameList();

		switch (preset_multiple_registers(HMI, 1100, 40, data_HMI ))
		{
			case 1: // not responded
			if (SerialUSB_Ok)
			{
				SerialUSB.print("Связи нет -> ");
				SerialUSB.println(currentSlave[HMI]);
			}

			initSubTask = 0;
			HMIstatus[currentSlave[HMI]] = lost;
			return 99;

			break;

			case portReady:
			if (SerialUSB_Ok)
			{
				SerialUSB.print("Подключен/отключен -> ");
				SerialUSB.println(currentSlave[HMI]);
			}


			initSubTask ++;  // Changed
			break;

			default:
			return 0;
			break;
		}
		break;

		case 8:

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


		switch (preset_multiple_registers(HMI, 1805, 1, iecModeBuf))
		{
			case 1: // not responded
			if (SerialUSB_Ok)
			{
				SerialUSB.print("Связи нет -> ");
				SerialUSB.println(currentSlave[HMI]);
			}

			initSubTask = 0;
			HMIstatus[currentSlave[HMI]] = lost;
			return 99;

			break;

			case portReady:
			if (SerialUSB_Ok)
			{
				SerialUSB.print("IEC 104 -> ");
				SerialUSB.println(currentSlave[HMI]);
			}

			taskFlag[HMI] = standBy;  // Ready for new task
			initSubTask ++;  // Changed
			break;

			default:
			return 0;
			break;
		}


		break;



		case 9:     // *************** Preset Start HMI Page

		if (serialFlag[HMI] == taskComplete)
		clearHMIdata(0);

		switch (totalSensors)
		{
			case 1 ... 5:
			data_HMI[0] = detectors_5;  // 10 detectors
			break;

			case 6 ... 12:
			data_HMI[0] = detectors_10;  // 10 detectors
			break;

			case 13 ... 16:
			data_HMI[0] = detectors_16;  // 10 detectors
			break;

			case 17 ... 20:
			data_HMI[0] = detectors_20;  // 10 detectors
			break;

			case 21 ... 30:
			data_HMI[0] = detectors_30;  // 10 detectors
			break;

			case 31 ... 40:
			data_HMI[0] = detectors_40;  // 10 detectors
			break;

			default:

			break;

		}

		//     }

		switch ( preset_multiple_registers(HMI, 500, 1, data_HMI ))
		{
			case 1: // not responded
			if (SerialUSB_Ok)
			{
				SerialUSB.print("Связи нет -> ");
				SerialUSB.println(currentSlave[HMI]);
			}
			initSubTask = 0;
			HMIstatus[currentSlave[HMI]] = lost;
			return 99;

			break;

			case portReady:

			initSubTask++;

			if (SerialUSB_Ok)
			{
				SerialUSB.print("Активация стартовой страницы -> ");
				SerialUSB.println(currentSlave[HMI]);
			}

			HMIstatus[currentSlave[HMI]] = ok;
			taskFlag[HMI] = taskComplete;

			if (SerialUSB_Ok)
			{
				SerialUSB.print("HMIstatus -> ");
				SerialUSB.println(HMIstatus[currentSlave[HMI]]);
				SerialUSB.print("taskFlag[HMI] -> ");
				SerialUSB.println(taskComplete);
			}

			if (logHMI[currentSlave[HMI]] == 1)
			{
				logHMI[currentSlave[HMI]] = 0;

				sdString = "Панель -> ";
				sdString += currentSlave[HMI];
				sdString += " на связи";

				printData();
			}

			clearHMIdata(0);
			break;

			default:
			return 0;
			break;
		}
		break;

		default:

		initSubTask = 0;
		break;

	}

}



void coilArrayWrite(int16_t pos_, byte value_)
{
	int16_t arrPos = pos_ * 0.125;
	byte bitPos = pos_ - arrPos * 8;
	bitWrite(RS485_Coil[arrPos], bitPos, value_);

}

byte coilArrayRead(int16_t pos_)
{
	int16_t arrPos = pos_ * 0.125;
	byte bitPos = pos_ - arrPos * 8;
	byte value_ = bitRead(RS485_Coil[arrPos], bitPos);
	return value_;
}






int16_t erisError = 0;

byte getERISData(byte flow_, byte subTask_, int16_t *data_)
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
				//	SerialUSB.print("erisError -> ");
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
				//	SerialUSB.print("Concentration -> ");
				//	SerialUSB.println(portArray_[0]);
				erisResponded(flow_, data_);

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
		nextSlave(flow_);      // Next slave - 0 - nothing to do. Else - go to next slave
		subTask_++;
		return subTask_;
	}
	// subTask 0 Finish ***************************************
}





byte erisResponded(byte flow_, int16_t *data_)
{

	byte currentSensor = currentSlave[flow_];
	byte port_num = S_PORT[flow_];

	data_[1] = erisError;
	serialFlag[flow_] = msgDone;
	startPollTimer[flow_] = millis();   // Записать текущее время и засечь интервал
	lostPackets[currentSensor] = 0;
	gaugeData[currentSensor * 2 - 1] = data_[0]; // Показания датчика
	gaugeErrorCode[currentSensor] = data_[1]; // Ошибка датчика


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
		//	gaugeData[currentSensor * 2] = fault;                     // HMI: Датчик не ответил - цвет поля красный - код 0                                                                    // HMI: Датчик не ответил - цвет общего фона красный - код B00000001
	}



	if (logError[currentSensor] == 1 &&  data_[1] == 0)
	{
		logError[currentSensor] = 0;
		sdString = "Газоанализатор ->";
		sdString += currentSensor;
		sdString += ". Все ошибки сняты ";
		sdString += data_[1];
	}


	if (data_[0] < warningSetpoint[currentSensor] && data_[1] == 0)                 // Показания не превышают первый порог?
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
			sdString += warningSetpoint[currentSensor];
			sdString += ". Концентрация ниже первого порога";

			printData();
		}
	}


	if (data_[0] >= alarmSetpoint[currentSensor] && data_[1] == 0)          // Проверка на превышение второго порога  Alarm
	{
		gaugeData[currentSensor * 2] = alarm;                                       // Превышен второй порог, фон датчика красный


		//  IEC104data_M_SP[2 * currentSensor] = alarm;



		if (logRed[currentSensor] == 0)
		{
			logRed[currentSensor] = 1;
			sdString = "Газоанализатор -> ";
			sdString += currentSensor;
			sdString += " > ";
			sdString += alarmSetpoint[currentSensor];
			sdString += ". Авария, превышен второй порог.";

			printData();
		}

	}



	if (data_[0] >= warningSetpoint[currentSensor] &&  data_[0] < alarmSetpoint[currentSensor] && data_[1] == 0 && gaugeData[currentSensor * 2] != alarm)                   // Если показания превысили первый порог но не привысели второй - ЖЕЛТАЯ ЗОНА
	{
		gaugeData[currentSensor * 2] = warning;                                        // Превышен первый порог, фон датчика желтый



		//  IEC104data_M_SP[2 * currentSensor] = warning;


		if (logYellow[currentSensor] == 0)
		{
			logYellow[currentSensor] = 1;
			sdString = "Газоанализатор ->";
			sdString += currentSensor;
			sdString += " > ";
			sdString += warningSetpoint[currentSensor];
			sdString += ". Превышен первый порог.";

			printData();
		}

		if (logRed[currentSensor] == 1)
		{
			logRed[currentSensor] = 0;
			sdString = "Газоанализатор ->";
			sdString += currentSensor;
			sdString += " < ";
			sdString += alarmSetpoint[currentSensor];
			sdString += ".  Концентрация упала ниже второго порога";

			printData();
		}

	}
	// gaugeData[currentSlave_1 * 2] = gaugeData[currentSlave_1 * 2]; //| (SensorStatusByte[currentSlave_1] << 2);
}





















byte read_Input_registers(byte flow_, int16_t start_addr, byte count, int16_t *data_)
{
	int16_t function = 0x04;  /* Function: Read Holding Registers */
	int16_t ret;

	byte slave = currentSlave[flow_];



	//  unsigned char packet[REQUEST_QUERY_SIZE + CHECKSUM_SIZE];
	byte port_num = S_PORT[flow_];

	switch (serialFlag[flow_])
	{

		case taskComplete:
		serialFlag[flow_] = sendMsg;
		return 0;
		break;



		case sendMsg:  // Send packet

		build_request_packet(slave, function, start_addr, count, packet[flow_]);

		//	flushPort(flow_);
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

				serialFlag[flow_] = msgDone;
				return 1;  // TimeOut - not responded

			}

			return 0;  // do nothing
			break;


			default: // Respond received

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



byte gasPipeStatus[30];


byte IGAS_52_Sequence = 0;
//byte valveArray[];
byte currentPipe = 1;
unsigned long valvesTimer = 0;


byte pipeSlice = 0;
byte pipeDO = 0;
byte const pumpDO = B00000001;
byte const tankDO = B00000010;

void getPipeDo(byte currentPipe_)
{
	switch (currentPipe_)
	{
		case 1 ... 5:   // LDO3 - valves 0 - 6
		pipeSlice = 3;  // LDO 3
		pipeDO = B00000100 << currentPipe_;   // Pipe 1 - Starting from relay 3


		break;


		case 6 ... 12:   // LDO3 - valves 0 - 6
		pipeSlice = 4;  // LDO 3
		pipeDO = B00000001 << (currentPipe_ - 6);   // Pipe 6 - Starting from first relay

		break;
	}
}


//
//
// void actPipeValve(byte action_)
// {
// 	slaveX2X.push(3);
// 	updateSchematic = update;
//
// 	switch (action_)
// 	{
// 		case 0:
// 		if (pipeSlice == 3)
// 		X2X_Data_Array[3][Data_0] &= ~pipeDO;
// 		else
// 		{
// 			slaveX2X.push(4);
// 			X2X_Data_Array[4][Data_0] &= ~pipeDO;
// 		}
// 		break;
//
// 		case 1:
// 		if (pipeSlice == 3)
// 		X2X_Data_Array[3][Data_0] |= pipeDO;
// 		else
// 		{
// 			slaveX2X.push(4);
// 			X2X_Data_Array[4][Data_0] |= pipeDO;
// 		}
// 		break;
//
// 	}
//
// }
//
// void actServiceVlv(byte vavle_, byte action_)
// {
// 	if (action_)
// 	X2X_Data_Array[3][Data_0] |= vavle_ ;
// 	else
// 	X2X_Data_Array[3][Data_0] &= ~vavle_ ;
//
// 	slaveX2X.push(3);
//
// 	updateSchematic = update;
// }



// byte leakCheck = 0;


byte pneumoInService = 0;
byte leakMode = 0;
byte pneumoSubTask = 0;

enum
{
	standbyMode,
	pumpLeakMode,
	pipeLeakMode,
};


enum
{
	leakOpenValves,

};


byte checkPneumoErrorFlag = 0;

byte pneumoDebugMode = 0;



byte airPressureOk = 1;
byte pipeBlocked = 1;

/*
byte IGAS_52()
{
byte airSwitch = Data_0;
byte vacSwitch = Data_1;
switch (IGAS_52_Sequence)
{

case serviceCheck:

//	if (checkPneumoError())
//	break;

if (enabled[currentSlave[RS1]] == 0)  // If detector disabled
{
gaugeData[currentSlave[RS1] * 2 - 1] = hide;     // Hide zero on HMI main screen
gaugeData[currentSlave[RS1] * 2] = ok;           // Color - ok

nextSlave(RS1);      // Next slave
return 0;
}


if (pneumoDebugMode)
{
SerialUSB.print("Current Pipe -> ");
SerialUSB.println(currentSlave[RS1]);
SerialUSB.println("1. checkPneumoError complete");
}




if (pneumoDebugMode)
{
SerialUSB.println("2. pneumoInService complete");
SerialUSB.println("3. serviceCheck");
}
IGAS_52_Sequence++;

break;

case openPipeValve:

currentPipe = currentSlave[RS1];
valvesTimer = currentTime;
getPipeDo(currentPipe); // Get Pipe valve address
actPipeValve(openValve);
pneumoData[startVlvEnum + currentPipe * 2] = vlvOpen;
HMIqueue.push(updatePneumo);
IGAS_52_Sequence++;

if (pneumoDebugMode)
SerialUSB.println("4. openPipeValve");
break;


case checkPipe:

if (pneumoDebugMode)
SerialUSB.println("5. checkPipe -> Start");

if (currentTime - valvesTimer > stepDelay)
{
if (X2X_Data_Array[5][airSwitch] == airPressureOk)   // Air pressure ok
pneumoData[airRelay] = state_Ok;
else
pneumoData[airRelay] = state_Fault;



HMIqueue.push(updatePneumo);

IGAS_52_Sequence++;

if (pneumoDebugMode)
SerialUSB.println("5. checkPipe");
}
break;



case openPumpValve:

if (pneumoDebugMode)
SerialUSB.println("6. openValves");

actServiceVlv(pumpDO, openValve);
actServiceVlv(tankDO, openValve);
actPipeValve(openValve);

pneumoData[airValve] = vlvOpen;
pneumoData[vacValve] = vlvOpen;
pneumoData[startVlvEnum + currentPipe * 2] = vlvOpen;

HMIqueue.push(updatePneumo);
valvesTimer = currentTime;
IGAS_52_Sequence++;

if (pneumoDebugMode)
SerialUSB.println("7. openPumpValve finished");

break;

case pumpDown:
if (currentTime - valvesTimer > pumpingTime)
{
IGAS_52_Sequence++;
if (pneumoDebugMode)
SerialUSB.println("8. pumpDown");
}

break;

case closeTankValve:

actServiceVlv(tankDO, closeValve);
pneumoData[vacValve] = vlvClosed;

HMIqueue.push(updatePneumo);
valvesTimer = currentTime;



if (X2X_Data_Array[5][vacSwitch] != pipeBlocked)   // Pipeline is blocked
{
pneumoData[vacRelay] = state_Ok;
pneumoData[startVlvEnum + currentPipe * 2 + 1] = state_Ok;
}
else
{
pneumoData[vacRelay] = state_Fault;
pneumoData[startVlvEnum + currentPipe * 2 + 1] = state_Fault;

//	gaugeData[currentPipe * 2 - 1] = hide; // Показания датчика
//	gaugeData[currentPipe * 2] = fault; // Ошибка линии

}
HMIqueue.push(updatePneumo);
HMIqueue.push(updateFrontPage);
IGAS_52_Sequence++;

if (pneumoDebugMode)
SerialUSB.println("9. closeTankValve");


break;

case closePumpValve:

if (currentTime - valvesTimer > tankDelayTime)
{
actServiceVlv(pumpDO, closeValve);

pneumoData[airValve] = vlvClosed;
HMIqueue.push(updatePneumo);

valvesTimer = currentTime;
IGAS_52_Sequence++;
valvesTimer = currentTime;

if (pneumoDebugMode)
SerialUSB.println("10. closePumpValve");

}
break;




case stabilizing:

if (currentTime - valvesTimer > stabilizingTime)
{
// Stabilizing pressure

valvesTimer = currentTime;
IGAS_52_Sequence++;

if (pneumoDebugMode)
SerialUSB.println("11. stabilizing");
}
break;


case gasMeasurements:

if  (serviceMode == 1)
{
IGAS_52_Sequence++;
break;
}


if (getDetectorData(RS1, 0, data_S1))
{
// Get gas Measurements
valvesTimer = currentTime;
IGAS_52_Sequence++;

}

break;

case finished:

if (currentTime - valvesTimer > idleTime)
{
actPipeValve(closeValve);

pneumoData[startVlvEnum + currentPipe * 2] = vlvClosed;
HMIqueue.push(updatePneumo);

IGAS_52_Sequence = 0;

if (pneumoDebugMode)
{
SerialUSB.println("Cycle finished *********");
SerialUSB.println();
}


}
break;
}

}

*/

unsigned long debugTimer = 0;


/*
byte checkBeaconTimer()
{
if (AlarmSoundOff == 1)
if (currentTime - soundTimer >  BuzzerOffTime)
{
AlarmSoundOff = 0;
UpdateRelays();
slaveX2X.push(2);
HMIqueue.push(updateResetBeacon);
}

}
*/


//
//
// byte hmiStateBuffer = 0;
//
// byte HMITasker()
// {
// 	//*************************************************************************
// 	//               HMI -> Tasker
// 	//*************************************************************************
//
//
// 	//  Check HMI Connected!  Start
//
// 	if (HMIstatus[currentSlave[HMI]] != ok && taskFlag[HMI] == taskComplete)
// 	{
// 		HMIqueue.push(updateInitHMI);   // Push Task
//
//
// 		if (currentTime - HMIupdTimer > 500)
// 		{
// 			updateColor();
// 			UpdateRelays();
// 			//	moveDataIEC();  // Enable for IEC104
// 			HMIupdTimer = currentTime;
// 		}
// 		if (HMItaskerDebug)
// 		SerialUSB.println("HMI Not Found");
// 	}
// 	//  Finish Check HMI Connected!
//
//
// 	// Time machine -> put timeout task
//
// 	if ((currentTime - HMIupdTimer > 500) && (btnState == updated))   // Push Task - "get Pressed Btn" each 100 ms
// 	{
// 		if (HMIstatus[currentSlave[HMI]] == ok)
// 		{
//
// 			HMIqueue.push(updateButtons);  // Push task to queue
// 			HMIqueue.push(updateFrontPage);
//
// 			HMIupdTimer = currentTime;     // Restart upd timer
// 			btnState = inProgress;		   // Upd btn state - inProgress - prevent second PUSH before complete
// 		}
//
// 		updateColor();
// 		UpdateRelays();
// 		//	moveDataIEC();  // Enable for IEC104
//
// 		//	checkBeaconTimer();
// 		if (HMItaskerDebug)
// 		SerialUSB.println("Push New Task");
// 	}
// 	// Finish
//
//
// 	if (taskFlag[HMI] == taskComplete)	  // If Task complete HMI in IDLE mode - check for new task
// 	{
// 		if (HMIqueue.isEmpty() == FALSE)	  // Is there any Task for HMI?
// 		{
// 			//SerialUSB.println("Proceed New Task");
// 			HMITask = HMIqueue.shift();		  // Move Task to proceed
// 			taskFlag[HMI] = inProgress;	  // Change Task status - inProgress
//
// 			if (HMItaskerDebug)
// 			{
// 				SerialUSB.print("HMITask -> ");
// 				SerialUSB.println(HMITask);
// 			}
// 		}
// 		else return 0;						  // If nothing to do - break and wait for any Task!
// 	}
//
//
// 	switch (HMITask)
// 	{
//
// 		case standByCase:
//
// 		break;
//
//
// 		case updateInitHMI:
// 		{
// 			initHMI();
//
// 			if (HMIstatus[currentSlave[HMI]] == lost)
// 			{
// 				if (logHMI[currentSlave[HMI]] == 1)
// 				{
// 					logHMI[currentSlave[HMI]] = 0;
//
// 					sdString = "Панель -> ";
// 					sdString += currentSlave[HMI];
// 					sdString += " потеряна связь";
//
// 					printData();
// 				}
// 				currentSlave[HMI]++;
// 			}
// 			if (currentSlave[HMI] > totalDisplays)
// 			currentSlave[HMI] = 1;
// 		}
// 		break;
//
//
// 		case updateFrontPage:  // Send Sensor Data to HMI ***************************************
//
// 		/*
// 		if (serialFlag[HMI] == taskComplete)
// 		{
//
// 		for (byte i=1; i<25; i++)
// 		if (pneumoData[startVlvEnum + i*2 + 1] == vlvAlarm)
// 		{
//
// 		gaugeData[i*2 - 1] = hide;
// 		gaugeData[i*2] = blocked;
// 		gaugeData[0] = HMIRedScreen;
//
// 		SerialUSB.print(i);
// 		SerialUSB.print(" -> ");
// 		SerialUSB.print(gaugeData[i*2 - 1]);
// 		SerialUSB.print(" -> ");
// 		SerialUSB.println(gaugeData[i*2]);
// 		SerialUSB.println("***************");
//
// 		//	gaugeData[currentSensor * 2 - 1] = hide;                // HMI: Датчик не ответил - надпись на поле датчика "не подключен"- код " - 1"
// 		//	gaugeData[currentSensor * 2] = lost;
// 		}
// 		}
// 		*/
//
// 		switch (preset_multiple_registers(HMI, 0, (2 * totalSensors + 1), processedData))
// 		{
// 			case 1: // not responded
//
// 			HMIstatus[currentSlave[HMI]] = lost;
// 			if (HMItaskerDebug)
// 			{
// 				SerialUSB.print("LOST -> ");
// 				SerialUSB.println(currentSlave[HMI]);
// 			}
// 			//   subTask_HMI = 0;
// 			break;
//
// 			case portReady:
// 			// SerialUSB.println("updateFrontPage ->  DONE");
// 			taskFlag[HMI] = taskComplete; // Ready for new task
// 			break;
// 		}
// 		break;
//
//
// 		case updateSliceStatus: // Send relay module status data to HMI
//
// 		if (serialFlag[HMI] == taskComplete)
// 		{
// 			data_HMI[0] = BR_Fail[0];
// 		}
//
//
// 		//SerialUSB.println(char_ch1[0]);
// 		switch (preset_multiple_registers(HMI, 200, 2, data_HMI)) // Preset registers
// 		{
// 			case 1:  // Lost
//
// 			if (HMItaskerDebug)
// 			{
// 				SerialUSB.print("LOST -> ");
// 				SerialUSB.println(currentSlave[HMI]);
// 			}
// 			HMIstatus[currentSlave[HMI]] = lost;
// 			break;
//
// 			case portReady:
// 			taskFlag[HMI] = taskComplete; // Ready for new task
// 			break;
//
// 		}
// 		break;
//
//
// 		case updateIPtable:  // Update Ethernet Clients
//
// 		if (IEC104_E1.ipUpdate != 0 || mb1.ipUpdateMb != 0)  //  New IP addresses?
// 		{
// 			if (IEC104_E1.ipUpdate == 1 || mb1.ipUpdateMb == 1)  // Move IP data base to UART buffer
// 			{
// 				byte cnt104 = 0;
// 				for (byte i = 0; i < 6; i++)
// 				for (byte j = 0; j < 4; j++)
// 				{
//
// 					// start Added for MB TCP
// 					if (Ethernet1Mode == TCPIP)
// 					{
// 						data_HMI[cnt104] = mb1.IPlistMB[i][j];
// 					}
// 					else
// 					// finish Added for MB TCP
//
// 					data_HMI[cnt104] = IEC104_E1.IPlist[i][j];
// 					cnt104++;
// 				}
// 				mb1.ipUpdateMb == 2;
// 				IEC104_E1.ipUpdate = 2;   // Move data to buffer once for each update
// 			}
//
// 			if (preset_multiple_registers(HMI, 301, 24, data_HMI) == portReady) // Preset registers
// 			{
// 				IEC104_E1.ipUpdate = 0;  // Done - standby IP update to HMI
// 				mb1.ipUpdateMb = 0;
// 				taskFlag[HMI] = taskComplete; // Ready for new task
// 			}
// 		}
// 		break;
//
//
// 		case updateDateTime:  // Any time SYNC from IEC 104?
//
// 		if (rtc_clock.rtcFlag == RTC_FLAG_VALID) // Any time SYNC from IEC 104?
// 		{
// 			switch (rtcTask)
// 			{
// 				case 0:  // Preset time and date
// 				if (serialFlag[HMI] == taskComplete)
// 				{
// 					//	SerialUSB.println("Preset data HMI");
// 					data_HMI[0] = rtc_clock.get_minutes();
// 					data_HMI[1] = rtc_clock.get_hours();
// 					data_HMI[2] = rtc_clock.get_days();
// 					data_HMI[3] = rtc_clock.get_months();
// 					data_HMI[4] = rtc_clock.get_years();
// 					data_HMI[5] = 1; // Set
// 				}
//
// 				if (preset_multiple_registers(HMI, 401, 6, data_HMI) == portReady)
// 				rtcTask = 1;
//
// 				break;
//
// 				case 1:  // Trigger Time data to move to HMI RTC
//
// 				if (serialFlag[HMI] == taskComplete)
// 				data_HMI[0] = 16;
//
// 				if (preset_multiple_registers(HMI, 1802, 1, data_HMI) == portReady)
// 				{
// 					rtc_clock.rtcFlag = RTC_FLAG_IDLE;
// 					HMITask = updateResetBtns; // Reset command buttons
// 					rtcTask = 0;  // Reset rtc HMI update task
// 					//	SerialUSB.println("HMI RTC Done");
// 				}
// 				break;
//
// 			}
// 		}
// 		break;
//
//
//
// 		case updateDetectorStatus: // Send relay module status data to HMI
//
// 		if (serialFlag[HMI] == taskComplete)
// 		{
// 			data_HMI[0] = gaugeData[currentSlave[RS1] * 2 - 1];
// 		}
//
//
// 		//SerialUSB.println(char_ch1[0]);
// 		switch (preset_multiple_registers(HMI, 1381, 1, data_HMI)) // Preset registers
// 		{
// 			case 1:  // Lost
//
// 			if (HMItaskerDebug)
// 			{
// 				SerialUSB.print("LOST -> ");
// 				SerialUSB.println(currentSlave[HMI]);
// 			}
// 			HMIstatus[currentSlave[HMI]] = lost;
// 			break;
//
// 			case portReady:
// 			HMITask = updateFrontPage;
// 			break;
//
// 		}
// 		break;
//
//
// 		case updateResetBeacon: // Send relay module status data to HMI
//
// 		if (serialFlag[HMI] == taskComplete)
// 		{
// 			data_HMI[0] = AlarmSoundOff;
// 			//	SerialUSB.println("ResetBeacon");
// 		}
//
// 		//SerialUSB.println(char_ch1[0]);
// 		switch (preset_multiple_registers(HMI, 1807, 1, data_HMI)) // Preset registers
// 		{
// 			case 1:  // Lost
//
// 			if (HMItaskerDebug)
// 			{
// 				SerialUSB.print("LOST -> ");
// 				SerialUSB.println(currentSlave[HMI]);
// 			}
// 			HMIstatus[currentSlave[HMI]] = lost;
// 			break;
//
// 			case portReady:
// 			HMITask = updateFrontPage;
// 			break;
//
// 		}
// 		break;
//
//
// 		case updateResetBtns: // Reset all pushed command buttons on HMI
// 		resetBtns();
// 		break;
//
//
// 		case updateButtons:  // Check if any command btn pressed on HMI
// 		checkNewCommand();
// 		break;
//
//
//
// 		case updateRelayToHMI: // Show relay config on HMI panel
// 		relayToHMI();
// 		break;
//
// 		case updateSaveRelay:  // Save Relay confog from HMI to Memory card
// 		saveRelayToMemory();
// 		break;
//
// 		case updateEnableDetLoad:  // Show Gas Type config on HMI panel
// 		gasTypesToHMI();
// 		break;
//
// 		case updateEnableDetSave: // Enable or disable Detectors in system
// 		enableDisableDetector();
// 		break;
//
// 		case updateSetRTC: // Set date and time
// 		setRTCdata();
// 		break;
//
// 		case updateSerialLoad: // Send Serial line port number to HMI
// 		serialPortsToHMI();
// 		break;
//
// 		case updateSerialSave:  // Save new port number from HMI to SD memory card
// 		saveSerialPortToSD();
// 		break;
//
// 		case updateBeaconCommand:  // Turn off alarm sound relays
// 		switchOffSound();
//
// 		break;
//
//
// 		case updateRelayReset: // Reset all pushed command buttons on HMI
// 		resetManualRelay();
// 		break;
//
// 		case updateEmulationCommand:  // Emulate Signal from detector
// 		emuleDetector();
// 		break;
//
// 		case updateTestRelayCommand: // Reset all pushed command buttons on HMI
// 		setManualRelay();
// 		break;
//
// 		case updateSrvRelayLoad: // Reset all pushed command buttons on HMI
// 		loadServiceRelay();
// 		break;
//
// 		// Added in 2_0_5 Version
//
// 		case updateSrvRelaySave: // Reset all pushed comand buttons on HMI
// 		saveServiceRelay();
// 		break;
//
// 		case updateRebootPLC:  // reboot plc
// 		softReset();
// 		break;
//
// 		case updateToolBox: // Reset all pushed command buttons on HMI
// 		toolBox();
// 		break;
//
// 		case updateProtocol:
// 		outProtocol();
// 		break;
//
//
//
// 		default: // Go back to start
// 		/*
// 		currentSlave[HMI]++;
// 		if (currentSlave[HMI] > totalDisplays)
// 		currentSlave[HMI] = 1;
// 		*/
//
// 		break;
// 	}
//
// }

/*
1801.0 - activate Toolbox
1801.1 - Activate IEC104/Modbus TCP

1802.0 - Load Relay settings
1802.1 - Save Relay settings
1802.2 - Load Enabled/Disabled detectors
1802.3 - Save Enabled/Disabled detectors
1802.4 - Set Time and Date
1802.5 - Load Serial Ports
1802.6 - Save Serial Ports
1802.7 - Silence beacon

1803.0 - on Change Toolbox tilt
1803.1 - reset Manual Relay Mode
1803.2 - accept emulation mode
1803.3 - Set test relay button
1803.4 - Load Service Relay Settings
1803.5 - Save Service Relay Settings
1803.6 - Reboot system
1803.7 - Choose IEC104
*/



/*
1. check if gas pipe status is OK
2. Open compressed air valve and tank valve
3. Pump for 7 seconds (tunable)
4. Close tank valve
5. 1sec delay
6. Close compressed air
7. 10 sec delay
8. get measurements

Continuously check:
1. Compressed air relay
2. Vacuum relay
a) Has to be open all the time
b) if vacuum - line is blocked

Each 5 cycle:
check compressor time (change oil)
check leak

*/


// HMITask = updateResetBtns;
/*
enum
{
closeAllValves,
pumpDown,
leakTest,

};
*/


/*
void leakCheck()
{
switch ()
{
case closeAllValves:

break;

}

}
*/




/*
int graytobin(int grayVal, int nbits ) {
// Bn-1 = Bn XOR Gn-1   source of method:  http://stackoverflow.com/questions/5131476/gray-code-to-binary-conversion but include correction
int binVal = 0;
bitWrite(binVal, nbits - 1, bitRead(grayVal, nbits - 1)); // MSB stays the same
for (int b = nbits - 1; b > 0; b-- ) {
// XOR bits
if (bitRead(binVal, b) == bitRead(grayVal, b - 1)) { // binary bit and gray bit-1 the same
bitWrite(binVal, b - 1, 0);
}
else {
bitWrite(binVal, b - 1, 1);
}
}
return binVal;
}

*/


// SD










void SD_Clear(File dir)
{

	while (true)
	{

		File entry =  dir.openNextFile();

		if (! entry)
		{
			// no more files
			break;
		}

		/*	for (uint8_t i = 0; i < numTabs; i++)
		{

		SerialUSB.print('\t');

		}
		*/
		SD.remove(entry.name());

		// 		if (entry.isDirectory())
		// 		{
		//
		// 			SerialUSB.println("/");
		//
		// 			printDirectory(entry, numTabs + 1);
		//
		// 		}
		// 		else
		// 		{
		//
		// 			// files have sizes, directories do not
		//
		// 			SerialUSB.print("\t\t");
		//
		// 			SerialUSB.println(entry.size(), DEC);
		//
		// 		}

		entry.close();

	}
}




void printDirectory(File dir, int16_t numTabs)
{

	while (true)
	{

		File entry =  dir.openNextFile();

		if (! entry)
		{
			// no more files
			break;
		}

		/*	for (uint8_t i = 0; i < numTabs; i++)
		{

		SerialUSB.print('\t');

		}
		*/
		SerialUSB.print(entry.name());

		if (entry.isDirectory())
		{

			SerialUSB.println("/");

			printDirectory(entry, numTabs + 1);

		}
		else
		{

			// files have sizes, directories do not

			SerialUSB.print("\t\t");

			SerialUSB.println(entry.size(), DEC);

		}

		entry.close();

	}
}










void modbusProceed(int16_t *regs,  int16_t start_addr_,  int16_t reg_count_, int16_t Value_)
{
	int16_t registerAddr_ = start_addr_;
	//	byte addr_;
	byte writeReg = 0;
	if (IGAS_mb_X.func == FC_WRITE_REGS || IGAS_mb_X.func == FC_WRITE_REG)
	writeReg = 1;


	for (byte i = 0; i <= reg_count_ - 1; i++)
	{
		switch (registerAddr_)
		{
			// Bar abs. pressure_Bar

			case 0:

			//	regs[i] = convertFloat(pressure_Bar, 0);

			break;


			case 97:   // Reboot

			if (writeReg)
			{
				root = SD.open("/");
				printDirectory(root,0);
			}

			regs[i] = 0;

			break;


			case 98:   // Delete files on sd

			if (writeReg)
			{
				root = SD.open("/");
				SD_Clear(root);
			}
			else
			regs[i] = 0;

			break;


			case 99:   // Создать файл с содержанием из регистров 100 - 150 и названием с регистра 151-165

			if (writeReg)
			{
				//delay(4000);
				// 				SDname[0] = 88;
				// 				SDname[1] = 50;
				// 				SDname[2] = 88;
				// 				SDname[3] = 46;
				// 				SDname[4] = 116;
				// 				SDname[5] = 120;
				// 				SDname[6] = 116;

				// 				SerialUSB.print("File -> ");
				// 				SerialUSB.println(SDname);
				// 				SerialUSB.print("Lines -> ");
				// 				SerialUSB.println(SD_lines);

				// 				SerialUSB.print("Lines -> ");
				// 				SerialUSB.println(SD_lines);

				saveConfig(SDname, outArray, SD_lines);
				//	saveConfig("X2X.txt", outArray, SD_lines);
				//	root = SD.open("/");
				//SD_Clear(root);
				memset(SDname, 0, sizeof(SDname)); // Обнуление массива SDname

				regs[i] = 99;
			}
			else
			regs[i] = 0;

			break;


			case 100:	//	Lines quantity


			if (writeReg)
			SD_lines = Value_;

			regs[i] = SD_lines;
			break;

			case 101 ... 150: //	data

			if (writeReg)
			outArray[registerAddr_ - 100] = Value_;

			regs[i] = outArray[registerAddr_ - 100];
			break;

			case 151 ... 185: //	File name


			if (writeReg)
			SDname[registerAddr_ - 151] = Value_;

			regs[i] = SDname[registerAddr_ - 151];

			break;





			case 600:
			break;

			case 800:   // PinCode Sevice

			if (writeReg)
			{
				// 				if (Value_ == 2210)
				;// 				pinCode = 1;
				// 				else pinCode = 0;
			}


			//			regs[i] = pinCode;

			break;




			case 900:   // BootLoader

			//	noInterrupts();
			softReset();
			//		GoToBootloader();

			break;




			default:
			regs[i] = -1;     // reply -1, if option is not available.
			break;
		}
		registerAddr_++;
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

byte regsNoRespond(byte flow_)
{

	byte port_num = S_PORT[flow_];
	serialFlag[flow_] = msgDone;

	byte currentSensor = currentSlave[flow_];

	//if (flow_ != RS1)
	if (lostPackets[currentSensor] < LostPacketError)
	{
		lostPackets[currentSensor]++;
		return (0);
	}

	startPollTimer[flow_] = millis();

	gaugeData[currentSensor * 2 - 1] = hide;                // HMI: Датчик не ответил - надпись на поле датчика "не подключен"- код " - 1"
	gaugeData[currentSensor * 2] = lost;                     // HMI: Датчик не ответил - цвет поля красный - код 0                                                                    // HMI: Датчик не ответил - цвет общего фона красный - код B00000001
	gaugeErrorCode[currentSensor] = 0;


	// SerialUSB.println("detector lost");
	//   if (Ethernet1Mode == TCPIP)
	//     mb1.Ireg(currentSensor-1, 0);

	// 	for (byte i = 0; i < 10; i++)
	// 	X2X_Data_Array[currentSlave[X2X]][i + Data_0];



	//  IEC104data_M_SP[2 * currentSensor - 1] = 0;
	//  IEC104data_M_SP[2 * currentSensor] = lost;



	if (logDisconnect[currentSensor] == 0)
	{
		logDisconnect[currentSensor] = 1;
		sdString = "Газоанализатор ->";
		sdString += currentSensor;
		sdString += " не отвечает";
		printData();
		HMIqueue.push(updateDetectorStatus);
	}

}



byte regsResponded(byte flow_, int16_t *data_)
{

	byte currentSensor = currentSlave[flow_];
	byte port_num = S_PORT[flow_];

	serialFlag[flow_] = msgDone;
	startPollTimer[flow_] = millis();   // Записать текущее время и засечь интервал
	lostPackets[currentSensor] = 0;
	gaugeData[currentSensor * 2 - 1] = data_[0]; // Показания датчика
	gaugeErrorCode[currentSensor] = data_[1]; // Ошибка датчика


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



	if (gaugeErrorCode[currentSensor] != 0)
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



	if (logError[currentSensor] == 1 &&  gaugeErrorCode[currentSensor] == 0)
	{
		logError[currentSensor] = 0;
		sdString = "Газоанализатор ->";
		sdString += currentSensor;
		sdString += ". Все ошибки сняты ";
		sdString += data_[1];

	}



	if (data_[0] < warningSetpoint[currentSensor] && data_[1] == 0)                 // Показания не превышают первый порог?
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
			sdString += warningSetpoint[currentSensor];
			sdString += ". Концентрация ниже первого порога";

			printData();
		}
	}


	if (data_[0] >= alarmSetpoint[currentSensor] && data_[1] == 0)          // Проверка на превышение второго порога  Alarm
	{
		gaugeData[currentSensor * 2] = alarm;                                       // Превышен второй порог, фон датчика красный




		//  IEC104data_M_SP[2 * currentSensor] = alarm;



		if (logRed[currentSensor] == 0)
		{
			logRed[currentSensor] = 1;
			sdString = "Газоанализатор -> ";
			sdString += currentSensor;
			sdString += " > ";
			sdString += alarmSetpoint[currentSensor];
			sdString += ". Авария, превышен второй порог.";

			printData();
		}

	}



	if (data_[0] >= warningSetpoint[currentSensor] &&  data_[0] < alarmSetpoint[currentSensor] && data_[1] == 0 && gaugeData[currentSensor * 2] != alarm)                   // Если показания превысили первый порог но не привысели второй - ЖЕЛТАЯ ЗОНА
	{
		gaugeData[currentSensor * 2] = warning;                                        // Превышен первый порог, фон датчика желтый



		//  IEC104data_M_SP[2 * currentSensor] = warning;


		if (logYellow[currentSensor] == 0)
		{
			logYellow[currentSensor] = 1;
			sdString = "Газоанализатор ->";
			sdString += currentSensor;
			sdString += " > ";
			sdString += warningSetpoint[currentSensor];
			sdString += ". Превышен первый порог.";

			printData();
		}

		if (logRed[currentSensor] == 1)
		{
			logRed[currentSensor] = 0;
			sdString = "Газоанализатор ->";
			sdString += currentSensor;
			sdString += " < ";
			sdString += alarmSetpoint[currentSensor];
			sdString += ".  Концентрация упала ниже второго порога";

			printData();
		}

	}
	// gaugeData[currentSlave_1 * 2] = gaugeData[currentSlave_1 * 2]; //| (SensorStatusByte[currentSlave_1] << 2);
}




byte saveScope(byte slave_)
{
	String sdLine;
	char fileName[] ="1X010101.txt";
	int daysR_ = rtc_clock.get_days();
	int monthR_ = rtc_clock.get_months();
	int yearR_ = rtc_clock.get_years();
	
	int buf_;


	fileName[0] = slave_+'0';
	buf_ = (yearR_-2000)*0.1;
	fileName[2] = buf_+'0';
	fileName[3] = (yearR_-2000 - buf_*10) + '0';
	buf_ = monthR_* 0.1;
	fileName[4] = buf_+'0';
	fileName[5] = (monthR_ - buf_*10) + '0';
	buf_ = daysR_*0.1;
	fileName[6] = buf_+'0';
	fileName[7] = (daysR_ - buf_*10) + '0';

	/*
	while (SD.exists(fileName))
	{
	SD.remove(fileName);
	}
	*/
	//	else return (0);


	File file_ = SD.open(fileName, FILE_WRITE);
	
	if (lctDebug)
	{
		SerialUSB.print("Sd file name -> ");
		SerialUSB.println(fileName);
	}

	if (!file_) {
		SerialUSB.println("File not exist ");
		file_.close();
		return (0);
	}

	if (lctDebug)
	{
		SerialUSB.print("Sd file open successfully-> ");
		SerialUSB.println(fileName);
	}

	file_.print(rtc_clock.get_hours());
	file_.print(": ");
	file_.print(rtc_clock.get_minutes());
	file_.print(": ");
	file_.println(rtc_clock.get_seconds());
	// 	file_.print(" ");
	// 	file_.print(daynames[rtc_clock.get_day_of_week() - 1]);
	// 	file_.print(": ");
	file_.print(rtc_clock.get_days());
	file_.print(".");
	file_.print(rtc_clock.get_months());
	file_.print(".");
	file_.println(rtc_clock.get_years());
	file_.println("");


	for (byte i = 0; i < 200; i++)
	{
		dataString = String(scopeArray[scopeA][i]);
		file_.print(dataString);
		file_.print(" ");
		dataString = String(scopeArray[scopeB][i]);
		file_.print(dataString);
		file_.print(" ");
		dataString = String(scopeArray[scopeC][i]);
		file_.print(dataString);
		file_.println(" ");
		// 		dataString = String(scopeArray[encoder][i]);
		// 		file_.println(dataString);

	}

	file_.println("");
	file_.println("Fin");   // Last String - "Fin"

	file_.close();

	return (1);
}





inline void addNewASDU(uint16_t selection, uint16_t ASDU_)
{
	if (newASDU_Debug)
	{
		SerialUSB.print("Module ID -> "); SerialUSB.println(selection);
	}
	
	switch (selection)
	{
		case LCP:
		{
			auto* dev = new LCP1116(); memset(dev,0,sizeof(*dev));
			dev->ID=selection; dev->ASDU=ASDU_; dev->ConnectLost=false; dev->deviceFault=false;
			system_Devices[totalNodes] = dev;
			if (newASDU_Debug) {
				SerialUSB.print("Added LCP1116 at index ");
				SerialUSB.print(totalNodes);
				SerialUSB.print(" ID="); SerialUSB.print(dev->ID);
				SerialUSB.print(" ASDU="); SerialUSB.println(dev->ASDU);
			}
			++totalNodes;
			break;
		}
		case LDO:
		{
			auto* dev = new LDO1118(); memset(dev,0,sizeof(*dev));
			dev->ID=selection; dev->ASDU=ASDU_; dev->ConnectLost=true; dev->deviceFault=false;
			system_Devices[totalNodes] = dev;
			if (newASDU_Debug) {
				SerialUSB.print("Added LDO1118 at index ");
				SerialUSB.print(totalNodes);
				SerialUSB.print(" ID="); SerialUSB.print(dev->ID);
				SerialUSB.print(" ASDU="); SerialUSB.println(dev->ASDU);
			}
			++totalNodes;
			break;
		}
		case LAI: {
			auto* dev = new LAI1118(); memset(dev,0,sizeof(*dev));
			dev->ID=selection; dev->ASDU=ASDU_; dev->ConnectLost=true; dev->deviceFault=false;
			system_Devices[totalNodes] = dev;
			if (newASDU_Debug) {
				SerialUSB.print("Added LAI1118 at index ");
				SerialUSB.print(totalNodes);
				SerialUSB.print(" ID="); SerialUSB.print(dev->ID);
				SerialUSB.print(" ASDU="); SerialUSB.println(dev->ASDU);
			}
			++totalNodes;
			break;
		}
		case LDI8: {
			auto* dev = new LDI1118(); memset(dev,0,sizeof(*dev));
			dev->ID=selection; dev->ASDU=ASDU_; dev->ConnectLost=true; dev->deviceFault=false;
			system_Devices[totalNodes] = dev;
			if (newASDU_Debug) {
				SerialUSB.print("Added LDI1118 at index ");
				SerialUSB.print(totalNodes);
				SerialUSB.print(" ID="); SerialUSB.print(dev->ID);
				SerialUSB.print(" ASDU="); SerialUSB.println(dev->ASDU);
			}
			++totalNodes;
			break;
		}
		case LDI16: {
			auto* dev = new LDI1116(); memset(dev,0,sizeof(*dev));
			dev->ID=selection; dev->ASDU=ASDU_; dev->ConnectLost=true; dev->deviceFault=false;
			system_Devices[totalNodes] = dev;
			if (newASDU_Debug) {
				SerialUSB.print("Added LDI1116 at index ");
				SerialUSB.print(totalNodes);
				SerialUSB.print(" ID="); SerialUSB.print(dev->ID);
				SerialUSB.print(" ASDU="); SerialUSB.println(dev->ASDU);
			}
			++totalNodes;
			break;
		}
		case LCT:
		{
			auto* dev = new LCT1114(); memset(dev,0,sizeof(*dev));
			dev->ID=selection; dev->ASDU=ASDU_; dev->ConnectLost=true; dev->deviceFault=false;
			system_Devices[totalNodes] = dev;
			if (newASDU_Debug) {
				SerialUSB.print("Added LCT1114 at index ");
				SerialUSB.print(totalNodes);
				SerialUSB.print(" ID="); SerialUSB.print(dev->ID);
				SerialUSB.print(" ASDU="); SerialUSB.println(dev->ASDU);
			}
			++totalNodes;
			break;
		}
		
		case LCT_2:
		{
			auto* dev = new LCT1114_2(); memset(dev,0,sizeof(*dev));
			dev->ID=selection; dev->ASDU=ASDU_; dev->ConnectLost=true; dev->deviceFault=false;
			system_Devices[totalNodes] = dev;
			if (newASDU_Debug) {
				SerialUSB.print("Added LCT1114_2 at index ");
				SerialUSB.print(totalNodes);
				SerialUSB.print(" ID="); SerialUSB.print(dev->ID);
				SerialUSB.print(" ASDU="); SerialUSB.println(dev->ASDU);
			}
			++totalNodes;
			break;
		}
		
		case DM910:
		{
			auto* dev = new DM91_0(); memset(dev,0,sizeof(*dev));
			dev->ID=selection; dev->ASDU=ASDU_; dev->ConnectLost=true; dev->deviceFault=false;
			system_Devices[totalNodes] = dev;
			if (newASDU_Debug) {
				SerialUSB.print("Added DM910 at index ");
				SerialUSB.print(totalNodes);
				SerialUSB.print(" ID="); SerialUSB.print(dev->ID);
				SerialUSB.print(" ASDU="); SerialUSB.println(dev->ASDU);
			}

			++totalNodes;
			break;
		}
		
		case DM910H:
		{
			auto* dev = new DM91_H(); memset(dev,0,sizeof(*dev));
			dev->ID=selection; dev->ASDU=ASDU_; dev->ConnectLost=true; dev->deviceFault=false;
			system_Devices[totalNodes] = dev;
			if (newASDU_Debug) {
				SerialUSB.print("Added DM910H at index ");
				SerialUSB.print(totalNodes);
				SerialUSB.print(" ID="); SerialUSB.print(dev->ID);
				SerialUSB.print(" ASDU="); SerialUSB.println(dev->ASDU);
			}
			
			dev->LUR = 0;
			++totalNodes;
			break;
		}
		
		case PVT100:
		{
			auto* dev = new PVT_100(); memset(dev,0,sizeof(*dev));
			dev->ID=selection; dev->ASDU=ASDU_; dev->ConnectLost=true; dev->deviceFault=false;
			system_Devices[totalNodes] = dev;
			if (newASDU_Debug) {
				SerialUSB.print("Added PVT100 at index ");
				SerialUSB.print(totalNodes);
				SerialUSB.print(" ID="); SerialUSB.print(dev->ID);
				SerialUSB.print(" ASDU="); SerialUSB.println(dev->ASDU);
			}
			++totalNodes;
			break;
		}
		
		default:
		SerialUSB.println("Invalid selection.");
		break;
	}
}



uint8_t dataUpd(uint8_t prev_, uint8_t now_)
{
	if (now_ != prev_)  // деление на ноль и не равно старому значению - обновляем
	{
		return 1;
	}


	return 0;

}


uint8_t dataUpd(int16_t prev_, int16_t now_)
{
	if (now_ == 0 && prev_ != 0)  // деление на ноль и не равно старому значению - обновляем
	{
		return 1;
	}

	if (now_ == 0 && prev_ == 0) // оба нуля - не  обновляем
	{
		return 0;
	}

	if (now_ == prev_)			// значения равны - не обновляем
	return 0;

	float buffer = 1.0*(prev_ - now_)/now_;

	buffer = abs(buffer);

	if (buffer > 0.005)
	{
		//	SerialUSB.println(" change! ");
		return 1;
	}
	else
	{
		//SerialUSB.println(" not change ");
		return 0;
	}
}


uint8_t dataUpd(float prev_, float now_)
{
	if (dmDebugSpont)
	{
		SerialUSB.println();
		SerialUSB.print("prev = ");
		SerialUSB.println(prev_);
		SerialUSB.print("now_= ");
		SerialUSB.println(now_);
	}

	
	if (now_ == 0 && prev_ != 0)  // деление на ноль и не равно старому значению - обновляем
	{
		if (dmDebugSpont)
		{
			SerialUSB.println("now_ == 0 && prev_ != 0");
		}
		return 1;
	}

	if (now_ == 0 && prev_ == 0) // оба нуля - не  обновляем
	{
		if (dmDebugSpont)
		SerialUSB.println("now_ == 0 && prev_ == 0");
		return 0;
	}

	if (now_ == prev_)			// значения равны - не обновляем
	{
		if (dmDebugSpont)
		SerialUSB.println("now_ == prev_");
		return 0;
	}
	

	float buffer = prev_ - now_;

	buffer = abs(buffer);

	if (buffer > 0.005)
	{
		if (dmDebugSpont)
		SerialUSB.println("buffer > 0.005");
		return 1;
	}
	else
	{
		//SerialUSB.println(" not change ");
		return 0;
	}
	
	
}


uint8_t dataUpd(float prev_, float now_, float gap_)
{
	if (dmDebugSpont)
	{
		SerialUSB.println();
		SerialUSB.print("prev = ");
		SerialUSB.println(prev_);
		SerialUSB.print("now_= ");
		SerialUSB.println(now_);
	}

	
	if (now_ == 0 && prev_ != 0)  // деление на ноль и не равно старому значению - обновляем
	{
		if (dmDebugSpont)
		{
			SerialUSB.println("now_ == 0 && prev_ != 0");
		}
		return 1;
	}

	if (now_ == 0 && prev_ == 0) // оба нуля - не  обновляем
	{
		if (dmDebugSpont)
		SerialUSB.println("now_ == 0 && prev_ == 0");
		return 0;
	}

	if (now_ == prev_)			// значения равны - не обновляем
	{
		if (dmDebugSpont)
		SerialUSB.println("now_ == prev_");
		return 0;
	}
	

	float buffer = prev_ - now_;

	buffer = abs(buffer);

	if (buffer > gap_)
	{
		if (dmDebugSpont)
		{
			SerialUSB.print("buffer > ");
			SerialUSB.println(gap_);
		}
		
		return 1;
	}
	else
	{
		//SerialUSB.println(" not change ");
		return 0;
	}
	
	
}

/*
void INX_update(void** devices, int16_t* data_X2X_, uint16_t k)  // Есть ли обновление?
{
LAI1118* device_ = (LAI1118*)devices[k];

//ch 1
if(dataUpd(device_->AnalogInput01, data_X2X_[1]))
{
bitSet(device_->ME_Upd, 0);  // set the bit corresponding to analog input 3
device_->AnalogInput01 = data_X2X_[1];  // update the previous value with the new value
}
}
*/

// void INX_update(void** devices, int16_t* data_X2X_, uint16_t k)
// {
//
// 	// TF update
// 	LAI1118* device_ = (LAI1118*)devices[k];
//
// 	float* floatPtr = &device_->AnalogInput01;
//
// 	for (uint8_t i = 1; i <= device_->TF_qnt; i++)
// 	{
// 		laiFloat.bytes[3] = highByte(data_X2X[2*i]);
// 		laiFloat.bytes[2] = lowByte(data_X2X[2*i]);
// 		laiFloat.bytes[1] = highByte(data_X2X[2*i-1]);
// 		laiFloat.bytes[0] = lowByte(data_X2X[2*i-1]);
//
// 		*floatPtr = laiFloat.val;
// 		++floatPtr;
//
// 	}
// }



inline void checkConnection(DeviceHeader* hdr, bool newState)
{
	if (hdr->ConnectLost != newState)
	{
		if (IEC_Data_Debug)
		{
			SerialUSB.print("Module - ");
			SerialUSB.print(hdr->ID);
			if (newState)
			SerialUSB.println(", Lost");
			else
			SerialUSB.println(", online");
		}
	}
	hdr->ConnectLost = newState;
}



static char cmdBuf[64];
static uint8_t cmdLen = 0;

void pollUsbConsole()
{
	while (SerialUSB.available())
	{
		char c = (char)SerialUSB.read();

		// End-of-line: accept CR or LF
		if (c == '\r' || c == '\n')
		{
			// swallow second char of CRLF / ignore empty line
			if (cmdLen == 0) continue;

			cmdBuf[cmdLen] = '\0';
			dispatchUsbCmd(cmdBuf);
			cmdLen = 0;
			continue;
		}

		// backspace support
		if (c == '\b' || c == 0x7F)
		{
			if (cmdLen) cmdLen--;
			continue;
		}

		if (cmdLen < sizeof(cmdBuf) - 1)
		cmdBuf[cmdLen++] = c;
		else
		cmdLen = 0; // overflow -> reset
	}
}
//
// void checkConnection(DeviceCommon* device,uint8_t state_)
// {
// 	if (device->ConnectLost != state_)
// 	{
// 		device->ConnectLost = state_; // Потеря связи
// 		//	bitWrite(device->StatusUpd,COMM,1); // Данные по Connected изменились
//
// 		if (IEC_Data_Debug)
// 		SerialUSB.print("Module - ");
// 		SerialUSB.print(device->ID);
// 		SerialUSB.println(", Lost");
//
// 	}
// }



// void resetUpdFlags(DeviceCommon* device_)
// {
// 	if (device_->SP_Upd||device_->ME_Upd||device_->TF_Upd)  // If any changes found
// 	{
// 		IEC104_E1.spontaneousSend = 1; // run station spontaneous transmission
// 		IEC104_E2.spontaneousSend = 1; // run station spontaneous transmission
// 		device_->SP_Upd = 0; // Reset update flags
// 		device_->ME_Upd = 0; // Reset update flags
// 		device_->TF_Upd = 0; // Reset update flags
// 	}
// 	device_->StatusUpd = 0;
// }

// void linkDescriptor(DeviceCommon* device, uint16_t* spOffset, uint16_t* dataChng_M_SP)
// {
// 	//	uint8_t QDS;
// 	if (device->ConnectLost || device->deviceFault)
// 	QDS_ = QDS_Invalid;
// 	else
// 	QDS_ = QDS_Good;
//
// 	uint16_t idx = *spOffset;
//
// 	IEC104data_M_SP[idx] = device->ConnectLost;
// 	IEC104data_M_SP[idx + 1] = device->deviceFault | QDS;
//
// 	if (bitRead(device->StatusUpd, COMM) || bitRead(device->StatusUpd, FAULT))
// 	{
// 		IEC104_E1.spontaneousSend = 1;
// 		IEC104_E2.spontaneousSend = 1;
// 		device->StatusUpd = 0;
//
// 		device->SP_Upd = 0xFF;
// 		device->ME_Upd = 0xFF;
// 		device->TF_Upd = 0xFF;
//
// 		IEC104spont_M_SP[(*dataChng_M_SP)++] = idx;
// 		IEC104spont_M_SP[(*dataChng_M_SP)++] = idx + 1;
// 	}
//
// 	*spOffset += 2;
// }


// Общая функция для обработки M_SP_NA_1 у любого устройства,
// у которого есть поля:
//   - static constexpr uint8_t SP_COUNT;
//   - uint32_t DigitalInput;
//   - bool     ConnectLost;
//   - bool     deviceFault;

inline DeviceHeader* getX2XByAddr(uint8_t addr)
{
	if (addr >= g_x2xCount) return nullptr;
	return static_cast<DeviceHeader*>(system_Devices[addr]);   // addr == index
}

inline DeviceHeader* getSensorBySlave(uint8_t slave)
{
	// slave: 1..g_sensCount
	if (slave == 0 || slave > g_sensCount) return nullptr;
	uint16_t idx = g_sensBase + (uint16_t)(slave - 1);
	if (idx >= totalNodes) return nullptr;
	return static_cast<DeviceHeader*>(system_Devices[idx]);
}


static inline DeviceHeader* getX2XBySlave(uint8_t slave)
{
	// slave: 1..g_x2xCount
	if (slave == 0 || slave > g_x2xCount) return nullptr;
	return static_cast<DeviceHeader*>(system_Devices[slave]); // индекс = slave
}



static inline DeviceHeader* getSensBySlave(uint16_t slave)
{
	if (slave == 0 || slave > g_sensCount) return nullptr;
	uint16_t idx = g_sensBase + (uint16_t)(slave - 1);
	if (idx >= totalNodes) return nullptr;
	return static_cast<DeviceHeader*>(system_Devices[idx]);
}



static inline void normalizeX2XSlave()
{
	// если модулей нет
	if (g_x2xCount == 0) {
		currentSlave[X2X] = 0;
		return;
	}

	// допустимые слейвы: 1..g_x2xCount
	if (currentSlave[X2X] == 0 || currentSlave[X2X] > g_x2xCount) {
		currentSlave[X2X] = 1;
	}
}



static inline void getCountsById(uint8_t id, uint16_t& sp, uint16_t& me, uint16_t& tf)
{
	switch (id)
	{
		case LCP:    sp = LCP1116::SP_COUNT;  me = LCP1116::ME_COUNT;  tf = LCP1116::TF_COUNT;  break;
		case LDO:    sp = LDO1118::SP_COUNT;  me = LDO1118::ME_COUNT;  tf = LDO1118::TF_COUNT;  break;
		case LAI:    sp = LAI1118::SP_COUNT;  me = LAI1118::ME_COUNT;  tf = LAI1118::TF_COUNT;  break;
		case LDI8:   sp = LDI1118::SP_COUNT;  me = LDI1118::ME_COUNT;  tf = LDI1118::TF_COUNT;  break;
		case LDI16:  sp = LDI1116::SP_COUNT;  me = LDI1116::ME_COUNT;  tf = LDI1116::TF_COUNT;  break;
		case LCT:    sp = LCT1114::SP_COUNT;  me = LCT1114::ME_COUNT;  tf = LCT1114::TF_COUNT;  break;
		case LCT_2:  sp = LCT1114_2::SP_COUNT;me = LCT1114_2::ME_COUNT;tf = LCT1114_2::TF_COUNT;break;

		case DM910:  sp = DM91_0::SP_COUNT;   me = DM91_0::ME_COUNT;   tf = DM91_0::TF_COUNT;   break;
		case DM910H: sp = DM91_H::SP_COUNT;   me = DM91_H::ME_COUNT;   tf = DM91_H::TF_COUNT;   break;
		case PVT100: sp = PVT_100::SP_COUNT;  me = PVT_100::ME_COUNT;  tf = PVT_100::TF_COUNT;  break;

		default:     sp = 0; me = 0; tf = 0; break;
	}
}

static inline void calcIec104Totals(uint16_t& spTotal, uint16_t& meTotal, uint16_t& tfTotal)
{
	spTotal = 0; meTotal = 0; tfTotal = 0;

	for (uint16_t i = 0; i < totalNodes; ++i)
	{
		auto* hdr = static_cast<DeviceHeader*>(system_Devices[i]);
		if (!hdr) continue;

		uint16_t sp, me, tf;
		getCountsById(hdr->ID, sp, me, tf);

		spTotal += sp;
		meTotal += me;
		tfTotal += tf;
	}
}

static inline uint16_t addReserve(uint16_t n, uint8_t percent, uint16_t minAdd)
{
	uint32_t nn = n;
	nn += (nn * percent) / 100;
	nn += minAdd;
	if (nn > 65535) nn = 65535;
	return (uint16_t)nn;
}

bool allocIec104Buffers(uint8_t reservePercent, uint16_t reserveMin)
{
	uint16_t sp, me, tf;
	calcIec104Totals(sp, me, tf);

	IEC104_SP_N = addReserve(sp, reservePercent, reserveMin);
	IEC104_ME_N = addReserve(me, reservePercent, reserveMin);
	IEC104_TF_N = addReserve(tf, reservePercent, reserveMin);

	// выделяем
	IEC104data_M_SP  = (int16_t*)malloc(IEC104_SP_N * sizeof(int16_t));
	IEC104spont_M_SP = (int16_t*)malloc(IEC104_SP_N * sizeof(int16_t));

	IEC104data_M_ME  = (int16_t*)malloc(IEC104_ME_N * sizeof(int16_t));
	IEC104spont_M_ME = (int16_t*)malloc(IEC104_ME_N * sizeof(int16_t));
	IEC104_M_ME_QDS  = (uint8_t*)malloc(IEC104_ME_N * sizeof(uint8_t));

	IEC104data_M_TF  = (float*)  malloc(IEC104_TF_N * sizeof(float));
	IEC104spont_M_TF = (int16_t*)malloc(IEC104_TF_N * sizeof(int16_t));
	IEC104_M_TF_QDS  = (uint8_t*)malloc(IEC104_TF_N * sizeof(uint8_t));

	// changeBuffer логично делать = max(SP,ME,TF) или под твой алгоритм
	uint16_t chN = IEC104_SP_N;
	if (IEC104_ME_N > chN) chN = IEC104_ME_N;
	if (IEC104_TF_N > chN) chN = IEC104_TF_N;
	changeBuffer = (int16_t*)malloc(chN * sizeof(int16_t));

	// проверка на провал
	if (!IEC104data_M_SP || !IEC104spont_M_SP ||
	!IEC104data_M_ME || !IEC104spont_M_ME || !IEC104_M_ME_QDS ||
	!IEC104data_M_TF || !IEC104spont_M_TF || !IEC104_M_TF_QDS ||
	!changeBuffer)
	{
		SerialUSB.println("IEC104 alloc failed");
		return false;
	}

	// обнуляем
	memset(IEC104data_M_SP,  0, IEC104_SP_N * sizeof(int16_t));
	memset(IEC104spont_M_SP, 0, IEC104_SP_N * sizeof(int16_t));

	memset(IEC104data_M_ME,  0, IEC104_ME_N * sizeof(int16_t));
	memset(IEC104spont_M_ME, 0, IEC104_ME_N * sizeof(int16_t));
	memset(IEC104_M_ME_QDS,  0, IEC104_ME_N * sizeof(uint8_t));

	memset(IEC104data_M_TF,  0, IEC104_TF_N * sizeof(float));
	memset(IEC104spont_M_TF, 0, IEC104_TF_N * sizeof(int16_t));
	memset(IEC104_M_TF_QDS,  0, IEC104_TF_N * sizeof(uint8_t));

	memset(changeBuffer,     0, chN * sizeof(int16_t));

	SerialUSB.print("IEC104 sizes SP/ME/TF = ");
	SerialUSB.print(IEC104_SP_N); SerialUSB.print("/");
	SerialUSB.print(IEC104_ME_N); SerialUSB.print("/");
	SerialUSB.println(IEC104_TF_N);

	return true;
}


// допустимые ID для X2X.txt (только модули, без PLC)
static inline bool isValidX2XModuleId(uint16_t id)
{
	switch (id) {
		case LDO:
		case LAI:
		case LDI8:
		case LDI16:
		case LCT:
		case LCT_2:
		return true;
		default:
		return false;
	}
}

// Возвращает false, если конфиг кривой (нашли LCP/0 или мусор)
bool loadX2XModulesOrFail(const char* fname, int16_t* outArray)
{
	int16_t ASDU_blank = 1;
	
	loadConfig(fname, outArray);
	
  	outArray[0] = 2;
  	outArray[1] = 2;
 	outArray[2] = 6;
// 	outArray[3] = 0;
// 	outArray[4] = 16;
		
	const uint16_t n = outArray[0];

	// n строк в outArray[1..n]
	for (uint16_t i = 1; i <= n; ++i)
	{
		const uint16_t id = outArray[i];

		// 0 (LCP) — явная ошибка в X2X.txt
		if (id == LCP) {
			SerialUSB.print("[CFG ERROR] ");
			SerialUSB.print(fname);
			SerialUSB.print(": line ");
			SerialUSB.print(i);
			SerialUSB.println(": LCP/0 is forbidden in X2X.txt (PLC is implicit at index 0).");
			return false;
		}

		// Любой ID не из набора X2X модулей — ошибка
		if (!isValidX2XModuleId(id)) {
			SerialUSB.print("[CFG ERROR] ");
			SerialUSB.print(fname);
			SerialUSB.print(": line ");
			SerialUSB.print(i);
			SerialUSB.print(": invalid X2X module ID = ");
			SerialUSB.println(id);
			return false;
		}
	}

	// если всё ок — добавляем PLC заранее, затем модули
	// PLC должен быть уже добавлен до вызова этой функции или добавь здесь:
	// addNewASDU(LCP, ASDU_blank);

	g_x2xCount = n;               // ТОЛЬКО модули
	for (uint16_t i = 1; i <= n; ++i) {
		addNewASDU(outArray[i], ASDU_blank); // модули в system_Devices[1..n]
	}

	return true;
}
