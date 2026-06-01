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
* Ethernet server accept upto 6 simultaneous connections plus 2 spare sockets.
* For more accurate information, please look at the IEC 104 section of the Datasheet.
*
* Related files :\n
* \ref IGAS_104.c\n
* \ref IGAS_104.h.\n
*/


/*----------------------------------------------------------------------------
*        Headers
*----------------------------------------------------------------------------*/

#ifndef IGAS_104_SingleASDU
#define IGAS_104_SingleASDU

#include <IGAS_Eth.h>
#include <IGAS_RTC.h>


const int t1_104 = 15000;							// ms  t1 - reply timeOut
const int t3_104 = 20000;							// ms  t3 - silence communication time



// IEC 60870-5-104 – Quality descriptor (QDS) bits
constexpr uint8_t QDS_IV            = 0x80;  // Invalidity indicator (IV)
constexpr uint8_t QDS_NT            = 0x40;  // Not topical (NT)
constexpr uint8_t QDS_SB            = 0x20;  // Substituted (SB)
constexpr uint8_t QDS_BL            = 0x10;  // Blocked (BL)

// Common composite QDS values
constexpr uint8_t QDS_OK            = 0x00;                                // Valid, topical, not substituted, not blocked
constexpr uint8_t QDS_INVALID       = QDS_IV;                              // Invalid
constexpr uint8_t QDS_NOT_TOPICAL   = QDS_NT;                              // Not topical
constexpr uint8_t QDS_SUBSTITUTED   = QDS_SB;                              // Substituted
constexpr uint8_t QDS_BLOCKED       = QDS_BL;                              // Blocked
constexpr uint8_t QDS_ALL_BAD       = QDS_IV | QDS_NT | QDS_SB | QDS_BL;   // All flags set


const int sockPoll = 2000;						    // Socket check poll timer, ms
const int iecTimeOut = 1000;						// Socket check poll timer, ms

const int rxTxBuffer = 500;						    // RX TX one message proceed buffer size

const byte Uframe = 0x04;							// U frame identifier
const byte START2 = 0x68;							// IEC104 Start byte
const byte M_ME_NB_1 = 0xb;							// Measured value, scaled value
const byte M_ME_TE_1 = 0x23;						// Measured value, scaled value, with time tag CP56Time2a
const byte M_SP_NA_1 = 0x1;							// Single-point information
const byte M_SP_TB_1 = 0x1E;						// Single-point information with time tag CP56Time2a
const byte C_IC_NA_1 = 0x64;						// C_IC_NA_1 General Interrogation Command
const byte C_CS_NA_1 = 0x67;						// Clock Synchronization Command

const byte M_ME_NC_1 = 0x0D;								// Measured value, short floating point number
const byte M_ME_TF_1 = 0x24;								// Measured value, short floating point number with time tag CP56Time2a


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

const byte closeConnection = 99;					// Close connection flag

// Socket Status
static const uint8_t CLOSED      = 0x00;
static const uint8_t LISTEN      = 0x14;
static const uint8_t ESTABLISHED = 0x17;
static const uint8_t CLOSE_WAIT  = 0x1C;
byte const bufSize = MAX_CLIENT_NUM;
byte const bufer = MAX_CLIENT_NUM;

byte const IECDebug = 0;

class IEC_104
{

	public:

	IEC_104(byte csPin);

	void init104(RTC_clock &RTC_Class);
	void LOOP(int16_t* data_M_SP, int16_t* spont_M_SP, int16_t* data_M_ME, int16_t* spont_M_ME,uint8_t* data_M_ME_QDS_,float* data_M_TF, int16_t* spont_M_TF, uint8_t* IEC104_M_TF_QDS_);
	byte spontaneousSend = 0;						 // Set cyclic TX time, ms
	int size_M_SP = 0;								 // 104 Gas System data array size
	int size_M_ME = 0;
	int size_M_TF = 0;
	
	IPAddress IPlist[MAX_CLIENT_NUM];
	uint16_t PORTlist[MAX_CLIENT_NUM];
	int dataChng_M_SP_ = 0;							 // Data change counter
	int dataChng_M_ME_ = 0;							 // Data change counter
	int dataChng_M_TF_ = 0;							 // Data change counter
	
	 uint16_t spStartAddr = 40101;					    // IOA start address
	 uint16_t meStartAddr = 41101;					    // IOA start address
	 uint16_t tfStartAddr = 42101;
	
	byte ipUpdate = 0;
	byte setTime104;
	int ASDU = 1;							// Station Address
	byte _csPin;

	private:


