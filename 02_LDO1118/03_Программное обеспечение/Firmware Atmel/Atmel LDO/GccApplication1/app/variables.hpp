/**
 * @file variables.hpp
 * @brief Объявления глобальных переменных проекта и общих идентификаторов.
 *
 * @details
 * Заголовок содержит extern-объявления глобального состояния, определённого в variables.cpp.
 * Также здесь размещены проектные перечисления (индексы выходов, режимы, тип потенциометра).
 *
 * Требования к использованию:
 * - Не добавлять сюда определений (кроме inline/constexpr), чтобы исключить множественную линковку.
 * - Для массивов предпочтительно фиксировать размер (вместо "extern T a[]"), чтобы компилятор
 *   мог выполнять проверку индексации и типизации.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * Базовые типы проекта
 * ============================================================================ */

#ifndef byte
/**
 * @brief Проектный алиас для байта.
 *
 * @details
 * Используется для совместимости с историческим кодом. В новом коде предпочтительно
 * применять uint8_t напрямую.
 */
typedef uint8_t byte;
#endif

/* ============================================================================
 * Вперёд-объявления классов (определения и экземпляры — в variables.cpp)
 * ============================================================================ */

class IGAS_mb_485;
class IGAS_display_LED;
class IGAS_Eeprom_Class;

/* ============================================================================
 * Экземпляры подсистем
 * ============================================================================ */

extern IGAS_mb_485        IGAS_mb_X;
extern IGAS_display_LED   IGAS_disp;
extern IGAS_Eeprom_Class  IGAS_EEPROM;

/* ============================================================================
 * Идентификация/версия
 * ============================================================================ */

/** @brief Идентификатор/версия прошивки (проектный формат). */
extern uint16_t firmwareDef;

/* ============================================================================
 * Общие константы проекта
 * ============================================================================ */

extern const uint16_t WarmingUpTime;
extern const int16_t  IGASonstart;
extern const byte     heartBit;
extern const byte     EEPROM_Start_Adress[8];

/* ============================================================================
 * Битовые позиции ошибок/состояний
 * ============================================================================ */

extern const byte bitFailCh1;
extern const byte bitFailCh2;
extern const byte bitIRFault;
extern const byte EEPROMFail;
extern const byte bitCleanWindow;
extern const byte heaterFail;
extern const byte internalBrake;
extern const byte T_calibr_Fail;
extern const byte bitOptics;

/* ============================================================================
 * Пины управления
 * ============================================================================ */

extern const byte lampDrive;
extern const byte irLedDrive;
extern const byte ledPin;
extern const byte heaterPin;
extern const byte displayEnbl;

extern const byte PIN_POT_CS;
extern const byte PIN_POT_UD;
extern const byte PIN_DATA;

/* ============================================================================
 * RS-485 / связь
 * ============================================================================ */

extern uint16_t COMM_BPS;
extern const byte txEnPin;
extern byte resetTransmitLED;

extern const byte turnOn;
extern const byte switchOff;
extern const byte breakerOff;
extern const byte breakerOn;

/* ============================================================================
 * Рабочие/сервисные флаги и переменные
 * ============================================================================ */

extern byte    heaterSwitch;
extern byte    lastError;
extern int16_t stateByte;

extern byte changeStatusMsg;
extern byte blinkIt;

extern int16_t blockErrorByte;
extern byte    zeroShift;
extern byte    skipStep;
extern byte    settingsFlag;

extern byte    dirtFlag;
extern int16_t refCornerBackUp;
extern byte    heaterTuneStatus;

/* ============================================================================
 * Калибровка: y = k(x - b)
 * ============================================================================ */

extern int16_t oldZero;
extern int16_t oldTopLimitX;
extern int16_t oldTopLimitY;

extern int16_t b[9];
extern int16_t gasConcentrationY[9];
extern int16_t sensorAnalogX[9];
extern float   k[9];

extern byte serviceMode;
extern byte getCurve;

extern byte i_i;

extern uint8_t  ADC_Alarm;
extern uint16_t watchdogCNT;

/* ============================================================================
 * Сообщения/индикация
 * ============================================================================ */

