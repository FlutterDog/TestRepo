// IGAS_104_PackData.h
#ifndef IGAS_104_PACKDATA_H
#define IGAS_104_PACKDATA_H

#include <Arduino.h>         // SerialUSB
#include <IGAS_STRUCT_2.h>     // DeviceCommon и конкретные структуры устройств

#define sp_Invalid  B10000000
#define sp_Good  B00000000

#define setQDS_invalid(value) ((value) |= (B10000000))
#define setQDS_valid(value) ((value) &= (B01111111))



class IGAS_104_PackData {
	public:
	IGAS_104_PackData();                                              // конструктор
	~IGAS_104_PackData();                                             // деструктор Ч освободит пам€ть

	void init(void** devices, uint16_t totalDevices);                // инициализаци€ и выделение буферов
	void prepare(bool IECDebug = false);                             // сбор данных в loop()

	// √еттеры дл€ передачи в IEC104-библиотеку:
	int16_t* getSPData()    const { return data_M_SP;  }             // буфер SP-значений
	int16_t* getSPSpont()   const { return spont_M_SP; }             // индексы спорадичных SP
	uint16_t getSPCount()   const { return count_SP;   }             // сколько SP-точек собрано

	int16_t* getMEData()    const { return data_M_ME;  }             // буфер ME-значений
	int16_t* getMESpont()   const { return spont_M_ME; }             // индексы спорадичных ME
	uint16_t getMECount()   const { return count_ME;   }             // сколько ME-точек собрано

	float*   getTFData()    const { return data_M_TF;  }             // буфер TF-значений
	int16_t* getTFSpont()   const { return spont_M_TF; }             // индексы спорадичных TF
	uint16_t getTFCount()   const { return count_TF;   }             // сколько TF-точек собрано

	private:
	void**    devices;                                               // массив указателей на структуры устройств
	uint16_t  totalDevices;                                          // число устройств

	size_t    max_SP, max_ME, max_TF;                                // максимальные размеры буферов

	int16_t*  data_M_SP;     // динамический буфер дл€ SP     // [0..max_SP-1]
	int16_t*  spont_M_SP;    // динамический буфер индексов SP// [0..max_SP-1]
	int16_t*  data_M_ME;     // динамический буфер дл€ ME     // [0..max_ME-1]
	int16_t*  spont_M_ME;    // динамический буфер индексов ME// [0..max_ME-1]
	float*    data_M_TF;     // динамический буфер дл€ TF     // [0..max_TF-1]
	int16_t*  spont_M_TF;    // динамический буфер индексов TF// [0..max_TF-1]

	int8_t*  M_ME_QDS;    // динамический буфер индексов TF// [0..max_TF-1]
	int8_t*  M_TF_QDS;    // динамический буфер индексов TF// [0..max_TF-1]

	uint16_t  count_SP;      // сколько SP-точек в последней prepare()
	uint16_t  count_ME;      // сколько ME-точек в последней prepare()
	uint16_t  count_TF;      // сколько TF-точек в последней prepare()
	
	
};

#endif // IGAS_104_PACKDATA_H
