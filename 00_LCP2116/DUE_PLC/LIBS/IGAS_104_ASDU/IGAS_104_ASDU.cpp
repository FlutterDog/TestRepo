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


#include "IGAS_104_ASDU.h"


EthernetClient CLIENT[MAX_CLIENT_NUM+1];
EthernetServer iec104Server(2404);							// IEC 104 Port
IPAddress blankIP(0, 0, 0, 0);								// blank IP mask

byte const initSerialDebug = 0;


IEC_104::IEC_104(byte csPin)
{
	_csPin = csPin;
}




// iec104DataTranssmition - reply to general interogation
void IEC_104::iec104DataTranssmition(int16_t* data_M_SP, int16_t* data_M_ME, float* data_M_TF,byte transsmitionCause,uint8_t QDS_valid, byte whoIs_, uint16_t device_)		// Transmit USER Data using TX Cause
{
	int16_t fullPackageNumber = size_M_SP * 0.025;
	int16_t lastPackageLength = size_M_SP - fullPackageNumber * 40;									// Calc packets to transfer full data
	
	int16_t dataAdr =  0;
	int16_t ioAddr = getStartAddress_SP(device_);
	byte uDataInPacket = 40;

	if (fullPackageNumber)
	for (int16_t j = 0; j < fullPackageNumber; j++)
	{
		
		transmit_M_SP_NA_1(data_M_SP, uDataInPacket, ioAddr, dataAdr, transsmitionCause, whoIs_);		// Transmit M_SP_NA_1 packet
		ioAddr += uDataInPacket;  // +1 - first data address in packet - M_ME_NB_1, transferred in another packet
		dataAdr += uDataInPacket; // +1 - first data address in packet - M_ME_NB_1, transferred in another packet
		delay(interframe_);
	}

	if (lastPackageLength)
	{
		transmit_M_SP_NA_1(data_M_SP, lastPackageLength, ioAddr, dataAdr, transsmitionCause, whoIs_);
		delay(interframe_);
	}
	

	// Transmit Measured value

	fullPackageNumber = size_M_ME * 0.04;
	lastPackageLength = size_M_ME - fullPackageNumber * 25;									// Calc packets to transfer full data

	dataAdr =  0;
	ioAddr = getStartAddress_ME(device_);
	uDataInPacket = 25;

	if (fullPackageNumber)
	for (int16_t j = 0; j < fullPackageNumber; j++)
	{
		transmit_M_ME_NB_1(data_M_ME, QDS_valid, uDataInPacket, ioAddr, dataAdr, transsmitionCause, whoIs_);		// Transmit M_SP_NA_1 packet
		ioAddr += uDataInPacket;  // first data address in packet - M_ME_NB_1, transferred in another packet
		dataAdr += uDataInPacket; // first data address in packet - M_ME_NB_1, transferred in another packet
		delay(interframe_);
	}

	if (lastPackageLength)
	{
		transmit_M_ME_NB_1(data_M_ME, QDS_valid, lastPackageLength, ioAddr, dataAdr, transsmitionCause, whoIs_);
		delay(interframe_);
	}


	// Transmit Short floating point value


	// 	fullPackageNumber = size_M_TF * 0.025;
	// 	lastPackageLength = size_M_TF - fullPackageNumber * 40;									// Calc packets to transfer full data


	fullPackageNumber = size_M_TF * 0.04;
	lastPackageLength = size_M_TF - fullPackageNumber * 25;
	
	dataAdr =  0;
	ioAddr = getStartAddress_TF(device_);

	uDataInPacket = 25;


	if (fullPackageNumber)
	for (int16_t j = 0; j < fullPackageNumber; j++)
	{
		transmit_M_ME_NC_1(data_M_TF,  QDS_valid, uDataInPacket, ioAddr, dataAdr, transsmitionCause, whoIs_);		// Transmit M_ME_NC_1 packet
		ioAddr += uDataInPacket;  // first data address in packet - M_ME_NB_1, transferred in another packet
		dataAdr += uDataInPacket; // first data address in packet - M_ME_NB_1, transferred in another packet
		delay(interframe_);
	}

	if (lastPackageLength)
	{
		transmit_M_ME_NC_1(data_M_TF,QDS_valid, lastPackageLength, ioAddr, dataAdr, transsmitionCause, whoIs_);
		delay(interframe_);
	}



	T3_104[whoIs_] = millis();																	// Update t3 silence timer
}





void IEC_104::iec104DataSpontan(int16_t* data_M_SP, int16_t* spont_M_SP,int16_t* data_M_ME,int16_t* spont_M_ME, uint8_t QDS_valid, float* data_M_TF,int16_t* spont_M_TF, byte transsmitionCause, byte whoIs_, uint16_t device_)	// Transmit USER Data using TX Cause
{
	// 	SerialUSB.print("IEC  dataChng_M_SP_ -> ");
	// 	SerialUSB.println(dataChng_M_SP_);
	
	int16_t fullPackageNumber = dataChng_M_SP_ * 0.025;
	int16_t lastPackageLength = dataChng_M_SP_ - fullPackageNumber * 40;

	// 	SerialUSB.print("IEC  fullPackageNumber -> ");
	// 	SerialUSB.println(fullPackageNumber);
	// 	SerialUSB.print("IEC  lastPackageLength -> ");
	// 	SerialUSB.println(lastPackageLength);

	int16_t dataAdr =  0;

	byte  uDataInPacket = 40;

	if (fullPackageNumber)
	for (int16_t j = 0; j < fullPackageNumber; j++)
	{
		if (initSerialDebug)
		{
			SerialUSB.println("104 139");
		}
		spontan_M_SP_TB_1(data_M_SP, spont_M_SP,uDataInPacket, dataAdr, transsmitionCause, whoIs_, device_);		// With time tag
		dataAdr += uDataInPacket;
	}

	if (initSerialDebug)
	{
		SerialUSB.println("104 147");
	}

	if (lastPackageLength)
	spontan_M_SP_TB_1(data_M_SP, spont_M_SP, lastPackageLength, dataAdr, transsmitionCause, whoIs_, device_);		// With time tag

	if (initSerialDebug)
	{
		SerialUSB.println("104 155");
	}

	// Spontaneous measured value transmission

	fullPackageNumber = dataChng_M_ME_ * 0.1;
	lastPackageLength = dataChng_M_ME_ - fullPackageNumber * 10;

	dataAdr =  0;
	uDataInPacket = 10;

	if (fullPackageNumber)
	for (int16_t j = 0; j < fullPackageNumber; j++)
	{
		spontan_M_ME_TE_1(data_M_ME, spont_M_ME, QDS_valid,uDataInPacket, dataAdr, transsmitionCause, whoIs_, device_);		// With time tag
		dataAdr += uDataInPacket;
	}

	if (lastPackageLength)
	spontan_M_ME_TE_1(data_M_ME,spont_M_ME, QDS_valid, lastPackageLength, dataAdr, transsmitionCause, whoIs_, device_);		// With time tag

	if (initSerialDebug)
	{
		SerialUSB.println("104 178");
	}

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
	for (int16_t j = 0; j < fullPackageNumber; j++)
	{
		spontan_M_ME_TF_1(data_M_TF, spont_M_TF, QDS_valid,uDataInPacket, dataAdr, transsmitionCause, whoIs_, device_);		// With time tag
		dataAdr += uDataInPacket;
	}

	if (lastPackageLength)
	spontan_M_ME_TF_1(data_M_TF,spont_M_TF, QDS_valid, lastPackageLength, dataAdr, transsmitionCause, whoIs_, device_);		// With time tag

	if (initSerialDebug)
	{
		SerialUSB.println("104 211");
	}
	
	T3_104[whoIs_] = millis();																	// Update t3 silence timer
}



void IEC_104::LOOP(void** devices, int16_t* data_M_SP, int16_t* spont_M_SP, int16_t* data_M_ME,  int16_t* spont_M_ME,float* data_M_TF, int16_t* spont_M_TF)
{
	Ethernet.select(_csPin);
	//checkSockets();
	for (int16_t i = 0; i < MAX_SOCK_NUM; i++)								// Go through all sockets
	{
		if (w5500.readSnSR(i) == CLOSED)
		{
			if (clientState[i] == STARTDT_act)
			stopClient(i);
		}
	}
	
	monitorIncoming(devices, data_M_SP, data_M_ME, data_M_TF);		// Check incoming data to sockets

	monitorSporadic(devices, data_M_SP, spont_M_SP, data_M_ME, spont_M_ME, data_M_TF,  spont_M_TF); // Spontaneous send
	
}



uint16_t IEC_104::getStartAddress_SP(uint16_t device_)
{
	switch (device_)
	{
		case LCP:
		
		return LCP_Start_Addr_SP;
		
		break;
		
		case LDO:
		
		return LDO_Start_Addr_SP;
		
		break;
		
		case LAI:
		
		return LAI_Start_Addr_SP;
		
		break;
		
		case LDI8:
		
		return LDI8_Start_Addr_SP;
		
		break;
		
		case LCT:
		
		return LCT_Start_Addr_SP;
		
		break;
		
		case LDI16:
		
		return LDI16_Start_Addr_SP;
		
		break;
		
		
		case IR_SF6:
		
		return IR_SF6_Start_Addr_SP;
		
		break;
		
		case DM910:
		
		return DM910_Start_Addr_SP;
		
		break;
		
		case DM910H:
		
		return DM910H_Start_Addr_SP;
		
		break;
		
		case PVT100:
		
		return PVT100_Start_Addr_SP;
		
		break;
		
		
	}
	
}



