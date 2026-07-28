#pragma once
#ifndef IGAS_CHANNEL_PRINTER_H
#define IGAS_CHANNEL_PRINTER_H

#include "IGAS_STRUCT_2.h"
#include "IGAS_ChannelNames.h"   // массивы человекочитаемых имён каналов
#include <Arduino.h>

#include <ctype.h>
#include <string.h>
#include <stdio.h>   // snprintf

// -----------------------------------------------------------------------------
// ВАЖНО ПРО SWver
// -----------------------------------------------------------------------------
// Если SWver у вас определён как #define SWver "1.0.0" — extern не нужен.
// Если SWver у вас глобальная переменная, объявите её тип ТУТ правильно.
// По умолчанию предполагаем:  const char SWver[] = "1.0.0";

extern const float SWver;       // <-- если у вас const char* : extern const char* SWver;

// QDS-флаги должны быть определены в вашем проекте:
// QDS_OK = 0x00, QDS_IV = 0x80

// -----------------------------------------------------------------------------
// Внешние переменные из вашего проекта
// -----------------------------------------------------------------------------
extern void*    system_Devices[];
extern uint16_t totalNodes;

// -----------------------------------------------------------------------------
// Forward declaration: dispatchUsbCmd() вызывает handleGetCommand()
// -----------------------------------------------------------------------------
inline void handleGetCommand();

// -----------------------------------------------------------------------------
// Утилиты строк
// -----------------------------------------------------------------------------
static inline void trimInPlace2(char* s)
{
	// left
	char* p = s;
	while (*p && isspace((unsigned char)*p)) ++p;
	if (p != s) memmove(s, p, strlen(p) + 1);

	// right
	size_t n = strlen(s);
	while (n && isspace((unsigned char)s[n-1])) s[--n] = '\0';
}

static inline void toLowerInPlace(char* s)
{
	for (; *s; ++s) *s = (char)tolower((unsigned char)*s);
}

// -----------------------------------------------------------------------------
// QDS → строка
// -----------------------------------------------------------------------------
inline const char* qdsToString(uint8_t qds)
{
	return (qds & QDS_IV) ? "Invalid" : "Good";
}

// -----------------------------------------------------------------------------
// Имя устройства по ID
// -----------------------------------------------------------------------------
inline const char* getDeviceName(uint8_t id)
{
	switch (id) {
		case LCP:     return "LCP1116";
		case LDO:     return "LDO1118";
		case LDI8:    return "LDI1118";
		case LDI16:   return "LDI1116";
		case LAI:     return "LAI1118";
		case LCT:     return "LCT1114";
		case LCT_2:   return "LCT1114_2";
		case DM910:   return "DM91_0";
		case DM910H:  return "DM91_H";
		case PVT100:  return "PVT_100";
		default:      return "Unknown";
	}
}

// -----------------------------------------------------------------------------
// Команды консоли
// -----------------------------------------------------------------------------
static inline void handleVerCommand()
{
	SerialUSB.println();
	SerialUSB.println(" **************************************************************************************");
	SerialUSB.println(" Version ****************************************************************************");
	SerialUSB.println(" **************************************************************************************");
	SerialUSB.print  (" FW: ");
	SerialUSB.println(SWver);
	SerialUSB.println(" **************************************************************************************");
	SerialUSB.println();
}

static inline void handleHelpCommand()
{
	SerialUSB.println();
	SerialUSB.println("Commands:");
	SerialUSB.println("  get   - print IEC104 dataset");
	SerialUSB.println("  ver   - print FW version");
	SerialUSB.println("  dev   - print Devices list");
	SerialUSB.println("  help  - this help");
	SerialUSB.println("  restart - reboot MCU");

	SerialUSB.println();
}

static inline void cpuRestart()
{
	// Стандартный reset через Reset Controller:
	// KEY = 0xA5, PROCRST = 1, PERRST = 1 (reset периферии)
	RSTC->RSTC_CR = RSTC_CR_KEY(0xA5) | RSTC_CR_PROCRST | RSTC_CR_PERRST;

	// Ждём пока ресет сработает
	while (1) { }
}

static inline void handleRestartCommand()
{
	SerialUSB.println();
	SerialUSB.println(" **************************************************************************************");
	SerialUSB.println(" Restart ****************************************************************************");
	SerialUSB.println(" **************************************************************************************");
	SerialUSB.println(" Rebooting now...");
	SerialUSB.flush();
	delay(20);

	cpuRestart();
}


