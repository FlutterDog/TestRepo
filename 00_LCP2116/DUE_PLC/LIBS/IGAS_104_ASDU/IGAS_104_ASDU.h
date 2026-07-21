/* ----------------------------------------------------------------------------
*         IGAS Software Package License
* ----------------------------------------------------------------------------
* Copyright (c) 2011-2022, IGAS
*
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are strictly prohibited
*
* IGAS's name may not be used without specific prior written permission.
* ----------------------------------------------------------------------------
*/

/*
* The IEC104 driver provides the interface to configure and use the IEC104
* communication protocol.
*
* It manages:
* M_ME_NB_1
* M_SP_NA_1
* M_SP_TB_1
* C_IC_NA_1
* C_CS_NA_1
*
*
* Ethernet server accept upto 6 simultaneous Connecteds plus 2 spare sockets.
* For more accurate information, please look at the IEC 104 section of the Datasheet.
*
* Related files :\n
* \ref IGAS_104.c\n
* \ref IGAS_104.h.\n
*/


/*----------------------------------------------------------------------------
*        Headers
*----------------------------------------------------------------------------*/

#ifndef IGAS_104_ASDU
#define IGAS_104_ASDU

#include <IGAS_Eth.h>
#include <IGAS_RTC.h>
#include <IGAS_STRUCT.h>

#define setQDS_invalid(value) ((value) |= (B10000000))
#define setQDS_valid(value) ((value) &= (B01111111))

#define QDS_Invalid  B10000000
#define QDS_Good  B00000000
byte const ETH1_ = 10;     // CS SPI pin
byte const ETH2_ = 48;      // CS SPI pin

const int t1_104 = 50000;//15000;							// ms  t1 - reply timeOut
const int t3_104 = 20000;//20000;							// ms  t3 - silence communication time






const int sockPoll = 2000;						    // Socket check poll timer, ms
const int iecTimeOut = 1000;						// Socket check poll timer, ms

const int rxTxBuffer = 500;						    // RX TX one message proceed buffer size

const byte Uframe = 0x04;							// U frame identifier
const byte START2 = 0x68;							// IEC104 Start byte
const byte M_ME_NB_1 = 0xb;	 // 11					// Measured value, scaled value
const byte M_ME_TE_1 = 0x23; //	35					// Measured value, scaled value, with time tag CP56Time2a
const byte M_SP_NA_1 = 0x1;	 //  1					// Single-point information
const byte M_SP_TB_1 = 0x1E; //30					// Single-point information with time tag CP56Time2a
const byte C_IC_NA_1 = 0x64;						// C_IC_NA_1 General Interrogation Command
const byte C_CS_NA_1 = 0x67;						// Clock Synchronization Command

const byte M_ME_NC_1 = 0x0D; // 13					// Measured value, short floating point number
const byte M_ME_TF_1 = 0x24; // 36					// Measured value, short floating point number with time tag CP56Time2a


const byte QDS = 0;									// QDS variable


const byte SOCK_empty = 10;							// Socket Empty flag
const byte spontaneous = 3;							// COT: 3 <Spontaneous>
const byte STARTDT_act = 7;							// STARTDT activation byte
const byte STARTDT_con = 11;						// STARTDT con byte
const byte TESTFR_act = 67;							// TESTFR act byte
const byte TESTFR_con = 0x83;						// TESTFR con byte
const byte APCI_size = 0x4;							// APCI frame size
const byte STOPDT_act = 19;							// STOPDT act byte
const byte STOPDT_con = 0x23;						// STOPDT con byte
const byte S_frame = 01;							// S frame byte
const byte I_frame = 00;							// I frame byte

const byte ACT_con = 0x7;							// Activation con byte
const byte ACT_term = 10;							// Activation term byte
const byte stationInterrogation = 0x14;				// QOI stationInterrogation byte
const byte cyclic = 0x01;							// Cause of transmission - cyclic TX
const byte inrogen = 20;							// Cause of transmission - inrogen TX
const byte singleByte = 1;							// Single byte data
const byte singleByteFrame = 14;					// Single frame size

const byte MAX_CLIENT_NUM = 8;


const byte cntError = 1;							// RX/TX counter error Flag
const byte cntOk = 0;								// RX/TX counter in tolerance Flag

const byte closeConnected = 99;					// Close Connected flag

// Socket Status

static const uint8_t CLOSED      = 0x00;
static const uint8_t INIT        = 0x13;
static const uint8_t LISTEN      = 0x14;
static const uint8_t SYNSENT     = 0x15;
static const uint8_t SYNRECV     = 0x16;
static const uint8_t ESTABLISHED = 0x17;
static const uint8_t FIN_WAIT    = 0x18;
static const uint8_t CLOSING     = 0x1A;
static const uint8_t TIME_WAIT   = 0x1B;
static const uint8_t CLOSE_WAIT  = 0x1C;
static const uint8_t LAST_ACK    = 0x1D;
static const uint8_t UDP         = 0x22;
static const uint8_t IPRAW       = 0x32;
static const uint8_t MACRAW      = 0x42;
static const uint8_t PPPOE       = 0x5F;