uint16_t IEC_104::getStartAddress_ME(uint16_t device_)
{
	switch (device_)
	{
		case LCP:
		
		return LCP_Start_Addr_ME;
		
		break;
		
		case LDO:
		
		return LDO_Start_Addr_ME;
		
		break;
		
		case LAI:
		
		return LAI_Start_Addr_ME;
		
		break;
		
		case LDI8:
		
		return LDI8_Start_Addr_ME;
		
		break;
		
		case LCT:
		
		return LCT_Start_Addr_ME;
		
		break;
		
		case LDI16:
		
		return LDI16_Start_Addr_ME;
		
		break;
		
		
		case IR_SF6:
		
		return IR_SF6_Start_Addr_ME;
		
		break;
		
		case DM910:
		
		return DM910_Start_Addr_ME;
		
		break;
		
		case DM910H:
		
		return DM910H_Start_Addr_ME;
		
		break;
		
		case PVT100:
		
		return PVT100_Start_Addr_ME;
		
		break;
		
	}
	
}



uint16_t IEC_104::getStartAddress_TF(uint16_t device_)
{
	switch (device_)
	{
		case LCP:
		
		return LCP_Start_Addr_TF;
		
		break;
		
		case LDO:
		
		return LDO_Start_Addr_TF;
		
		break;
		
		case LAI:
		
		return LAI_Start_Addr_TF;
		
		break;
		
		case LDI8:
		
		return LDI8_Start_Addr_TF;
		
		break;
		
		case LCT:
		
		return LCT_Start_Addr_TF;
		
		break;
		
		case LDI16:
		
		return LDI16_Start_Addr_TF;
		
		break;
		
		
		case IR_SF6:
		
		return IR_SF6_Start_Addr_TF;
		
		break;
		
		case DM910:
		
		return DM910_Start_Addr_TF;
		
		break;
		
		case DM910H:
		
		return DM910H_Start_Addr_TF;
		
		break;
		
		case PVT100:
		
		return PVT100_Start_Addr_TF;
		
		break;
		
		
	}
	
}