static inline void handleDevCommand()
{
	SerialUSB.println();
	SerialUSB.println(" **************************************************************************************");
	SerialUSB.println(" Devices list *************************************************************************");
	SerialUSB.println(" **************************************************************************************");

	SerialUSB.print(" totalNodes = ");
	SerialUSB.println(totalNodes);
	SerialUSB.println();

	// Заголовок таблицы (лучше табами — кириллица не ломает сетку)
	SerialUSB.println("\tIdx\tID\tName");
	SerialUSB.println("\t---\t--\t----");

	for (uint16_t k = 0; k < totalNodes; ++k)
	{
		auto* hdr = static_cast<DeviceHeader*>(system_Devices[k]);
		if (!hdr) {
			SerialUSB.print("\t");
			SerialUSB.print(k);
			SerialUSB.println("\t<NULL>");
			continue;
		}

		SerialUSB.print("\t");
		SerialUSB.print(k);                 SerialUSB.print('\t');   // позиция в массиве
		SerialUSB.print(hdr->ID);           SerialUSB.print('\t');   // ID
		SerialUSB.println(getDeviceName(hdr->ID));

	}

	SerialUSB.println();
	SerialUSB.println(" **************************************************************************************");
}



// cmdLine — вся строка (уже в буфере)
static inline void dispatchUsbCmd(char* cmdLine)
{
	trimInPlace2(cmdLine);
	toLowerInPlace(cmdLine);
	if (cmdLine[0] == '\0') return;

	// отделяем команду от параметров: "cmd arg1 arg2"
	char* cmd = cmdLine;
	char* args = strchr(cmdLine, ' ');
	if (args) {
		*args++ = '\0';
		while (*args == ' ') ++args;
	}
	(void)args; // пока не используем

	if (!strcmp(cmd, "get"))  { handleGetCommand(); return; }
	if (!strcmp(cmd, "ver"))  { handleVerCommand(); return; }
	if (!strcmp(cmd, "dev"))  { handleDevCommand(); return; }
	if (!strcmp(cmd, "help")) { handleHelpCommand(); return; }
	if (!strcmp(cmd, "restart")) { handleRestartCommand(); return; }


	SerialUSB.print("Unknown command: ");
	SerialUSB.println(cmd);
	SerialUSB.println("Type 'help'");
}

// -----------------------------------------------------------------------------
// Печать одной строки таблицы (TAB + фиксированная сетка пробелами)
// Формат:  TYPE  IOA  VALUE  QDS  DESC
// -----------------------------------------------------------------------------
static inline void printRow(const char* type, int ioa, const char* value, const char* qds, const char* desc)
{
	SerialUSB.print('\t');

	// TYPE
	SerialUSB.print(type);
	SerialUSB.print("  "); // чуть плотнее

	// IOA (без ведущих нулей, но с аккуратным выравниванием)
	if (ioa < 10) SerialUSB.print(' ');
	SerialUSB.print(ioa);
	SerialUSB.print("  ");

	// VALUE — выравниваем до ширины 8
	const int value_field_width = 8;
	int val_len = (int)strlen(value);
	for (int i = val_len; i < value_field_width; ++i) SerialUSB.print(' ');
	SerialUSB.print(value);

	SerialUSB.print("  ");
	SerialUSB.print(qds);

	SerialUSB.print("  ");
	SerialUSB.println(desc);
}

// -----------------------------------------------------------------------------
// 1) Печать SP-каналов
// SP[0] = ConnectLost (0/1), остальные SP = биты DigitalInput (i-1)
// QDS: при ConnectLost/ошибке все SP кроме первого → Invalid
// -----------------------------------------------------------------------------
template<typename Dev>
void printSP(Dev* dev, uint16_t& spOffset, const char* const* SP_names)
{
	for (uint8_t i = 0; i < Dev::SP_COUNT; ++i)
	{
		bool bitValue =
		(i == 0) ? (bool)dev->ConnectLost
		: (((dev->DigitalInput >> (i - 1)) & 1u) ? true : false);

		const char* valueStr = bitValue ? "1" : "0";

		// QDS
		uint8_t qds = QDS_OK;
		if ((dev->ConnectLost || dev->deviceFault) && i != 0)
		qds |= QDS_IV;

		// Описание: для SP[0] дописываем статус словами
		char descBuf[96];
		const char* baseName = (SP_names ? SP_names[i] : "");
		if (i == 0) {
			snprintf(descBuf, sizeof(descBuf), "%s %s", baseName, bitValue ? "ОБРЫВ" : "На связи");
			} else {
			strncpy(descBuf, baseName, sizeof(descBuf));
			descBuf[sizeof(descBuf)-1] = '\0';
		}

		printRow("SP", spOffset, valueStr, qdsToString(qds), descBuf);
		++spOffset;
	}
}

// -----------------------------------------------------------------------------
// 2) Печать ME-каналов (все ME invalid при обрыве/ошибке)
// -----------------------------------------------------------------------------
template<typename Dev>
void printME(Dev* dev, uint16_t& meOffset, const char* const* ME_names)
{
	char valBuf[16];

	for (uint8_t i = 0; i < Dev::ME_COUNT; ++i)
	{
		int16_t cur = dev->ME_current[i];
		snprintf(valBuf, sizeof(valBuf), "%d", cur);

		uint8_t qds = QDS_OK;
		if (dev->ConnectLost || dev->deviceFault) qds |= QDS_IV;

		const char* name = ME_names ? ME_names[i] : "";
		printRow("ME", meOffset, valBuf, qdsToString(qds), name);
		++meOffset;
	}
}

