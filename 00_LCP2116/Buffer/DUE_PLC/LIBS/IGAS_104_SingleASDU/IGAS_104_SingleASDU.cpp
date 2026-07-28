/* ----------------------------------------------------------------------------
*         IGAS Software Package License
* ----------------------------------------------------------------------------
* Copyright (c) 2012-2022, IGAS
*
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are strictly prohibited
*
* IGAS's name may not be used without specific prior written permission.
* ----------------------------------------------------------------------------
*/


#include "IGAS_104_SingleASDU.h"


EthernetClient CLIENT[MAX_CLIENT_NUM+1];
EthernetServer iec104Server(2404);							// IEC 104 Port
IPAddress blankIP(0, 0, 0, 0);								// blank IP mask




IEC_104::IEC_104(byte csPin)
{
	_csPin = csPin;
}


void IEC_104::iec104DataTranssmition(int16_t* data_M_SP, int16_t* data_M_ME, uint8_t* data_M_ME_QDS_, float* data_M_TF,uint8_t* IEC104_M_TF_QDS_, byte transsmitionCause, byte whoIs_)		// Transmit USER Data using TX Cause
{
	int fullPackageNumber = size_M_SP * 0.025;
	int lastPackageLength = size_M_SP - fullPackageNumber * 40;									// Calc packets to transfer full data

	int dataAdr =  0;
	int ioAddr = spStartAddr;
	byte uDataInPacket = 40;

	if (fullPackageNumber)
	for (int j = 0; j < fullPackageNumber; j++)
	{
		transmit_M_SP_NA_1(data_M_SP, uDataInPacket, ioAddr, dataAdr, transsmitionCause, whoIs_);		// Transmit M_SP_NA_1 packet
		ioAddr += uDataInPacket;  // +1 - first data address in packet - M_ME_NB_1, transferred in another packet
		dataAdr += uDataInPacket; // +1 - first data address in packet - M_ME_NB_1, transferred in another packet
	}

	if (lastPackageLength)
	transmit_M_SP_NA_1(data_M_SP, lastPackageLength, ioAddr, dataAdr, transsmitionCause, whoIs_);

	// Transmit Measured value

	fullPackageNumber = size_M_ME * 0.04;
	lastPackageLength = size_M_ME - fullPackageNumber * 25;									// Calc packets to transfer full data

	dataAdr =  0;
	ioAddr = meStartAddr;
	uDataInPacket = 25;

	if (fullPackageNumber)
	for (int j = 0; j < fullPackageNumber; j++)
	{
		transmit_M_ME_NB_1(data_M_ME, data_M_ME_QDS_, uDataInPacket, ioAddr, dataAdr, transsmitionCause, whoIs_);		// Transmit M_SP_NA_1 packet
		ioAddr += uDataInPacket;  // first data address in packet - M_ME_NB_1, transferred in another packet
		dataAdr += uDataInPacket; // first data address in packet - M_ME_NB_1, transferred in another packet
		
	}

	if (lastPackageLength)
	transmit_M_ME_NB_1(data_M_ME, data_M_ME_QDS_, lastPackageLength, ioAddr, dataAdr, transsmitionCause, whoIs_);



	// Transmit Short floating point value


	// 	fullPackageNumber = size_M_TF * 0.025;
	// 	lastPackageLength = size_M_TF - fullPackageNumber * 40;									// Calc packets to transfer full data


	fullPackageNumber = size_M_TF * 0.04;
	lastPackageLength = size_M_TF - fullPackageNumber * 25;
	
	dataAdr =  0;
	ioAddr = tfStartAddr;

	uDataInPacket = 25;


	if (fullPackageNumber)
	for (int j = 0; j < fullPackageNumber; j++)
	{
		transmit_M_ME_NC_1(data_M_TF,  IEC104_M_TF_QDS_, uDataInPacket, ioAddr, dataAdr, transsmitionCause, whoIs_);		// Transmit M_ME_NC_1 packet
		ioAddr += uDataInPacket;  // first data address in packet - M_ME_NB_1, transferred in another packet
		dataAdr += uDataInPacket; // first data address in packet - M_ME_NB_1, transferred in another packet
	}

	if (lastPackageLength)
	transmit_M_ME_NC_1(data_M_TF,IEC104_M_TF_QDS_, lastPackageLength, ioAddr, dataAdr, transsmitionCause, whoIs_);




	T3_104[whoIs_] = millis();																	// Update t3 silence timer
}





void IEC_104::iec104DataSpontan(int16_t* data_M_SP, int16_t* spont_M_SP,int16_t* data_M_ME,int16_t* spont_M_ME, uint8_t* data_M_ME_QDS_, float* data_M_TF,int16_t* spont_M_TF, uint8_t* IEC104_M_TF_QDS_, byte transsmitionCause, byte whoIs_)	// Transmit USER Data using TX Cause
{
	// 	SerialUSB.print("IEC  dataChng_M_TF_ -> ");
	// 	SerialUSB.println(dataChng_M_TF_);
	
	int fullPackageNumber = dataChng_M_SP_ * 0.025;
	int lastPackageLength = dataChng_M_SP_ - fullPackageNumber * 40;

	int dataAdr =  0;

	byte  uDataInPacket = 40;

	if (fullPackageNumber)
	for (int j = 0; j < fullPackageNumber; j++)
	{
		spontan_M_SP_TB_1(data_M_SP, spont_M_SP,uDataInPacket, dataAdr, transsmitionCause, whoIs_);		// With time tag
		dataAdr += uDataInPacket;
	}

	if (lastPackageLength)
	spontan_M_SP_TB_1(data_M_SP, spont_M_SP, lastPackageLength, dataAdr, transsmitionCause, whoIs_);		// With time tag

	// Spontaneous measured value transmission

	fullPackageNumber = dataChng_M_ME_ * 0.1;
	lastPackageLength = dataChng_M_ME_ - fullPackageNumber * 10;

	dataAdr =  0;
	uDataInPacket = 10;

	if (fullPackageNumber)
	for (int j = 0; j < fullPackageNumber; j++)
	{
		spontan_M_ME_TE_1(data_M_ME, spont_M_ME, data_M_ME_QDS_,uDataInPacket, dataAdr, transsmitionCause, whoIs_);		// With time tag
		dataAdr += uDataInPacket;
	}

	if (lastPackageLength)
	spontan_M_ME_TE_1(data_M_ME,spont_M_ME, data_M_ME_QDS_, lastPackageLength, dataAdr, transsmitionCause, whoIs_);		// With time tag



	// Spontaneous Float value transmission
	
	
	// 	SerialUSB.print("IEC  dataChng_M_TF_ -> ");
	// 	SerialUSB.println(dataChng_M_TF_);
	//

	
	//	fullPackageNumber = dataChng_M_TF_ * 0.025;
	//	lastPackageLength = dataChng_M_TF_ - fullPackageNumber * 40;
	
	fullPackageNumber = dataChng_M_TF_ * 0.1;
	lastPackageLength = dataChng_M_TF_ - fullPackageNumber * 10;

	dataAdr =  0;
	uDataInPacket = 10;
	

	if (fullPackageNumber)
	for (int j = 0; j < fullPackageNumber; j++)
	{
		spontan_M_ME_TF_1(data_M_TF, spont_M_TF, IEC104_M_TF_QDS_,uDataInPacket, dataAdr, transsmitionCause, whoIs_);		// With time tag
		dataAdr += uDataInPacket;
	}

	if (lastPackageLength)
	spontan_M_ME_TF_1(data_M_TF,spont_M_TF, IEC104_M_TF_QDS_, lastPackageLength, dataAdr, transsmitionCause, whoIs_);		// With time tag




	T3_104[whoIs_] = millis();																	// Update t3 silence timer
}