extern byte msgGaugeWrmUp;
extern byte msgFactoryDefault;
extern byte msgSignalBelowLvl;
extern byte msgEEPROMError;
extern byte msgRespondOk;
extern byte wirenEnableMSG;
extern byte msgDirtyGlass;
extern byte msgWirenError;
extern byte msgMagnetZero;
extern byte msgMagnetSpan;

/* ============================================================================
 * Диагностика/сырые данные
 * ============================================================================ */

extern int16_t fullRAW[4];

extern byte RelayCoil_1;
extern byte RelayCoil_2;

extern byte magnetOption;
extern byte startMagnetDelay;

extern int16_t spanZero;

/* ============================================================================
 * Таймеры/время
 * ============================================================================ */

extern unsigned long msgStartTime;
extern unsigned long startSmoothTimer;
extern unsigned long magnetTimer;
extern unsigned int  magnetTimeOut;
extern unsigned long steadyTimer;
extern unsigned long steadyWdt;
extern unsigned long spanTimer;

extern uint16_t smoothDelay;

extern unsigned long msgWarmUpStartTime;
extern unsigned long startTaskTime;
extern unsigned long starttimer10;
extern unsigned long startTimer2;
extern unsigned long startTimerSec;

/* ============================================================================
 * Градиенты/агрегация/устойчивость
 * ============================================================================ */

extern byte gradAggr[4];
extern byte gradSmooth[4];
extern byte arrgCnt[4];

extern int16_t newPoint;
extern byte    currentPoint;
extern int16_t hotTemp;
extern int16_t tempDiff;
extern unsigned long startDelayTime;
extern uint16_t wirenTimerSP;
extern uint8_t bitRate;

extern bool breakerStatus;

extern int16_t upperHeatLimit;
extern int16_t lowerHeatLimit;

/* ============================================================================
 * Индексы выходов (outputSignals[])
 * ============================================================================ */

/**
 * @brief Индексы элементов массива outputSignals[].
 * @details Значения используются в логике Modbus/индикации.
 */
typedef enum OutputSignalIndex_e
{
    concentration = 0,  /**< Текущее значение концентрации. */
    currentState  = 1,  /**< Текущее состояние/код режима. */
    smooth        = 2,  /**< Сглаженное значение (если применяется). */
    oSLength      = 3   /**< Размер массива outputSignals[]. */
} OutputSignalIndex_t;

/** @brief Выходные параметры, доступные интерфейсам. */
extern int16_t outputSignals[oSLength];

/**
 * @brief EEPROM-адрес флага «устройство настроено».
 * @details Исторический макрос. Предпочтительно перенести в eeprom_map.h при консолидации карты.
 */
#define EE_SETTINGS_FLAG 100u

/* ============================================================================
 * TREND/каналы
 * ============================================================================ */

extern byte tail;
extern const byte trendLimit;

/** @brief Количество каналов измерения; фиксировано в variables.cpp. */
extern const byte totalChannels;

/* Индексы каналов */
extern const byte pyro1;
extern const byte delta;
extern const byte ref2;
extern const byte tempPyro;
extern const byte tempHeater;

/* ============================================================================
 * Буферы измерений/обработки
 * ============================================================================ */

extern int16_t RAW[4];
extern int16_t AVERAGED[4];
extern int16_t NORMALIZED[3];

extern float   normKoefPir1;
extern float   normKoefRef;

extern float   PIDFILTER[3];
extern int16_t GAPFILTERED[3];
extern int16_t STRIKE_H[3];
extern int16_t STRIKE_L[3];

extern int16_t SUBSTRACTED[4];
extern int16_t OUTSIGNAL[4];

extern int16_t averageBuffer[4][10];

extern int16_t cornerSTONE[4];
extern int16_t TREND[3][41];

extern const float SMOOTHfilter[4];
extern const float AGGRESSfilter[4];

/* ============================================================================
 * Периоды тикеров
 * ============================================================================ */

extern const byte     timer10delay;
extern const byte     timer2delay;
extern const uint16_t timerSecDelay;

/* ============================================================================
 * Время/состояния
 * ============================================================================ */