// -----------------------------------------------------------------------------
// 3) Печать TF-каналов (все TF invalid при обрыве/ошибке)
// -----------------------------------------------------------------------------
template<typename Dev>
void printTF(Dev* dev, uint16_t& tfOffset, const char* const* TF_names)
{
	char valBuf[24];

	for (uint8_t i = 0; i < Dev::TF_COUNT; ++i)
	{
		snprintf(valBuf, sizeof(valBuf), "%.3f", dev->TF_current[i]);

		uint8_t qds = QDS_OK;
		if (dev->ConnectLost || dev->deviceFault) qds |= QDS_IV;

		const char* name = TF_names ? TF_names[i] : "";
		printRow("TF", tfOffset, valBuf, qdsToString(qds), name);
		++tfOffset;
	}
}

// -----------------------------------------------------------------------------
// 4) Общий обход всех устройств на команду "get"
// -----------------------------------------------------------------------------
inline void handleGetCommand()
{
	uint16_t spOffset = 0, meOffset = 0, tfOffset = 0;

	SerialUSB.println();
	SerialUSB.println(" **************************************************************************************");
	SerialUSB.println(" Start IEC104 data set ****************************************************************");
	SerialUSB.println(" **************************************************************************************");
	SerialUSB.println();

	for (uint16_t k = 0; k < totalNodes; ++k)
	{
		auto* hdr = static_cast<DeviceHeader*>(system_Devices[k]);

		SerialUSB.println();
		SerialUSB.print(" Device № = "); SerialUSB.println(k);
		SerialUSB.print(getDeviceName(hdr->ID));
		SerialUSB.print(" ID=");   SerialUSB.print(hdr->ID);
		SerialUSB.print(" ASDU="); SerialUSB.println(hdr->ASDU);
		SerialUSB.println();

		SerialUSB.println("\tТип  IOA   Значение     QDS   Описание");
		SerialUSB.println("\t---------------------------------------------------------------");

		switch (hdr->ID)
		{
			case LCP:
			printSP<LCP1116>(static_cast<LCP1116*>(hdr), spOffset, LCP1116_SP_names);
			break;

			case LDO:
			printSP<LDO1118>(static_cast<LDO1118*>(hdr), spOffset, LDO1118_SP_names);
			SerialUSB.println("\t----");
			printME<LDO1118>(static_cast<LDO1118*>(hdr), meOffset, LDO1118_ME_names);
			break;

			case LDI8:
			printSP<LDI1118>(static_cast<LDI1118*>(hdr), spOffset, LDI1118_SP_names);
			break;

			case LDI16:
			printSP<LDI1116>(static_cast<LDI1116*>(hdr), spOffset, LDI1116_SP_names);
			break;

			case LAI:
			printSP<LAI1118>(static_cast<LAI1118*>(hdr), spOffset, LAI1118_SP_names);
			SerialUSB.println("\t----");
			printTF<LAI1118>(static_cast<LAI1118*>(hdr), tfOffset, LAI1118_TF_names);
			break;

			case LCT:
			printSP<LCT1114>(static_cast<LCT1114*>(hdr), spOffset, LCT1114_SP_names);
			SerialUSB.println("\t----");
			printTF<LCT1114>(static_cast<LCT1114*>(hdr), tfOffset, LCT1114_TF_names);
			break;

			case LCT_2:
			printSP<LCT1114_2>(static_cast<LCT1114_2*>(hdr), spOffset, LCT1114_2_SP_names);
			SerialUSB.println("\t----");
			printTF<LCT1114_2>(static_cast<LCT1114_2*>(hdr), tfOffset, LCT1114_2_TF_names);
			break;

			case DM910:
			printSP<DM91_0>(static_cast<DM91_0*>(hdr), spOffset, DM91_0_SP_names);
			SerialUSB.println("\t----");
			printTF<DM91_0>(static_cast<DM91_0*>(hdr), tfOffset, DM91_0_TF_names);
			break;

			case DM910H:
			printSP<DM91_H>(static_cast<DM91_H*>(hdr), spOffset, DM91_H_SP_names);
			SerialUSB.println("\t----");
			printTF<DM91_H>(static_cast<DM91_H*>(hdr), tfOffset, DM91_H_TF_names);
			break;

			case PVT100:
			printSP<PVT_100>(static_cast<PVT_100*>(hdr), spOffset, PVT_100_SP_names);
			SerialUSB.println("\t----");
			printME<PVT_100>(static_cast<PVT_100*>(hdr), meOffset, PVT_100_ME_names);
			break;

			default:
			SerialUSB.print("Unknown device ID=");
			SerialUSB.println(hdr->ID);
			break;
		}

		SerialUSB.println("\t---------------------------------------------------------------");
	}

	SerialUSB.println();
	SerialUSB.println(" **************************************************************************************");
	SerialUSB.println(" IEC104 data set FIN ******************************************************************");
	SerialUSB.println(" **************************************************************************************");
	SerialUSB.println();
}






#endif // IGAS_CHANNEL_PRINTER_H