void IEC_104::LOOP(int16_t* data_M_SP, int16_t* spont_M_SP, int16_t* data_M_ME,  int16_t* spont_M_ME,uint8_t* data_M_ME_QDS_,float* data_M_TF, int16_t* spont_M_TF, uint8_t* IEC104_M_TF_QDS_)
{
	Ethernet.select(_csPin);
	

	monitorIncoming(data_M_SP, data_M_ME, data_M_ME_QDS_,data_M_TF, IEC104_M_TF_QDS_);		// Check incoming data to sockets

	//SerialUSB.println("stationTransmit -> Start");
	stationTransmit(data_M_SP,spont_M_SP, data_M_ME, spont_M_ME, data_M_ME_QDS_,data_M_TF, spont_M_TF, IEC104_M_TF_QDS_);		// Spontaneous data transmission if needed
	//SerialUSB.println("stationTransmit -> finished");
	
	if (millis() - iecTimePoll > iecTimeOut)					// Check IEC timeout - t0 - t3
	{
		//SerialUSB.print("IEC_timeout -> ");
		//	SerialUSB.println(iecTimePoll);
		IEC_timeout();
		//SerialUSB.println("IEC_timeout -> Finish");
	}


	if (millis() - socketTimer > sockPoll)					// Check sockets for any issues
	{
		
		//SerialUSB.print("checkSockets -> ");
		//SerialUSB.println(socketTimer);
		//	checkSockets();
		//SerialUSB.println("checkSockets -> Finish");
	}
	
}



void IEC_104:: IEC_timeout()
{

	for (byte i=0; i<MAX_CLIENT_NUM; i++)
	{
		if ((millis() - T3_104[i] > t3_104) && (clientState[i] == STARTDT_act))   // no packets > t3 timeout
		{
			APCI(TESTFR_act, i);										 //  Send connection tester - TESTFR_act
			T1_104[i] = millis();
			UTESTER[i] = 1;									  			 // Activate T1 timeout
			//	SerialUSB.print("Send Tester -> ");
			//	SerialUSB.println(T3_104[i]);
		}

		if (UTESTER[i] == 1 && (millis() - T1_104[i] > t1_104) )					 // No reply on tester frame. t1 time out
		{
			UTESTER[i] = 0;												 // De-Activate T1 timeout
			stopClient(i);
			if (IECDebug)
			SerialUSB.println ("Respond timeout - Stop Client ");									 // Stop client
		}
	}

	iecTimePoll = millis();								 // Update IEC_Timer check timer

}



void IEC_104::stationTransmit(int16_t* data_M_SP, int16_t* spont_M_SP,int16_t* data_M_ME,int16_t* spont_M_ME,uint8_t* data_M_ME_QDS_, float* data_M_TF,int16_t* spont_M_TF,  uint8_t* IEC104_M_TF_QDS_)			 // Spontaneous data transmission
{
	if (spontaneousSend)
	{
		for (byte i=0; i<MAX_CLIENT_NUM; i++)
		if (clientState[i] == STARTDT_act)
		iec104DataSpontan(data_M_SP,spont_M_SP,data_M_ME, spont_M_ME, data_M_ME_QDS_,data_M_TF, spont_M_TF, IEC104_M_TF_QDS_, spontaneous, i);
	}
	

	spontaneousSend = 0;												 // De-Activate transmission flag
}



byte IEC_104::checkSockets()											 // Check sockets for any issues
{
	byte readySocket = 0;
	byte freezeSocket = 0;
	byte frozenSock[9] = {0, };
	uint8_t sockStatus;


	for (int i = 0; i < MAX_SOCK_NUM; i++)								// Go through all sockets
	{
		sockStatus = w5500.readSnSR(i);
		
		//	SerialUSB.print(F("Socket# "));
		//	SerialUSB.print(i);
		//	SerialUSB.print(F(" :0x"));
		//	SerialUSB.println(sockStatus,16);
		

		switch(sockStatus)
		{
			case CLOSED:
			readySocket++;		// socket ready for incoming
			if (IECDebug)
			SerialUSB.println ("CLOSED++ ");
			break;

			case LISTEN:
			readySocket++;		// socket ready for incoming
			if (IECDebug)
			SerialUSB.println ("LISTEN++ ");
			break;

			case CLOSE_WAIT:	// frozen socket in CLOSE_WAIT status
			if (IECDebug)
			SerialUSB.println ("CLOSE_WAIT ++");
			freezeSocket++;
			frozenSock[i] = 1;
			break;
		}

		
		if (socketState[i] != SOCK_empty && sockStatus!= ESTABLISHED)	// Check for non valid connections
		{
			CLIENT[i].stop();											// Stop client
			socketState[i] = SOCK_empty;								// Clear socket
			clientState[i] = STOPDT_act;								// Clear IEC flag
			ipUpdate = 1;												// Update HMI IP table
			IPlist[i] = blankIP;										// Clear client IP from data base
			if (IECDebug)
			SerialUSB.println("Client -> stop ");
		}
		
	}



	if (freezeSocket>4)
	for (int i=0; i<MAX_SOCK_NUM; i++)
	if (frozenSock[i] == 1)
	w5500.execCmdSn(i,Sock_CLOSE);										// reset frozen CLOSE_WAIT sockets


	socketTimer = millis();									// Update Socket check timer
	if (IECDebug)
	{
		SerialUSB.print("Available sock -> ");
		SerialUSB.println(readySocket);
	}
	
	
	return readySocket;													// 0 - no socket available for connection at the moment.
}



void IEC_104::serviceFrame104(byte causeTX_, byte whoIs_, byte command_)
{
	iec104TransmissionArray[0] = START2;											//START
	iec104TransmissionArray[1] = singleByteFrame;									//APDU length (14)
	iec104TransmissionArray[2] = lowByte(TXcnt[whoIs_]);							//LSB N(S)
	iec104TransmissionArray[3] = highByte(TXcnt[whoIs_]);							//MSB N(S)
	iec104TransmissionArray[4] = lowByte(RXcnt[whoIs_]);							//LSB N(R)
	iec104TransmissionArray[5] = highByte(RXcnt[whoIs_]);							//LSB N(R)
	iec104TransmissionArray[6] = command_;											// command
	iec104TransmissionArray[7] = singleByte;										// Data quantity
	iec104TransmissionArray[8] = causeTX_;											// Transmission cause  Activation CON(7)
	iec104TransmissionArray[9] = 0x0;												// Init station (optional)
	iec104TransmissionArray[10] = lowByte(ASDU);								// Station Address
	iec104TransmissionArray[11] = highByte(ASDU);								// Station Address
	iec104TransmissionArray[12] = 0x0;												//  Data address IOA  LB
	iec104TransmissionArray[13] = 0x0;												//	Data address IOA  HB
	iec104TransmissionArray[14] = 0x0;												//	Data address IOA
	iec104TransmissionArray[15] = stationInterrogation;								//  QOI: General Interrogation (20)

	CLIENT[whoIs_].write(iec104TransmissionArray, iec104TransmissionArray[1] + 2);	//  Move data to Socket buffer
	TXcnt[whoIs_] += 2;
	if (IECDebug)
	{															//  increment counter
		SerialUSB.print("TXcnt -> ");
		SerialUSB.println(TXcnt[whoIs_]);
		SerialUSB.print("RXcnt -> ");
		SerialUSB.println(RXcnt[whoIs_]);
	}
}