void IEC_104::monitorSporadic(void** devices, int16_t* data_M_SP, int16_t* spont_M_SP, int16_t* data_M_ME,  int16_t* spont_M_ME,float* data_M_TF, int16_t* spont_M_TF) // Spontaneous send
{
	ASDUx = 0;

	for (uint8_t k=0; k<totalASDU; k++)
	{
		//k = findASDU(devices,ASDUx_ );
		
		ASDUx = ((DeviceCommon*)devices[k])->ASDU;
		dataChng_M_SP_ = 0;
		dataChng_M_ME_ = 0;
		dataChng_M_TF_ = 0;
		byte QDS_;
		
		switch (((DeviceCommon*)devices[k])->ID)
		{
			case LDO:
			{
				if (initSerialDebug)
				{
					SerialUSB.println("104 229");
				}
				
				LDO1118* device = (LDO1118*)devices[k];
				checkQDS_Link(device, data_M_SP,spont_M_SP,QDS_);

				resetUpdFlags(device);

				stationTransmit(devices, data_M_SP,spont_M_SP, data_M_ME, spont_M_ME, QDS_, data_M_TF, spont_M_TF, LDO);		// Spontaneous data transmission if needed
				
				
			}
			break;

			case LAI:
			{
				if (initSerialDebug)
				{
					SerialUSB.println("104 244");
				}
				
				LAI1118* device = (LAI1118*)devices[k];

				size_M_SP = device->SP_qnt;	 // 104 Gas System data array size
				size_M_ME = device->ME_qnt;
				size_M_TF = device->TF_qnt;

				if (checkQDS_Link(device, data_M_SP,spont_M_SP,QDS_) == 1) // 1 - статус связи изменился. Отправляем все данные с меткой
				{
					device->SP_Upd = 0xFF;
					device->ME_Upd = 0xFF;
				}
				
				// SP data
				if (device->SP_Upd)
				for (uint8_t i = 0; i < (size_M_SP-1); i++)
				if (bitRead(device->SP_Upd, i))
				{
					data_M_SP[i+1] = bitRead(device->DigitalInput, i) | QDS_; //  +1 = start address for SP DI data
					spont_M_SP[dataChng_M_SP_] = i+1;  //  +1 = start address for SP DI data
					dataChng_M_SP_++;
				}
				
				// ME data
// 				if (device->ME_Upd)
// 				for (uint8_t i = 0; i < size_M_ME; i++)
// 				if (bitRead(device->ME_Upd, i))
// 				{
// 					int16_t* analogInputPtr = &device->AnalogInput01 + i; // get a pointer to the current analog input
// 					data_M_ME[i] = *analogInputPtr;
// 					spont_M_ME[dataChng_M_ME_] = i;
// 					dataChng_M_ME_++;
// 				}
// 				
				// TF data
				float* floatPtr = &device->AnalogInput01; // get a pointer to the current analog input
				//	uint64_t updBuf = device->TF_Upd;
				
				if (device->TF_Upd)
				{
					for (uint8_t i = 0; i < size_M_TF; i++)
					{
						if (bitRead(device->TF_Upd, i))
						{
							data_M_TF[i] = *floatPtr;
							spont_M_TF[dataChng_M_TF_] = i;
							dataChng_M_TF_++;
						}
						++floatPtr;
					}
				}
				
				resetUpdFlags(device);
				
				stationTransmit(devices,data_M_SP,spont_M_SP, data_M_ME, spont_M_ME, QDS_,data_M_TF, spont_M_TF, LAI);		// Spontaneous data transmission if needed
			}
			break;

			case LDI8:
			{
				if (initSerialDebug)
				{
					SerialUSB.println("104 244");
				}
				
				LDI1118* device = (LDI1118*)devices[k];
				size_M_SP = device->SP_qnt;	 // 104 Gas System data array size
				size_M_ME = device->ME_qnt;
				size_M_TF = device->TF_qnt;

				if (checkQDS_Link(device, data_M_SP,spont_M_SP,QDS_) == 1) // 1 - статус связи изменился. Отправляем все данные с меткой
				{
					device->SP_Upd	= 0xFF;
				}
				
				// SP data
				if (device->SP_Upd)
				for (uint8_t i = 0; i < size_M_SP-1; i++)
				if (bitRead(device->SP_Upd, i))
				{
					data_M_SP[i+1] = bitRead(device->DigitalInput, i) | QDS_; //  +1 = start address for SP DI data
					spont_M_SP[dataChng_M_SP_] = i+1;  //  +1 = start address for SP DI data
					dataChng_M_SP_++;
				}
				
				resetUpdFlags(device);
				
				stationTransmit(devices,data_M_SP,spont_M_SP, data_M_ME, spont_M_ME, QDS_,data_M_TF, spont_M_TF, LDI8);		// Spontaneous data transmission if needed
			}

			break;
			
			case LDI16:
			{
				if (initSerialDebug)
				{
					SerialUSB.println("104 299");
				}
				
				LDI1116* device = (LDI1116*)devices[k];
				size_M_SP = device->SP_qnt;	 // 104 Gas System data array size
				size_M_ME = device->ME_qnt;
				size_M_TF = device->TF_qnt;

				if (checkQDS_Link(device, data_M_SP,spont_M_SP,QDS_) == 1) // 1 - статус связи изменился. Отправляем все данные с меткой
				{
					device->SP_Upd	= 0xFF;
				}
				
				// SP data
				if (device->SP_Upd)
				for (uint8_t i = 0; i < size_M_SP-1; i++)
				if (bitRead(device->SP_Upd, i))
				{
					data_M_SP[i+1] = bitRead(device->DigitalInput, i) | QDS_; //  +1 = start address for SP DI data
					spont_M_SP[dataChng_M_SP_] = i+1;  //  +1 = start address for SP DI data
					dataChng_M_SP_++;
				}
				

				
				resetUpdFlags(device);
				
				stationTransmit(devices,data_M_SP,spont_M_SP, data_M_ME, spont_M_ME, QDS_,data_M_TF, spont_M_TF, LDI16);		// Spontaneous data transmission if needed
			}

			break;
			
			case LCT:
			{
				if (initSerialDebug)
				{
					SerialUSB.println("104 300");
				}
				LCT1114* device = (LCT1114*)devices[k];
				
				size_M_SP = device->SP_qnt;	 // 104 Gas System data array size
				size_M_ME = device->ME_qnt;
				size_M_TF = device->TF_qnt;
				
				if (checkQDS_Link(device, data_M_SP,spont_M_SP,QDS_) == 1) // 1 - статус связи изменился. Отправляем все данные с меткой
				{
					device->SP_Upd	= 0xFFFFF;
					device->TF_Upd = 0x3FFFFFFFFFF;
				}
				
				// SP data
				if (device->SP_Upd)
				for (uint8_t i = 0; i < (size_M_SP-1); i++)
				if (bitRead(device->SP_Upd, i))
				{
					data_M_SP[i+1] = bitRead(device->DigitalInput, i) | QDS_; //  +1 = start address for SP DI data
					spont_M_SP[dataChng_M_SP_] = i+1;  //  +1 = start address for SP DI data
					dataChng_M_SP_++;
				}
				
				
				// TF data
				float* floatPtr = &device->RMS_A; // get a pointer to the current analog input
				//	uint64_t updBuf = device->TF_Upd;
				
				
				
				if (device->TF_Upd)
				{
					for (uint8_t i = 0; i < size_M_TF; i++)
					{
						if (bitRead(device->TF_Upd, i))
						{
							data_M_TF[i] = *floatPtr;
							spont_M_TF[dataChng_M_TF_] = i;
							dataChng_M_TF_++;
						}
						++floatPtr;
					}
				}
				
				
				
				resetUpdFlags(device);
				
				if (initSerialDebug)
				{
					SerialUSB.println("104 345");
				}
				
				stationTransmit(devices,data_M_SP,spont_M_SP, data_M_ME, spont_M_ME, QDS_,data_M_TF, spont_M_TF, LCT);		// Spontaneous data transmission if needed
				
				if (initSerialDebug)
				{
					SerialUSB.println("104 350");
				}
			}
			
			break;
			
			case LCT_2:
			{
				if (initSerialDebug)
				{
					SerialUSB.println("104 300");
				}
				LCT1114_2* device = (LCT1114_2*)devices[k];
				
				size_M_SP = device->SP_qnt;	 // 104 Gas System data array size
				size_M_ME = device->ME_qnt;
				size_M_TF = device->TF_qnt;
				
				if (checkQDS_Link(device, data_M_SP,spont_M_SP,QDS_) == 1) // 1 - статус связи изменился. Отправляем все данные с меткой
				{
					device->SP_Upd	= 0xFFFFF;
					device->TF_Upd = 0x3FFFFFFFFFF;
				}
				
				// SP data
				if (device->SP_Upd)
				for (uint8_t i = 0; i < (size_M_SP-1); i++)
				if (bitRead(device->SP_Upd, i))
				{
					data_M_SP[i+1] = bitRead(device->DigitalInput, i) | QDS_; //  +1 = start address for SP DI data
					spont_M_SP[dataChng_M_SP_] = i+1;  //  +1 = start address for SP DI data
					dataChng_M_SP_++;
				}
				
				
				// TF data
				float* floatPtr = &device->RMS_A; // get a pointer to the current analog input
				//	uint64_t updBuf = device->TF_Upd;
				
				
				
				if (device->TF_Upd)
				{
					for (uint8_t i = 0; i < size_M_TF; i++)
					{
						if (bitRead(device->TF_Upd, i))
						{
							data_M_TF[i] = *floatPtr;
							spont_M_TF[dataChng_M_TF_] = i;
							dataChng_M_TF_++;
						}
						++floatPtr;
					}
				}
				
				
				
				resetUpdFlags(device);
				
				if (initSerialDebug)
				{
					SerialUSB.println("104 345");
				}
				
				stationTransmit(devices,data_M_SP,spont_M_SP, data_M_ME, spont_M_ME, QDS_,data_M_TF, spont_M_TF, LCT);		// Spontaneous data transmission if needed
				
				if (initSerialDebug)
				{
					SerialUSB.println("104 350");
				}
			}
			
			break;
			
			
			case DM910:
			{
				if (initSerialDebug)
				{
					SerialUSB.println("104 353");
				}
				
				DM91_0* device = (DM91_0*)devices[k];
				// fill array with data
				size_M_SP = device->SP_qnt;	 // 104 Gas System data array size
				size_M_ME = device->ME_qnt;
				size_M_TF = device->TF_qnt;
				
				
				if (checkQDS_Link(device, data_M_SP,spont_M_SP,QDS_) == 1) // 1 - статус связи изменился. Отправляем все данные с меткой
				{
					device->SP_Upd	= 0x3FF;
					device->ME_Upd = 0x3FFFFFFFFFF;
					device->TF_Upd = 0xFF;
				}
				
				// SP data
				if (device->SP_Upd)
				for (uint8_t i = 0; i < (size_M_SP-1); i++)
				if (bitRead(device->SP_Upd, i))
				{
					data_M_SP[i+1] = bitRead(device->ConnectLost, i) | QDS_; //  +1 = start address for SP DI data
					spont_M_SP[dataChng_M_SP_] = i+1;  //  +1 = start address for SP DI data
					dataChng_M_SP_++;
				}
				
				
				
				// TF data
				if (device->TF_Upd)
				for (uint8_t i = 0; i < size_M_TF; i++)
				if (bitRead(device->TF_Upd, i))
				{
					float* analogInputPtr = &device->pressureDM + i; // get a pointer to the current analog input
					data_M_TF[i] = *analogInputPtr;
					spont_M_TF[dataChng_M_TF_] = i;
					dataChng_M_TF_++;
				}
				
				resetUpdFlags(device);
				
				stationTransmit(devices,data_M_SP,spont_M_SP, data_M_ME, spont_M_ME, QDS_,data_M_TF, spont_M_TF,DM910);		// Spontaneous data transmission if needed
				
			}
			
			break;
			
			
			
			case DM910H:
			{
				if (initSerialDebug)
				{
					SerialUSB.println("104 353");
				}
				
				DM91_H* device = (DM91_H*)devices[k];
				// fill array with data
				size_M_SP = device->SP_qnt;	 // 104 Gas System data array size
				size_M_ME = device->ME_qnt;
				size_M_TF = device->TF_qnt;
				
				
				if (checkQDS_Link(device, data_M_SP,spont_M_SP,QDS_) == 1) // 1 - статус связи изменился. Отправляем все данные с меткой
				{
					device->SP_Upd	= 0x3FF;
					device->ME_Upd = 0x3FFFFFFFFFF;
					device->TF_Upd = 0xFF;
				}
				
				// SP data
				if (device->SP_Upd)
				for (uint8_t i = 0; i < (size_M_SP-1); i++)
				if (bitRead(device->SP_Upd, i))
				{
					data_M_SP[i+1] = bitRead(device->ConnectLost, i) | QDS_; //  +1 = start address for SP DI data
					spont_M_SP[dataChng_M_SP_] = i+1;  //  +1 = start address for SP DI data
					dataChng_M_SP_++;
				}
				
				
				
				// TF data
				if (device->TF_Upd)
				for (uint8_t i = 0; i < size_M_TF; i++)
				if (bitRead(device->TF_Upd, i))
				{
					float* analogInputPtr = &device->pressureDM + i; // get a pointer to the current analog input
					data_M_TF[i] = *analogInputPtr;
					spont_M_TF[dataChng_M_TF_] = i;
					dataChng_M_TF_++;
				}

				resetUpdFlags(device);
				
				stationTransmit(devices,data_M_SP,spont_M_SP, data_M_ME, spont_M_ME, QDS_,data_M_TF, spont_M_TF,DM910H);		// Spontaneous data transmission if needed
				
			}
			
			break;
			
			
			
			case PVT100:
			{
				if (initSerialDebug)
				{
					SerialUSB.println("104 PVT100");
				}
				
				PVT_100* device = (PVT_100*)devices[k];

				size_M_SP = device->SP_qnt;	 // 104 Gas System data array size
				size_M_ME = device->ME_qnt;
				size_M_TF = device->TF_qnt;

				if (checkQDS_Link(device, data_M_SP,spont_M_SP,QDS_) == 1) // 1 - статус связи изменился. Отправляем все данные с меткой
				{
					device->SP_Upd = 0xFF;
					device->ME_Upd = 0xFF;
				}
				
				// SP data
				// 				if (device->SP_Upd)
				// 				for (uint8_t i = 0; i < (size_M_SP-1); i++)
				// 				if (bitRead(device->SP_Upd, i))
				// 				{
				// 					data_M_SP[i+1] = bitRead(device->DigitalInput, i) | QDS_; //  +1 = start address for SP DI data
				// 					spont_M_SP[dataChng_M_SP_] = i+1;  //  +1 = start address for SP DI data
				// 					dataChng_M_SP_++;
				// 				}
				
				// ME data
				if (device->ME_Upd)
				for (uint8_t i = 0; i < size_M_ME; i++)
				if (bitRead(device->ME_Upd, i))
				{
					int16_t* analogInputPtr = &device->PVTtemp + i; // get a pointer to the current analog input
					data_M_ME[i] = *analogInputPtr;
					spont_M_ME[dataChng_M_ME_] = i;
					dataChng_M_ME_++;
				}
				
				resetUpdFlags(device);
				
				stationTransmit(devices,data_M_SP,spont_M_SP, data_M_ME, spont_M_ME, QDS_,data_M_TF, spont_M_TF, PVT100);		// Spontaneous data transmission if needed
			}
			break;

			
			
		}
	}

	//SerialUSB.println("stationTransmit -> finished");

	if (initSerialDebug)
	{
		SerialUSB.println("104 419");
	}
	
	// end spontaneous send
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
			APCI(TESTFR_act, i);										 //  Send Connected tester - TESTFR_act
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



void IEC_104::stationTransmit(void** devices,int16_t* data_M_SP, int16_t* spont_M_SP,int16_t* data_M_ME,int16_t* spont_M_ME,uint8_t QDS_valid, float* data_M_TF,int16_t* spont_M_TF, uint16_t device_)			 // Spontaneous data transmission
{
	int16_t asduBuffer = findASDU(devices, ASDUx);
	
	if (spontaneousSend && asduBuffer>0)
	{
		for (byte i=0; i<MAX_CLIENT_NUM; i++)
		if (clientState[i] == STARTDT_act && bitRead(activeASDU[i],asduBuffer))
		iec104DataSpontan(data_M_SP,spont_M_SP,data_M_ME, spont_M_ME, QDS_valid,data_M_TF, spont_M_TF, spontaneous, i, device_);
	}
	

	spontaneousSend = 0;												 // De-Activate transmission flag
}



byte IEC_104::checkSockets()											 // Check sockets for any issues
{
	byte readySocket = 0;
	byte freezeSocket = 0;
	byte frozenSock[9] = {0,0,0,0,0,0,0,0,0};
	uint8_t wizSockStatus;

	if (IECSockDebug)
	{
		SerialUSB.print("Eth: ");
		SerialUSB.println(_csPin);
		SerialUSB.println("Sock states: ");
	}
	
	if (IECSockInfDebug)
	SerialUSB.println("************");
	

	for (int i = 0; i < MAX_SOCK_NUM; i++)								// Go through all sockets
	{
		wizSockStatus = w5500.readSnSR(i);
		
		if (IECSockInfDebug)
		{
			SerialUSB.println("************");
			SerialUSB.print(F("Socket#"));
			SerialUSB.print(i);
			SerialUSB.print(F(" :0x"));
			SerialUSB.print(wizSockStatus,16);
			
			switch(wizSockStatus)
			{
				case CLOSED:
				readySocket++;		// socket ready for incoming
				if (IECSockInfDebug)
				SerialUSB.print ("CLOSED++ ");
				break;
				
				case INIT:
				if (IECSockInfDebug)
				SerialUSB.print ("INIT");
				break;

				case LISTEN:
				readySocket++;		// socket ready for incoming
				if (IECSockInfDebug)
				SerialUSB.print ("LISTEN");
				break;
				
				case SYNSENT:
				if (IECSockInfDebug)
				SerialUSB.print ("SYNSENT");
				break;
				
				case SYNRECV:
				if (IECSockInfDebug)
				SerialUSB.print ("SYNRECV");
				break;
				
				case ESTABLISHED:	// frozen socket in CLOSE_WAIT status
				if (IECSockInfDebug)
				SerialUSB.print ("ESTABLISHED");
				break;
				
				case FIN_WAIT:
				if (IECSockInfDebug)
				SerialUSB.print ("FIN_WAIT");
				break;
				
				case CLOSING:
				if (IECSockInfDebug)
				SerialUSB.print ("CLOSING");
				break;
				
				case TIME_WAIT:
				if (IECSockInfDebug)
				SerialUSB.print ("TIME_WAIT");
				break;
				

				case CLOSE_WAIT:	// frozen socket in CLOSE_WAIT status
				if (IECSockInfDebug)
				SerialUSB.print ("CLOSE_WAIT");
				freezeSocket++;
				frozenSock[i] = 1;
				break;
				
				case LAST_ACK:
				if (IECSockInfDebug)
				SerialUSB.print ("LAST_ACK");
				break;
				
				case UDP:
				readySocket++;		// socket ready for incoming
				if (IECSockInfDebug)
				SerialUSB.print ("UDP");
				break;
				
				case IPRAW:
				if (IECSockInfDebug)
				SerialUSB.print ("IPRAW");
				break;
				
				case MACRAW:
				if (IECSockInfDebug)
				SerialUSB.print ("MACRAW");
				break;
				
				case PPPOE:
				if (IECSockInfDebug)
				SerialUSB.print ("PPPOE");
				break;
			}
		}
		
		if (socket104_State[i] != SOCK_empty && wizSockStatus!= ESTABLISHED)	// Check for invalid Connections
		{
			CLIENT[i].stop();											// Stop client
			socket104_State[i] = SOCK_empty;								// Clear socket
			clientState[i] = STOPDT_act;								// Clear IEC flag
			ipUpdate = 1;												// Update HMI IP table
			IPlist[i] = blankIP;										// Clear client IP from data base
			
			if (IECSockDebug)
			{
				SerialUSB.println(" -> remove 104 client <<<<<<<<<<<<<<<<<<<<<<<<<<<<");
			}
			
		}
		else if (IECDebug)
		SerialUSB.println();
		
	}



	if (freezeSocket>4)
	for (int i=0; i<MAX_SOCK_NUM; i++)
	if (frozenSock[i] == 1)
	w5500.execCmdSn(i,Sock_CLOSE);										// reset frozen CLOSE_WAIT sockets


	socketTimer = millis();									// Update Socket check timer
	if (IECSockDebug)
	{
		SerialUSB.print("Available sock -> ");
		SerialUSB.println(readySocket);
	}
	
	
	return readySocket;													// 0 - no socket available for Connected at the moment.
}



void IEC_104::serviceFrame104(byte causeTX_, byte whoIs_, byte command_, int16_t ASDUx)
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
	iec104TransmissionArray[10] = lowByte(ASDUx);								// Station Address
	iec104TransmissionArray[11] = highByte(ASDUx);								// Station Address
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



void IEC_104::transmit_M_ME_NB_1(int16_t* data_M_ME, uint8_t QDS_valid, byte numIx, int16_t startAddr, int16_t dataStartCount_, byte transsmitionCause_, byte whoIs_)
{
	int16_t ioaAddr_ = startAddr;
	int16_t dataArrayCount = dataStartCount_;


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
	iec104TransmissionArray[10] = lowByte(ASDUx);								// Station Address
	iec104TransmissionArray[11] = highByte(ASDUx);								// Station Address

	for (int i = 0; i < numIx; i++)
	{

		iec104TransmissionArray[12 + i * 6] = lowByte(ioaAddr_);					// IOA address
		iec104TransmissionArray[13 + i * 6] = highByte(ioaAddr_);					// IOA address
		iec104TransmissionArray[14 + i * 6] = 0;									// IOA address
		iec104TransmissionArray[15 + i * 6] = lowByte(data_M_ME[dataArrayCount]);	// User Data
		iec104TransmissionArray[16 + i * 6] = highByte(data_M_ME[dataArrayCount]);	// User Data
		iec104TransmissionArray[17 + i * 6] = QDS_valid;									// QOS


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




void IEC_104::transmit_M_ME_NC_1(float* data_M_SP, uint8_t IEC104_M_TF_QDS_, byte numIx, int16_t startAddr, int16_t dataStartCount_, byte transsmitionCause_, byte whoIs_)
{
	int16_t ioaAddr_ = startAddr;
	int16_t dataArrayCount = dataStartCount_;


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
	iec104TransmissionArray[10] = lowByte(ASDUx);								// Station Address
	iec104TransmissionArray[11] = highByte(ASDUx);								// Station Address

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
		iec104TransmissionArray[19 + i * 8] = IEC104_M_TF_QDS_;		 // QDS


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




void IEC_104::transmit_M_SP_NA_1(int16_t* data_M_SP, byte numIx, int16_t startAddr, int16_t dataStartCount_, byte transsmitionCause_, byte whoIs_)
{
	int16_t ioaAddr_ = startAddr;
	int16_t dataArrayCount = dataStartCount_;

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
	iec104TransmissionArray[10] = lowByte(ASDUx);									// Station Address
	iec104TransmissionArray[11] = highByte(ASDUx);									// Station Address

	for (int16_t i = 0; i < numIx; i++)
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




void IEC_104::spontan_M_SP_NA_1(int16_t* data_M_SP,int16_t* spont_M_SP, byte numIx, int16_t dataStartCount_, byte transsmitionCause_, byte whoIs_, uint16_t device_)
{
	int16_t dataArrayCount = dataStartCount_;
	
	uint16_t spStartAddr_ = getStartAddress_SP(device_);
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
	iec104TransmissionArray[10] = lowByte(ASDUx);												// Station Address
	iec104TransmissionArray[11] = highByte(ASDUx);												// Station Address

	for (int16_t i = 0; i < numIx; i++)
	{
		iec104TransmissionArray[12 + i * 4] = lowByte(spStartAddr_+spont_M_SP[dataArrayCount]);		// IOA address
		iec104TransmissionArray[13 + i * 4] = highByte(spStartAddr_+spont_M_SP[dataArrayCount]);	// IOA address
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
void IEC_104::spontan_M_SP_TB_1(int16_t* data_M_SP,int16_t* spont_M_SP, byte numIx, int16_t dataStartCount_, byte transsmitionCause_, byte whoIs_, uint16_t device_)
{
	int16_t dataArrayCount = dataStartCount_;
	byte shift_;
	byte rtYear = rtc_clock104 ->get_years2();															// Get PLC date and Time
	byte rtMonth = rtc_clock104 -> get_months();
	byte rtDay = rtc_clock104 -> get_days();
	byte rtHour =rtc_clock104 -> get_hours();
	byte rtMinutes = rtc_clock104 -> get_minutes();
	int16_t rtSeconds = rtc_clock104 -> get_seconds() * 1000;

	uint16_t spStartAddr_ = getStartAddress_SP(device_);


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
	iec104TransmissionArray[10] = lowByte(ASDUx);													// Station Address
	iec104TransmissionArray[11] = highByte(ASDUx);													// Station Address

	for (int16_t i = 0; i < numIx; i++)
	{
		shift_ = i * 11;
		iec104TransmissionArray[12 + shift_] = lowByte(spStartAddr_+spont_M_SP[dataArrayCount]);		// IOA address
		iec104TransmissionArray[13 + shift_] = highByte(spStartAddr_+spont_M_SP[dataArrayCount]);		// IOA address
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
void IEC_104::spontan_M_ME_TE_1(int16_t* data_M_ME,int16_t* spont_M_ME, uint8_t QDS_valid, byte numIx, int16_t dataStartCount_, byte transsmitionCause_, byte whoIs_, uint16_t device_)
{
	int16_t dataArrayCount = dataStartCount_;
	byte shift_;
	byte rtYear = rtc_clock104 ->get_years2();															// Get PLC date and Time
	byte rtMonth = rtc_clock104 -> get_months();
	byte rtDay = rtc_clock104 -> get_days();
	byte rtHour =rtc_clock104 -> get_hours();
	byte rtMinutes = rtc_clock104 -> get_minutes();
	int16_t rtSeconds = rtc_clock104 -> get_seconds() * 1000;
	uint16_t meStartAddr_ = getStartAddress_ME(device_);

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
	iec104TransmissionArray[10] = lowByte(ASDUx);													// Station Address
	iec104TransmissionArray[11] = highByte(ASDUx);													// Station Address

	if (IECDebug)
	SerialUSB.println("** ME");
	for (int16_t i = 0; i < numIx; i++)
	{
		if (IECDebug)
		{
			SerialUSB.print("Number->");
			SerialUSB.print(dataArrayCount);
			SerialUSB.print(" addr->");
			SerialUSB.print(meStartAddr_+spont_M_ME[dataArrayCount]);
			SerialUSB.print(" data->");
			SerialUSB.print(data_M_ME[spont_M_ME[dataArrayCount]]);
			SerialUSB.print(" QDS->");
			SerialUSB.println(QDS_valid);
			
		}
		shift_ = i * 13;
		iec104TransmissionArray[12 + shift_] = lowByte(meStartAddr_+spont_M_ME[dataArrayCount]);		// IOA address
		iec104TransmissionArray[13 + shift_] = highByte(meStartAddr_+spont_M_ME[dataArrayCount]);		// IOA address
		iec104TransmissionArray[14 + shift_] = 0;														// IOA address
		iec104TransmissionArray[15 + shift_] = lowByte(data_M_ME[spont_M_ME[dataArrayCount]]);			// User Data
		iec104TransmissionArray[16 + shift_] = highByte(data_M_ME[spont_M_ME[dataArrayCount]]);	// User Data
		iec104TransmissionArray[17 + shift_] = QDS_valid;


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

	if (initSerialDebug)
	{
		SerialUSB.println("104 1031");
	}
	
	CLIENT[whoIs_].write(iec104TransmissionArray, iec104TransmissionArray[1] + 2);
	TXcnt[whoIs_] += 2;

	if (initSerialDebug)
	{
		SerialUSB.println("104 1039");
	}
	
	if (IECDebug)
	{																					// Increment TX counter
		SerialUSB.print("TXcnt -> ");
		SerialUSB.println(TXcnt[whoIs_]);
		SerialUSB.print("RXcnt -> ");
		SerialUSB.println(RXcnt[whoIs_]);
	}
}









// Single with time TAG
void IEC_104::spontan_M_ME_TF_1(float* data_M_TF,int16_t* spont_M_TF,  uint8_t IEC104_M_TF_QDS_, byte numIx, int16_t dataStartCount_, byte transsmitionCause_, byte whoIs_, uint16_t device_)
{
	
	//
	
	
	//	SerialUSB.println(data_M_ME[0]);
	
	
	int16_t dataArrayCount = dataStartCount_;
	byte shift_;
	byte rtYear = rtc_clock104 ->get_years2();															// Get PLC date and Time
	byte rtMonth = rtc_clock104 -> get_months();
	byte rtDay = rtc_clock104 -> get_days();
	byte rtHour =rtc_clock104 -> get_hours();
	byte rtMinutes = rtc_clock104 -> get_minutes();
	int16_t rtSeconds = rtc_clock104 -> get_seconds() * 1000;
	uint16_t tfStartAddr_ = getStartAddress_TF(device_);

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
	iec104TransmissionArray[10] = lowByte(ASDUx);													// Station Address
	iec104TransmissionArray[11] = highByte(ASDUx);													// Station Address




	union {
		float fval;
		byte bval[4];
	} floatAsBytes;
	//	int temp=0;

	if (IECDebug)
	SerialUSB.println("** TF");
	
	for (int16_t i = 0; i < numIx; i++)
	{
		if (IECDebug)
		{
			SerialUSB.print("Number->");
			SerialUSB.print(dataArrayCount);
			SerialUSB.print(" addr->");
			SerialUSB.print(tfStartAddr_+spont_M_TF[dataArrayCount]);
			SerialUSB.print(" data->");
			SerialUSB.print(data_M_TF[spont_M_TF[dataArrayCount]]);
			SerialUSB.print(" QDS->");
			SerialUSB.println(IEC104_M_TF_QDS_);
		}
		
		floatAsBytes.fval = data_M_TF[spont_M_TF[dataArrayCount]];
		shift_ = i * 15;
		iec104TransmissionArray[12 + shift_] = lowByte(tfStartAddr_+spont_M_TF[dataArrayCount]);			// IOA address
		iec104TransmissionArray[13 + shift_] = highByte(tfStartAddr_+spont_M_TF[dataArrayCount]);		// IOA address
		iec104TransmissionArray[14 + shift_] = 0;														// IOA address
		iec104TransmissionArray[15 + shift_] = floatAsBytes.bval[0];		// User Data
		iec104TransmissionArray[16 + shift_] = floatAsBytes.bval[1];			// User Data
		iec104TransmissionArray[17 + shift_] = floatAsBytes.bval[2];		// User Data
		iec104TransmissionArray[18 + shift_] = floatAsBytes.bval[3];		// User Data
		
		iec104TransmissionArray[19 + shift_] = IEC104_M_TF_QDS_;		 // QDS



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
	int16_t TIME_PLC[6];
	int16_t TIME_104[6];
	//	byte updateRTC104 = 0;

	TIME_104[0] = iec104ReciveArray[21];									// IEC Year
	TIME_104[1] = iec104ReciveArray[20];									// IEC Month
	TIME_104[2] = iec104ReciveArray[19];									// IEC Days
	TIME_104[3] = iec104ReciveArray[18];									// IEC Hours
	TIME_104[4] = iec104ReciveArray[17];									// IEC Minutes
	int16_t rtSeconds = word(iec104ReciveArray[16], iec104ReciveArray[15]);		// IEC seconds + millis
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
				socket104_State[i] = SOCK_empty;			// Clear socket activation state
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

	// 	if (IEClinkDebug)
	// 	{
	//
	// 		SerialUSB.print("IncomingSocket -> ");
	// 		SerialUSB.println(incomingSocket);
	//
	// 		SerialUSB.print("Incoming IP -> ");
	// 		SerialUSB.println(remoteIP);
	//
	// 		SerialUSB.print("Incoming Port -> ");
	// 		SerialUSB.println(incomingPort);
	// 	}

	//	if (clientState[incomingSocket] != STARTDT_act)
	//	checkIncomingIP(remoteIP);

	checkSockets();

	byte totalClients = 0;
	for (byte i=0; i<MAX_CLIENT_NUM; i++)
	{
		if (socket104_State[i] != SOCK_empty)
		totalClients++;
	}

	if (IEClinkDebug)
	{
		SerialUSB.println();
		SerialUSB.println();
		SerialUSB.print("-> Incoming host -> ");
		SerialUSB.println(incomingSocket);
		SerialUSB.print("-> Incoming port -> ");
		SerialUSB.println(incomingPort);
	}



	/*
	if (totalClients > 5)
	{
	socket104_State[incomingSocket] = empty;				// Clear socket activation
	CLIENT[incomingSocket].stop();
	clientState[incomingSocket] = stop;
	w5500.execCmdSn(incomingSocket,Sock_CLOSE);
	IPlist[incomingSocket] = blankIP;
	ipUpdate = 1;
	return empty;  // New client not accepted - Sockets overload
	}
	checkMaxClients();
	*/


	if (socket104_State[incomingSocket] == incomingSocket && IPlist[incomingSocket] == remoteIP && PORTlist[incomingSocket] == incomingPort)		// Already in our base?
	{
		if (IEClinkDebug)
		SerialUSB.println("Socket in base");
		return incomingSocket;
	}
	


	socket104_State[incomingSocket] = incomingSocket;			// New? - save socket number in client base
	CLIENT[incomingSocket] = iec104Server.available();		// Make client object for new incoming
	IPlist[incomingSocket] = remoteIP;
	PORTlist[incomingSocket] = incomingPort;
	ipUpdate = 1;
	
	if (IEClinkDebug)
	{
		
		SerialUSB.print("+ Add Host! -> ");
		SerialUSB.println(incomingSocket);
		SerialUSB.print("+ Add Socket -> ");
		SerialUSB.println(incomingSocket);

		SerialUSB.print("+ Add IP -> ");
		SerialUSB.println(remoteIP);
		
		SerialUSB.print("+ Add Port -> ");
		SerialUSB.println(incomingPort);
	}

	//SerialUSB.println("New add to base");
	return incomingSocket;									// Client form our base - keep communicating

}




void IEC_104 :: monitorIncoming(void** devices, int16_t* data_M_SP, int16_t* data_M_ME, float* data_M_TF)
{

	//SerialUSB.println();
	//SerialUSB.println("*********************");

	//SerialUSB.print("Eth -> ");
	//	SerialUSB.println(\);
	if (initSerialDebug)
	{
		SerialUSB.println("104 1389");
	}
	
	CLIENT[bufer] = iec104Server.available();				// Check if anyone got incoming

	if (initSerialDebug)
	SerialUSB.println("104 1396");

	if (!CLIENT[bufer].available())						// got incoming?
	{
		//SerialUSB.println("no incoming");
		if (initSerialDebug)
		SerialUSB.println("104 1405");
		return;
	}
	
	if (initSerialDebug)
	SerialUSB.println("104 1412");
	
	
	if (IECDebug)
	SerialUSB.println("********************");
	// SerialUSB.println(_csPin);

	byte whoIs_ = proceedClient();						// Validate incoming

	if (initSerialDebug)
	SerialUSB.println("104 1428");
	


	proceedReply(devices, data_M_SP, data_M_ME,data_M_TF,whoIs_);						// We got a valid client - reply!
	
	if (initSerialDebug)
	SerialUSB.println("104 1442");
	
	//	SerialUSB.println("*******");

}


void IEC_104 :: proceedReply(void** devices, int16_t* data_M_SP, int16_t* data_M_ME,  float* data_M_TF,byte whoIs_)
{
	if (IECDebug)
	SerialUSB.println("proceed Reply Func");

	int16_t QuerryType = getQuerryType(whoIs_);						//APDU

	if (initSerialDebug)
	SerialUSB.println("104 1517");

	if (QuerryType == closeConnected)
	{
		stopClient(whoIs_);
		if (IECDebug)
		SerialUSB.println("proceed Reply -> closeConnected");
		return;
	}

	if (initSerialDebug)
	SerialUSB.println("104 1529");
	
	if (IEClinkDebug)
	{
		SerialUSB.print("Host State -> ");
		printClientState(clientState[whoIs_]);
		SerialUSB.print("QuerryType -> ");
		printClientState(QuerryType);
	}
	
	if (clientState[whoIs_] == STOPDT_act) // Check if it's a new Connected
	switch (QuerryType)
	{
		case STARTDT_act:										// U-frame STARTDT act

		TXcnt[whoIs_] = 0;										// Reset TX RX counters
		RXcnt[whoIs_] = 0;										// Reset TX RX counters
		if (IECDebug)
		SerialUSB.println("<- RX STARTDT_act");
		APCI(STARTDT_con,whoIs_);
		if (IECDebug)								//  Send APCI STARTDT_con
		SerialUSB.println("-> TX STARTDT_con");
		clientState[whoIs_] = STARTDT_act;						// Approved client
		
		
		if (IECDebug)
		{
			SerialUSB.print("	clientState -> ");
			printClientState(clientState[whoIs_]);
		}
		return;

		break;


		default:
		
		if (IEClinkDebug)								//  Send APCI STARTDT_con
		{
			
			SerialUSB.print(QuerryType);
			SerialUSB.println(" -> QuerryType not STARTDT_act !!!!!!!");
		}
		
		stopClient(whoIs_);
		
		return;

		break;
	}
	
	
	switch (QuerryType)
	{
		
		case TESTFR_act:										//  TESTFR act
		if (IECDebug)
		SerialUSB.println("<- RX TESTFR_act");
		APCI(TESTFR_con, whoIs_);								// Send APCI TESTFR_con
		if (IECDebug)
		SerialUSB.println("-> TX TESTFR_con");
		break;

		case TESTFR_con:										// TESTFR act
		if (IECDebug)
		SerialUSB.println("<- RX TESTFR con");
		UTESTER[whoIs_] = 0;									// Deactivate communication tester packet
		// SerialUSB.println(T3_104[whoIs_]);
		break;


		case STOPDT_act: //STOPDT act
		if (IECDebug)
		SerialUSB.println("<- RX STOPDT_act");
		APCI(STOPDT_con, whoIs_);								// Send APCI STOPDT_con
		stopClient(whoIs_);
		if (IECDebug)	{
			SerialUSB.println("-> TX STOPDT_con");
			SerialUSB.println("Stop client ************");
		}
		break;

		
		case S_frame:											// S - Frame

		T3_104[whoIs_] = millis();
		if (IECDebug)
		SerialUSB.println("<- RX S_frame");
		//	SerialUSB.println(T3_104[whoIs_]);
		break;

		// >>>>>>>>>>>>>>>>>>>>>>>>>>>
		
		case I_frame:											// I frame

		switch (iec104ReciveArray[6])
		{
			case C_IC_NA_1:										// Station interrogation
			{
				ASDUx = 0;
				byte QDS_;
				
				ASDUx = iec104ReciveArray[11] << 8 | iec104ReciveArray[10];
				
				int16_t k;
				//k = ASDUx - ASDU;  // ASDU = starting ASDU for system
				k = findASDU(devices, ASDUx);
				
				
				if (IECDebug)
				{
					SerialUSB.print("ASDUx -> ");
					SerialUSB.print(ASDUx);
					SerialUSB.print(", k -> ");
					SerialUSB.println(k);
				}

				
				if (IECDebug)
				SerialUSB.println("<- RX C_IC_NA_1");
				
				serviceFrame104(ACT_con, whoIs_, C_IC_NA_1,ASDUx);		// Activation confirmation
				delay(interframe_);
				if (IECDebug)
				SerialUSB.println("-> TX ACT_con");
				
				// Iterate through LCP network devices and make 104 arrays
				
				
				/*
				for (uint8_t k=0; k<totalASDU; k++)  // Проходим по всем устройствам подключенным к LCP
				{
				ASDUx = ASDU + k; // ASDU - стартовое значение (смещение в сети), ASDUx - текущее ASDU обрабатываемого прибора
				delay(3);
				*/
				
				if(k >= 0 && k <= asduTail)	// If ASDU in our range proceed reply, if not - empty respond
				{
					
					bitSet(activeASDU[whoIs_],k);
					if (IEClinkDebug)
					{
						SerialUSB.print("activeASDU's -> ");
						SerialUSB.println((uint16_t)activeASDU[whoIs_]);
					}
					
					
					switch (((DeviceCommon*)devices[k])->ID)
					{
						case LCP:
						{
							if (IECDebug)
							SerialUSB.println("<- LCP Inrogen");
							LCP1116* device = (LCP1116*)devices[k];
							size_M_SP = device->SP_qnt;								 // 104 Gas System data array size
							size_M_ME = device->ME_qnt;
							size_M_TF = device->TF_qnt;
							data_M_SP[0] = device->deviceStatus;
							
							if (device->ConnectLost)
							QDS_ = QDS_Invalid;
							else
							QDS_ = QDS_Good;
							
							iec104DataTranssmition(data_M_SP, data_M_ME, data_M_TF, inrogen,QDS_, whoIs_, LCP);  // Single Point Information
						}
						break;
						
						case LDO:
						{
							LDO1118* device = (LDO1118*)devices[k];
							size_M_SP = device->SP_qnt;								 // 104 Gas System data array size
							size_M_ME = device->ME_qnt;
							size_M_TF = device->TF_qnt;
							data_M_SP[0] = device->ConnectLost;
							if (device->ConnectLost)
							QDS_ = QDS_Invalid;
							else
							QDS_ = QDS_Good;
							iec104DataTranssmition(data_M_SP, data_M_ME, data_M_TF, inrogen,QDS_, whoIs_, LDO);  // Single Point Information
						}
						break;

						case LAI:
						{
							LAI1118* device = (LAI1118*)devices[k];
							// fill array with data
							size_M_SP = device->SP_qnt;								 // 104 Gas System data array size
							size_M_ME = device->ME_qnt;
							size_M_TF = device->TF_qnt;
							
							if (device->ConnectLost) // нет связи
							QDS_= QDS_Invalid;
							else QDS_ = QDS_Good;
							
							data_M_SP[0] = device->ConnectLost;
							//SP
							for (uint8_t i = 1; i <= size_M_SP; i++)
							data_M_SP[i] = bitRead(device->DigitalInput, i-1) | QDS_;
							
							// ME
// 							for (uint8_t i = 0; i < size_M_ME; i++)
// 							{
// 								int16_t* analogInputPtr = &device->AnalogInput01 + i; // get a pointer to the current analog input
// 								data_M_ME[i] = *analogInputPtr;
// 							}
							
							//TF
							for (uint8_t i = 0; i < size_M_TF; i++)
							{
								float* analogInputPtr = &device->AnalogInput01 + i; // get a pointer to the current analog input
								data_M_TF[i] = *analogInputPtr;
							}
							
							iec104DataTranssmition(data_M_SP, data_M_ME, data_M_TF, inrogen,QDS_, whoIs_, LAI);  // Single Point Information
							
						}
						break;

						case LDI8:
						{
							LDI1118* device = (LDI1118*)devices[k];
							// fill array with data
							size_M_SP = device->SP_qnt;								 // 104 Gas System data array size
							size_M_ME = device->ME_qnt;
							size_M_TF = device->TF_qnt;
							
							if (device->ConnectLost) // нет связи
							QDS_= QDS_Invalid;
							else QDS_ = QDS_Good;
							
							data_M_SP[0] = device->ConnectLost;
							//SP
							for (uint8_t i = 1; i <= size_M_SP; i++)
							data_M_SP[i] = bitRead(device->DigitalInput, i-1) | QDS_;
							
							
							iec104DataTranssmition(data_M_SP, data_M_ME, data_M_TF, inrogen,QDS_, whoIs_, LDI8);  // Single Point Information
						}
						break;
						
						case LDI16:
						{
							LDI1116* device = (LDI1116*)devices[k];
							// fill array with data
							size_M_SP = device->SP_qnt;								 // 104 Gas System data array size
							size_M_ME = device->ME_qnt;
							size_M_TF = device->TF_qnt;
							
							if (device->ConnectLost) // нет связи
							QDS_= QDS_Invalid;
							else QDS_ = QDS_Good;
							
							data_M_SP[0] = device->ConnectLost;
							//SP
							for (uint8_t i = 1; i <= size_M_SP; i++)
							data_M_SP[i] = bitRead(device->DigitalInput, i-1) | QDS_;
							
							
							iec104DataTranssmition(data_M_SP, data_M_ME, data_M_TF, inrogen,QDS_, whoIs_, LDI16);  // Single Point Information
						}
						break;
						
						case LCT:

						{
							LCT1114* device = (LCT1114*)devices[k];
							// fill array with data
							size_M_SP = device->SP_qnt;	 // 104 Gas System data array size
							size_M_ME = device->ME_qnt;
							size_M_TF = device->TF_qnt;
							
							if (device->ConnectLost) // нет связи
							QDS_= QDS_Invalid;
							else QDS_ = QDS_Good;
							
							data_M_SP[0] = device->ConnectLost;
							//SP
							for (uint8_t i = 1; i <= size_M_SP; i++) // size_M_SP-1 -> because first address - Link status
							data_M_SP[i] = bitRead(device->DigitalInput, i-1) | QDS_;
							
							//TF
							float* floatPtr = &device->RMS_A; // get a pointer to the current analog input
							for (uint8_t i = 0; i < size_M_TF; i++)
							{
								data_M_TF[i] = *floatPtr;
								++floatPtr;
							}
							iec104DataTranssmition(data_M_SP, data_M_ME, data_M_TF, inrogen,QDS_, whoIs_, LCT);  // Single Point Information
							
						}
						
						break;
						
						case LCT_2:

						{
							LCT1114_2* device = (LCT1114_2*)devices[k];
							// fill array with data
							size_M_SP = device->SP_qnt;	 // 104 Gas System data array size
							size_M_ME = device->ME_qnt;
							size_M_TF = device->TF_qnt;
							
							if (device->ConnectLost) // нет связи
							QDS_= QDS_Invalid;
							else QDS_ = QDS_Good;
							
							data_M_SP[0] = device->ConnectLost;
							//SP
							for (uint8_t i = 1; i <= size_M_SP; i++) // size_M_SP-1 -> because first address - Link status
							data_M_SP[i] = bitRead(device->DigitalInput, i-1) | QDS_;
							
							//TF
							float* floatPtr = &device->RMS_A; // get a pointer to the current analog input
							for (uint8_t i = 0; i < size_M_TF; i++)
							{
								data_M_TF[i] = *floatPtr;
								++floatPtr;
							}
							iec104DataTranssmition(data_M_SP, data_M_ME, data_M_TF, inrogen,QDS_, whoIs_, LCT);  // Single Point Information
							
						}
						
						break;
						
						case DM910:
						{
							DM91_0* device = (DM91_0*)devices[k];
							// fill array with data
							size_M_SP = device->SP_qnt;	 // 104 Gas System data array size
							size_M_ME = device->ME_qnt;
							size_M_TF = device->TF_qnt;
							
							if (device->ConnectLost) // нет связи
							QDS_= QDS_Invalid;
							else QDS_ = QDS_Good;
							
							data_M_SP[0] = device->ConnectLost;
							//SP
							for (uint8_t i = 1; i <= size_M_SP; i++) // size_M_SP-1 -> because first address - Link status
							data_M_SP[i] = bitRead(device->deviceStatus, i-1) | QDS_;
							
							//TF
							for (uint8_t i = 0; i < size_M_TF; i++)
							{
								float* analogInputPtr = &device->pressureDM + i; // get a pointer to the current analog input
								data_M_TF[i] = *analogInputPtr;
							}
							
							iec104DataTranssmition(data_M_SP, data_M_ME, data_M_TF, inrogen,QDS_, whoIs_, DM910);  // Single Point Information
						}
						
						break;
						
						
						
						case DM910H:
						{
							DM91_H* device = (DM91_H*)devices[k];
							// fill array with data
							size_M_SP = device->SP_qnt;	 // 104 Gas System data array size
							size_M_ME = device->ME_qnt;
							size_M_TF = device->TF_qnt;
							
							if (device->ConnectLost) // нет связи
							QDS_= QDS_Invalid;
							else QDS_ = QDS_Good;
							
							data_M_SP[0] = device->ConnectLost;
							//SP
							for (uint8_t i = 1; i <= size_M_SP; i++) // size_M_SP-1 -> because first address - Link status
							data_M_SP[i] = bitRead(device->deviceStatus, i-1) | QDS_;
							
							//TF
							for (uint8_t i = 0; i < size_M_TF; i++)
							{
								float* analogInputPtr = &device->pressureDM + i; // get a pointer to the current analog input
								data_M_TF[i] = *analogInputPtr;
							}
							
							iec104DataTranssmition(data_M_SP, data_M_ME, data_M_TF, inrogen,QDS_, whoIs_, DM910H);  // Single Point Information
						}
						
						break;
						
						
						
						case PVT100:
						{
							PVT_100* device = (PVT_100*)devices[k];
							// fill array with data
							size_M_SP = device->SP_qnt;								 // 104 Gas System data array size
							size_M_ME = device->ME_qnt;
							size_M_TF = device->TF_qnt;
							
							if (device->ConnectLost) // нет связи
							QDS_= QDS_Invalid;
							else QDS_ = QDS_Good;
							
							data_M_SP[0] = device->ConnectLost;
							// 							//SP
							// 							for (uint8_t i = 1; i <= size_M_SP; i++)
							// 							data_M_SP[i] = bitRead(device->DigitalInput, i-1) | QDS_;
							
							// ME
							for (uint8_t i = 0; i < size_M_ME; i++)
							{
								int16_t* analogInputPtr = &device->PVTtemp + i; // get a pointer to the current analog input
								data_M_ME[i] = *analogInputPtr;
							}
							
							iec104DataTranssmition(data_M_SP, data_M_ME, data_M_TF, inrogen,QDS_, whoIs_, PVT100);  // Single Point Information
							
						}
						break;
						
					}
				}
				
				
				if (IECDebug)
				SerialUSB.println("TX Data");
				serviceFrame104(ACT_term, whoIs_, C_IC_NA_1,ASDUx);		// Activation termination
				if (IECDebug)
				SerialUSB.println("TX ACT_term");
			}
			
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


	int16_t TXmaster = word(iec104ReciveArray[5], iec104ReciveArray[4]);		// Calc Master TX and RX counters
	int16_t RXmaster = word(iec104ReciveArray[3], iec104ReciveArray[2]);

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
	socket104_State[whoIs_] = SOCK_empty;
	CLIENT[whoIs_].stop();
	ipUpdate = 1;
	IPlist[whoIs_] = blankIP;
	activeASDU[whoIs_] = 0;
	PORTlist[whoIs_] = 0;
}




byte IEC_104 :: getQuerryType(byte whoIs_)
{

	byte i = 0;				// byte counter

	if (initSerialDebug)
	SerialUSB.println("104 1874");
	
	i += dataToBuffer(whoIs_, i, 2);			// check APDU packet length
	
	if (initSerialDebug)
	SerialUSB.println("104 1879");

	byte APDUlen = iec104ReciveArray[1];
	
	

	if (IECDebug)
	{
		SerialUSB.print("APDUlen -> ");
		SerialUSB.println(APDUlen);
	}

	i += dataToBuffer(whoIs_, i, APDUlen+2);	// Read first packet in socket buffer

	if (initSerialDebug)
	SerialUSB.println("104 1892");

	i = 0;
	byte wizBuf = 0;
	/*
	while (CLIENT[whoIs_].available())					// Remove second message
	{
	if (initSerialDebug)
	SerialUSB.println("104 1900");
	wizBuf = CLIENT[whoIs_].read();
	i++;
	if (dataDebug)
	{
	SerialUSB.print(wizBuf);
	SerialUSB.print(" ");
	}
	}
	*/
	if (initSerialDebug)
	SerialUSB.println("104 1907");
	
	if (IECDebug)
	{
		SerialUSB.println();
		SerialUSB.print("Tail -> ");
		SerialUSB.println(i);
	}


	switch (APDUlen)
	{
		case Uframe:
		{
			if (IECDebug)
			{
				SerialUSB.println("RX U frame received");
				SerialUSB.print("	Command -> ");
				printClientState(iec104ReciveArray[2]);
			}

			return iec104ReciveArray[2];								// command
		}
		break;


		default:
		{
			//	if (checkCNT(whoIs_) == cntOk)  // Check counters
			//	{
			RXcnt[whoIs_] += 2;										// Increment RX incoming packet counter
			if (IECDebug)
			{
				SerialUSB.println ("RX I frame received");
				SerialUSB.print("	Command -> ");
				printClientState(iec104ReciveArray[2]);
				SerialUSB.print("	TXcnt -> ");
				SerialUSB.println(TXcnt[whoIs_]);
				SerialUSB.print("	RXcnt -> ");
				SerialUSB.println(RXcnt[whoIs_]);
				
			}
			return I_frame;
		}
		break;

	}
}


//byte const dataDebug = 1;
byte IEC_104 :: dataToBuffer(byte whoIs_, byte startCnt, byte dataQt_)
{
	byte i = startCnt;
	if (initSerialDebug)
	SerialUSB.println("104 1951");
	//	while (CLIENT[whoIs_].available() && i<dataQt_)					// Read U packet from client ring buffer

	//	SerialUSB.print("available -> ");
	//	SerialUSB.println(CLIENT[whoIs_].available());


	byte iecWDT = 0;
	while (i<dataQt_)					// Read U packet from client ring buffer
	{
		while ( (CLIENT[whoIs_].available() == 0) && (iecWDT < 5))
		{
			SerialUSB.print("Delayed ");
			delay(1);
			iecWDT++;
		}
		
		iec104ReciveArray[i] = CLIENT[whoIs_].read();
		if (dataDebug)
		{
			SerialUSB.print(iec104ReciveArray[i]);
			SerialUSB.print(" ");
		}

		i++;
	}
	
	if (initSerialDebug)
	SerialUSB.println("104 1967");
	
	if (dataDebug)
	SerialUSB.println();

	if (IECDebug)
	{
		SerialUSB.print("dataToBuffer received -> ");
		SerialUSB.println(i);
	}
	return i;

}


byte IEC_104::checkQDS_Link(DeviceCommon* device, int16_t* data_M_SP,int16_t* spont_M_SP, uint8_t& QDS_)
{
	if (device->ConnectLost == true) // нет связи
	QDS_= QDS_Invalid;
	else QDS_ = QDS_Good;
	
	if (bitRead(device->StatusUpd, 1))   // if we got communication status changed
	{
		if (_csPin == ETH2_)  // If port 2 on the way - update finished
		device->StatusUpd = 0;
		
		data_M_SP[0] = device->ConnectLost;
		spont_M_SP[0] = 0;
		dataChng_M_SP_++;
		spontaneousSend = 1;
		return 1; // change all data;
	}
	
	return 0;
}

void IEC_104::printSockStatus(uint8_t sockStatus_)
{
	switch(sockStatus_)
	{
		case CLOSED:
		SerialUSB.println ("CLOSED++ ");
		break;
		
		case INIT:
		SerialUSB.println ("INIT");
		break;

		case LISTEN:
		SerialUSB.println ("LISTEN");
		break;
		
		case SYNSENT:
		SerialUSB.println ("SYNSENT");
		break;
		
		case SYNRECV:
		SerialUSB.println ("SYNRECV");
		break;
		
		case ESTABLISHED:	// frozen socket in CLOSE_WAIT status
		SerialUSB.println ("ESTABLISHED");
		break;
		
		case FIN_WAIT:
		SerialUSB.println ("FIN_WAIT");
		break;
		
		case CLOSING:
		SerialUSB.println ("CLOSING");
		break;
		
		case TIME_WAIT:
		SerialUSB.println ("TIME_WAIT");
		break;
		
		case CLOSE_WAIT:	// frozen socket in CLOSE_WAIT status
		SerialUSB.println ("CLOSE_WAIT");
		break;
		
		case LAST_ACK:
		SerialUSB.println ("LAST_ACK");
		break;
		
		case UDP:
		SerialUSB.println ("UDP");
		break;
		
		case IPRAW:
		SerialUSB.println ("IPRAW");
		break;
		
		case MACRAW:
		SerialUSB.println ("MACRAW");
		break;
		
		case PPPOE:
		
		SerialUSB.println ("PPPOE");
		break;
		
		
	}
}


void IEC_104::printClientState(uint8_t clientState_)
{
	switch(clientState_)
	{
		case SOCK_empty:
		SerialUSB.println ("SOCK_empty ");
		break;
		
		case spontaneous:
		SerialUSB.println ("spontaneous");
		break;

		case STARTDT_act:
		SerialUSB.println ("STARTDT_act");
		break;
		
		case STARTDT_con:
		SerialUSB.println ("STARTDT_con");
		break;
		
		case TESTFR_act:
		SerialUSB.println ("TESTFR_act");
		break;
		
		case TESTFR_con:	// frozen socket in CLOSE_WAIT status
		SerialUSB.println ("TESTFR_con");
		break;
		
		case APCI_size:
		SerialUSB.println ("APCI_size");
		break;
		
		case STOPDT_act:
		SerialUSB.println ("STOPDT_act");
		break;
		
		case STOPDT_con:
		SerialUSB.println ("STOPDT_con");
		break;
		
		case S_frame:	// frozen socket in CLOSE_WAIT status
		SerialUSB.println ("S_frame");
		break;
		
		case I_frame:
		SerialUSB.println ("I_frame");
		break;
		
		default:
		SerialUSB.println ("Unknown");
		break;
		
	}
}

/*
void IEC_104::removeClient(byte whoIs_)
{
if (IEClinkDebug)
SerialUSB.println ("--Flush ");

if (initSerialDebug)
SerialUSB.println("104 2135");

while (CLIENT[whoIs_].available())						// Flush Socket buffer
CLIENT[whoIs_].read();

if (initSerialDebug)
SerialUSB.println("104 2141");

CLIENT[whoIs_].stop();

if (initSerialDebug)
SerialUSB.println("104 2146");
// Stop client
socket104_State[whoIs_] = SOCK_empty;								// Clear socket
clientState[whoIs_] = STOPDT_act;								// Clear IEC flag
ipUpdate = 1;												// Update HMI IP table
IPlist[whoIs_] = blankIP;										// Clear client IP from data base
if (IEClinkDebug)
{
SerialUSB.print("-- remove port -> ");
SerialUSB.print(PORTlist[whoIs_]);
SerialUSB.print("!! remove socket -> ");
SerialUSB.println(whoIs_);
}
TXcnt[whoIs_] = 0;  // Reset TX RX counters
RXcnt[whoIs_] = 0;  // Reset TX RX counters

//	stackDebug = millis();
//	SerialUSB.println(millis() - stackDebug);
while (CLIENT[whoIs_].available())			// Read I packet from client ring buffer
CLIENT[whoIs_].read();



clientState[whoIs_] = STOPDT_act;
socket104_State[whoIs_] = SOCK_empty;
CLIENT[whoIs_].stop();
ipUpdate = 1;
IPlist[whoIs_] = blankIP;
activeASDU[whoIs_] = 0;

PORTlist[whoIs_] = 0;
activeASDU[whoIs_] = 0;

}

*/

void IEC_104::resetUpdFlags(DeviceCommon* device_)
{
	if (dataChng_M_SP_||dataChng_M_ME_||dataChng_M_TF_)  // If any changes found
	{
		spontaneousSend = 1; // run station spontaneous transmission
		
		if (_csPin == ETH2_)  // If port 2 on the way - update done
		{
			device_->SP_Upd = 0; // Reset update flags
			device_->ME_Upd = 0; // Reset update flags
			device_->TF_Upd = 0; // Reset update flags
		}
	}
}


int16_t IEC_104::findASDU(void** devices,int16_t ASDUx_ )
{
	for (uint8_t i=0; i<totalASDU; i++)
	{
		if (((DeviceCommon*)devices[i])->ASDU == ASDUx_)
		return i;
	}
	return -1;
}