byte const bufSize = MAX_CLIENT_NUM;
byte const bufer = MAX_CLIENT_NUM;

byte const IECDebug = 0;
byte const IEClinkDebug = 0;
byte const dataDebug = 0;
byte const interframe_ = 3;
byte const IECSockDebug = 0;
byte const IECSockInfDebug = 0;

class IEC_104
{

	public:

	IEC_104(byte csPin);

	void init104(RTC_clock &RTC_Class);
	void LOOP(void** devices, int16_t* data_M_SP, int16_t* spont_M_SP, int16_t* data_M_ME, int16_t* spont_M_ME,float* data_M_TF, int16_t* spont_M_TF);
	byte spontaneousSend = 0;						 // Set cyclic TX time, ms
	
	IPAddress IPlist[MAX_CLIENT_NUM];
	uint16_t PORTlist[MAX_CLIENT_NUM];

	uint16_t totalASDU = 1;
	uint16_t asduTail = 1;

	byte ipUpdate = 0;
	byte setTime104;
	
	uint16_t ASDU = 1;							// Station Address

	byte _csPin;
	
	uint16_t spStartAddr = 40101;					    // IOA start address
	uint16_t meStartAddr = 43101;					    // IOA start address
	uint16_t tfStartAddr = 41101;
	
	
	uint16_t LCP_Start_Addr_SP;
	uint16_t LDO_Start_Addr_SP;
	uint16_t LAI_Start_Addr_SP;
	uint16_t LDI8_Start_Addr_SP;
	uint16_t LCT_Start_Addr_SP;
	uint16_t LDI16_Start_Addr_SP;
	uint16_t IR_SF6_Start_Addr_SP;
	uint16_t DM910_Start_Addr_SP;
	uint16_t DM910H_Start_Addr_SP;
	uint16_t PVT100_Start_Addr_SP;

	uint16_t LCP_Start_Addr_ME;
	uint16_t LDO_Start_Addr_ME;
	uint16_t LAI_Start_Addr_ME;
	uint16_t LDI8_Start_Addr_ME;
	uint16_t LCT_Start_Addr_ME;
	uint16_t LDI16_Start_Addr_ME;
	uint16_t IR_SF6_Start_Addr_ME;
	uint16_t DM910_Start_Addr_ME;
	uint16_t DM910H_Start_Addr_ME;
	uint16_t PVT100_Start_Addr_ME;

	uint16_t LCP_Start_Addr_TF;
	uint16_t LDO_Start_Addr_TF;
	uint16_t LAI_Start_Addr_TF;
	uint16_t LDI8_Start_Addr_TF;
	uint16_t LCT_Start_Addr_TF;
	uint16_t LDI16_Start_Addr_TF;
	uint16_t IR_SF6_Start_Addr_TF;
	uint16_t DM910_Start_Addr_TF;
	uint16_t DM910H_Start_Addr_TF;
	uint16_t PVT100_Start_Addr_TF;

	private:

	uint16_t size_M_SP = 0;								 // 104 Gas System data array size
	uint16_t size_M_ME = 0;
	uint16_t size_M_TF = 0;