void IEC_104::APCI(byte function_, byte whoIs_)
{
	iec104TransmissionArray[0] = START2;											//START
	iec104TransmissionArray[1] = APCI_size;											//APCI size
	iec104TransmissionArray[2] = function_;											//APDU: reason
	iec104TransmissionArray[3] = 0;
	iec104TransmissionArray[4] = 0;
	iec104TransmissionArray[5] = 0;
	CLIENT[whoIs_].write(iec104TransmissionArray, iec104TransmissionArray[1] + 2);	// Move data to Socket buffer
	T3_104[whoIs_] = millis();												// Reset t3 timer

}



void IEC_104::transmit_M_ME_NB_1(int16_t* data_M_ME, uint8_t* data_M_ME_QDS_, byte numIx, int startAddr, int dataStartCount_, byte transsmitionCause_, byte whoIs_)
{
	int ioaAddr_ = startAddr;
	int dataArrayCount = dataStartCount_;


	iec104TransmissionArray[0] = START2;											// START
	iec104TransmissionArray[1] = 10 + numIx * 6;									// APDU size (250)
	iec104TransmissionArray[2] = lowByte(TXcnt[whoIs_]);							// LSB N(S)
	iec104TransmissionArray[3] = highByte(TXcnt[whoIs_]);							// MSB N(S)
	iec104TransmissionArray[4] = lowByte(RXcnt[whoIs_]);							// LSB N(R)
	iec104TransmissionArray[5] = highByte(RXcnt[whoIs_]);							// LSB N(R)
	iec104TransmissionArray[6] = M_ME_NB_1;											// M_ME_NB_1 (11)
	iec104TransmissionArray[7] = numIx;												// User data quantity
	iec104TransmissionArray[8] = transsmitionCause_;								// Cause of TX
	iec104TransmissionArray[9] = 0;													// init station, optional
	iec104TransmissionArray[10] = lowByte(ASDU);								// Station Address
	iec104TransmissionArray[11] = highByte(ASDU);								// Station Address

	for (int i = 0; i < numIx; i++)
	{

		iec104TransmissionArray[12 + i * 6] = lowByte(ioaAddr_);					// IOA address
		iec104TransmissionArray[13 + i * 6] = highByte(ioaAddr_);					// IOA address
		iec104TransmissionArray[14 + i * 6] = 0;									// IOA address
		iec104TransmissionArray[15 + i * 6] = lowByte(data_M_ME[dataArrayCount]);	// User Data
		iec104TransmissionArray[16 + i * 6] = highByte(data_M_ME[dataArrayCount]);	// User Data
		iec104TransmissionArray[17 + i * 6] = data_M_ME_QDS_[dataArrayCount];									// QOS


		ioaAddr_ ++;													// Every gauge consists 1 concentration and 8 status flags
		dataArrayCount++;
	}
	CLIENT[whoIs_].write(iec104TransmissionArray, iec104TransmissionArray[1] + 2);	// Move data to Socket buffer
	TXcnt[whoIs_] += 2;
	if (IECDebug)
	{
		SerialUSB.print("TXcnt -> ");
		SerialUSB.println(TXcnt[whoIs_]);
		SerialUSB.print("RXcnt -> ");
		SerialUSB.println(RXcnt[whoIs_]);
	}												// Increment TX counter

}




void IEC_104::transmit_M_ME_NC_1(float* data_M_SP, uint8_t* IEC104_M_TF_QDS_, byte numIx, int startAddr, int dataStartCount_, byte transsmitionCause_, byte whoIs_)
{
	int ioaAddr_ = startAddr;
	int dataArrayCount = dataStartCount_;


	iec104TransmissionArray[0] = START2;											// START
	iec104TransmissionArray[1] = 10 + numIx * 8;									// APDU size (250)
	iec104TransmissionArray[2] = lowByte(TXcnt[whoIs_]);							// LSB N(S)
	iec104TransmissionArray[3] = highByte(TXcnt[whoIs_]);							// MSB N(S)
	iec104TransmissionArray[4] = lowByte(RXcnt[whoIs_]);							// LSB N(R)
	iec104TransmissionArray[5] = highByte(RXcnt[whoIs_]);							// LSB N(R)
	iec104TransmissionArray[6] = M_ME_NC_1;											// M_ME_TF_1 (11)
	iec104TransmissionArray[7] = numIx;												// User data quantity
	iec104TransmissionArray[8] = transsmitionCause_;								// Cause of TX
	iec104TransmissionArray[9] = 0;													// init station, optional
	iec104TransmissionArray[10] = lowByte(ASDU);								// Station Address
	iec104TransmissionArray[11] = highByte(ASDU);								// Station Address

	//	byte QDS = 0;   // Add QDS signal

	union {
		float fval;
		byte bval[4];
	} floatAsBytes;
	//	int temp=0;
	
	
	for (int i = 0; i < numIx; i++)
	{

		// elpaDensity;
		floatAsBytes.fval = data_M_SP[dataArrayCount];


		iec104TransmissionArray[12 + i * 8] = lowByte(ioaAddr_);					// IOA address
		iec104TransmissionArray[13 + i * 8] = highByte(ioaAddr_);					// IOA address
		iec104TransmissionArray[14 + i * 8] = 0;									// IOA address
		iec104TransmissionArray[15 + i * 8] = floatAsBytes.bval[0];					// User Data
		iec104TransmissionArray[16 + i * 8] = floatAsBytes.bval[1];					// User Data
		iec104TransmissionArray[17 + i * 8] = floatAsBytes.bval[2];					// User Data
		iec104TransmissionArray[18 + i * 8] = floatAsBytes.bval[3];					// User Data
		iec104TransmissionArray[19 + i * 8] = IEC104_M_TF_QDS_[dataArrayCount];		 // QDS


		ioaAddr_ ++;													// Every gauge consists 1 concentration and 8 status flags
		dataArrayCount++;

	}
	CLIENT[whoIs_].write(iec104TransmissionArray, iec104TransmissionArray[1] + 2);	// Move data to Socket buffer
	TXcnt[whoIs_] += 2;
	if (IECDebug)
	{
		SerialUSB.print("TXcnt -> ");
		SerialUSB.println(TXcnt[whoIs_]);
		SerialUSB.print("RXcnt -> ");
		SerialUSB.println(RXcnt[whoIs_]);
	}												// Increment TX counter

}