extern byte year;
extern byte month;
extern byte normalMode;

extern int16_t channel1MaxValue;
extern int16_t channel2MaxValue;
extern int16_t channel1MinValue;
extern int16_t channel2MinValue;

extern byte reverseAI;
extern byte averageCounter;
extern byte newDataArrived;

extern unsigned long startSampleTime;
extern uint16_t      period;
extern unsigned long IRlampStartTime;
extern uint16_t      IRTime;
extern uint16_t      sampleTime;

extern int16_t  stableRef;
extern const int16_t toleranceRefTime;
extern unsigned long refStabTimer;
extern byte     refStable;
extern byte     stableGate;

extern int16_t DERIVATIVE[4];

extern byte gain;

/** @brief Флаг запроса программной перезагрузки (устанавливается обработчиками команд). */
extern volatile uint8_t reboot;

extern float shiftCompensKoef;

/* ============================================================================
 * Статусы/флаги Modbus и измерения
 * ============================================================================ */

extern byte impactByte;
extern byte slaveAddress;
extern int16_t lowSignal;
extern int16_t cleanSetPoint;

extern unsigned long currentTime;

extern byte toolBoxActive;
extern const int16_t limitSF6;
extern const int16_t limitCH4;
extern int16_t gasLimit;

extern byte thermoShock;
extern byte startShock;
extern byte gasDetected;
extern byte autoZero;
extern byte validatedSilence;

extern unsigned long negativeGasTime;
extern byte          negativeGas;

extern byte  gasChannelFlag;
extern byte  refChannelFlag;
extern int16_t refNoiseSP;
extern int16_t gasNoiseSP;
extern uint16_t cornerDrvtive;

extern byte stabFlagCntSP;
extern byte mechanicalDriftSP;

extern byte stabIdleCnt;
extern byte stabActiveCnt;

extern byte gasActiveCnt;
extern byte refActiveCnt;
extern byte gasFlagCnt;

extern byte gasDetSP;
extern byte stablePirSP;
extern byte gasFlagCntSP;
extern byte thermoCounter;
extern byte thermoGradientSP;

/* ============================================================================
 * Потенциометр
 * ============================================================================ */

extern uint8_t currentPot;
extern uint8_t runPot;
extern uint8_t potTarget;

/* Таймерные счётчики (тикеры 10 мс/2 мс/1 с) */
extern byte timer10;
extern byte timer2;
extern byte timerSec;

/* ============================================================================
 * Биты/коды состояния
 * ============================================================================ */

/**
 * @brief Битовые коды состояния (проектный формат).
 * @details Значения используются при формировании stateByte/impactByte и аналогичных полей.
 */
typedef enum StateBits_e
{
    idle   = 0b00000000,
    steady = 0b00000001,
    active = 0b00000010
} StateBits_t;

/* ============================================================================
 * Тип потенциометра
 * ============================================================================ */

/**
 * @brief Тип установленного потенциометра (аппаратная модификация).
 * @details Определяет алгоритм управления (например, кодирование potTarget).
 */
typedef enum PotType_e
{
    hc75Pot = 0,
    mcpPot  = 1
} PotType_t;

/** @brief Текущий выбранный тип потенциометра (см. PotType_t). */
extern uint8_t potType;

