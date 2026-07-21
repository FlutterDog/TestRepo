#pragma once

#include "IGAS_STRUCT_2.h"   // здесь у вас Dev::TF_COUNT и dev->TF_current[]
#include "IGAS_ChannelNames.h"
// -----------------------------------------------------------------------------
// 0) Функции-конвертеры, напрямую в этот файл (без отдельного helper.h)
// -----------------------------------------------------------------------------

// Преобразование пары 16-битных регистров в float
static inline float regsToFloat(int16_t loReg, int16_t hiReg)
{
	union { uint32_t u32; float f; } conv;
	conv.u32 = (uint32_t(uint16_t(hiReg)) << 16)
	|  uint32_t(uint16_t(loReg));
	return conv.f;
}

// Заполнение массива TF_current из подряд идущих регистров,
// Dev::TF_COUNT штук, начиная со смещения startReg
template<typename Dev>
inline void X2X_to_Float(Dev* dev, const int16_t* data_X2X, uint8_t startReg = 0)
{
	for (uint8_t i = 0; i < Dev::TF_COUNT; ++i)
	{
		dev->TF_current[i] = regsToFloat(
		data_X2X[startReg + 2*i + 0],
		data_X2X[startReg + 2*i + 1]
		);
	}
}

// -----------------------------------------------------------------------------
// 1) extern-объявления IEC104-массивов
// -----------------------------------------------------------------------------
extern int16_t* IEC104data_M_SP;
extern int16_t* IEC104spont_M_SP;

extern int16_t* IEC104data_M_ME;
extern int16_t* IEC104spont_M_ME;
extern uint8_t* IEC104_M_ME_QDS;

extern float*   IEC104data_M_TF;
extern int16_t* IEC104spont_M_TF;
extern uint8_t* IEC104_M_TF_QDS;

// размеры (нужны вместо sizeof(массив))
extern uint16_t IEC104_SP_N;
extern uint16_t IEC104_ME_N;
extern uint16_t IEC104_TF_N;

// -----------------------------------------------------------------------------
// 2) шаблон обработки SP-каналов
// -----------------------------------------------------------------------------
template<typename Dev>
inline void processSP(Dev* dev, uint16_t& spOffset, uint16_t& dataChng_SP)
{
	for (uint8_t i = 0; i < Dev::SP_COUNT; ++i)
	{
		uint8_t bitValue = 0;

		if (i == 0) {
			// SP[0] = связь: 1 = lost, 0 = ok
			bitValue = (uint8_t)(dev->ConnectLost ? 1 : 0);
			} else {
			// SP[1] = DI0, SP[2]=DI1, ...
			bitValue = (uint8_t)((dev->DigitalInput >> (i - 1)) & 0x01);
		}

		// QDS: fault не используем
		uint8_t qds = QDS_OK;

		// Опционально: если хочешь помечать DI как invalid при потере связи:
		// if (dev->ConnectLost && i != 0) qds |= QDS_IV;

		uint8_t value = (uint8_t)(bitValue | qds);
		uint8_t oldValue = IEC104data_M_SP[spOffset];

		if (value != oldValue)
		{
			IEC104data_M_SP[spOffset]       = value;
			IEC104spont_M_SP[dataChng_SP++] = spOffset;
		}

		++spOffset;
	}
}



// -----------------------------------------------------------------------------
// 4) шаблон обработки ME-каналов
// -----------------------------------------------------------------------------
template<typename Dev>
inline void processME(Dev* dev, uint16_t& meOffset, uint16_t& dataChng_ME)
{

	uint8_t qds = QDS_OK;
	for (uint8_t i = 0;  i < Dev::ME_COUNT;  ++i)
	{
		// 1) текущее значение из структуры
		int16_t cur = dev->ME_current[i];
		

		// 3) старые значения для сравнения
		int16_t prevVal = IEC104data_M_ME[meOffset];
		uint8_t prevQds = IEC104_M_ME_QDS[meOffset];

		// 4) если само значение или метка качества изменились — обновляем
		if (cur != prevVal || qds != prevQds)
		{
			IEC104data_M_ME[meOffset]       = cur;
			IEC104_M_ME_QDS[meOffset]       = qds;
			IEC104spont_M_ME[dataChng_ME++] = meOffset;
		}

		// 5) следующий индекс
		++meOffset;
	}
}

// -----------------------------------------------------------------------------
// 3) шаблон обработки TF-каналов
// -----------------------------------------------------------------------------
template<typename Dev>
inline void processTF(Dev* dev, uint16_t& tfOffset, uint16_t& dataChng_TF)
{
	for (uint8_t i = 0; i < Dev::TF_COUNT; ++i)
	{
		float    cur    = dev->TF_current[i];
		uint8_t  qds    = QDS_OK;

		if (dev->ConnectLost) qds |= QDS_IV;
		if (dev->deviceFault) qds |= QDS_IV;

		float    prevVal = IEC104data_M_TF[tfOffset];
		uint8_t  prevQds = IEC104_M_TF_QDS[tfOffset];

		if (cur != prevVal || qds != prevQds)
		{
			IEC104data_M_TF[tfOffset]       = cur;
			IEC104_M_TF_QDS[tfOffset]       = qds;
			IEC104spont_M_TF[dataChng_TF++] = tfOffset;
		}
		++tfOffset;
	}
}