void IEC_104::transmit_M_SP_NA_1(int16_t* data_M_SP, byte numIx, int startAddr, int dataStartCount_, byte transsmitionCause_, byte whoIs_)
{
	int ioaAddr_ = startAddr;
	int dataArrayCount = dataStartCount_;

	iec104TransmissionArray[0] = START2;												// START
	iec104TransmissionArray[1] = 10 + numIx * 4;										// APDU size (250)
	iec104TransmissionArray[2] = lowByte(TXcnt[whoIs_]);								// LSB N(S)
	iec104TransmissionArray[3] = highByte(TXcnt[whoIs_]);								// MSB N(S)
	iec104TransmissionArray[4] = lowByte(RXcnt[whoIs_]);								// LSB N(R)
	iec104TransmissionArray[5] = highByte(RXcnt[whoIs_]);								// LSB N(R)
	iec104TransmissionArray[6] = M_SP_NA_1;						    					// M_SP_NA_1 (11)
	iec104TransmissionArray[7] = numIx;													// User data quantity
	iec104TransmissionArray[8] = transsmitionCause_;									// Cause of TX
	iec104TransmissionArray[9] = 0;														// init station, optional
	iec104TransmissionArray[10] = lowByte(ASDU);									// Station Address
	iec104TransmissionArray[11] = highByte(ASDU);									// Station Address

	for (int i = 0; i < numIx; i++)
	{

		iec104TransmissionArray[12 + i * 4] = lowByte(ioaAddr_);						// IOA address
		iec104TransmissionArray[13 + i * 4] = highByte(ioaAddr_);						// IOA address
		iec104TransmissionArray[14 + i * 4] = 0;										// IOA address
		iec104TransmissionArray[15 + i * 4] = lowByte(data_M_SP[dataArrayCount]);// | B10000000;		// User Data

		ioaAddr_++;
		dataArrayCount++;
	}

	CLIENT[whoIs_].write(iec104TransmissionArray, iec104TransmissionArray[1] + 2);		// Move data to Socket buffer
	TXcnt[whoIs_] += 2;
	if (IECDebug)
	{															// Increment TX counter
		SerialUSB.print("TXcnt -> ");
		SerialUSB.println(TXcnt[whoIs_]);
		SerialUSB.print("RXcnt -> ");
		SerialUSB.println(RXcnt[whoIs_]);
	}
}




void IEC_104::spontan_M_SP_NA_1(int16_t* data_M_SP,int16_t* spont_M_SP, byte numIx, int dataStartCount_, byte transsmitionCause_, byte whoIs_)
{
	int dataArrayCount = dataStartCount_;

	iec104TransmissionArray[0] = START2;															// START
	iec104TransmissionArray[1] = 10 + numIx * 4;													// APDU size (250)
	iec104TransmissionArray[2] = lowByte(TXcnt[whoIs_]);											// LSB N(S)
	iec104TransmissionArray[3] = highByte(TXcnt[whoIs_]);											// MSB N(S)
	iec104TransmissionArray[4] = lowByte(RXcnt[whoIs_]);											// LSB N(R)
	iec104TransmissionArray[5] = highByte(RXcnt[whoIs_]);											// LSB N(R)
	iec104TransmissionArray[6] = M_SP_NA_1;															// M_SP_NA_1
	iec104TransmissionArray[7] = numIx;																// User data quantity
	iec104TransmissionArray[8] = transsmitionCause_;												// Cause of TX
	iec104TransmissionArray[9] = 0;																	// init station, optional
	iec104TransmissionArray[10] = lowByte(ASDU);												// Station Address
	iec104TransmissionArray[11] = highByte(ASDU);												// Station Address

	for (int i = 0; i < numIx; i++)
	{
		iec104TransmissionArray[12 + i * 4] = lowByte(spStartAddr+spont_M_SP[dataArrayCount]);		// IOA address
		iec104TransmissionArray[13 + i * 4] = highByte(spStartAddr+spont_M_SP[dataArrayCount]);	// IOA address
		iec104TransmissionArray[14 + i * 4] = 0;													// IOA address
		iec104TransmissionArray[15 + i * 4] = lowByte(data_M_SP[spont_M_SP[dataArrayCount]]);		// User Data

		dataArrayCount++;
	}

	CLIENT[whoIs_].write(iec104TransmissionArray, iec104TransmissionArray[1] + 2);					// Move data to Socket buffer
	TXcnt[whoIs_] += 2;
	if (IECDebug)
	{																			// Increment TX counter
		SerialUSB.print("TXcnt -> ");
		SerialUSB.println(TXcnt[whoIs_]);
		SerialUSB.print("RXcnt -> ");
		SerialUSB.println(RXcnt[whoIs_]);
	}
}




// Single with time TAG
void IEC_104::spontan_M_SP_TB_1(int16_t* data_M_SP,int16_t* spont_M_SP, byte numIx, int dataStartCount_, byte transsmitionCause_, byte whoIs_)
{
	int dataArrayCount = dataStartCount_;
	byte shift_;
	byte rtYear = rtc_clock104 ->get_years2();															// Get PLC date and Time
	byte rtMonth = rtc_clock104 -> get_months();
	byte rtDay = rtc_clock104 -> get_days();
	byte rtHour =rtc_clock104 -> get_hours();
	byte rtMinutes = rtc_clock104 -> get_minutes();
	int rtSeconds = rtc_clock104 -> get_seconds() * 1000;


	iec104TransmissionArray[0] = START2;																// START
	iec104TransmissionArray[1] = 10 + numIx * 11;														// APDU size (250)
	iec104TransmissionArray[2] = lowByte(TXcnt[whoIs_]);												// LSB N(S)
	iec104TransmissionArray[3] = highByte(TXcnt[whoIs_]);												// MSB N(S)
	iec104TransmissionArray[4] = lowByte(RXcnt[whoIs_]);												// LSB N(R)
	iec104TransmissionArray[5] = highByte(RXcnt[whoIs_]);												// LSB N(R)
	iec104TransmissionArray[6] = M_SP_TB_1;						    									// M_SP_TB_1
	iec104TransmissionArray[7] = numIx;																	// User data quantity
	iec104TransmissionArray[8] = transsmitionCause_;													// Cause of TX
	iec104TransmissionArray[9] = 0;																		// init station, optional
	iec104TransmissionArray[10] = lowByte(ASDU);													// Station Address
	iec104TransmissionArray[11] = highByte(ASDU);													// Station Address

	for (int i = 0; i < numIx; i++)
	{
		shift_ = i * 11;
		iec104TransmissionArray[12 + shift_] = lowByte(spStartAddr+spont_M_SP[dataArrayCount]);		// IOA address
		iec104TransmissionArray[13 + shift_] = highByte(spStartAddr+spont_M_SP[dataArrayCount]);		// IOA address
		iec104TransmissionArray[14 + shift_] = 0;														// IOA address
		iec104TransmissionArray[15 + shift_] = lowByte(data_M_SP[spont_M_SP[dataArrayCount]]);			// User Data

		// time tag
		iec104TransmissionArray[16 + shift_] = lowByte(rtSeconds);										// millis
		iec104TransmissionArray[17 + shift_] = highByte(rtSeconds);										// millis
		iec104TransmissionArray[18 + shift_] = rtMinutes;												// Minutes
		iec104TransmissionArray[19 + shift_] = rtHour;													// Hours
		iec104TransmissionArray[20 + shift_] = rtDay;													// Day
		iec104TransmissionArray[21 + shift_] = rtMonth;													// Month
		iec104TransmissionArray[22 + shift_] = rtYear;													// Year

		dataArrayCount++;
	}

	CLIENT[whoIs_].write(iec104TransmissionArray, iec104TransmissionArray[1] + 2);
	TXcnt[whoIs_] += 2;
	if (IECDebug)
	{																			// Increment TX counter
		SerialUSB.print("TXcnt -> ");
		SerialUSB.println(TXcnt[whoIs_]);
		SerialUSB.print("RXcnt -> ");
		SerialUSB.println(RXcnt[whoIs_]);
	}
}




