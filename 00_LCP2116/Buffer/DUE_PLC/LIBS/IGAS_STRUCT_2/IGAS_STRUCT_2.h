#ifndef IGAS_STRUCT_2
#define IGAS_STRUCT_2

#include <cstdint>

#define bitReadLong(value, bit)   (((value) >> (bit)) & 0x01)
#define bitSetLong(value, bit)    ((value) |= (1ULL << (bit)))
#define bitClearLong(value, bit)  ((value) &= ~(1ULL << (bit)))
#define bitWriteLong(value, bit, bitvalue) \
(bitvalue ? bitSetLong(value, bit) : bitClearLong(value, bit))

// -----------------------------------------------------------------------------
// —писки ID устройств
// -----------------------------------------------------------------------------
enum : uint8_t {
	LCP = 0, // ѕроцессорный модуль
	LDO,	 // ћодуль реле
	LAI,	 // ћодуль аналоговых входов
	LDI8,	 // ћодуль LAI в режиме дискретов
	LCT,	// ћодуль коммутационного ресурса
	LDI16,  // ћодуль дискретных входов
	LCT_2,  // ћодуль коммутационного ресурса
	X2Xsize, 

	IR_SF6   = 101, // газоанализатор IGAS IR
	DM910    = 102, // ѕлотномер 
	DM910H   = 103,// ѕлотномер с влагой
	PVT100   = 104,// ƒатчик температуры
	ERIS210  = 105,// √азоанализатор ERIS
	fireDet  = 106,// ƒатчик пламени
	binar    = 107,// √азоанализатор Ѕинар
	proSense = 108,// √азоанализатор ProSense
	SensSize
};

// ¬вдение метаданных дл€ обь€влени€ структуры ћЁ 104

// -----------------------------------------------------------------------------
// ќбщий заголовок
// -----------------------------------------------------------------------------


#pragma pack(push,1)
struct DeviceHeader {
	uint8_t  ID;
	uint16_t ASDU;
	bool  ConnectLost;   // 0 Ц св€зь OK, 1 Ц потер€ св€зи
	bool  deviceFault;   // 0 Ц нет аварии,   1 Ц аварийное состо€ние
};
#pragma pack(pop)
static_assert(sizeof(DeviceHeader) == 1 + 2 + 1 + 1,
"DeviceHeader has unexpected padding");





// -----------------------------------------------------------------------------
// LCP1116: SP-теги (св€зь + авари€) + цифровой вход (unused)
// -----------------------------------------------------------------------------


#pragma pack(push,1)
struct LCP1116 : DeviceHeader {
	static constexpr uint8_t SP_COUNT = 0;
	static constexpr uint8_t ME_COUNT = 0;
	static constexpr uint8_t TF_COUNT = 0;

	uint32_t DigitalInput;

	// --- ћ≈“јƒјЌЌџ≈, не мен€ющие Ђpackї и размер экземпл€ра ---
	
};
#pragma pack(pop)






// -----------------------------------------------------------------------------
// LDO1118: SP-теги + цифровой вход + 1 ME-канал
// -----------------------------------------------------------------------------
struct LDO1118 : DeviceHeader {
	static constexpr uint8_t SP_COUNT = 1;
	static constexpr uint8_t ME_COUNT = 1;
	static constexpr uint8_t TF_COUNT = 0;

	uint32_t DigitalInput;

	union {
		struct { int16_t setPoint; };
		int16_t ME_current[ME_COUNT];
	};
};
static_assert(sizeof(LDO1118().ME_current) == sizeof(int16_t) * LDO1118::ME_COUNT,
"LDO1118 ME_current size mismatch");

// -----------------------------------------------------------------------------
// LDI1116: SP_COUNT=18 (16 DI) + цифровой вход
// -----------------------------------------------------------------------------
struct LDI1116 : DeviceHeader {
	static constexpr uint8_t SP_COUNT = 17;
	static constexpr uint8_t ME_COUNT = 0;
	static constexpr uint8_t TF_COUNT = 0;

	uint32_t DigitalInput;
};

// -----------------------------------------------------------------------------
// LDI1118: SP_COUNT=10 (8 DI) + цифровой вход
// -----------------------------------------------------------------------------
struct LDI1118 : DeviceHeader {
	static constexpr uint8_t SP_COUNT = 9;
	static constexpr uint8_t ME_COUNT = 0;
	static constexpr uint8_t TF_COUNT = 0;

	uint32_t DigitalInput;
};

// -----------------------------------------------------------------------------
// LAI1118: SP_COUNT=10 (8 DI), TF_COUNT=8 + цифровой вход
// -----------------------------------------------------------------------------
#pragma pack(push,1)
struct LAI1118 : DeviceHeader {
	static constexpr uint8_t SP_COUNT = 9;  // 0=св€зь, 1=авари€, 2..9=DI1..DI8
	static constexpr uint8_t ME_COUNT = 0;
	static constexpr uint8_t TF_COUNT = 8;

