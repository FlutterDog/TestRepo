// IGAS_104_PackData.cpp
#include "IGAS_104_PackData.h"
#include <IGAS_104_SingleASDU.h>   // QDS_Invalid/QDS_Good

IGAS_104_PackData::IGAS_104_PackData()
: devices(nullptr),
totalDevices(0),
max_SP(0), max_ME(0), max_TF(0),
data_M_SP(nullptr),  spont_M_SP(nullptr),
data_M_ME(nullptr),  spont_M_ME(nullptr),
data_M_TF(nullptr),  spont_M_TF(nullptr),
count_SP(0), count_ME(0), count_TF(0)
{}  // пустой конструктор

IGAS_104_PackData::~IGAS_104_PackData() {
	delete[] data_M_SP;    delete[] spont_M_SP;    // освобождаем SP-буферы
	delete[] data_M_ME;    delete[] spont_M_ME;    // освобождаем ME-буферы
	delete[] data_M_TF;    delete[] spont_M_TF;    // освобождаем TF-буферы
}

void IGAS_104_PackData::init(void** devs, uint16_t total) {
	devices       = devs;              // сохран€ем указатель на массив устройств
	totalDevices  = total;             // сохран€ем число устройств

	// считаем общий размер дл€ каждого типа данных
	for (uint16_t k = 0; k < totalDevices; ++k) {
		auto* d = (DeviceCommon*)devices[k];
		max_SP += d->SP_qnt;             // накапливаем SP_qnt
		max_ME += d->ME_qnt;             // накапливаем ME_qnt
		max_TF += d->TF_qnt;             // накапливаем TF_qnt
	}

	// один раз выдел€ем пам€ть под буферы
	data_M_SP  = new int16_t[max_SP];  // буфер SP-значений
	spont_M_SP = new int16_t[max_SP];  // буфер индексов SP
	data_M_ME  = new int16_t[max_ME];  // буфер ME-значений
	spont_M_ME = new int16_t[max_ME];  // буфер индексов ME
	data_M_TF  = new float   [max_TF]; // буфер TF-значений
	spont_M_TF = new int16_t [max_TF]; // буфер индексов TF
}

void IGAS_104_PackData::prepare(bool IECDebug) {
	count_SP = count_ME = count_TF = 0;  // сброс счЄтчиков перед сбором

	if (IECDebug) {
		SerialUSB.print(F("PackData: devices="));
		SerialUSB.print(totalDevices);
		SerialUSB.println(F(" Ч start"));
	}

	// пробегаем по каждому устройству
	for (uint16_t k = 0; k < totalDevices; ++k) {
		DeviceCommon* dev = (DeviceCommon*)devices[k];
		
		uint8_t qdsFlag = dev->ConnectLost ? sp_Invalid : sp_Good;

		// ---- SINGLE POINTS (SP) ----
		if (dev->SP_Upd) {                                      // если есть изменени€
			for (uint8_t i = 0; i < dev->SP_qnt; ++i) {
				if (bitRead(dev->SP_Upd, i)) {                      // соответствующий бит = 1?
					int16_t v = QDS;                                  // по умолчанию Ч просто QDS
					switch (dev->ID) {
						case LAI:
						v = bitRead(((LAI1118*)dev)->DigitalInput, i);
						break;
						
						case LDI8:
						v = bitRead(((LDI1118*)dev)->DigitalInput, i);
						break;
						
						// Е другие типы устройств Е
					}
					
					if(qdsFlag)
					setQDS_invalid(v);
					
					data_M_SP [count_SP]  = v;                        // пишем в буфер значений
					spont_M_SP[count_SP]  = count_SP;                 // индекс спорадичной точки
					++count_SP;                                       // инкремент счЄтчика
				}
			}
			dev->SP_Upd = 0;                                      // сбрасываем флаги SP-изменений
		}

		// ---- MEASURED VALUES (ME) ----
		if (dev->ME_Upd) {                                     // если есть изменени€
			for (uint8_t i = 0; i < dev->ME_qnt; ++i) {
				if (bitRead(dev->ME_Upd, i)) {                     // бит = 1?
					int16_t v = 0;
					switch (dev->ID) {
						case PVT100:
						v = *(&((PVT_100*)dev)->PVTtemp + i);
						break;
						// Е другие типы Е
					}
					data_M_ME [count_ME] = v;                        // записываем значение
					spont_M_ME[count_ME] = count_ME;                 // индекс изменени€
					M_ME_QDS[count_ME] = qdsFlag;				 // метка качества сигнала
					
					++count_ME;                                      // счЄтчик ME
				}
			}
			dev->ME_Upd = 0;                                     // сброс флагов ME-изменений
		}

		// ---- FLOAT VALUES (TF) ----
		if (dev->TF_Upd) {                                     // если есть изменени€
			for (uint8_t i = 0; i < dev->TF_qnt; ++i) {
				if (bitRead(dev->TF_Upd, i)) {                     // проверка бита
					float v = 0.0f;
					switch (dev->ID) {
						case DM910:
						v = *(&((DM91_0*)dev)->pressureDM + i);
						break;
						// Е другие типы Е
					}
					data_M_TF [count_TF]  = v;                       // записываем float
					spont_M_TF[count_TF]  = count_TF;                // индекс TF-изменени€
					M_TF_QDS[count_ME] = qdsFlag;				 // метка качества сигнала
					++count_TF;                                      // счЄтчик TF
				}
			}
			dev->TF_Upd = 0;                                     // сброс флагов TF-изменений
		}
	}

	if (IECDebug) {
		SerialUSB.print(F("PackData done: SP="));
		SerialUSB.print(count_SP);
		SerialUSB.print(F(", ME="));
		SerialUSB.print(count_ME);
		SerialUSB.print(F(", TF="));
		SerialUSB.println(count_TF);
	}
}