// Single with time TAG
void IEC_104::spontan_M_ME_TE_1(int16_t* data_M_ME,int16_t* spont_M_ME, uint8_t* data_M_ME_QDS_, byte numIx, int dataStartCount_, byte transsmitionCause_, byte whoIs_)
{
	int dataArrayCount = dataStartCount_;
	byte shift_;
	byte rtYear = rtc_clock104 ->get_years2();															// Get PLC date and Time
	byte rtMonth = rtc_clock104 -> get_months();
	byte rtDay = rtc_clock104 -> get_days();
	byte rtHour =rtc_clock104 -> get_hours();
	byte rtMinutes = rtc_clock104 -> get_minutes();
	int rtSeconds = rtc_clock104 -> get_seconds() * 1000;


	iec104TransmissionArray[0] = START2;																// START
	iec104TransmissionArray[1] = 10 + numIx * 13;														// APDU size (250)
	iec104TransmissionArray[2] = lowByte(TXcnt[whoIs_]);												// LSB N(S)
	iec104TransmissionArray[3] = highByte(TXcnt[whoIs_]);												// MSB N(S)
	iec104TransmissionArray[4] = lowByte(RXcnt[whoIs_]);												// LSB N(R)
	iec104TransmissionArray[5] = highByte(RXcnt[whoIs_]);												// LSB N(R)
	iec104TransmissionArray[6] = M_ME_TE_1;						    									// M_SP_TB_1
	iec104TransmissionArray[7] = numIx;																	// User data quantity
	iec104TransmissionArray[8] = transsmitionCause_;													// Cause of TX
	iec104TransmissionArray[9] = 0;																		// init station, optional
	iec104TransmissionArray[10] = lowByte(ASDU);													// Station Address
	iec104TransmissionArray[11] = highByte(ASDU);													// Station Address

	if (IECDebug)
	SerialUSB.println("** ME");
	for (int i = 0; i < numIx; i++)
	{
		if (IECDebug)
		{
			SerialUSB.print("Number->");
			SerialUSB.print(dataArrayCount);
			SerialUSB.print(" addr->");
			SerialUSB.print(meStartAddr+spont_M_ME[dataArrayCount]);
			SerialUSB.print(" data->");
			SerialUSB.print(data_M_ME[spont_M_ME[dataArrayCount]]);
			SerialUSB.print(" QDS->");
			SerialUSB.println(data_M_ME_QDS_[spont_M_ME[dataArrayCount]]);
			
		}
		shift_ = i * 13;
		iec104TransmissionArray[12 + shift_] = lowByte(meStartAddr+spont_M_ME[dataArrayCount]);		// IOA address
		iec104TransmissionArray[13 + shift_] = highByte(meStartAddr+spont_M_ME[dataArrayCount]);		// IOA address
		iec104TransmissionArray[14 + shift_] = 0;														// IOA address
		iec104TransmissionArray[15 + shift_] = lowByte(data_M_ME[spont_M_ME[dataArrayCount]]);			// User Data
		iec104TransmissionArray[16 + shift_] = highByte(data_M_ME[spont_M_ME[dataArrayCount]]);	// User Data
		iec104TransmissionArray[17 + shift_] = data_M_ME_QDS_[spont_M_ME[dataArrayCount]];


		// time tag
		iec104TransmissionArray[18 + shift_] = lowByte(rtSeconds);										// millis
		iec104TransmissionArray[19 + shift_] = highByte(rtSeconds);										// millis
		iec104TransmissionArray[20 + shift_] = rtMinutes;												// Minutes
		iec104TransmissionArray[21 + shift_] = rtHour;													// Hours
		iec104TransmissionArray[22 + shift_] = rtDay;													// Day
		iec104TransmissionArray[23 + shift_] = rtMonth;													// Month
		iec104TransmissionArray[24 + shift_] = rtYear;													// Year

		dataArrayCount++;
	}

	CLIENT[whoIs_].write(iec104TransmissionArray, iec104TransmissionArray[1] + 2);
	TXcnt[whoIs_] += 2;
	if (IECDebug)
	{																					// Increment TX counter
		SerialUSB.print("TXcnt -> ");
		SerialUSB.println(TXcnt[whoIs_]);
		SerialUSB.print("RXcnt -> ");
		SerialUSB.println(RXcnt[whoIs_]);
	}
}