/**
 * @page config_parameters Конфигурационные параметры и статусные переменные
 *
 * @brief Сводная карта параметров, доступных через Modbus и/или EEPROM, а также ключевых RAM-статусов.
 *
 * @details
 * Страница фиксирует «внешний контракт» прошивки:
 * - какие параметры считаются настройками (меняются извне и/или сохраняются в EEPROM);
 * - какие значения являются константами сборки/алгоритма (зафиксированы в прошивке);
 * - какие поля являются статусами/диагностикой (живут в RAM и отражают текущее состояние).
 *
 * Принятый словарь (единые термины документации):
 * - REF: «опорный канал»
 * - GAS: «газовый канал»
 * - delta / OUTSIGNAL[delta] / SUBSTRACTED[delta]: «Δ (разность GAS–REF)»
 * - IRTime: «длительность импульса ИК-источника»
 * - sampleTime: «период семплирования»
 * - lowSignal: «порог аварийно низкого сигнала»
 * - cleanSetPoint: «порог недопустимого загрязнения оптики»
 * - stableGate / stablePirSP / refNoiseSP / gasNoiseSP: «порог …»
 * - lowerHeatLimit / upperHeatLimit: «температура включения/выключения нагрева», единицы: 0.1 °C
 * - bitRate: «скорость RS-485» (в прошивке хранится/передаётся как u8-код/индекс)
 * - COMM_BPS: «скорость по умолчанию, бод/с»
 *
 * Обозначения:
 * - HR xxxx — Holding Register Modbus с адресацией как в коде (0-based). В ряде SCADA адрес может
 *   отображаться как 40001+xxxx — это «представление мастера», а не адрес в прошивке.
 * - EEPROM — байтовый адрес из eeprom_map.h (макрос EE_*).
 * - Доступ: R / W / R/W. Для W (write-only) при чтении обычно возвращается 0 или текущее значение (см. обработчик).
 * - Применение: «сразу» — действует немедленно; «через reboot» — требует перезагрузки (устанавливается \ref reboot).
 *
 * Связанные модули:
 * - Modbus обработчик регистров: modbusProceed() (modbus_regs.cpp)
 * - Карта EEPROM: eeprom_map.h
 * - Глобальные переменные: variables.hpp / variables.cpp
 *
 * ---------------------------------------------------------------------------
 * @section cp_settings Настройки (изменяемые извне; Modbus и/или EEPROM)
 * ---------------------------------------------------------------------------
 *
 * Таблица включает параметры, которые:
 * - имеют явный регистр Modbus, и/или
 * - сохраняются в EEPROM (контракт совместимости прошивки), и/или
 * - критичны для конфигурации изделия/сервиса.
 *
 * | Параметр (символ) | Тип / единицы | Значение по умолчанию | Modbus HR | Доступ | EEPROM | Применение / комментарий |
 * |---|---:|---:|---:|:---:|---:|---|
 * | \ref slaveAddress | u8 (1..247) | задаётся инициализацией | 11 | R/W | \ref EE_SLAVE_ADDR | Применяется сразу: обновляется IGAS_mb_X.slave; запись фиксируется в EEPROM. |
 * | \ref bitRate | u8 (код/индекс скорости RS-485) | задаётся инициализацией | 102 | R/W | \ref EE_BITRATE | Изменение скорости RS-485 требует перезапуска коммуникаций: выставляется \ref reboot=1. |
 * | \ref lowSignal | u16 (порог) | задаётся инициализацией | 12 | R/W | \ref EE_LOW_SIGNAL | «Порог аварийно низкого сигнала». В коде объявлен как s16, но трактуется как u16-порог. |
 * | \ref cleanSetPoint | u16 (порог) | задаётся инициализацией | 78 | R/W | \ref EE_CLEAN_SETPOINT | «Порог недопустимого загрязнения оптики». |
 * | \ref zeroShift | u8 | задаётся инициализацией | 88 | R/W | \ref EE_ZERO_SHIFT | «Поправка нуля». В обработчике Modbus читается/пишется напрямую из EEPROM. |
 * | Маска блокировок/ошибок | u16 (битовая маска) | — | 50 | R/W | \ref EE_BLOCK_ERROR | HR49 — чтение; HR50 — чтение/запись. Оперативная копия: \ref blockErrorByte (синхронизация — прикладной логикой). |
 * | \ref heaterSwitch | u8 (0/1) | задаётся инициализацией | 297 | R/W | \ref EE_HEATER_SWITCH | HR207 — команда без сохранения; HR297 — с сохранением + вызов heaterBreaker() при изменении. |
 * | \ref period | u16, мс | задаётся инициализацией | 210 | R/W | \ref EE_PERIOD_MS | «Период измерительного цикла». HR209 — чтение текущего. |
 * | \ref IRTime | u16, мс | задаётся инициализацией | 214 | R/W | \ref EE_IRTIME_MS | «Длительность импульса ИК-источника». HR213 — чтение. |
 * | \ref sampleTime | u16, мс | задаётся инициализацией | 212 | R/W* | — | «Период семплирования». В текущем обработчике HR212 изменяет только RAM (без EEPROM). В карте EEPROM предусмотрен \ref EE_SAMPLE_TIME_B (исторический формат u8). |
 * | \ref magnetOption | u8 (0/1) | задаётся инициализацией | 216 | R/W | \ref EE_MAGNET_OPT | Опция магнитной процедуры/калибровки. HR215 — чтение. |
 * | \ref stablePirSP | u16 (порог) | задаётся инициализацией | 225 | R/W | \ref EE_STABLE_PIR_SP_B | «Порог стабилизации PIR при старте». EEPROM хранит u8 (исторический формат). HR220 — чтение (просмотр). |
 * | \ref refNoiseSP | u16 (порог) | задаётся инициализацией | 226 | R/W | \ref EE_REF_NOISE | «Порог шума опорного канала». HR221 — чтение (просмотр). |
 * | \ref gasNoiseSP | u16 (порог) | задаётся инициализацией | 227 | R/W | \ref EE_GAS_NOISE | «Порог шума газового канала». HR222 — чтение (просмотр). |
 * | \ref thermoGradientSP | u16 (порог), 0.1 °C | задаётся инициализацией | 228 | R/W | \ref EE_THERMO_GRAD_B | «Порог термоградиента». EEPROM хранит u8 (исторический формат). HR223 — чтение (просмотр). |
 * | \ref gain | u8 (0/1) | задаётся инициализацией | 299 | R/W | \ref EE_GAIN | При смене пересчитываются \ref lowerHeatLimit / \ref upperHeatLimit / \ref thermoGradientSP, меняется analogReference(), затем значения сохраняются в EEPROM. |
 * | \ref lowerHeatLimit | u16, 0.1 °C | задаётся инициализацией | 595 | R/W | \ref EE_LOWER_HEAT | «Температура включения нагрева». HR597 — чтение. |
 * | \ref upperHeatLimit | u16, 0.1 °C | задаётся инициализацией | 596 | R/W | \ref EE_UPPER_HEAT | «Температура выключения нагрева». HR598 — чтение. |
 * | \ref stableGate | u8 (порог) | задаётся инициализацией | 302 | R/W | \ref EE_STABLE_GATE | «Порог устойчивости» (логика валидизации). |
 * | \ref reverseAI | u8 (0/1) | задаётся инициализацией | 699 | R/W | \ref EE_REVERSE_AI | Реверс аналогового интерфейса. |
 * | \ref potType | u8 (enum \ref PotType_t) | задаётся инициализацией | 698 | R/W | \ref EE_POT_TYPE | Тип потенциометра — аппаратная модификация. Требует \ref reboot=1. |
 * | \ref potTarget | u8 (код) | задаётся инициализацией | 700 | R/W | \ref EE_POT_TARGET_B | Для hc75Pot принудительно выставляется младший бит; для mcpPot дополнительно \ref runPot=1. |
 * | Серийный номер (паспорт/сервис) | u16 | — | 39 / 590 | R / R/W* | \ref EE_SERIAL_NO | HR39 — чтение; HR590 — запись только при \ref serviceMode=1. |
 * | Год/месяц выпуска (сервис) | u8/u8 | — | 592 / 593 | R/W* | \ref EE_YEAR / \ref EE_MONTH | Запись только при \ref serviceMode=1; месяц валидируется 1..12. |
 * | Команда перезагрузки | — | — | 594 | W | — | Устанавливает \ref reboot=1. |
 * | Переход в бутлоадер | — | — | 900 | W | — | cli(); GoToBootloader(); возврат не предполагается. |
 *
 * Примечание по сервисному режиму:
 * - HR580: запись ключа 20118 → \ref serviceMode = 1 (разрешение записи защищённых регистров, например 590/592/593).
 *
 * ---------------------------------------------------------------------------
 * @section cp_calibration Калибровочные массивы и операции
 * ---------------------------------------------------------------------------
 *
 * Базовые массивы калибровки:
 * - \ref gasConcentrationY[0..7] — Y-точки (концентрация/единицы проекта)
 * - \ref sensorAnalogX[0..7]     — X-точки (ADC/сигнал в проектных единицах)
 *
 * Основные операции через Modbus:
 * - HR3      — «калибровка по точке (ноль)»: формирует sensorAnalogX[0] = OUTSIGNAL[Δ] + EEPROM(EE_ZERO_SHIFT) и сохраняет.
 * - HR4..9   — «калибровка по точке» 1..6 по текущему сигналу OUTSIGNAL[Δ] с записью Y=Value_.
 * - HR10     — «калибровка по точке» 7 по текущему сигналу OUTSIGNAL[Δ] с записью Y=Value_.
 * - HR20..27 — чтение X-точек.
 * - HR30..37 — чтение Y-точек.
 * - HR70..77 — прямая запись Y-точек (R/W).
 * - HR80..87 — прямая запись X-точек (R/W).
 * - HR90     — команда «сохранить все точки в EEPROM».
 *
 * Важно:
 * - Реальная политика сохранения реализована gasToEEPROM() и сохранена «как в исходной реализации»
 *   (включая запись соседних точек для HR4..9).
 *
 * ---------------------------------------------------------------------------
 * @section cp_build_consts Константы сборки / алгоритма (фиксированы в прошивке)
 * ---------------------------------------------------------------------------
 *
 * Эти значения задаются в variables.cpp и/или в коде инициализации и не предполагаются к изменению через Modbus
 * в текущей реализации (если не добавлять отдельные регистры в будущем).
 *
 * | Идентификатор | Тип | Где задаётся | Назначение |
 * |---|---:|---|---|
 * | \ref firmwareDef | u16 | variables.cpp | Идентификатор/версия прошивки (HR40 — чтение). |
 * | \ref WarmingUpTime | u16 | variables.cpp | Время прогрева до рабочего режима (проектная логика). |
 * | \ref trendLimit | u8 | variables.cpp | Длина буфера тренда TREND. |
 * | \ref totalChannels | u8 | variables.cpp | Количество каналов измерения (газовый/опорный/температуры). |
 * | \ref timer10delay / \ref timer2delay / \ref timerSecDelay | u8/u8/u16 | variables.cpp | Периоды тикеров 10 мс / 2 мс / 1 с. |
 * | \ref SMOOTHfilter[] / \ref AGGRESSfilter[] | float[4] | variables.cpp | Коэффициенты фильтрации по каналам. |
 * | Пины управления | u8 | variables.cpp | lampDrive/irLedDrive/ledPin/heaterPin/displayEnbl + PIN_POT_* (зависят от разводки платы). |
 * | Битовые позиции ошибок | u8 | variables.cpp | bitFailCh1..bitOptics — позиции в словах статуса/масках. |
 *
 * Алгоритмические уставки, не имеющие Modbus/EEPROM-контракта в текущем обработчике:
 * - \ref stabFlagCntSP, \ref mechanicalDriftSP, \ref gasDetSP, \ref gasFlagCntSP и др.
 * При необходимости их можно «легализовать» как настройки, добавив:
 * (1) Modbus HR, (2) адрес EEPROM, (3) правила валидации и политику применения.
 *
 * ---------------------------------------------------------------------------
 * @section cp_status Статусы и диагностические переменные (оперативные, RAM)
 * ---------------------------------------------------------------------------
 *
 * Эти поля отражают текущее состояние алгоритма/диагностики и обычно:
 * - не сохраняются в EEPROM;
 * - меняются только прошивкой;
 * - доступны через диагностические регистры Modbus (частично) либо используются локально.
 *
 * | Параметр | Тип | Назначение / источник |
 * |---|---:|---|
 * | \ref outputSignals[] | s16[oSLength] | Основные «выходы» для интерфейсов; HR0/1 читают concentration/currentState. |
 * | \ref impactByte | u8 | Биты воздействий/состояний (HR507/527). |
 * | \ref currentTime | u32, мс | Текущее время, обновляется системным таймером. |
 * | \ref RAW[] / \ref AVERAGED[] / \ref NORMALIZED[] | s16[] | Буферы измерений: сырые/усреднённые/нормализованные. |
 * | \ref PIDFILTER[] / \ref GAPFILTERED[] | float[] / s16[] | Промежуточные результаты фильтрации. |
 * | \ref TREND[][], \ref cornerSTONE[] | s16[][] | Буферы тренда и «угловые точки». |
 * | \ref ADC_Alarm | u8 | Диагностический флаг АЦП (HR696 — сброс). |
 * | \ref watchdogCNT | u16 | Счётчик watchdog (HR697 — чтение). |
 * | \ref reboot | volatile u8 | Запрос программной перезагрузки (устанавливается рядом регистров; должен иметь единственное определение). |
 * | \ref serviceMode | u8 | Флаг сервисного режима (HR580 — активация ключом). |
 * | \ref runPot / \ref currentPot | u8 | Процедура установки потенциометра: целевое/текущее/флаг запуска. |
 * | \ref spanZero | u16 (бит-в-бит s16) | «Плавающая поправка нуля»: вычисляется и сохраняется через HR15 (формат хранения s16 как word). |
 *
 * ---------------------------------------------------------------------------
 * @section cp_passport Паспортный блок EEPROM (Modbus HR 9000..9025)
 * ---------------------------------------------------------------------------
 *
 * Диапазон HR9000..9025 реализует прямой доступ к паспортному блоку EEPROM (адреса 500..550)
 * без дополнительных побочных действий (кроме записи в EEPROM при writeCmd).
 *
 * | Modbus HR | EEPROM | Поле | Тип/формат |
 * |---:|---:|---|---|
 * | 9000 | \ref EE_GAS_CODE | Код целевого газа | u16 |
 * | 9001 | \ref EE_UNITS_CODE | Код единиц измерения | u16 |
 * | 9002 | \ref EE_RANGE_MIN | Диапазон min | s16 (бит-в-бит) |
 * | 9003 | \ref EE_RANGE_MAX | Диапазон max/FS | s16 (бит-в-бит) |
 * | 9004 | \ref EE_TMIN_C | Tmin эксплуатации | s16 (бит-в-бит) |
 * | 9005 | \ref EE_TMAX_C | Tmax эксплуатации | s16 (бит-в-бит) |
 * | 9006 | \ref EE_VNOM_01V | Номинал питания (0.1 V) | u16 |
 * | 9007 | \ref EE_WARRANTY_MON | Гарантия (мес.) | u16 |
 * | 9008 | \ref EE_HW_REV | Ревизия аппаратуры | u16 |
 * | 9009 | \ref EE_RUNTIME_LO | Наработка LO (часы) | u16 |
 * | 9010 | \ref EE_RUNTIME_HI | Наработка HI (часы) | u16 |
 * | 9011 | \ref EE_POWER_CYCLES | Число включений питания | u16 |
 * | 9012 | \ref EE_SVC_YEAR | Год обслуживания | u16 |
 * | 9013 | \ref EE_SVC_MONTH | Месяц обслуживания | u16 (1..12) |
 * | 9014 | \ref EE_ALARM1 | Alarm1 | s16 (бит-в-бит) |
 * | 9015 | \ref EE_ALARM2 | Alarm2 | s16 (бит-в-бит) |
 * | 9016 | \ref EE_OPTION_FLAGS | Флаги опций | u16 |
 * | 9017 | \ref EE_VARIANT_CODE | Вариант исполнения | u16 |
 * | 9018 | \ref EE_LAST_ERROR | Последний код ошибки | u16 |
 * | 9019 | \ref EE_ERROR_COUNT | Счётчик ошибок | u16 |
 * | 9020 | \ref EE_NEXTVER_YEAR | Год следующей поверки | u16 |
 * | 9021 | \ref EE_NEXTVER_MONTH | Месяц следующей поверки | u16 (1..12) |
 * | 9022 | \ref EE_SENSOR_TYPE | Тип сенсора/модуля | u16 |
 * | 9023 | \ref EE_COUNTRY_CODE | Код страны | u16 |
 * | 9024 | \ref EE_INFO_CRC | CRC паспортного блока | u16 |
 * | 9025 | \ref EE_RESERVED_69 | Резерв | u16 |
 */