	void monitorSporadic(void** devices, int16_t* data_M_SP, int16_t* spont_M_SP, int16_t* data_M_ME,  int16_t* spont_M_ME,float* data_M_TF, int16_t* spont_M_TF); // Spontaneous send
	void iec104DataTranssmition(int16_t* data_M_SP, int16_t* data_M_ME, float* data_M_TF, byte transsmitionCause, uint8_t QDS_valid, byte whoIs_, uint16_t device_);
	void iec104DataSpontan(int16_t* data_M_SP, int16_t* spont_M_SP,int16_t* data_M_ME,int16_t* spont_M_ME, uint8_t QDS_valid, float* data_M_TF,int16_t* spont_M_TF, byte transsmitionCause, byte whoIs_, uint16_t device_);// Transmit USER Data using TX Cause
	void serviceFrame104(byte causeTX_, byte whoIs_, byte command_,int16_t ASDUx);
	void APCI(byte function_, byte whoIs_);
	void transmitPacket(int16_t* data_M_SP, byte numIx, int16_t ioaStartAddr, int16_t dataStartCount_, byte transsmitionCause_, byte whoIs_);
	void transmit_M_SP_NA_1(int16_t* data_M_SP, byte numIx, int16_t ioaStartAddr, int16_t dataStartCount_, byte transsmitionCause_, byte whoIs);
	void transmit_M_ME_NC_1(float* data_M_SP,uint8_t IEC104_M_TF_QDS_, byte numIx, int16_t startAddr, int16_t dataStartCount_, byte transsmitionCause_, byte whoIs_);
	void transmit_M_ME_NB_1(int16_t* data_M_SP, uint8_t QDS_valid, byte numIx, int16_t ioaStartAddr, int16_t dataStartCount_, byte transsmitionCause_, byte whoIs_);
	void spontan_M_SP_NA_1(int16_t* data_M_SP,int16_t* spont_M_SP, byte numIx, int16_t dataStartCount_, byte transsmitionCause_, byte whoIs, uint16_t device_);
	void spontan_M_SP_TB_1(int16_t* data_M_SP,int16_t* spont_M_SP, byte numIx, int16_t dataStartCount_, byte transsmitionCause_, byte whoIs, uint16_t device_);
	void spontan_M_ME_TE_1(int16_t* data_M_SP,int16_t* spont_M_SP,  uint8_t QDS_valid,byte numIx, int16_t dataStartCount_, byte transsmitionCause_, byte whoIs_, uint16_t device_);
	void spontan_M_ME_TF_1(float* data_M_ME,int16_t* spont_M_ME,  uint8_t IEC104_M_TF_QDS_, byte numIx, int16_t dataStartCount_, byte transsmitionCause_, byte whoIs_, uint16_t device_);
	void printClientState(uint8_t clientState_);
	int16_t findASDU(void** devices,int16_t ASDUx_ );
	uint16_t getStartAddress_SP(uint16_t device_);
	uint16_t getStartAddress_ME(uint16_t device_);
	uint16_t getStartAddress_TF(uint16_t device_);
	//void removeClient(byte whoIs_);
	
	byte checkCNT(byte whoIs_);
	void stopClient(byte whoIs_);
	byte getQuerryType(byte whoIs_);
	byte dataToBuffer(byte whoIs_, byte startCnt, byte dataQt_);

	void checkTime();
	void IEC_timeout();

	void clockConf(byte whoIs_);
	void proceedReply(void** devices, int16_t* data_M_SP, int16_t* data_M_ME,  float* data_M_TF, byte whoIs_);
	void monitorIncoming(void** devices, int16_t* data_M_SP, int16_t* data_M_ME, float* data_M_TF);
	byte proceedClient();
	void checkIncomingIP(IPAddress newIP_);
	byte checkSockets();
	void stationTransmit(void** devices,int16_t* data_M_SP, int16_t* spont_M_SP,int16_t* data_M_ME,int16_t* spont_M_ME, uint8_t data_M_ME_QDS_,float* data_M_TF,int16_t* spont_M_TF, uint16_t device_);
	void resetUpdFlags(DeviceCommon* device_);
	byte checkQDS_Link(DeviceCommon* device, int16_t* data_M_SP,int16_t* spont_M_SP, uint8_t& QDS_);
	void printSockStatus(uint8_t sockStatus_);
	uint16_t TXcnt[MAX_CLIENT_NUM+1], RXcnt[MAX_CLIENT_NUM+1];		// TX RX counters
	

	uint8_t iec104ReciveArray[rxTxBuffer];						// 104 RX buffer
	uint8_t iec104TransmissionArray[rxTxBuffer];				// 104 TX buffer
	RTC_clock *rtc_clock104;									// RTC pointer

	unsigned long microTime;


	byte clientState[MAX_CLIENT_NUM+1] = {STOPDT_act,STOPDT_act,STOPDT_act,STOPDT_act,STOPDT_act,STOPDT_act,STOPDT_act,STOPDT_act,STOPDT_act, };		// IEC104 client state
	byte socket104_State[MAX_CLIENT_NUM+1] = {SOCK_empty, SOCK_empty,SOCK_empty,SOCK_empty,SOCK_empty,SOCK_empty,SOCK_empty,SOCK_empty,SOCK_empty,};		// Socket state
	uint32_t activeASDU[MAX_CLIENT_NUM+1] = {0};

	unsigned long T1_104[MAX_CLIENT_NUM+1];						// Respond time out
	unsigned long T3_104[MAX_CLIENT_NUM+1];						// Silence timer (test Connected)
	byte UTESTER[MAX_CLIENT_NUM+1];								// Connected test packet Flag

	byte incomingSocket;
	unsigned long socketTimer;
	unsigned long iecTimePoll;
	unsigned long stackDebug;
	uint16_t incomingPort;
	
	uint16_t dataChng_M_SP_ = 0;							 // Data change counter
	uint16_t dataChng_M_ME_ = 0;							 // Data change counter
	uint16_t dataChng_M_TF_ = 0;							 // Data change counter
	uint16_t ASDUx;

};

#endif