// Single with time TAG
void IEC_104::spontan_M_ME_TF_1(float* data_M_ME,int16_t* spont_M_ME,  uint8_t* IEC104_M_TF_QDS_, byte numIx, int dataStartCount_, byte transsmitionCause_, byte whoIs_)
{
	
	//
	
	
	//	SerialUSB.println(data_M_ME[0]);
	
	
	int dataArrayCount = dataStartCount_;
	byte shift_;
	byte rtYear = rtc_clock104 ->get_years2();															// Get PLC date and Time
	byte rtMonth = rtc_clock104 -> get_months();
	byte rtDay = rtc_clock104 -> get_days();
	byte rtHour =rtc_clock104 -> get_hours();
	byte rtMinutes = rtc_clock104 -> get_minutes();
	int rtSeconds = rtc_clock104 -> get_seconds() * 1000;


	iec104TransmissionArray[0] = START2;																// START
	iec104TransmissionArray[1] = 10 + numIx * 15;														// APDU size (250)
	iec104TransmissionArray[2] = lowByte(TXcnt[whoIs_]);												// LSB N(S)
	iec104TransmissionArray[3] = highByte(TXcnt[whoIs_]);												// MSB N(S)
	iec104TransmissionArray[4] = lowByte(RXcnt[whoIs_]);												// LSB N(R)
	iec104TransmissionArray[5] = highByte(RXcnt[whoIs_]);												// LSB N(R)
	iec104TransmissionArray[6] = M_ME_TF_1;						    									// M_SP_TB_1
	iec104TransmissionArray[7] = numIx;																	// User data quantity
	iec104TransmissionArray[8] = transsmitionCause_;													// Cause of TX
	iec104TransmissionArray[9] = 0;																		// init station, optional
	iec104TransmissionArray[10] = lowByte(ASDU);													// Station Address
	iec104TransmissionArray[11] = highByte(ASDU);													// Station Address




	union {
		float fval;
		byte bval[4];
	} floatAsBytes;
	//	int temp=0;

	if (IECDebug)
	SerialUSB.println("** TF");
	
	for (int i = 0; i < numIx; i++)
	{
		if (IECDebug)
		{
			SerialUSB.print("Number->");
			SerialUSB.print(dataArrayCount);
			SerialUSB.print(" addr->");
			SerialUSB.print(tfStartAddr+spont_M_ME[dataArrayCount]);
			SerialUSB.print(" data->");
			SerialUSB.print(data_M_ME[spont_M_ME[dataArrayCount]]);
			SerialUSB.print(" QDS->");
			SerialUSB.println(IEC104_M_TF_QDS_[spont_M_ME[dataArrayCount]]);
		}
		
		floatAsBytes.fval = data_M_ME[spont_M_ME[dataArrayCount]];
		shift_ = i * 15;
		iec104TransmissionArray[12 + shift_] = lowByte(tfStartAddr+spont_M_ME[dataArrayCount]);			// IOA address
		iec104TransmissionArray[13 + shift_] = highByte(tfStartAddr+spont_M_ME[dataArrayCount]);		// IOA address
		iec104TransmissionArray[14 + shift_] = 0;														// IOA address
		iec104TransmissionArray[15 + shift_] = floatAsBytes.bval[0];		// User Data
		iec104TransmissionArray[16 + shift_] = floatAsBytes.bval[1];			// User Data
		iec104TransmissionArray[17 + shift_] = floatAsBytes.bval[2];		// User Data
		iec104TransmissionArray[18 + shift_] = floatAsBytes.bval[3];		// User Data
		
		iec104TransmissionArray[19 + shift_] = IEC104_M_TF_QDS_[spont_M_ME[dataArrayCount]];		 // QDS



		// time tag
		iec104TransmissionArray[20 + shift_] = lowByte(rtSeconds);										// millis
		iec104TransmissionArray[21 + shift_] = highByte(rtSeconds);										// millis
		iec104TransmissionArray[22 + shift_] = rtMinutes;												// Minutes
		iec104TransmissionArray[23 + shift_] = rtHour;													// Hours
		iec104TransmissionArray[24 + shift_] = rtDay;													// Day
		iec104TransmissionArray[25 + shift_] = rtMonth;													// Month
		iec104TransmissionArray[26 + shift_] = rtYear;													// Year

		
		// 		SerialUSB.print("dataArrayCount -> ");
		// 		SerialUSB.println(dataArrayCount);
		//
		//		SerialUSB.print("spont_M_ME[dataArrayCount] -> ");
		// 		SerialUSB.println(spont_M_ME[dataArrayCount]);
		
		//		SerialUSB.print("IEC104_M_TF_QDS_ -> ");
		// 		SerialUSB.println(IEC104_M_TF_QDS_[spont_M_ME[dataArrayCount]]
		dataArrayCount++;
	}

	CLIENT[whoIs_].write(iec104TransmissionArray, iec104TransmissionArray[1] + 2);
	TXcnt[whoIs_] += 2;
	if (IECDebug)
	{																					// Increment TX counter
		SerialUSB.print("TXcnt -> ");
		SerialUSB.println(TXcnt[whoIs_]);
		SerialUSB.print("RXcnt -> ");
		SerialUSB.println(RXcnt[whoIs_]);
	}
}























void IEC_104:: init104(RTC_clock &RTC_Class)
{
	rtc_clock104 = &RTC_Class;														// Pointer to RTC class
}



void IEC_104:: clockConf(byte whoIs_)
{

	iec104TransmissionArray[0] = START2;											// START
	iec104TransmissionArray[1] = 0x14;												// APDU size (250)
	iec104TransmissionArray[2] = lowByte(TXcnt[whoIs_]);							// LSB N(S)
	iec104TransmissionArray[3] = highByte(TXcnt[whoIs_]);							// MSB N(S)
	iec104TransmissionArray[4] = lowByte(RXcnt[whoIs_]);							// LSB N(R)
	iec104TransmissionArray[5] = highByte(RXcnt[whoIs_]);							// LSB N(R)
	iec104TransmissionArray[6] = C_CS_NA_1;											// C_CS_NA_1
	iec104TransmissionArray[7] = 1;													// User data quantity
	iec104TransmissionArray[8] = ACT_con;											// Activation confirmation
	iec104TransmissionArray[9] = 0;													// init station, optional
	iec104TransmissionArray[10] = lowByte(ASDU);								// Station Address
	iec104TransmissionArray[11] = highByte(ASDU);								// Station Address

	checkTime();

	iec104TransmissionArray[12] = iec104ReciveArray[12];							// IOA address
	iec104TransmissionArray[13] = iec104ReciveArray[13];							// IOA address
	iec104TransmissionArray[14] = iec104ReciveArray[14];							// IOA address
	iec104TransmissionArray[15] = iec104ReciveArray[15];							// Seconds
	iec104TransmissionArray[16] = iec104ReciveArray[16];							// Seconds
	iec104TransmissionArray[17] = iec104ReciveArray[17];							// Minutes
	iec104TransmissionArray[18] = iec104ReciveArray[18];							// Hours
	iec104TransmissionArray[19] = iec104ReciveArray[19];							// Day
	iec104TransmissionArray[20] = iec104ReciveArray[20];							// Month
	iec104TransmissionArray[21] = iec104ReciveArray[21];							// Year


	CLIENT[whoIs_].write(iec104TransmissionArray, iec104TransmissionArray[1] + 2);
	TXcnt[whoIs_] += 2;																// Increment TX counter
	if (IECDebug)
	{
		SerialUSB.print("TXcnt -> ");
		SerialUSB.println(TXcnt[whoIs_]);
		SerialUSB.print("RXcnt -> ");
		SerialUSB.println(RXcnt[whoIs_]);
	}

	T3_104[whoIs_] = millis();

}