	void iec104DataTranssmition(int16_t* data_M_SP, int16_t* data_M_ME, uint8_t* data_M_ME_QDS_, float* data_M_TF, uint8_t* IEC104_M_TF_QDS_, byte transsmitionCause, byte whoIs_);
	void iec104DataSpontan(int16_t* data_M_SP, int16_t* spont_M_SP,int16_t* data_M_ME,int16_t* spont_M_ME, uint8_t* data_M_ME_QDS_, float* data_M_TF,int16_t* spont_M_TF, uint8_t* IEC104_M_TF_QDS_, byte transsmitionCause, byte whoIs_);// Transmit USER Data using TX Cause
	void serviceFrame104(byte causeTX_, byte whoIs_, byte command_);
	void APCI(byte function_, byte whoIs_);
	void transmitPacket(int16_t* data_M_SP, byte numIx, int ioaStartAddr, int dataStartCount_, byte transsmitionCause_, byte whoIs_);
	void transmit_M_SP_NA_1(int16_t* data_M_SP, byte numIx, int ioaStartAddr, int dataStartCount_, byte transsmitionCause_, byte whoIs);
	void transmit_M_ME_NC_1(float* data_M_SP,uint8_t* IEC104_M_TF_QDS_, byte numIx, int startAddr, int dataStartCount_, byte transsmitionCause_, byte whoIs_);
	void transmit_M_ME_NB_1(int16_t* data_M_SP, uint8_t* data_M_ME_QDS_, byte numIx, int ioaStartAddr, int dataStartCount_, byte transsmitionCause_, byte whoIs_);
	void spontan_M_SP_NA_1(int16_t* data_M_SP,int16_t* spont_M_SP, byte numIx, int dataStartCount_, byte transsmitionCause_, byte whoIs);
	void spontan_M_SP_TB_1(int16_t* data_M_SP,int16_t* spont_M_SP, byte numIx, int dataStartCount_, byte transsmitionCause_, byte whoIs);
	void spontan_M_ME_TE_1(int16_t* data_M_SP,int16_t* spont_M_SP,  uint8_t* data_M_ME_QDS_,byte numIx, int dataStartCount_, byte transsmitionCause_, byte whoIs_);
	void spontan_M_ME_TF_1(float* data_M_ME,int16_t* spont_M_ME,  uint8_t* IEC104_M_TF_QDS_, byte numIx, int dataStartCount_, byte transsmitionCause_, byte whoIs_);
	
	byte checkCNT(byte whoIs_);
	void stopClient(byte whoIs_);
	byte getQuerryType(byte whoIs_);
	byte dataToBuffer(byte whoIs_, byte startCnt, byte dataQt_);

	void checkTime();
	void IEC_timeout();

	void clockConf(byte whoIs_);
	void proceedReply(int16_t* data_M_SP, int16_t* data_M_ME, uint8_t* data_M_ME_QDS_,  float* data_M_TF,uint8_t* IEC104_M_TF_QDS_, byte whoIs_);
	void monitorIncoming(int16_t* data_M_SP, int16_t* data_M_ME, uint8_t* data_M_ME_QDS_, float* data_M_TF, uint8_t* IEC104_M_TF_QDS_);
	byte proceedClient();
	void checkIncomingIP(IPAddress newIP_);
	byte checkSockets();
	void stationTransmit(int16_t* data_M_SP, int16_t* spont_M_SP,int16_t* data_M_ME,int16_t* spont_M_ME, uint8_t* data_M_ME_QDS_,float* data_M_TF,int16_t* spont_M_TF, uint8_t* IEC104_M_TF_QDS_);


	int TXcnt[MAX_CLIENT_NUM+1], RXcnt[MAX_CLIENT_NUM+1];		// TX RX counters
	

	uint8_t iec104ReciveArray[rxTxBuffer];						// 104 RX buffer
	uint8_t iec104TransmissionArray[rxTxBuffer];				// 104 TX buffer
	RTC_clock *rtc_clock104;									// RTC pointer

	unsigned long microTime;


	byte clientState[MAX_CLIENT_NUM+1] = {STOPDT_act, };		// IEC104 client state
	byte socketState[MAX_CLIENT_NUM+1] = {SOCK_empty, };		// Socket state


	unsigned long T1_104[MAX_CLIENT_NUM+1];						// Respond time out
	unsigned long T3_104[MAX_CLIENT_NUM+1];						// Silence timer (test connection)
	byte UTESTER[MAX_CLIENT_NUM+1];								// connection test packet Flag

	byte incomingSocket;
	unsigned long socketTimer;
	unsigned long iecTimePoll;
	unsigned long stackDebug;
	uint16_t incomingPort;

};

#endif

