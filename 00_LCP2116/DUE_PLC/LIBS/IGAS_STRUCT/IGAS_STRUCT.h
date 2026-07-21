#ifndef IGAS_STRUCT
#define IGAS_STRUCT

#define bitReadLong(value, bit) (((value) >> (bit)) & 0x01)
#define bitSetLong(value, bit) ((value) |= (1ULL << (bit)))
#define bitClearLong(value, bit) ((value) &= ~(1ULL << (bit)))
#define bitWriteLong(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))

enum
{
	LCP,
	LDO,	// 1
	LAI,	// 2
	LDI8,	// 3
	LCT,	// 4
	LDI16,	// 5
	LCT_2,	// 6
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
// Define the structures

struct DeviceCommon {
	uint8_t ID;					// device ID
	uint16_t ASDU;					// device ID
	uint8_t StatusUpd;		//
	uint8_t ConnectLost;
	uint8_t deviceStatus;
	uint8_t SP_qnt;
	uint8_t ME_qnt;
	uint8_t TF_qnt;
	uint32_t SP_Upd;  // if bit 1 = corresponding SP bit in DigitalInput changed value and must be added to sporadic
	uint64_t ME_Upd;  // if bit 1 = corresponding ME data in structure updated and must be added to sporadic, e.g bit 1 updated , hense AnalogInput02 in LAI1118 changed value
	uint64_t TF_Upd;  // if bit 1 = corresponding TF data in structure updated and must be added to sporadic, e.g bit 1 updated , hense RMS_B in LCT1114 changed value
};


struct LCP1116 : public DeviceCommon
{

};

struct LDO1118 : public DeviceCommon
{
	uint8_t setPoint;  // Upd status in high Bit
};

struct LDI1116 : public DeviceCommon
{

	uint16_t DigitalInput;
	//	uint8_t DigitalInput02;

};

struct LDI1118 : public DeviceCommon
{

	uint16_t DigitalInput;

};


struct LAI1118 : public DeviceCommon
{
	uint8_t DigitalInput;  // SP data
	float AnalogInput01;
	float AnalogInput02;
	float AnalogInput03;
	float AnalogInput04;
	float AnalogInput05;
	float AnalogInput06;
	float AnalogInput07;
	float AnalogInput08;
};

// LCT ********


struct LCT1114 : public DeviceCommon
{
	
	uint32_t DigitalInput;	// SP data
	// 	uint8_t PhA_Contact_On;		//1
	// 	uint8_t PhA_Contact_Off;	//2
	// 	uint8_t PhB_Contact_On;		//3
	// 	uint8_t PhB_Contact_Off;	//4
	// 	uint8_t PhC_Contact_On;		//5
	// 	uint8_t PhC_Contact_Off;	//6
	// 	uint8_t PhA_Fail;			//7
	// 	uint8_t PhB_Fail;			//8
	// 	uint8_t PhC_Fail;			//9
	// 	uint8_t partialSwitch;		//10
	// 	uint8_t lastOperation;		//11
	// 	uint8_t ownTimeOff_Warn_Bit;	//12
	// 	uint8_t ownTimeOff_Alarm_Bit;	// 13