void IEC_104:: checkTime()
{
	int TIME_PLC[6];
	int TIME_104[6];
	//	byte updateRTC104 = 0;

	TIME_104[0] = iec104ReciveArray[21];									// IEC Year
	TIME_104[1] = iec104ReciveArray[20];									// IEC Month
	TIME_104[2] = iec104ReciveArray[19];									// IEC Days
	TIME_104[3] = iec104ReciveArray[18];									// IEC Hours
	TIME_104[4] = iec104ReciveArray[17];									// IEC Minutes
	int rtSeconds = word(iec104ReciveArray[16], iec104ReciveArray[15]);		// IEC seconds + millis
	TIME_104[5] = rtSeconds*0.001;											// IEC Seconds

	TIME_PLC[0] = rtc_clock104 -> get_years2();
	TIME_PLC[1] = rtc_clock104 -> get_months();
	TIME_PLC[2] = rtc_clock104 -> get_days();
	TIME_PLC[3] = rtc_clock104 -> get_hours();
	TIME_PLC[4] = rtc_clock104 -> get_minutes();
	TIME_PLC[5] = rtc_clock104 -> get_seconds();

	byte valid = 0;
	
	
	if (abs(TIME_PLC[4]*60 + TIME_PLC[5] - TIME_104[4]*60 - TIME_104[5]) < 5)
	for (byte i = 0; i <= 3; i++)											// Check time tolerance
	{
		if (TIME_PLC[3-i] != TIME_104[3-i])
		break;
		valid++;
	}

	if (valid == 4)  // Time in tolerance
	return;

	//	for (byte i=0; i<6; i++)
	//	SerialUSB.println( TIME_104[i]);
	if (IECDebug)
	{
		SerialUSB.println("Set clock");
		/*
		SerialUSB.print( TIME_104[0]);
		SerialUSB.print(" ");
		SerialUSB.print( TIME_104[1]);
		SerialUSB.print(" ");
		SerialUSB.print( TIME_104[2]);
		SerialUSB.print(" ");
		SerialUSB.print( TIME_104[3]);
		SerialUSB.print(" ");
		SerialUSB.print( TIME_104[4]);
		SerialUSB.print(" ");
		SerialUSB.print( TIME_104[5]);
		SerialUSB.print(" ");
		*/
	}
	
	// time out of tolerance - update Time and Calendar
	
	//	(int hour, int minute, int second)
	//	rtc_clock104 -> set_time(TIME_104[3], TIME_104[4], 0);
	//(int day, int month, uint16_t year)
	//	rtc_clock104 -> set_date(TIME_104[2], TIME_104[1], TIME_104[0]);
	
	if (IECDebug)
	SerialUSB.println("RTC_FLAG_WAIT -> Set ");
	
	rtc_clock104->rtcFlag = RTC_FLAG_WAIT;
	
	//						//( Rtc*  wYear,      ucMonth,     ucDay,	  ucHour,	  ucMinute,    ucSecond )
	rtc_clock104 -> updateRTC(RTC, TIME_104[0],TIME_104[1],TIME_104[2],TIME_104[3],TIME_104[4],TIME_104[5]);
	
}



void IEC_104::checkIncomingIP(IPAddress remoteIP_)
{
	byte valid = 0;
	for (byte i=0; i<MAX_CLIENT_NUM; i++)
	{
		if (i != incomingSocket)						// check for IP duplication (except current client)
		{
			for (byte j = 0; j < 4; j++)
			{
				if (IPlist[i][3-j] != remoteIP_[3-j])	// Parse IP in reverse direction - faster way
				{
					valid = 0;							// Break loop if IP not match
					break;
				}
				valid++;
			}


			if (valid == 4)								// duplicated IP - remove and release socket
			{
				socketState[i] = SOCK_empty;			// Clear socket activation state
				CLIENT[i].stop();
				clientState[i] = STOPDT_act;			// Stop client activation state
				w5500.execCmdSn(i,Sock_CLOSE);			// Close socket
				IPlist[i] = blankIP;					// Clear IP from client data base
				ipUpdate = 1;							// Update HMI connected client table
				if (IECDebug)
				SerialUSB.println("Duplicated IP - stop");
			}
			valid = 0;
		}
	}
}


byte IEC_104::proceedClient()
{

	IPAddress remoteIP = CLIENT[bufSize].remoteIP();		// Get IP of incoming Client
	incomingSocket = CLIENT[bufSize].getSocketNumber();  	// Get Socket of client
	//	byte valid = 0;
	
	incomingPort = CLIENT[bufSize].remotePort();
	
	if (IECDebug)
	{
		
		SerialUSB.print("IncomingSocket -> ");
		SerialUSB.println(incomingSocket);

		SerialUSB.print("Incoming IP -> ");
		SerialUSB.println(remoteIP);
		
		SerialUSB.print("Incoming Port -> ");
		SerialUSB.println(incomingPort);
	}

	//	if (clientState[incomingSocket] != STARTDT_act)
	//	checkIncomingIP(remoteIP);

	checkSockets();

	byte totalClients = 0;
	for (byte i=0; i<MAX_CLIENT_NUM; i++)
	{
		if (socketState[i] != SOCK_empty)
		totalClients++;
	}

	if (IECDebug)
	{
		SerialUSB.print("Connected hosts -> ");
		SerialUSB.println(totalClients);
	}



	/*
	if (totalClients > 5)
	{
	socketState[incomingSocket] = empty;				// Clear socket activation
	CLIENT[incomingSocket].stop();
	clientState[incomingSocket] = stop;
	w5500.execCmdSn(incomingSocket,Sock_CLOSE);
	IPlist[incomingSocket] = blankIP;
	ipUpdate = 1;
	return empty;  // New client not accepted - Sockets overload
	}
	checkMaxClients();
	*/


	if (socketState[incomingSocket] == incomingSocket && IPlist[incomingSocket] == remoteIP && PORTlist[incomingSocket] == incomingPort)		// Already in our base?
	{
		if (IECDebug)
		SerialUSB.println("Socket in base");
		return incomingSocket;
	}
	


	socketState[incomingSocket] = incomingSocket;			// New? - save socket number in client base
	CLIENT[incomingSocket] = iec104Server.available();		// Make client object for new incoming
	IPlist[incomingSocket] = remoteIP;
	PORTlist[incomingSocket] = incomingPort;
	ipUpdate = 1;
	
	if (IECDebug)
	{
		
		SerialUSB.print("****  New Host! -> ");
		SerialUSB.println(incomingSocket);
		SerialUSB.print("**** New incomingSocket -> ");
		SerialUSB.println(incomingSocket);

		SerialUSB.print("**** New IP -> ");
		SerialUSB.println(remoteIP);
		
		SerialUSB.print("**** New incomingPort -> ");
		SerialUSB.println(incomingPort);
	}

	//SerialUSB.println("New add to base");
	return incomingSocket;									// Client form our base - keep communicating

}




void IEC_104 :: monitorIncoming(int16_t* data_M_SP, int16_t* data_M_ME, uint8_t* data_M_ME_QDS_, float* data_M_TF, uint8_t* IEC104_M_TF_QDS_)
{

	//SerialUSB.println();
	//SerialUSB.println("*********************");

	//SerialUSB.print("Eth -> ");
	//	SerialUSB.println(_csPin);

	CLIENT[bufer] = iec104Server.available();				// Check if anyone got incoming


	if (!CLIENT[bufer].available())						// got incoming?
	{
		//SerialUSB.println("no incoming");
		return;
	}
	
	if (IECDebug)
	SerialUSB.println("********************");
	// SerialUSB.println(_csPin);

	byte whoIs_ = proceedClient();						// Validate incoming

	if (IECDebug)
	{
		SerialUSB.print("Incoming whoIs_ -> ");
		SerialUSB.println(whoIs_);
	}


	proceedReply(data_M_SP, data_M_ME, data_M_ME_QDS_,data_M_TF,IEC104_M_TF_QDS_,whoIs_);						// We got a valid client - reply!
	
	//	SerialUSB.println("*******");

}