	uint32_t DigitalInput;   // регистр 0 Ч 8 младших бит

	union {
		struct {
			float AnalogInput01;
			float AnalogInput02;
			float AnalogInput03;
			float AnalogInput04;
			float AnalogInput05;
			float AnalogInput06;
			float AnalogInput07;
			float AnalogInput08;
		};
		float TF_current[TF_COUNT];
	};
};
#pragma pack(pop)
static_assert(sizeof(LAI1118().TF_current) == sizeof(float) * LAI1118::TF_COUNT,
"LAI1118 TF_current size mismatch");

// -----------------------------------------------------------------------------
// LCT1114: SP_COUNT=13 (11 DI), TF_COUNT=42 + цифровой вход
// -----------------------------------------------------------------------------
struct LCT1114 : DeviceHeader {
	static constexpr uint8_t SP_COUNT = 20;
	static constexpr uint8_t ME_COUNT = 0;
	static constexpr uint8_t TF_COUNT = 42;

	uint32_t DigitalInput;

	/*
	0 Ц ќшибка фазы A
	1 Ц ќшибка фазы B
	2 Ц ќшибка фазы C
	3 Ц „астичное переключение фаз (регистратор зафиксировал неполное переключение на одну из фаз)
	4 Ц Ќаправление последней операции (0 Ц отключение, 1 Ц включение)
	5 Ц ѕредупреждение: собственное врем€ отключени€ превысило порог предупреждени€
	6 Ц јвари€: собственное врем€ отключени€ превысило порог аварии
	7 Ц ѕредупреждение: собственное врем€ включени€ превысило порог предупреждени€
	8 Ц јвари€: собственное врем€ включени€ превысило порог аварии
	9 Ц ѕредупреждение: полное врем€ отключени€ превысило порог предупреждени€
	10 Ц јвари€: полное врем€ отключени€ превысило порог аварии
	11 Ц ѕредупреждение: полное врем€ включени€ превысило порог предупреждени€
	12 Ц јвари€: полное врем€ включени€ превысило порог аварии
	*/
	

	union {
		struct {
			float RMS_A;
			float RMS_B;
			float RMS_C;
			float ph_A_passed_resource;
			float ph_B_passed_resource;
			float ph_C_passed_resource;
			float ph_A_last_arc_lifetime;
			float ph_B_last_arc_lifetime;
			float ph_C_last_arc_lifetime;
			float ph_A_last_sw2off_curr;
			float ph_B_last_sw2off_curr;
			float ph_C_last_sw2off_curr;
			float ph_A_last_sw2off_passed_res;
			float ph_B_last_sw2off_passed_res;
			float ph_C_last_sw2off_passed_res;
			float ph_A_operation_cntr;
			float ph_B_operation_cntr;
			float ph_C_operation_cntr;
			float ph_A_mechanical_wear;
			float ph_B_mechanical_wear;
			float ph_C_mechanical_wear;
			float ph_A_electrical_wear;
			float ph_B_electrical_wear;
			float ph_C_electrical_wear;
			float ph_A_last_sw_elec_wear;
			float ph_B_last_sw_elec_wear;
			float ph_C_last_sw_elec_wear;
			float full_off_time_A;
			float full_on_time_A;
			float full_off_time_B;
			float full_on_time_B;
			float full_off_time_C;
			float full_on_time_C;
			float temperaturePT;
			float shortCircuitOn;
			float shortCircuitOff;
			float operOff_On;
			float operOn_Off;
			float ownTimeOffState;
			float ownTimeOnState;
			float fullTimeOffState;
			float fullTimeOnState;
		};
		float TF_current[TF_COUNT];
	};
};
static_assert(sizeof(LCT1114().TF_current) == sizeof(float) * LCT1114::TF_COUNT,
"LCT1114 TF_current size mismatch");

// -----------------------------------------------------------------------------
// LCT1114_2: SP_COUNT=13 (11 DI), TF_COUNT=46 + цифровой вход
// -----------------------------------------------------------------------------
struct LCT1114_2 : DeviceHeader {
	static constexpr uint8_t SP_COUNT = 20;
	static constexpr uint8_t ME_COUNT = 0;
	static constexpr uint8_t TF_COUNT = 46;

	uint32_t DigitalInput;
	/*
	0 Ц ќшибка фазы A
	1 Ц ќшибка фазы B
	2 Ц ќшибка фазы C
	3 Ц „астичное переключение фаз (регистратор зафиксировал неполное переключение на одну из фаз)
	4 Ц Ќаправление последней операции (0 Ц отключение, 1 Ц включение)
	5 Ц ѕредупреждение: собственное врем€ отключени€ превысило порог предупреждени€
	6 Ц јвари€: собственное врем€ отключени€ превысило порог аварии
	7 Ц ѕредупреждение: собственное врем€ включени€ превысило порог предупреждени€
	8 Ц јвари€: собственное врем€ включени€ превысило порог аварии
	9 Ц ѕредупреждение: полное врем€ отключени€ превысило порог предупреждени€
	10 Ц јвари€: полное врем€ отключени€ превысило порог аварии
	11 Ц ѕредупреждение: полное врем€ включени€ превысило порог предупреждени€
	12 Ц јвари€: полное врем€ включени€ превысило порог аварии
	*/

