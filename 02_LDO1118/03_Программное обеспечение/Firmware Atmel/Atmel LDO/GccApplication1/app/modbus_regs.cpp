/**
 * @file modbus_regs.cpp
 * @brief Обработчик регистров Modbus: чтение/запись рабочих и паспортных параметров.
 *
 * @details
 * Функция modbusProceed() реализует маршрутизацию обращений к регистрам Modbus:
 *  - рабочие регистры: текущие значения, статусы, команды калибровки/сервиса;
 *  - паспортные регистры 9000..9025: прямой доступ к EEPROM (чтение/запись).
 *
 * Запись выполняется только для функций Modbus записи (FC06/FC16), определяемых по контексту
 * IGAS_mb_X.func. Для части параметров применяется валидация и ограничение диапазонов.
 *
 * @note
 * В коде используется расширение GCC: `case X ... Y`. Это требует совместимого toolchain.
 */

#include "modbus_regs.hpp"

#include <avr/eeprom.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <stdlib.h>

#include "eeprom_map.h"
#include "../Libs/ADC_init.h"
#include "../Libs/IGAS_mb.h"
#include "app.hpp"
#include "variables.hpp"

// --- Глобальный Modbus-стек (владение: модуль протокола) ---
extern IGAS_mb_485 IGAS_mb_X;

// --- Внешние функции проекта (используются здесь как команды/сервисы) ---
extern void replyDone(void);
extern void defaultSettings(void);
extern void gasToEEPROM(uint8_t idx);
extern void CheckADLadder(void);