void IEC_104 :: proceedReply(int16_t* data_M_SP, int16_t* data_M_ME, uint8_t* data_M_ME_QDS_,  float* data_M_TF, uint8_t* IEC104_M_TF_QDS_,byte whoIs_)
{

	int QuerryType = getQuerryType(whoIs_);						//APDU


	if (QuerryType == closeConnection)
	{
		if (IECDebug)
		SerialUSB.println("proceedReply closeConnection");
		return;
	}

	if (IECDebug)
	{
		SerialUSB.print("State -> ");
		SerialUSB.println(clientState[whoIs_]);
	}

	if (clientState[whoIs_] == STOPDT_act)
	switch (QuerryType)
	{
		case STARTDT_act:										// U-frame STARTDT act

		TXcnt[whoIs_] = 0;										// Reset TX RX counters
		RXcnt[whoIs_] = 0;										// Reset TX RX counters
		if (IECDebug)
		SerialUSB.println("RX STARTDT_act");
		APCI(STARTDT_con,whoIs_);
		if (IECDebug)								//  Send APCI STARTDT_con
		SerialUSB.println("TX STARTDT_con");
		clientState[whoIs_] = STARTDT_act;						// Approved client

		return;

		break;


		default:
		if (IECDebug)
		SerialUSB.println ("Flush ");
		while (CLIENT[whoIs_].available())						// Flush Socket buffer
		CLIENT[whoIs_].read();
		return;

		break;
	}
	


	switch (QuerryType)
	{
		case STARTDT_act:										// U-frame STARTDT act

		TXcnt[whoIs_] = 0;										// Reset TX RX counters
		RXcnt[whoIs_] = 0;										// Reset TX RX counters
		if (IECDebug)
		SerialUSB.println("RX STARTDT_act");
		APCI(STARTDT_con,whoIs_);								// Send APCI STARTDT_con
		if (IECDebug)
		SerialUSB.println("TX STARTDT_con");
		break;


		case TESTFR_act:										//  TESTFR act
		if (IECDebug)
		SerialUSB.println("RX TESTFR_act");
		APCI(TESTFR_con, whoIs_);								// Send APCI TESTFR_con
		if (IECDebug)
		SerialUSB.println("TX TESTFR_con");
		break;

		case TESTFR_con:										// TESTFR act
		if (IECDebug)
		SerialUSB.println("RX TESTFR con");
		UTESTER[whoIs_] = 0;									// Deactivate communication tester packet
		// SerialUSB.println(T3_104[whoIs_]);
		break;




		case STOPDT_act: //STOPDT act
		if (IECDebug)
		SerialUSB.println("RX STOPDT_act");
		APCI(STOPDT_con, whoIs_);								// Send APCI STOPDT_con
		stopClient(whoIs_);
		if (IECDebug)	{
			SerialUSB.println("TX STOPDT_con");
			SerialUSB.println("Stop client ************");
		}
		break;


		case S_frame:											// S - Frame

		T3_104[whoIs_] = millis();
		if (IECDebug)
		SerialUSB.println("RX S_frame");
		//	SerialUSB.println(T3_104[whoIs_]);
		break;

		// >>>>>>>>>>>>>>>>>>>>>>>>>>>

		case I_frame:											// I frame

		switch (iec104ReciveArray[6])
		{
			case C_IC_NA_1:										// Station interrogation
			if (IECDebug)
			SerialUSB.println("RX C_IC_NA_1");
			serviceFrame104(ACT_con, whoIs_, C_IC_NA_1);		// Activation confirmation
			if (IECDebug)
			SerialUSB.println("TX ACT_con");
			iec104DataTranssmition(data_M_SP, data_M_ME, data_M_ME_QDS_,data_M_TF, IEC104_M_TF_QDS_, inrogen, whoIs_);  // Single Point Information
			if (IECDebug)
			SerialUSB.println("TX Data");
			serviceFrame104(ACT_term, whoIs_, C_IC_NA_1);		// Activation termination
			if (IECDebug)
			SerialUSB.println("TX ACT_term");
			break;

			case C_CS_NA_1:										// Clock Synchronization Command
			if (IECDebug)
			SerialUSB.println("RX Clock sync");
			clockConf(whoIs_);
			if (IECDebug)
			SerialUSB.println("TX ActCon");
			break;


			default:
			// add possible cases
			break;
		}

		break;

	}

	
	
	//SerialUSB.print("Eth -> ");
	//SerialUSB.println(_csPin);
	//SerialUSB.println();
	//SerialUSB.println("****");
}



byte IEC_104 :: checkCNT(byte whoIs_)
{


	int TXmaster = word(iec104ReciveArray[5], iec104ReciveArray[4]);		// Calc Master TX and RX counters
	int RXmaster = word(iec104ReciveArray[3], iec104ReciveArray[2]);

	/*
	SerialUSB.print("TXmaster -> ");
	SerialUSB.println(TXmaster);
	SerialUSB.print("RXmaster -> ");
	SerialUSB.println(RXmaster);
	SerialUSB.print("TXcnt -> ");
	SerialUSB.println(TXcnt[whoIs_]);
	SerialUSB.print("RXcnt -> ");
	SerialUSB.println(RXcnt[whoIs_]);
	*/

	if (TXcnt[whoIs_] != TXmaster)											// Check Master and Slave counters
	return cntError;														// Non

	if (RXcnt[whoIs_] != RXmaster)
	return cntError;

	return cntOk;

}



void IEC_104 :: stopClient(byte whoIs_)
{
	TXcnt[whoIs_] = 0;  // Reset TX RX counters
	RXcnt[whoIs_] = 0;  // Reset TX RX counters

	//	stackDebug = millis();
	//	SerialUSB.println(millis() - stackDebug);
	while (CLIENT[whoIs_].available())			// Read I packet from client ring buffer
	CLIENT[whoIs_].read();



	clientState[whoIs_] = STOPDT_act;
	socketState[whoIs_] = SOCK_empty;
	CLIENT[whoIs_].stop();
	ipUpdate = 1;
	IPlist[whoIs_] = blankIP;
}




byte IEC_104 :: getQuerryType(byte whoIs_)
{

	byte i = 0;				// byte counter



	i += dataToBuffer(whoIs_, i, 2);			// check APDU packet length


	byte APDUlen = iec104ReciveArray[1];

	i += dataToBuffer(whoIs_, i, APDUlen+2);	// Read first packet in socket buffer


	switch (APDUlen)
	{
		case Uframe:
		if (IECDebug)
		SerialUSB.println("RX U frame  ");

		return iec104ReciveArray[2];								// command

		break;


		default:

		//	if (checkCNT(whoIs_) == cntOk)  // Check counters
		//	{
		RXcnt[whoIs_] += 2;										// Increment RX incoming packet counter
		if (IECDebug)
		{
			SerialUSB.print("TXcnt -> ");
			SerialUSB.println(TXcnt[whoIs_]);
			SerialUSB.print("RXcnt -> ");
			SerialUSB.println(RXcnt[whoIs_]);
			SerialUSB.println ("RX I frame ");
		}
		return I_frame;
		//	}
		//	else
		//	{
		//		stopClient(whoIs_);

		//SerialUSB.println ("Wrong Sequence ");
		//		return closeConnection;
		//	}
		break;

	}
}



byte IEC_104 :: dataToBuffer(byte whoIs_, byte startCnt, byte dataQt_)
{
	byte i = startCnt;

	while (CLIENT[whoIs_].available() && i<dataQt_)					// Read U packet from client ring buffer
	{
		iec104ReciveArray[i] = CLIENT[whoIs_].read();
		i++;
	}

	return i;

}