	float RMS_A;					//1		// TF data
	float RMS_B;					//2
	float RMS_C;					//3
	float ph_A_passed_resource;		//4
	float ph_B_passed_resource;		//5
	float ph_C_passed_resource;		//6
	float ph_A_last_arc_lifetime;	//7
	float ph_B_last_arc_lifetime;	//8
	float ph_C_last_arc_lifetime;	//9
	float ph_A_last_sw2off_curr;	//10
	float ph_B_last_sw2off_curr;	//11
	float ph_C_last_sw2off_curr;	//12
	float ph_A_last_sw2off_passed_res;	//13
	float ph_B_last_sw2off_passed_res;	//14
	float ph_C_last_sw2off_passed_res;	//15
	float ph_A_operation_cntr;			//16
	float ph_B_operation_cntr;			//17
	float ph_C_operation_cntr;			//18
	float ph_A_mechanical_wear;		//19
	float ph_B_mechanical_wear;		//20
	float ph_C_mechanical_wear;		//21
	float ph_A_electrical_wear;		//22
	float ph_B_electrical_wear;		//23
	float ph_C_electrical_wear;		//24
	float ph_A_last_sw_elec_wear;	//25
	float ph_B_last_sw_elec_wear;	//26
	float ph_C_last_sw_elec_wear;	//27
	float full_off_time_A;			//28
	float full_on_time_A;			//29
	float full_off_time_B;			//30
	float full_on_time_B;			//31
	float full_off_time_C;			//32
	float full_on_time_C;			//33
	float temperaturePT;			//34
	float shortCircuitOn;			//35
	float shortCircuitOff;			//36
	float operOff_On;				//37
	float operOn_Off;				//38
	float ownTimeOffState;			//39
	float ownTimeOnState;			//40
	float fullTimeOffState;			//41
	float fullTimeOnState;			//42
	
	
};

struct LCT1114_2 : public DeviceCommon
{
	
	uint32_t DigitalInput;	// SP data

	float RMS_A;					//1
	float RMS_B;					//2
	float RMS_C;					//3
	float ph_A_passed_resource;		//4
	float ph_B_passed_resource;		//5
	float ph_C_passed_resource;		//6
	float ph_A_last_arc_lifetime;	//7
	float ph_B_last_arc_lifetime;	//8
	float ph_C_last_arc_lifetime;	//9
	float ph_A_last_sw2off_curr;	//10
	float ph_B_last_sw2off_curr;	//11
	float ph_C_last_sw2off_curr;	//12
	float ph_A_last_sw2off_passed_res;	//13
	float ph_B_last_sw2off_passed_res;	//14
	float ph_C_last_sw2off_passed_res;	//15
	float ph_A_operation_cntr;			//16
	float ph_B_operation_cntr;			//17
	float ph_C_operation_cntr;			//18
	float ph_A_mechanical_wear;		//19
	float ph_B_mechanical_wear;		//20
	float ph_C_mechanical_wear;		//21
	float ph_A_electrical_wear;		//22
	float ph_B_electrical_wear;		//23
	float ph_C_electrical_wear;		//24
	float ph_A_last_sw_elec_wear;	//25
	float ph_B_last_sw_elec_wear;	//26
	float ph_C_last_sw_elec_wear;	//27
	float full_off_time_A;			//28
	float full_on_time_A;			//29
	float full_off_time_B;			//30
	float full_on_time_B;			//31
	float full_off_time_C;			//32
	float full_on_time_C;			//33
	float temperaturePT;			//34
	float shortCircuitOn;			//35
	float shortCircuitOff;			//36
	float operOff_On;				//37
	float operOn_Off;				//38
	float ownTimeOffState;			//39
	float ownTimeOnState;			//40
	float fullTimeOffState;			//41
	float fullTimeOnState;			//42
	
	float solenoidCurr_On;			//43
	float solenoidCurr_Off_1;		//44
	float solenoidCurr_Off_2;		//45
	float reArc;					//46
	
	
	
	
	
};

struct DM91_0 : public DeviceCommon
{
	float pressureDM;
	float pressure20DM;
	float densityDM;
	float themperatureDM;
};

struct DM91_H : public DeviceCommon
{
	float pressureDM;		// 0
	float pressure20DM;		// 1
	float densityDM;		// 2
	float themperatureDM;	// 3
	float dewPointDM;		// 4
	float PPMvDM;			// 5
	float RH_DM;			// 6
	float LUR;				// 7
};


struct PVT_100 : public DeviceCommon
{
	int16_t PVTtemp;
	int16_t PVThum;
};

#endif // IGAS_STRUCT