// --- Локальные утилиты: ограничение диапазона (валидация входных значений) ---
static inline uint16_t clamp_u16(uint16_t v, uint16_t lo, uint16_t hi)
{
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

static inline uint8_t clamp_u8(uint8_t v, uint8_t lo, uint8_t hi)
{
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

static constexpr uint16_t kServiceModeKey = 20118U;

/**
 * @brief Обработка диапазона регистров Modbus.
 *
 * @details
 *  - writeCmd определяется по функции Modbus в IGAS_mb_X (FC06/FC16).
 *  - Паспортные регистры 9000..9025 читаются/пишутся напрямую в EEPROM.
 *  - replyDone() вызывается только после фактического выполнения команды записи.
 *
 * @param regs        Буфер регистров (для чтения: заполняется значениями; для записи: возвращается текущее/0).
 * @param start_addr  Первый адрес регистра.
 * @param reg_count   Количество регистров в запросе.
 * @param Value_      Значение записи (для FC06/FC16 в текущей реализации вызывающего кода).
 */
void modbusProceed(int16_t* regs, int16_t start_addr, int16_t reg_count, int16_t Value_)
{
	uint16_t registerAddr = static_cast<uint16_t>(start_addr);
	const bool writeCmd = (IGAS_mb_X.func == FC_WRITE_REGS) || (IGAS_mb_X.func == FC_WRITE_REG);

	for (int i = 0; i < reg_count; ++i, ++registerAddr)
	{
		switch (registerAddr)
		{
			// =========================================================================
			// Рабочие регистры: онлайн-значения, статусы, команды калибровки/сервиса
			// =========================================================================

			// [HR 0000] Концентрация (онлайн)
			// Доступ: R
			// Источник: outputSignals[concentration]
			// Единицы: текущие инженерные единицы прибора (как настроено/сконфигурировано).
			case 0: // Концентрация в текущих единицах (онлайн).
				regs[i] = outputSignals[concentration];
				break;

			// [HR 0001] Код состояния / ошибки (онлайн)
			// Доступ: R
			// Источник: outputSignals[currentState]
			case 1: // Байт ошибок/состояния газоанализатора (онлайн).
				regs[i] = outputSignals[currentState];
				break;

			// [HR 0003] Команда калибровки нуля (захват опорного уровня)
			// Доступ: W (WRITE ONLY)
			// Действие при записи:
			//  - формирование X-точки (sensorAnalogX[0]) по текущему delta + zeroShift из EEPROM;
			//  - CheckADLadder();
			//  - сохранение через gasToEEPROM(1);
			//  - replyDone().
			// Ответ: 0
			case 3: // Команда: записать "ноль" по текущему сигналу (WRITE).
				if (writeCmd)
				{
					// Ноль рассчитывается из текущего delta и сохранённого zeroShift.
					sensorAnalogX[0] = OUTSIGNAL[delta] + static_cast<int16_t>(EERB(EE_ZERO_SHIFT));

					CheckADLadder();
					gasToEEPROM(1);
					replyDone();
				}
				regs[i] = 0;
				break;

			// [HR 0004..0009] Команды захвата точек калибровки 1..6
			// Доступ: W (WRITE ONLY)
			// Вход (Value_): Y (концентрация для точки)
			// Действие при записи:
			//  - X берётся как текущий OUTSIGNAL[delta];
			//  - запись Y в gasConcentrationY[idx], запись X в sensorAnalogX[idx];
			//  - CheckADLadder();
			//  - сохранение в EEPROM: gasToEEPROM(idx) и gasToEEPROM(idx+1) (наследованное поведение);
			//  - replyDone().
			// Ответ: 0
			// GCC extension: диапазон case 4..9
			case 4 ... 9: // Точки калибровки 1..6: записать Y (конц.) и X (ADC) по текущему сигналу (WRITE).
				if (writeCmd)
				{
					const uint8_t idx = static_cast<uint8_t>(registerAddr - 3U);
					gasConcentrationY[idx] = static_cast<uint16_t>(Value_);
					sensorAnalogX[idx]     = OUTSIGNAL[delta];

					CheckADLadder();
					// Сохраняем соседние точки (поведение сохранено как в исходной реализации).
					gasToEEPROM(idx);
					gasToEEPROM(static_cast<uint8_t>(idx + 1U));
					replyDone();
				}
				regs[i] = 0;
				break;

			// [HR 0010] Команда захвата точки калибровки 7
			// Доступ: W (WRITE ONLY)
			// Вход (Value_): Y (концентрация)
			// Действие при записи: аналогично точкам 1..6, но idx=7, сохранение gasToEEPROM(7), replyDone().
			// Ответ: 0
			case 10: // Точка калибровки 7: записать Y и X по текущему сигналу (WRITE).
				if (writeCmd)
				{
					gasConcentrationY[7] = static_cast<uint16_t>(Value_);
					sensorAnalogX[7]     = OUTSIGNAL[delta];

					CheckADLadder();
					gasToEEPROM(7);
					replyDone();
				}
				regs[i] = 0;
				break;

			// [HR 0011] Modbus slave address
			// Доступ: R/W
			// Единицы: 1..247 (валидация clamp)
			// Хранение: RAM (slaveAddress, IGAS_mb_X.slave) + EEPROM (EE_SLAVE_ADDR)
			// Применение: сразу (обновляется IGAS_mb_X.slave)
			case 11: // Modbus Slave Address (1..247) (R/W).
				if (writeCmd)
				{
					const uint8_t v = clamp_u8(static_cast<uint8_t>(Value_), 1U, 247U);
					slaveAddress    = v;
					IGAS_mb_X.slave = v;
					EEWB(EE_SLAVE_ADDR, v);
					replyDone();
				}
				regs[i] = slaveAddress;
				break;

			// [HR 0012] Порог «низкий сигнал сенсора»
			// Доступ: R/W
			// Тип: u16
			// Хранение: RAM (lowSignal) + EEPROM (EE_LOW_SIGNAL)
			case 12: // Порог «низкий сигнал сенсора» (u16) (R/W).
				if (writeCmd)
				{
					lowSignal = static_cast<uint16_t>(Value_);
					EEW(EE_LOW_SIGNAL, lowSignal);
					replyDone();
				}
				regs[i] = static_cast<int16_t>(lowSignal);
				break;

			// [HR 0013] Команда автокалибровки нуля
			// Доступ: W (WRITE ONLY)
			// Действие: calibrZero(); replyDone(); ответ 0
			case 13: // Команда: автоперекалибровка нуля (WRITE).
				if (writeCmd)
				{
					calibrZero();
					replyDone();
				}
				regs[i] = 0;
				break;

			// [HR 0014] Команда калибровки span по заданной концентрации
			// Доступ: W (WRITE ONLY)
			// Вход (Value_): концентрация (в текущих единицах калибровки span)
			// Действие: calibrSpan(Value_); replyDone(); ответ 0
			case 14: // Команда: калибровка span по заданной концентрации (WRITE).
				if (writeCmd)
				{
					calibrSpan(Value_);
					replyDone();
				}
				regs[i] = 0;
				break;

			// [HR 0015] Команда расчёта spanZero и запись в EEPROM
			// Доступ: W (WRITE ONLY)
			// Действие:
			//  - вычисление sz = 200 - SUBSTRACTED[delta];
			//  - spanZero хранится как u16 (бит-в-бит интерпретация s16);
			//  - запись в EEPROM EE_SPAN_ZERO_S16 (бит-в-бит);
			//  - replyDone(); ответ 0
			case 15: // Команда: расчёт spanZero (s16) и запись в EEPROM (WRITE).
				if (writeCmd)
				{
					const int16_t sz = static_cast<int16_t>(200 - SUBSTRACTED[delta]);
					spanZero = static_cast<uint16_t>(sz);               // внутреннее хранение как raw-биты
					EEW(EE_SPAN_ZERO_S16, static_cast<uint16_t>(sz));   // сохранение в EEPROM как s16 (бит-в-бит)
					replyDone();
				}
				regs[i] = 0;
				break;

			// [HR 0016] SpanZero (текущее значение)
			// Доступ: R
			// Тип: u16 (битовая интерпретация s16 исторически)
			case 16: // spanZero (READ).
				regs[i] = static_cast<int16_t>(spanZero);
				break;

			// [HR 0017] Порог «низкий сигнал сенсора» (текущее значение)
			// Доступ: R
			case 17: // lowSignal (READ).
				regs[i] = static_cast<int16_t>(lowSignal);
				break;

			// [HR 0020..0027] Калибровочные X-точки (ADC)
			// Доступ: R
			// Источник: sensorAnalogX[0..7]
			case 20 ... 27: // X-точки калибровки (ADC), чтение.
				regs[i] = static_cast<int16_t>(sensorAnalogX[registerAddr - 20U]);
				break;

			// [HR 0030..0037] Калибровочные Y-точки (концентрация)
			// Доступ: R
			// Источник: gasConcentrationY[0..7]
			case 30 ... 37: // Y-точки калибровки (концентрация), чтение.
				regs[i] = static_cast<int16_t>(gasConcentrationY[registerAddr - 30U]);
				break;

			// [HR 0039] Серийный номер (EEPROM)
			// Доступ: R
			// Источник: EE_SERIAL_NO
			case 39: // Серийный номер (u16) из EEPROM.
				regs[i] = static_cast<int16_t>(EER(EE_SERIAL_NO));
				break;

			// [HR 0040] Версия прошивки
			// Доступ: R
			// Источник: firmwareDef (константа проекта)
			case 40: // Версия прошивки (константа проекта).
				regs[i] = firmwareDef;
				break;

			// [HR 0048] Команда сброса к настройкам по умолчанию
			// Доступ: W (WRITE ONLY)
			// Действие:
			//  - defaultSettings();
			//  - синхронизация IGAS_mb_X.slave = slaveAddress;
			//  - диагностика "EEPROM пустая" по EE_LOW_SIGNAL (2 байта == 0xFF);
			//  - replyDone(); ответ 0
			case 48: // Команда: сброс к настройкам по умолчанию (WRITE).
				if (writeCmd)
				{
					defaultSettings();
					IGAS_mb_X.slave = slaveAddress;

					// Наследованный признак "EEPROM пустая" по двум байтам порога.
					// (Смысл сохранён: диагностика первичной инициализации.)
					if (EERB(EE_LOW_SIGNAL) == 0xFFU && EERB(EE_LOW_SIGNAL + 1U) == 0xFFU)
						msgEEPROMError = 1;
					else
						msgEEPROMError = 0;

					replyDone();
				}
				regs[i] = 0;
				break;

			// [HR 0049] Маска блокировок/ошибок (EEPROM)
			// Доступ: R
			case 49: // Маска блокировок/ошибок (u16) (READ).
				regs[i] = static_cast<int16_t>(EER(EE_BLOCK_ERROR));
				break;

			// [HR 0050] Маска блокировок/ошибок (EEPROM)
			// Доступ: R/W
			// Хранение: EEPROM EE_BLOCK_ERROR
			case 50: // Маска блокировок/ошибок (u16) (R/W).
				if (writeCmd)
				{
					EEW(EE_BLOCK_ERROR, static_cast<uint16_t>(Value_));
					replyDone();
				}
				regs[i] = static_cast<int16_t>(EER(EE_BLOCK_ERROR));
				break;

			// [HR 0070..0077] Прямая запись калибровочной кривой Y
			// Доступ: R/W
			// Источник/приёмник: gasConcentrationY[0..7] (без побочных действий кроме replyDone)
			case 70 ... 77: // Калибровочная кривая Y: прямое присваивание (R/W).
				if (writeCmd)
				{
					gasConcentrationY[registerAddr - 70U] = static_cast<uint16_t>(Value_);
					replyDone();
				}
				regs[i] = static_cast<int16_t>(gasConcentrationY[registerAddr - 70U]);
				break;

			// [HR 0078] Уставка "чистого окна" (cleanSetPoint)
			// Доступ: R/W
			// Хранение: RAM cleanSetPoint + EEPROM EE_CLEAN_SETPOINT
			case 78: // Уставка "чистого окна" (u16) (R/W).
				if (writeCmd)
				{
					cleanSetPoint = static_cast<uint16_t>(Value_);
					EEW(EE_CLEAN_SETPOINT, cleanSetPoint);
					replyDone();
				}
				regs[i] = static_cast<int16_t>(cleanSetPoint);
				break;

			// [HR 0079] Уставка "чистого окна" (cleanSetPoint) — чтение текущего
			// Доступ: R
			case 79: // Уставка "чистого окна" (u16) (READ).
				regs[i] = static_cast<int16_t>(cleanSetPoint);
				break;

			// [HR 0080..0087] Прямая запись калибровочной кривой X (ADC)
			// Доступ: R/W
			// Источник/приёмник: sensorAnalogX[0..7]
			case 80 ... 87: // Калибровочная кривая X (ADC): прямое присваивание (R/W).
				if (writeCmd)
				{
					sensorAnalogX[registerAddr - 80U] = static_cast<uint16_t>(Value_);
					replyDone();
				}
				regs[i] = static_cast<int16_t>(sensorAnalogX[registerAddr - 80U]);
				break;

			// [HR 0088] zeroShift (EEPROM)
			// Доступ: R/W
			// Хранение: EEPROM EE_ZERO_SHIFT
			case 88: // zeroShift (u8) (R/W).
				if (writeCmd)
				{
					EEWB(EE_ZERO_SHIFT, static_cast<uint8_t>(Value_));
					replyDone();
				}
				regs[i] = static_cast<int16_t>(EERB(EE_ZERO_SHIFT));
				break;

			// [HR 0089] zeroShift (EEPROM) — чтение
			// Доступ: R
			case 89: // zeroShift (u8) (READ).
				regs[i] = static_cast<int16_t>(EERB(EE_ZERO_SHIFT));
				break;

			// [HR 0090] Команда сохранения всех точек калибровки в EEPROM
			// Доступ: W (WRITE ONLY)
			// Действие: gasToEEPROM(1..7), replyDone(); ответ 0
			case 90: // Команда: сохранить все точки калибровки в EEPROM (WRITE).
				if (writeCmd)
				{
					for (uint8_t j = 1; j <= 7; j++) gasToEEPROM(j);
					replyDone();
				}
				regs[i] = 0;
				break;

			// [HR 0101] Команда пропуска шага в автокалибровке
			// Доступ: W (WRITE ONLY)
			// Действие: skipStep=1; replyDone(); ответ 0
			case 101: // Команда: пропустить шаг в автокалибровке (WRITE).
				if (writeCmd)
				{
					skipStep = 1;
					replyDone();
				}
				regs[i] = 0;
				break;

			// [HR 0102] Индекс скорости RS-485 (bitrate index)
			// Доступ: R/W
			// Валидация: Value_<6 иначе 4 (наследованное)
			// Хранение: EEPROM EE_BITRATE
			// Применение: через reboot (reboot=1)
			case 102: // Индекс скорости RS-485 (u8) (R/W). Требует перезапуска коммуникаций.
				if (writeCmd)
				{
					const uint8_t br = (Value_ < 6) ? static_cast<uint8_t>(Value_) : 4U;
					EEWB(EE_BITRATE, br);

					// Перенастройка UART выполняется через процедуру перезагрузки.
					reboot = 1;
					replyDone();
				}
				regs[i] = static_cast<int16_t>(EERB(EE_BITRATE));
				break;

			// [HR 0205] Год следующей поверки/обслуживания
			// Доступ: R
			// Источник: EEPROM EE_NEXTVER_YEAR
			case 205: // Год следующей поверки/обслуживания (u16) (READ): EEPROM[EE_NEXTVER_YEAR].
				regs[i] = static_cast<int16_t>(EER(EE_NEXTVER_YEAR));
				break;

			// [HR 0206] Месяц последнего обслуживания
			// Доступ: R
			// Источник: EEPROM EE_SVC_MONTH
			case 206: // Месяц последнего обслуживания (u16) (READ): EEPROM[EE_SVC_MONTH].
				regs[i] = static_cast<int16_t>(EER(EE_SVC_MONTH));
				break;

			// [HR 0207] Нагреватель: команда включения/выключения
			// Доступ: R/W
			// Вход: 0/1
			// Действие при записи: heaterBreaker(0/1), replyDone()
			// Ответ: текущее heaterSwitch
			case 207: // Нагрев (0/1): команда включения/выключения (R/W).
				if (writeCmd)
				{
					heaterBreaker(static_cast<uint8_t>(Value_ ? 1 : 0));
					replyDone();
				}
				regs[i] = heaterSwitch;
				break;

			// =========================================================================
			// Настроечные параметры измерительного цикла / ИК-источника (контрактные)
			// =========================================================================

			// [HR 0209] Период цикла (period), мс — чтение текущего
			// Доступ: R
			case 209: // Период цикла (мс) (READ).
				regs[i] = static_cast<int16_t>(period);
				break;

			// [HR 0210] Период цикла (period), мс — чтение/запись
			// Доступ: R/W
			// Хранение: RAM period + EEPROM EE_PERIOD_MS
			// Применение: по текущей логике — сразу после записи (без reboot)
			case 210: // Период цикла (мс) (R/W).
				if (writeCmd)
				{
					period = static_cast<uint16_t>(Value_);
					EEW(EE_PERIOD_MS, period);
					replyDone();
				}
				regs[i] = static_cast<int16_t>(period);
				break;

			// [HR 0211] Время выборки (sampleTime), мс — чтение текущего
			// Доступ: R
			case 211: // Время выборки (мс) (READ).
				regs[i] = static_cast<int16_t>(sampleTime);
				break;

			// [HR 0212] Время выборки (sampleTime), мс — запись без EEPROM
			// Доступ: W (фактически), при чтении возвращает текущий sampleTime
			// Хранение: RAM sampleTime (без сохранения)
			case 212: // Время выборки (мс) (WRITE без сохранения в EEPROM).
				if (writeCmd)
				{
					sampleTime = static_cast<uint16_t>(Value_);
					replyDone();
				}
				regs[i] = static_cast<int16_t>(sampleTime);
				break;

			// [HR 0213] Время свечения ИК-источника (IRTime), мс — чтение текущего
			// Доступ: R
			case 213: // Время свечения ИК-источника (мс) (READ).
				regs[i] = static_cast<int16_t>(IRTime);
				break;

			// [HR 0214] Время свечения ИК-источника (IRTime), мс — чтение/запись
			// Доступ: R/W
			// Хранение: RAM IRTime + EEPROM EE_IRTIME_MS
			case 214: // Время свечения ИК-источника (мс) (R/W).
				if (writeCmd)
				{
					IRTime = static_cast<uint16_t>(Value_);
					EEW(EE_IRTIME_MS, IRTime);
					replyDone();
				}
				regs[i] = static_cast<int16_t>(IRTime);
				break;

			// [HR 0215] Опция магнитной калибровки (magnetOption), 0/1 — чтение
			// Доступ: R
			case 215: // Опция магнитной калибровки (0/1) (READ).
				regs[i] = static_cast<int16_t>(magnetOption);
				break;

			// [HR 0216] Опция магнитной калибровки (magnetOption), 0/1 — чтение/запись
			// Доступ: R/W
			// Хранение: RAM magnetOption + EEPROM EE_MAGNET_OPT
			case 216: // Опция магнитной калибровки (0/1) (R/W).
				if (writeCmd)
				{
					magnetOption = static_cast<uint8_t>(Value_);
					EEWB(EE_MAGNET_OPT, magnetOption);
					replyDone();
				}
				regs[i] = static_cast<int16_t>(magnetOption);
				break;

			// [HR 0220..0223] Пороговые параметры (только чтение)
			// Доступ: R
			// Источник: переменные stablePirSP/refNoiseSP/gasNoiseSP/thermoGradientSP
			case 220: regs[i] = static_cast<int16_t>(stablePirSP);         break; // Стабилизация при старте (READ).
			case 221: regs[i] = static_cast<int16_t>(refNoiseSP);          break; // Порог шума REF (READ).
			case 222: regs[i] = static_cast<int16_t>(gasNoiseSP);          break; // Порог шума GAS (READ).
			case 223: regs[i] = static_cast<int16_t>(thermoGradientSP);    break; // Порог термоградиента (READ).

			// [HR 0225] Порог стабилизации при старте (stablePirSP)
			// Доступ: R/W
			// Хранение: RAM stablePirSP; EEPROM хранит u8 (EE_STABLE_PIR_SP_B) (исторический формат)
			// Валидация: clamp_u8 0..255
			case 225: // Стабилизация при старте (R/W). Хранение как u8 (исторический формат).
				if (writeCmd)
				{
					stablePirSP = static_cast<uint16_t>(Value_);
					EEWB(EE_STABLE_PIR_SP_B, clamp_u8(static_cast<uint8_t>(stablePirSP), 0U, 255U));
					replyDone();
				}
				regs[i] = static_cast<int16_t>(stablePirSP);
				break;

			// [HR 0226] Порог шума REF (refNoiseSP)
			// Доступ: R/W
			// Хранение: RAM refNoiseSP + EEPROM EE_REF_NOISE
			case 226: // Порог шума REF (R/W) (u16).
				if (writeCmd)
				{
					refNoiseSP = static_cast<uint16_t>(Value_);
					EEW(EE_REF_NOISE, refNoiseSP);
					replyDone();
				}
				regs[i] = static_cast<int16_t>(refNoiseSP);
				break;

			// [HR 0227] Порог шума GAS (gasNoiseSP)
			// Доступ: R/W
			// Хранение: RAM gasNoiseSP + EEPROM EE_GAS_NOISE
			case 227: // Порог шума GAS (R/W) (u16).
				if (writeCmd)
				{
					gasNoiseSP = static_cast<uint16_t>(Value_);
					EEW(EE_GAS_NOISE, gasNoiseSP);
					replyDone();
				}
				regs[i] = static_cast<int16_t>(gasNoiseSP);
				break;

			// [HR 0228] Порог термоградиента (thermoGradientSP)
			// Доступ: R/W
			// Хранение: RAM thermoGradientSP; EEPROM хранит u8 (EE_THERMO_GRAD_B) (исторический формат)
			// Валидация: clamp_u8 0..255
			case 228: // Порог термоградиента (R/W). EEPROM хранит u8 (исторический формат).
				if (writeCmd)
				{
					thermoGradientSP = static_cast<uint16_t>(Value_);
					EEWB(EE_THERMO_GRAD_B, clamp_u8(static_cast<uint8_t>(thermoGradientSP), 0U, 255U));
					replyDone();
				}
				regs[i] = static_cast<int16_t>(thermoGradientSP);
				break;

			// [HR 0297] Нагреватель: включение/выключение + сохранение
			// Доступ: R/W
			// Вход: 0/1
			// Хранение: RAM heaterSwitch + EEPROM EE_HEATER_SWITCH
			// Действие: при изменении — heaterBreaker(heaterSwitch); replyDone()
			case 297: // Нагрев (0/1) с сохранением в EEPROM (R/W).
				if (writeCmd)
				{
					const uint8_t v = static_cast<uint8_t>(Value_ ? 1 : 0);
					if (heaterSwitch != v)
					{
						heaterSwitch = v;
						EEWB(EE_HEATER_SWITCH, heaterSwitch);
						heaterBreaker(heaterSwitch);
					}
					replyDone();
				}
				regs[i] = heaterSwitch;
				break;

			// [HR 0298] Нагреватель: чтение состояния
			// Доступ: R
			case 298: // Нагрев (0/1) (READ).
				regs[i] = heaterSwitch;
				break;

			// [HR 0299] Gain x1/x4: смена опорного напряжения и пересчёт порогов
			// Доступ: R/W
			// Вход: 0/1
			// Действие при смене:
			//  - пересчёт lowerHeatLimit/upperHeatLimit/thermoGradientSP (масштаб x5 или /5);
			//  - analogReference(DEFAULT/INTERNAL);
			//  - запись в EEPROM: EE_THERMO_GRAD_B (u8), EE_GAIN, EE_UPPER_HEAT, EE_LOWER_HEAT;
			//  - replyDone().
			case 299: // Gain x1/x4: смена опорного напряжения и пересчёт порогов (R/W).
				if (writeCmd)
				{
					const uint8_t v = static_cast<uint8_t>(Value_ ? 1 : 0);
					if (v != gain)
					{
						gain = v;

						// Пороги масштабируются синхронно со сменой диапазона.
						if (gain == 0)
						{
							lowerHeatLimit   = static_cast<uint16_t>(lowerHeatLimit / 5U);
							upperHeatLimit   = static_cast<uint16_t>(upperHeatLimit / 5U);
							thermoGradientSP = static_cast<uint16_t>(thermoGradientSP / 5U);
							analogReference(DEFAULT);
						}
						else
						{
							lowerHeatLimit   = static_cast<uint16_t>(lowerHeatLimit * 5U);
							upperHeatLimit   = static_cast<uint16_t>(upperHeatLimit * 5U);
							thermoGradientSP = static_cast<uint16_t>(thermoGradientSP * 5U);
							analogReference(INTERNAL);
						}

						EEWB(EE_THERMO_GRAD_B, clamp_u8(static_cast<uint8_t>(thermoGradientSP), 0U, 255U));
						EEWB(EE_GAIN, gain);
						EEW(EE_UPPER_HEAT, upperHeatLimit);
						EEW(EE_LOWER_HEAT, lowerHeatLimit);
					}
					replyDone();
				}
				regs[i] = gain;
				break;

			// [HR 0302] stableGate
			// Доступ: R/W
			// Хранение: RAM stableGate + EEPROM EE_STABLE_GATE
			case 302: // stableGate (u8) (R/W).
				if (writeCmd)
				{
					stableGate = static_cast<uint8_t>(Value_);
					EEWB(EE_STABLE_GATE, stableGate);
					replyDone();
				}
				regs[i] = stableGate;
				break;

			// [HR 0312] Статус калибровки нагревателя
			// Доступ: R
			case 312: // Статус калибровки нагревателя (READ).
				regs[i] = heaterTuneStatus;
				break;

			// =========================================================================
			// Диагностические регистры: внутренние сигналы/буферы/состояния
			// =========================================================================

			// [HR 0500..0501] Дубли основных регистров 0..1 для удобства чтения
			// Доступ: R
			case 500 ... 501: // Дубли 0..1 для удобства (READ).
				regs[i] = outputSignals[registerAddr - 500U];
				break;

			// [HR 0502] Температура платы приёмника (внутренние единицы/формат SUBSTRACTED[tempPyro])
			// Доступ: R
			case 502: regs[i] = SUBSTRACTED[2 /* tempPyro   */];             break; // Температура платы приёмника.

			// [HR 0503] Температура нагревателя (внутренние единицы/формат SUBSTRACTED[tempHeater])
			// Доступ: R
			case 503: regs[i] = SUBSTRACTED[3 /* tempHeater */];             break; // Температура нагревателя.

			// [HR 0504..0505] GAP-фильтрованные каналы
			// Доступ: R
			case 504 ... 505: regs[i] = GAPFILTERED[registerAddr - 504U];    break; // GAP-фильтрованные каналы.

			// [HR 0506] Текущая оценка delta (OUTSIGNAL[delta])
			// Доступ: R
			case 506: regs[i] = OUTSIGNAL[0 /* delta */];                    break; // Текущая оценка (delta).

			// [HR 0507] Биты воздействий/состояний (impactByte)
			// Доступ: R
			case 507: regs[i] = impactByte;                                  break; // Биты воздействий/состояний.

			// [HR 0508..0509] RAW (прямой сигнал)
			// Доступ: R
			case 508 ... 509: regs[i] = RAW[registerAddr - 508U];            break; // RAW (прямой сигнал).

			// [HR 0510..0511] Усреднённые сигналы
			// Доступ: R
			case 510 ... 511: regs[i] = AVERAGED[registerAddr - 510U];       break; // Усреднённые сигналы.

			// [HR 0512..0513] Нормализованные сигналы
			// Доступ: R
			case 512 ... 513: regs[i] = NORMALIZED[registerAddr - 512U];     break; // Нормализованные сигналы.

			// [HR 0514..0515] Сигналы после PID-подавления шума
			// Доступ: R
			case 514 ... 515: regs[i] = PIDFILTER[registerAddr - 514U];      break; // После PID-подавления шума.

			// [HR 0519] Флаг детектирования газа (gasChannelFlag)
			// Доступ: R
			case 519: regs[i] = gasChannelFlag;                              break; // Флаг детектирования газа.

			// [HR 0520] Счётчик воздействий в "тишине" (stabIdleCnt)
			// Доступ: R
			case 520: regs[i] = stabIdleCnt;                                 break; // Счётчик воздействий в "тишине".

			// [HR 0521] Счётчик стабильности после воздействия (stabActiveCnt)
			// Доступ: R
			case 521: regs[i] = stabActiveCnt;                               break; // Счётчик стабильности после воздействия.

			// [HR 0522..0523] "Угловые точки" тренда (cornerSTONE[])
			// Доступ: R
			case 522 ... 523: regs[i] = cornerSTONE[registerAddr - 522U];    break; // "Угловые точки" тренда.

			// [HR 0524] Текущее spanZero
			// Доступ: R
			case 524: regs[i] = spanZero;                                    break; // Текущий spanZero.

			// [HR 0525] Разность нормализованных каналов (SUBSTRACTED[delta])
			// Доступ: R
			case 525: regs[i] = SUBSTRACTED[0 /* delta */];                  break; // Разность нормализованных каналов.

			// [HR 0526] Дублированная оценка delta (OUTSIGNAL[delta])
			// Доступ: R
			case 526: regs[i] = OUTSIGNAL[0 /* delta */];                    break; // Дублированная концентрация (delta).

			// [HR 0527] Дублированные биты воздействий/состояний (impactByte)
			// Доступ: R
			case 527: regs[i] = impactByte;                                  break; // Дублированные биты воздействий.

			// [HR 0528] Модуль градиента температуры (abs(DERIVATIVE[tempPyro]))
			// Доступ: R
			case 528: regs[i] = abs(DERIVATIVE[2 /* tempPyro */]);           break; // Модуль градиента температуры.

			// [HR 0551..0566] Фрагмент буфера тренда (TREND[ref2][...])
			// Доступ: R
			case 551 ... 566: regs[i] = TREND[1 /* ref2 */][registerAddr - 551U]; break; // Тренд канала.

			// =========================================================================
			// Сервисный режим / защищённые операции
			// =========================================================================

			// [HR 0580] Ключ сервисного режима
			// Доступ: W (WRITE ONLY)
			// Условие: запись значения kServiceModeKey (20118) → serviceMode=1
			// Назначение: разрешить запись защищённых сервисных регистров (590, 592, 593, ...)
			case 580: // Сервисный ключ: активация режима записи (WRITE).
				if (writeCmd && static_cast<uint16_t>(Value_) == kServiceModeKey)
				{
					serviceMode = 1;
					replyDone();
				}
				regs[i] = serviceMode;
				break;

			// [HR 0590] Сервис: серийный номер
			// Доступ: R/W*  (* запись только при serviceMode=1)
			// Хранение: EEPROM EE_SERIAL_NO
			case 590: // Сервис: прошивка серийного номера (R/W при serviceMode).
				if (serviceMode && writeCmd)
				{
					EEW(EE_SERIAL_NO, static_cast<uint16_t>(Value_));
					replyDone();
				}
				regs[i] = static_cast<int16_t>(EER(EE_SERIAL_NO));
				break;

			// [HR 0592] Сервис: год (u8)
			// Доступ: R/W*  (* запись только при serviceMode=1)
			// Хранение: EEPROM EE_YEAR
			case 592: // Сервис: год (u8) EEPROM[EE_YEAR] (R/W при serviceMode).
				if (serviceMode && writeCmd)
				{
					EEWB(EE_YEAR, static_cast<uint8_t>(Value_));
					replyDone();
				}
				regs[i] = static_cast<int16_t>(EERB(EE_YEAR));
				break;

			// [HR 0593] Сервис: месяц (1..12, u8)
			// Доступ: R/W*  (* запись только при serviceMode=1)
			// Валидация: clamp 1..12
			// Хранение: EEPROM EE_MONTH
			case 593: // Сервис: месяц (1..12, u8) EEPROM[EE_MONTH] (R/W при serviceMode).
				if (serviceMode && writeCmd)
				{
					EEWB(EE_MONTH, clamp_u8(static_cast<uint8_t>(Value_), 1U, 12U));
					replyDone();
				}
				regs[i] = static_cast<int16_t>(EERB(EE_MONTH));
				break;

			// [HR 0594] Команда: программная перезагрузка
			// Доступ: W (WRITE ONLY)
			// Действие: reboot=1; replyDone(); ответ 0
			case 594: // Команда: перезагрузка устройства (WRITE).
				if (writeCmd)
				{
					reboot = 1;
					replyDone();
				}
				regs[i] = 0;
				break;

			// [HR 0595] Нагрев: нижний порог (lowerHeatLimit)
			// Доступ: R/W
			// Хранение: RAM lowerHeatLimit + EEPROM EE_LOWER_HEAT
			case 595: // Гистерезис нагревателя: нижний порог (R/W).
				if (writeCmd)
				{
					lowerHeatLimit = static_cast<uint16_t>(Value_);
					EEW(EE_LOWER_HEAT, lowerHeatLimit);
					replyDone();
				}
				regs[i] = static_cast<int16_t>(lowerHeatLimit);
				break;

			// [HR 0596] Нагрев: верхний порог (upperHeatLimit)
			// Доступ: R/W
			// Хранение: RAM upperHeatLimit + EEPROM EE_UPPER_HEAT
			case 596: // Гистерезис нагревателя: верхний порог (R/W).
				if (writeCmd)
				{
					upperHeatLimit = static_cast<uint16_t>(Value_);
					EEW(EE_UPPER_HEAT, upperHeatLimit);
					replyDone();
				}
				regs[i] = static_cast<int16_t>(upperHeatLimit);
				break;

			// [HR 0597..0598] Нагрев: чтение порогов гистерезиса
			// Доступ: R
			case 597: regs[i] = static_cast<int16_t>(lowerHeatLimit); break; // Гистерезис: нижний (READ).
			case 598: regs[i] = static_cast<int16_t>(upperHeatLimit); break; // Гистерезис: верхний (READ).

			// [HR 0601..0607] Коэффициенты b[]
			// Доступ: R
			case 601 ... 607: regs[i] = b[registerAddr - 600U]; break; // b[i] (READ).

			// [HR 0611..0617] Коэффициенты k[]
			// Доступ: R
			case 611 ... 617: regs[i] = k[registerAddr - 610U]; break; // k[i] (READ).

			// [HR 0696] Команда: сброс признака ADC_Alarm
			// Доступ: W (WRITE ONLY)
			// Действие: ADC_Alarm=0; replyDone(); ответ: ADC_Alarm
			case 696: // Команда: сброс флага ADC_Alarm (WRITE).
				if (writeCmd)
				{
					ADC_Alarm = 0;
					replyDone();
				}
				regs[i] = ADC_Alarm;
				break;

			// [HR 0697] Счётчик watchdog (watchdogCNT)
			// Доступ: R
			case 697: // Watchdog counter (READ).
				regs[i] = watchdogCNT;
				break;

			// [HR 0698] Тип потенциометра (potType)
			// Доступ: R/W
			// Хранение: RAM potType + EEPROM EE_POT_TYPE
			// Применение: через reboot (reboot=1)
			case 698: // Тип потенциометра (R/W) + reboot.
				if (writeCmd)
				{
					potType = static_cast<uint8_t>(Value_);
					EEWB(EE_POT_TYPE, potType);
					reboot = 1;
					replyDone();
				}
				regs[i] = potType;
				break;

			// [HR 0699] Реверс AI (reverseAI), 0/1
			// Доступ: R/W
			// Хранение: RAM reverseAI + EEPROM EE_REVERSE_AI
			case 699: // Реверс AI (0/1) (R/W).
				if (writeCmd)
				{
					reverseAI = static_cast<uint8_t>(Value_);
					EEWB(EE_REVERSE_AI, reverseAI);
					replyDone();
				}
				regs[i] = reverseAI;
				break;

			// [HR 0700] Целевое положение потенциометра (potTarget)
			// Доступ: R/W
			// Вход: u8 (Value_)
			// Действие при записи:
			//  - если potType==hc75Pot: potTarget = (Value_|0x0001), запись EEPROM EE_POT_TARGET_B;
			//  - иначе: potTarget = Value_, runPot=1, запись EEPROM EE_POT_TARGET_B;
			//  - replyDone().
			case 700: // Целевое положение потенциометра (R/W).
				if (writeCmd)
				{
					const uint8_t v = static_cast<uint8_t>(Value_);
					if (potType == hc75Pot)
					{
						// Для HC75 используется формат с обязательным младшим битом.
						potTarget = static_cast<uint16_t>(v) | 0x0001U;
						EEWB(EE_POT_TARGET_B, v);
					}
					else
					{
						potTarget = static_cast<uint16_t>(v);
						runPot = 1;
						EEWB(EE_POT_TARGET_B, v);
					}
					replyDone();
				}
				regs[i] = static_cast<int16_t>(potTarget);
				break;

			// [HR 0800..0803] Максимумы/минимумы сигналов REF/GAS (fullRAW[])
			// Доступ: R
			case 800 ... 803: // Максимумы/минимумы сигналов REF/GAS (READ).
				regs[i] = fullRAW[registerAddr - 800U];
				break;

			// [HR 0900] Команда: переход в бутлоадер
			// Доступ: W (WRITE ONLY)
			// Действие: cli(); GoToBootloader(); (возврат не предполагается)
			case 900: // Команда: переход в бутлоадер (WRITE).
				if (writeCmd)
				{
					cli();               // аппаратный запрет прерываний (AVR)
					GoToBootloader();     // точка входа задаётся линковкой/бутлоадером
				}
				regs[i] = 0;
				break;

			// =========================================================================
			// Паспортные / сервисные регистры 9000..9025: прямой доступ к EEPROM
			// =========================================================================
			// Примечание по формату:
			// - В большинстве случаев поля трактуются как u16.
			// - Для некоторых полей используется “бит-в-бит” перенос s16 через приведение (см. 9002..9005, 9014..9015).
			// - Запись выполняется только при writeCmd; чтение — всегда из EEPROM.

			// [HR 9000] Паспорт: код газа (EEPROM EE_GAS_CODE)
			// Доступ: R/W
			case 9000:
				if (writeCmd) EEW(EE_GAS_CODE, static_cast<uint16_t>(Value_));
				regs[i] = static_cast<int16_t>(EER(EE_GAS_CODE));
				break;

			// [HR 9001] Паспорт: код единиц измерения (EEPROM EE_UNITS_CODE)
			// Доступ: R/W
			case 9001:
				if (writeCmd) EEW(EE_UNITS_CODE, static_cast<uint16_t>(Value_));
				regs[i] = static_cast<int16_t>(EER(EE_UNITS_CODE));
				break;

			// [HR 9002] Паспорт: нижняя граница диапазона (EEPROM EE_RANGE_MIN)
			// Доступ: R/W
			// Формат: s16 “бит-в-бит” (Value_ интерпретируется как int16_t, хранится как u16 без изменения бит)
			case 9002:
				if (writeCmd) EEW(EE_RANGE_MIN, static_cast<uint16_t>(static_cast<int16_t>(Value_)));
				regs[i] = static_cast<int16_t>(EER(EE_RANGE_MIN));
				break;

			// [HR 9003] Паспорт: верхняя граница диапазона (EEPROM EE_RANGE_MAX)
			// Доступ: R/W
			// Формат: s16 “бит-в-бит”
			case 9003:
				if (writeCmd) EEW(EE_RANGE_MAX, static_cast<uint16_t>(static_cast<int16_t>(Value_)));
				regs[i] = static_cast<int16_t>(EER(EE_RANGE_MAX));
				break;

			// [HR 9004] Паспорт: Tmin, °C (EEPROM EE_TMIN_C)
			// Доступ: R/W
			// Формат: s16 “бит-в-бит”
			case 9004:
				if (writeCmd) EEW(EE_TMIN_C, static_cast<uint16_t>(static_cast<int16_t>(Value_)));
				regs[i] = static_cast<int16_t>(EER(EE_TMIN_C));
				break;

			// [HR 9005] Паспорт: Tmax, °C (EEPROM EE_TMAX_C)
			// Доступ: R/W
			// Формат: s16 “бит-в-бит”
			case 9005:
				if (writeCmd) EEW(EE_TMAX_C, static_cast<uint16_t>(static_cast<int16_t>(Value_)));
				regs[i] = static_cast<int16_t>(EER(EE_TMAX_C));
				break;

			// [HR 9006] Паспорт: Vnom (0.1V) (EEPROM EE_VNOM_01V)
			// Доступ: R/W
			case 9006:
				if (writeCmd) EEW(EE_VNOM_01V, static_cast<uint16_t>(Value_));
				regs[i] = static_cast<int16_t>(EER(EE_VNOM_01V));
				break;

			// [HR 9007] Паспорт: гарантийный срок/месяцы (EEPROM EE_WARRANTY_MON)
			// Доступ: R/W
			case 9007:
				if (writeCmd) EEW(EE_WARRANTY_MON, static_cast<uint16_t>(Value_));
				regs[i] = static_cast<int16_t>(EER(EE_WARRANTY_MON));
				break;

			// [HR 9008] Паспорт: ревизия аппаратуры (EEPROM EE_HW_REV)
			// Доступ: R/W
			case 9008:
				if (writeCmd) EEW(EE_HW_REV, static_cast<uint16_t>(Value_));
				regs[i] = static_cast<int16_t>(EER(EE_HW_REV));
				break;

			// [HR 9009] Паспорт: runtime low (EEPROM EE_RUNTIME_LO)
			// Доступ: R/W
			case 9009:
				if (writeCmd) EEW(EE_RUNTIME_LO, static_cast<uint16_t>(Value_));
				regs[i] = static_cast<int16_t>(EER(EE_RUNTIME_LO));
				break;

			// [HR 9010] Паспорт: runtime high (EEPROM EE_RUNTIME_HI)
			// Доступ: R/W
			case 9010:
				if (writeCmd) EEW(EE_RUNTIME_HI, static_cast<uint16_t>(Value_));
				regs[i] = static_cast<int16_t>(EER(EE_RUNTIME_HI));
				break;

			// [HR 9011] Паспорт: число включений питания (EEPROM EE_POWER_CYCLES)
			// Доступ: R/W
			case 9011:
				if (writeCmd) EEW(EE_POWER_CYCLES, static_cast<uint16_t>(Value_));
				regs[i] = static_cast<int16_t>(EER(EE_POWER_CYCLES));
				break;

			// [HR 9012] Паспорт: год обслуживания (EEPROM EE_SVC_YEAR)
			// Доступ: R/W
			case 9012:
				if (writeCmd) EEW(EE_SVC_YEAR, static_cast<uint16_t>(Value_));
				regs[i] = static_cast<int16_t>(EER(EE_SVC_YEAR));
				break;

			// [HR 9013] Паспорт: месяц обслуживания (1..12) (EEPROM EE_SVC_MONTH)
			// Доступ: R/W
			// Валидация: clamp 1..12
			case 9013:
				if (writeCmd) EEW(EE_SVC_MONTH, clamp_u16(static_cast<uint16_t>(Value_), 1U, 12U));
				regs[i] = static_cast<int16_t>(EER(EE_SVC_MONTH));
				break;

			// [HR 9014] Паспорт: Alarm1 (EEPROM EE_ALARM1)
			// Доступ: R/W
			// Формат: s16 “бит-в-бит”
			case 9014:
				if (writeCmd) EEW(EE_ALARM1, static_cast<uint16_t>(static_cast<int16_t>(Value_)));
				regs[i] = static_cast<int16_t>(EER(EE_ALARM1));
				break;

			// [HR 9015] Паспорт: Alarm2 (EEPROM EE_ALARM2)
			// Доступ: R/W
			// Формат: s16 “бит-в-бит”
			case 9015:
				if (writeCmd) EEW(EE_ALARM2, static_cast<uint16_t>(static_cast<int16_t>(Value_)));
				regs[i] = static_cast<int16_t>(EER(EE_ALARM2));
				break;

			// [HR 9016] Паспорт: option flags (EEPROM EE_OPTION_FLAGS)
			// Доступ: R/W
			case 9016:
				if (writeCmd) EEW(EE_OPTION_FLAGS, static_cast<uint16_t>(Value_));
				regs[i] = static_cast<int16_t>(EER(EE_OPTION_FLAGS));
				break;

			// [HR 9017] Паспорт: variant code (EEPROM EE_VARIANT_CODE)
			// Доступ: R/W
			case 9017:
				if (writeCmd) EEW(EE_VARIANT_CODE, static_cast<uint16_t>(Value_));
				regs[i] = static_cast<int16_t>(EER(EE_VARIANT_CODE));
				break;

			// [HR 9018] Паспорт: last error (EEPROM EE_LAST_ERROR)
			// Доступ: R/W
			case 9018:
				if (writeCmd) EEW(EE_LAST_ERROR, static_cast<uint16_t>(Value_));
				regs[i] = static_cast<int16_t>(EER(EE_LAST_ERROR));
				break;

			// [HR 9019] Паспорт: error count (EEPROM EE_ERROR_COUNT)
			// Доступ: R/W
			case 9019:
				if (writeCmd) EEW(EE_ERROR_COUNT, static_cast<uint16_t>(Value_));
				regs[i] = static_cast<int16_t>(EER(EE_ERROR_COUNT));
				break;

			// [HR 9020] Паспорт: год следующей поверки (EEPROM EE_NEXTVER_YEAR)
			// Доступ: R/W
			case 9020:
				if (writeCmd) EEW(EE_NEXTVER_YEAR, static_cast<uint16_t>(Value_));
				regs[i] = static_cast<int16_t>(EER(EE_NEXTVER_YEAR));
				break;

			// [HR 9021] Паспорт: месяц следующей поверки (1..12) (EEPROM EE_NEXTVER_MONTH)
			// Доступ: R/W
			// Валидация: clamp 1..12
			case 9021:
				if (writeCmd) EEW(EE_NEXTVER_MONTH, clamp_u16(static_cast<uint16_t>(Value_), 1U, 12U));
				regs[i] = static_cast<int16_t>(EER(EE_NEXTVER_MONTH));
				break;

			// [HR 9022] Паспорт: тип сенсора (EEPROM EE_SENSOR_TYPE)
			// Доступ: R/W
			case 9022:
				if (writeCmd) EEW(EE_SENSOR_TYPE, static_cast<uint16_t>(Value_));
				regs[i] = static_cast<int16_t>(EER(EE_SENSOR_TYPE));
				break;

			// [HR 9023] Паспорт: код страны (EEPROM EE_COUNTRY_CODE)
			// Доступ: R/W
			case 9023:
				if (writeCmd) EEW(EE_COUNTRY_CODE, static_cast<uint16_t>(Value_));
				regs[i] = static_cast<int16_t>(EER(EE_COUNTRY_CODE));
				break;

			// [HR 9024] Паспорт: CRC информационного блока (EEPROM EE_INFO_CRC)
			// Доступ: R/W
			case 9024:
				if (writeCmd) EEW(EE_INFO_CRC, static_cast<uint16_t>(Value_));
				regs[i] = static_cast<int16_t>(EER(EE_INFO_CRC));
				break;

			// [HR 9025] Паспорт: резерв (EEPROM EE_RESERVED_69)
			// Доступ: R/W
			case 9025:
				if (writeCmd) EEW(EE_RESERVED_69, static_cast<uint16_t>(Value_));
				regs[i] = static_cast<int16_t>(EER(EE_RESERVED_69));
				break;

			// [HR ----] Неизвестный регистр
			// Доступ: R (диагностический ответ)
			// Ответ: -1
			default:
				// Неизвестный регистр: возвращаем диагностическое значение.
				regs[i] = -1;
				break;
		}
	}
}