	union {
		struct {
			float RMS_A;
			float RMS_B;
			float RMS_C;
			float ph_A_passed_resource;
			float ph_B_passed_resource;
			float ph_C_passed_resource;
			float ph_A_last_arc_lifetime;
			float ph_B_last_arc_lifetime;
			float ph_C_last_arc_lifetime;
			float ph_A_last_sw2off_curr;
			float ph_B_last_sw2off_curr;
			float ph_C_last_sw2off_curr;
			float ph_A_last_sw2off_passed_res;
			float ph_B_last_sw2off_passed_res;
			float ph_C_last_sw2off_passed_res;
			float ph_A_operation_cntr;
			float ph_B_operation_cntr;
			float ph_C_operation_cntr;
			float ph_A_mechanical_wear;
			float ph_B_mechanical_wear;
			float ph_C_mechanical_wear;
			float ph_A_electrical_wear;
			float ph_B_electrical_wear;
			float ph_C_electrical_wear;
			float ph_A_last_sw_elec_wear;
			float ph_B_last_sw_elec_wear;
			float ph_C_last_sw_elec_wear;
			float full_off_time_A;
			float full_on_time_A;
			float full_off_time_B;
			float full_on_time_B;
			float full_off_time_C;
			float full_on_time_C;
			float temperaturePT;
			float shortCircuitOn;
			float shortCircuitOff;
			float operOff_On;
			float operOn_Off;
			float ownTimeOffState;
			float ownTimeOnState;
			float fullTimeOffState;
			float fullTimeOnState;
			float solenoidCurr_On;
			float solenoidCurr_Off_1;
			float solenoidCurr_Off_2;
			float reArc;
		};
		float TF_current[TF_COUNT];
	};
};
static_assert(sizeof(LCT1114_2().TF_current) == sizeof(float) * LCT1114_2::TF_COUNT,
"LCT1114_2 TF_current size mismatch");

// -----------------------------------------------------------------------------
// DM91_0: SP_COUNT=2, TF_COUNT=4 + цифровой вход(unused)
// -----------------------------------------------------------------------------
#pragma pack(push,1)
struct DM91_0 : DeviceHeader {
	static constexpr uint8_t SP_COUNT = 1;
	static constexpr uint8_t ME_COUNT = 0;
	static constexpr uint8_t TF_COUNT = 4;

	uint32_t DigitalInput;

	union {
		struct {
			float pressureDM;
			float pressure20DM;
			float densityDM;
			float themperatureDM;
		};
		float TF_current[TF_COUNT];
	};
};
static_assert(sizeof(DM91_0().TF_current) == sizeof(float) * DM91_0::TF_COUNT,
"DM91_0 TF_current size mismatch");
#pragma pack(pop)

// -----------------------------------------------------------------------------
// DM91_H: SP_COUNT=2, TF_COUNT=8 + цифровой вход(unused)
// -----------------------------------------------------------------------------
struct DM91_H : DeviceHeader {
	static constexpr uint8_t SP_COUNT = 1;
	static constexpr uint8_t ME_COUNT = 0;
	static constexpr uint8_t TF_COUNT = 8;

	uint32_t DigitalInput;

	union {
		struct {
			float pressureDM;
			float pressure20DM;
			float densityDM;
			float themperatureDM;
			float dewPointDM;
			float PPMvDM;
			float RH_DM;
			float LUR;
		};
		float TF_current[TF_COUNT];
	};
};
static_assert(sizeof(DM91_H().TF_current) == sizeof(float) * DM91_H::TF_COUNT,
"DM91_H TF_current size mismatch");


// -----------------------------------------------------------------------------
// PVT_100: SP_COUNT=2, ME_COUNT=2, TF_COUNT=0 + цифровой вход(unused)
// -----------------------------------------------------------------------------
struct PVT_100 : DeviceHeader {
	static constexpr uint8_t SP_COUNT = 1;
	static constexpr uint8_t ME_COUNT = 2;
	static constexpr uint8_t TF_COUNT = 0;

	uint32_t DigitalInput;

	union {
		struct {
			int16_t PVTtemp;
			int16_t PVThum;
		};
		int16_t ME_current[ME_COUNT];
	};
};
static_assert(sizeof(PVT_100().ME_current) == sizeof(int16_t) * PVT_100::ME_COUNT,
"PVT_100 ME_current size mismatch");

#endif // IGAS_STRUCT_2


