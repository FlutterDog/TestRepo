// ToDo

// Кольцевой буффер записи дуги. Необходимо для засекания дуги при включении и отключении
// Изменить логику работы с концевиками. ожидать концевики не по времени заполнения буффера, а по времени прихода сигнала с концевика или
// по таймауту.
// Сделать 3 буффера ОВО


#include "main.h"
#include <avr/eeprom.h>
#include <util/atomic.h>
#include <stdint.h>
#include "osc_config.h"
#include <avr/pgmspace.h>

#define OSC_HDR_STATUS_OFFS  ((uint16_t)offsetof(osc_strike_header_t, status))

float firmVersion = 4.591;

// --- время/индексация ---
volatile uint32_t t0_ticks = 0;                // тик на входе в BUSY (опорная точка)
// Сколько тиков ISR на одну "точку" осциллограммы. Если 1 тик = 1 точка, то 1.
#define TPOINT_TICKS  (1u)
#define TICK_US   100u   // 1 тик ISR_microTimer = 100 мкс
// u16 buf_adc1[ADC_SMPLS] = {0};
// u16 buf_adc2[ADC_SMPLS] = {0};
// u16 buf_adc3[ADC_SMPLS] = {0};

// u16 buf_adc1[ADC_SMPLS] = {0};
// u16 buf_adc2[ADC_SMPLS] = {0};
// u16 buf_adc3[ADC_SMPLS] = {0};

// === Эмуляция АЦП: фаза на каждую из 3 фаз ===
static uint16_t emu_phase_acc_A = 0;
static uint16_t emu_phase_acc_B = 0;
static uint16_t emu_phase_acc_C = 0;

// ====== Параметры эмуляции АЦП (по желанию) ======
volatile uint8_t  GIS_Switched_Finished = 0;
volatile uint8_t  emulate_adc_wave = 0;   // 0=реальный АЦП, 1=синус из кода
// 12-битный «ноль» и амплитуда (подгони под свой АЦП: 2048 центр — пример)
#define ADC12_MID   2048
volatile uint16_t emu_Sine_amp   = 1800;      // амплитуда синуса (пик)
volatile uint16_t emu_phase_step  = EMU_PHASE_STEP_U16; // дефолтный шаг по частоте/дискрету

// ДОБАВИТЬ НОВЫЕ ПЕРЕМЕННЫЕ:
volatile uint16_t emu_start_phase_u16   = DEG_TO_U16PH(EMU_START_SINE_PHASE); // начальная фаза, 0..65535
volatile uint16_t emu_interphase_u16    = DEG_TO_U16PH(EMU_SINE_INTERPHASE_SHIFT);  // межфазный сдвиг, 0..65535


volatile uint8_t EMU_mode_Flag    = 0;   // 0 = железо, 1 = эмуляция
volatile uint8_t emu_Start_switching_Flag = 0;   // 0 = нет, 1 = выполнить коммутацию

volatile uint16_t arc_consistency_len = 6u;

/***********************  К О Н Ф И Г  И  С О С Т О Я Н И Е  (минимум)  ***********************/
/** @brief Периоды в тиках (1 тик = SAMPLE_PERIOD_US, обычно 100 мкс). Можешь переопределять. */
volatile uint16_t silenceTime_ticks        = 2000;   ///< окно тишины после завершения, по умолчанию 200 мс
volatile uint16_t contactMaxWaitTime_ticks = 3000;  ///< максимум ожидания контактов до SILENT, 300 мс
volatile uint16_t scopeMaxWaitTime_ticks   = 1500;  ///< максимум ожидания осциллографирования, 150 мс (≈ 400 сэмплов = 40 мс → с запасом)

/** @brief Таймер «тишины» (арминговый). */
static volatile uint32_t silence_t0      = 0;

/** @brief Вотчдог контактов: армирование/сработал/старт. Управляется из check_Input_States(). */
static volatile uint8_t  contact_wd_armed   = 0;
static volatile uint8_t  contact_wd_tripped = 0;  ///< TODO: аварийная обработка на твой вкус
static volatile uint32_t contact_wd_t0      = 0;

/** @brief Вотчдог осциллографирования: армирование/сработал/старт. Ведётся в ScopeCapture(). */
static volatile uint8_t  scope_wd_armed   = 0;
static volatile uint8_t  scope_wd_tripped = 0;    ///< TODO: аварийная обработка на твой вкус
static volatile uint32_t scope_wd_t0      = 0;

static inline uint8_t  clamp_odd_3_21(uint8_t v)  { if (v < 3) v = 3; if (!(v & 1)) v++; if (v > 21) v = 21; return v; }
static inline uint16_t clamp_u16(uint16_t v, uint16_t lo, uint16_t hi){ if(v<lo) v=lo; else if(v>hi) v=hi; return v; }
static inline int16_t  clamp_i16(int16_t v, int16_t lo, int16_t hi)  { if(v<lo) v=lo; else if(v>hi) v=hi; return v; }
static inline float    clamp_f(float v, float lo, float hi)          { if(v<lo) v=lo; else if(v>hi) v=hi; return v; }


uint8_t on_contact_lag_ticks[3];   /* лаг доп.контакта на ВКЛ, тики (по фазам) */
uint8_t off_contact_lag_ticks[3];  /* ранний уход доп.контакта на ВЫКЛ, тики (по фазам) */
/* ================== Настраиваемые параметры фильтров (RAM) ================== */
/**
* @brief Порог по модулю в «кодах АЦП» для деспайка и порог-персистентности.
* @details Если есть min_Arc_Current в амперах — заранее переведи в коды.
*/
volatile int16_t  g_noise_T_codes   = 40;    // ~2% от ±2048

/**
* @brief Минимальная длительность «настоящего» события (сэмплы, Ts=100 мкс).
*/
volatile uint16_t g_noise_L_min     = 8;     // 0.8 мс

/**
* @brief Hampel: размер окна (нечёт) и множитель порога (k·MAD).
*/
volatile uint8_t  g_hampel_W        = 9;     // 7..11 обычно
volatile float    g_hampel_k        = 3.0f;

/**
* @brief Ограничитель скорости изменения: |Δx| ≤ dv_max кодов/сэмпл.
*/
volatile int16_t  g_slew_dv_max     = 40;    // 30..60 для Fs=10 кГц, 50 Гц

/**
* @brief НЧ из режима MED3+LPF: постоянная времени τ в микросекундах.
*/
volatile uint16_t g_lpf_tau_us      = 700;   // 500..1000


#define EEADDR_ON_LAG_BASE   226u
#define EEADDR_OFF_LAG_BASE  229u
// helper для клампа 0..255
static inline uint8_t clamp_u16_to_u8(uint16_t v) { return (v > 255u) ? 255u : (uint8_t)v; }


/**
* @brief Глобальный статус «жизни коммутации» (верхнеуровневая FSM).
*/
typedef enum {
	standingBy  = 0,  ///< ждём первый факт начала коммутации на любой фазе
	startScope  = 1,  ///< первый факт зафиксирован — внешний код должен стартовать осциллографирование
	activeScope = 2,  ///< осциллографирование идёт (ставится внешним кодом)
	silentTime  = 3   ///< осциллографирование завершено, окно тишины от дребезга (ставится внешним кодом)
} scope_state_t;


volatile scope_state_t positionChanged = standingBy;

typedef enum {
	SS_STANDBY = 0,   // ждём появления АКТИВНОСТИ (!is_silent)
	SS_WAIT_SILENT,   // активность была; ждём тишину ИЛИ срабатывание таймаутов ожидания
	SS_INIT,          // одноразовый вход: фиксируем старт "окна тишины", сервис
	SS_RUNNING,       // идёт отсчёт silenceTime_ticks
	SS_DONE           // одноразовый вход: расчёты и возврат в STANDBY
} silence_state_t;

static silence_state_t s_state     = SS_STANDBY;
static uint32_t        s_t0_ticks  = 0;

uint8_t fullTimeOffWarnState = 0, fullTimeOffAlarmState = 0;

//	uint8_t ownTimeOnWarnState   = 0, ownTimeOnAlarmState   = 0;
uint8_t fullTimeOnWarnState  = 0, fullTimeOnAlarmState  = 0;



// Ты сам зовёшь это В МОМЕНТ ВХОДА В ОСЦИЛЛОГРАФИРОВАНИЕ (однократно).
static inline void activateSwitchFinTimer(void)
{
	// чтобы случайно не переармить, разрешим из STANDBY
	if (s_state == SS_STANDBY)
	s_state = SS_WAIT_SILENT;
}


// ====== Осциллограммы: константы разметки и формат ======



/* === Параметры теста (RW через регистры) === */
static volatile uint8_t  sine_test_enable    = 0;     // [2280] 0/1
static volatile uint16_t sine_test_amp       = 1024;  // [2281] 0..2047 (≈50% по умолчанию)
static volatile uint16_t sine_test_f_centi   = 5000;  // [2282] частота в Гц*100 (50.00 Гц)
static volatile uint16_t sine_test_phase_deg = 0;     // [2283] 0..359

/* === Диагностика шага (RO) === */
static volatile uint16_t sine_test_step_q16  = 0;     // [2286] фактический step (Q16)
static volatile uint16_t sine_test_Nsamp     = 0;     // [2287] расчетное число сэмплов на период
static volatile uint16_t sine_test_Ts_us     = SAMPLE_PERIOD_US; // [2285] для контроля Ts

static volatile  uint16_t wearStrikes = 0;

/** @brief Подавить обработку изменения состояний на одну итерацию после смены режима. */
//static uint8_t suppress_edges = 0;

/** @brief Флаг активного сценария эмуляции (file-scope). */
static volatile uint8_t emu_scenario_active = 0;
volatile uint32_t emu_contacts_t0_ticks = 0; ///< база времени сценария (в тиках)

// Размер данных одной секции: 3 фазы × N × 2 байта
#define STRIKE_DATA_BYTES   (3u * PHASE_SAMPLES_MAX * SAMPLE_BYTES_PER_PHASE)
#define STRIKE_SECTION_BYTES (FRAM_HDR_SIZE + STRIKE_DATA_BYTES)

// База массива осциллограмм в FRAM (НЕ пересекай с другими твоими областями!)
// Если у тебя занято начиная с 7500, поставь базу 0x0000 — как в примере:
#define FRAM_OSC_BASE                 0x0000u

// Смещение секции конкретного удара (0..2)
#define STRIKE_BASE_ADDR(k)   (uint16_t)(FRAM_OSC_BASE + (uint32_t)(k) * STRIKE_SECTION_BYTES)

// Смещения фаз внутри секции удара
#define PHASE_A_OFFSET                0u
#define PHASE_B_OFFSET   (PHASE_A_OFFSET + PHASE_SAMPLES_MAX * SAMPLE_BYTES_PER_PHASE)
#define PHASE_C_OFFSET   (PHASE_B_OFFSET + PHASE_SAMPLES_MAX * SAMPLE_BYTES_PER_PHASE)

// Помощник: смещение данных фазы внутри секции
static inline uint16_t phase_data_offset(uint8_t phase)
{
	switch (phase) {
		case PHASE_A: return PHASE_A_OFFSET;
		case PHASE_B: return PHASE_B_OFFSET;
		default:      return PHASE_C_OFFSET;
	}
}

// ====== Заголовок секции удара (без C++11 static_assert) ======
#define OSC_HDR_VERSION               1u
#define OSC_STAT_EMPTY       0u
#define OSC_STAT_RECORDING   1u
#define OSC_STAT_READY       2u
#define OSC_STAT_PROCESSING  3u
#define OSC_STAT_PROCESSED   4u



// Для контактов (в тиках ISR_microTimer):
volatile uint32_t emu_t0_ticks = 0;
volatile uint16_t emu_contact_start_Delay_ticks[phase_quantity] = { 0u, 5u, 10u };  ///< задержка старта фаз A/B/C
volatile uint16_t emu_contact_duration_ticks[phase_quantity]    = { 30u, 30u, 30u  }; ///< длительность переходной зоны
/** Пофазные задержки старта относительно общей базы (в тиках microtimer, 100 мкс/тик).
*  Если фаза не стартовала (метки нет) — кладём 0xFFFF как «нет данных». */
static volatile uint16_t g_phase_start_dt_ticks[phase_quantity] = { 0xFFFFu, 0xFFFFu, 0xFFFFu };
// Длительность горения дуги ПОСЛЕ начала разрыва контакта (в тиках) для A/B/C.
// 0 = дугу не гасим принудительно (идёт весь кадр синусом).
volatile uint16_t emu_arc_extinguish_ticks[phase_quantity] = {50,50,50};
// Для АЦП-эмуляции синуса (если включишь):

static inline uint16_t clamp_ticks_u16(uint16_t v)
{
	return (v > (uint16_t)PHASE_SAMPLES_MAX) ? (uint16_t)PHASE_SAMPLES_MAX : v;
}

/** 1) ВКЛ/ВЫКЛ эмуляции концевиков */
volatile uint8_t emulate_contacts = 0;


/** Компил-тайм проверка размера LUT */
#define STATIC_ASSERT(COND, MSG) typedef char static_assert_##MSG[(COND)?1:-1]


/**
* @brief Полная 12-битная LUT синуса на 256 точек: 0..4095, mid=2048.
*        Формула генерации: round((sin(2*pi*i/256)*0.5 + 0.5) * 4095)
*/
static const uint16_t sin_lut_256[256] PROGMEM = {
	2048,2098,2148,2198,2248,2298,2348,2398,2447,2496,2545,2594,2642,2690,2737,2784,
	2831,2877,2923,2968,3013,3057,3100,3143,3185,3226,3267,3307,3346,3385,3423,3459,
	3495,3530,3565,3598,3630,3662,3692,3722,3750,3777,3804,3829,3853,3876,3898,3919,
	3939,3958,3975,3992,4007,4021,4034,4045,4056,4065,4073,4080,4085,4089,4093,4094,
	4095,4094,4093,4089,4085,4080,4073,4065,4056,4045,4034,4021,4007,3992,3975,3958,
	3939,3919,3898,3876,3853,3829,3804,3777,3750,3722,3692,3662,3630,3598,3565,3530,
	3495,3459,3423,3385,3346,3307,3267,3226,3185,3143,3100,3057,3013,2968,2923,2877,
	2831,2784,2737,2690,2642,2594,2545,2496,2447,2398,2348,2298,2248,2198,2148,2098,
	2048,1997,1947,1897,1847,1797,1747,1697,1648,1599,1550,1501,1453,1405,1358,1311,
	1264,1218,1172,1127,1082,1038,995,952,910,869,828,788,749,710,672,636,
	600,565,530,497,465,433,403,373,345,318,291,266,242,219,197,176,
	156,137,120,103,88,74,61,50,39,30,22,15,10,6,2,1,
	0,1,2,6,10,15,22,30,39,50,61,74,88,103,120,137,
	156,176,197,219,242,266,291,318,345,373,403,433,465,497,530,565,
	600,636,672,710,749,788,828,869,910,952,995,1038,1082,1127,1172,1218,
	1264,1311,1358,1405,1453,1501,1550,1599,1648,1697,1747,1797,1847,1897,1947,1997
};

/* Защита от неполной таблицы на этапе компиляции (и в C, и в C++): */
#define STATIC_ASSERT(COND, MSG) typedef char static_assert_##MSG[(COND)?1:-1]
STATIC_ASSERT(sizeof(sin_lut_256)/sizeof(sin_lut_256[0]) == 256, lut_must_have_256_items);

// В C++ корректней так: атрибут сразу после ключевого слова struct
struct __attribute__((packed)) osc_strike_header_t {
	uint8_t  version;                 ///< версия формата
	uint8_t  auxType;                 ///< 1 = один блок контактов, 3 = три блока
	uint8_t  lastOperation;           ///< cb_Off_On / cb_On_Off (если нужно)
	uint8_t  reserved0;               ///< выравнивание/резерв

	uint16_t sample_period_us;        ///< шаг дискретизации (100 мкс)
	uint16_t samples_max_per_phase;   ///< лимит выборок на фазу (430)

	uint32_t t0_ticks;                ///< тик таймера на старте BUSY (база времени)
	uint32_t start_tick[3];           ///< метки "первого разрыва" по фазам (A,B,C)

	uint16_t written[3];              ///< фактически записано выборок по фазам
	uint16_t reserved1;               ///< резерв (под CRC/флаги)
	uint8_t  status;                 ///< см. OSC_STAT_*
	uint8_t  reserved2[3];           ///< выравнивание до 32 байт
	uint32_t seq;                    ///< монотонный номер удара
};


// Адрес выборки фазы ph, индекс i (0..PHASE_SAMPLES_MAX-1)
#define PHASE_BYTES_PER_BLOCK   (uint16_t)(PHASE_SAMPLES_MAX * 2u)
#define PHASE_BLOCK_OFFSET(ph)  (uint16_t)(FRAM_HDR_SIZE + (ph) * PHASE_BYTES_PER_BLOCK)
#define PHASE_SAMPLE_ADDR(base, ph, i) \
(uint16_t)((base) + PHASE_BLOCK_OFFSET(ph) + ((i) << 1))

/**
* @file dbg_tap_raw12.c (фрагмент)
* @brief Прямой «просвет» данных эмулятора АЦП: пишем сырые 12-бит коды в RAM,
*        чтобы смотреть 1:1, минуя FRAM и любые k[].
*/

/// 0=выкл (ничего не пишем), 1=вкл (пишем в буферы)
static volatile uint8_t dbg_tap_enable = 1;

/// Сколько сэмплов уже записано в RAM-буферы по фазам (0..PHASE_SAMPLES_MAX)
static volatile uint16_t dbg_written[phase_quantity] = {0,0,0};

/// RAM-буферы сырых кодов (0..4095), отдельно по фазам
static int16_t dbg_ct_raw[phase_quantity][PHASE_SAMPLES_MAX];



// Компилятор до C++11: делаем «статическую проверку размера» через typedef-хак
typedef char osc_hdr_size_check[(sizeof(struct osc_strike_header_t) == FRAM_HDR_SIZE) ? 1 : -1];
// ========= Локальное состояние текущего удара =========
static volatile uint16_t Scope_written_points[phase_quantity] = {0,0,0};
static volatile scope_state_t  scope_State[phase_quantity] = {standingBy,standingBy,standingBy};
static uint8_t g_strike_index = 0;

static osc_strike_header_t g_hdr;

extern char __heap_start, *__brkval;
#define FREE_RAM() ((int) &__heap_start - (int) __brkval)

#define CAPTURED  1

#define NUM_CHANNELS 9
enum
{
	inActive,
	waitStart,
	Active
};


float MOTOR_STOP_THRESHOLD = 0.5;

static uint8_t prev_state = 0;   // предыдущее «сырое» состояние (биты A.ON,A.OFF,B.ON,B.OFF,C.ON,C.OFF)
static uint8_t curr_state = 0;   // предыдущее «сырое» состояние (биты A.ON,A.OFF,B.ON,B.OFF,C.ON,C.OFF)

// Текущая секция, которую обрабатываем (или 0xFF, если нет)
//static uint8_t g_proc_idx = 0xFF;
static osc_strike_header_t g_proc_hdr;


/// Аккумуляторы суммы квадратов и счётчик окна — по фазам
static uint32_t rms_sumsq[3] = {0,0,0};
static uint16_t rms_count    = 0;

int16_t calibr_Y[2*NUM_CHANNELS + 1];
int16_t calibr_X[2*NUM_CHANNELS + 1];

uint16_t heartBeat = 0;

uint8_t AI_TypeByte = 0;

float k[NUM_CHANNELS + 1] = {1,1,1,1,1,1,1,1,1};
float b[NUM_CHANNELS + 1] = {0};

#define EpromSTART 123
#define EpromShift 8
const uint16_t EEPROM_Start_Adress[NUM_CHANNELS + 1] = {121, EpromSTART+EpromShift, EpromSTART+2*EpromShift, EpromSTART+3*EpromShift, EpromSTART+4*EpromShift, EpromSTART+5*EpromShift, EpromSTART+6*EpromShift, EpromSTART+7*EpromShift, EpromSTART+8*EpromShift, EpromSTART+9*EpromShift};     // стартовые адреса ячеек EEPROM для загрузки данных

/*==============================================================================
*  ГЛОБАЛЬНЫЕ СТАТУСЫ (FSM)
*==============================================================================*/


/**
* @brief Статус обработки концевика по каждой фазе (локальная FSM).
*/
typedef enum {
	PH_EDGE_STANDBY = 0,  ///< фаза в ожидании нового действия (старта)
	PH_EDGE_STARTED = 1,  ///< начало коммутации зафиксировано (первое изменение активного сигнала 1→0)
	PH_EDGE_SILENT  = 2   ///< завершение зафиксировано, фаза в «тишине» до конца коммутации
} ph_edge_state_t;



// Метки начала движения (первый разрыв активного контакта) по фазам:
// Метки начала движения (первый разрыв активного контакта) по фазам:
volatile uint32_t phase_first_edge_ticks[phase_quantity] = {0,0,0};
volatile uint32_t phase_fin_edge_ticks[phase_quantity] = {0,0,0};
static   uint8_t  fin_edge_latched[phase_quantity]     = {0,0,0};
volatile ph_edge_state_t phase_Contact_state[phase_quantity]       = { PH_EDGE_STANDBY, PH_EDGE_STANDBY, PH_EDGE_STANDBY };
volatile uint8_t  emu_active_is_on[phase_quantity]            = { 1u, 1u, 1u };        ///< 1: активный контакт ON; 0: активный OFF
volatile uint32_t mechSwitchTime[phase_quantity] = {0,0,0};
// Защёлка "метка уже записана в рамках текущей операции" по фазам:
//volatile uint8_t  phase_edge_latched[3] = {0,0,0};


// ==== Objects:
TIMER			timer;						// Timer for measurements
ADC_SPI			cts;						// Current Transformers
IGAS_X2X_485	modbus;						// Modbus protocol routines
// ================

// ==== Global variables:
//uint8_t		GLOBAL_MODE		= IDLE;			// Флаг режима записи осциллограмм с ТТ: IDLE - дежурный режим, BUSY - идёт запись, FULL - оба буфера заполнены.
uint32_t	timer1_millis;

unsigned long totalSwitchTimer;
uint16_t	enc_abs_status;
uint8_t lastOperationBuf[phase_quantity] = {0};
uint16_t ContactFinTimePoint[phase_quantity][3] = {0};
uint16_t ContactStartTimePoint[phase_quantity][3] = {0};

uint16_t	current_ph_A	= 0;			// Значение тока в дежурном режиме, А * 100. Фаза A
uint16_t	current_ph_B	= 0;			// Значение тока в дежурном режиме, А * 100. Фаза B
uint16_t	current_ph_C	= 0;			// Значение тока в дежурном режиме, А * 100. Фаза C


typedef enum {
	calcStandBy = 0,  ///< фаза в ожидании нового действия (старта)
	calcStarted = 1,  ///< начало коммутации зафиксировано (первое изменение активного сигнала 1→0)
	calcFinished  = 2   ///< завершение зафиксировано, фаза в «тишине» до конца коммутации
} calc_state_t;

uint8_t		buffer_new_data_Tail = 0;	// Флаг обновления данных в буфере осциллограмм. Исп-ся для старта вычислений значений структуры modbus_data
calc_state_t startCalculations = calcStandBy;      // Флаг - данные получены. запуск математической обработки
uint8_t		moveFRAM_Buffer = 0;      // Флаг - данные получены. запуск математической обработки
uint16_t	max_electrical_wear = 1240;
uint16_t	max_mechanical_wear = 5000;

uint16_t contactShakeCNT = 0;

int16_t	min_Arc_Current		= 10;		// Минимальное значение тока горения дуги
//uint8_t		zero_is_found		= false;


uint32_t	loop_500_start		= 0;		// Стартовое время цикла в 500 мс
uint32_t	reset_wd_timer = 0;					// Сброс таймаута момента последнего принятого в адрес модуля пакета по RS485.
uint8_t		x2x_state			= 0;		// Статус связи: были ли (1) входящие за последние CONN_TIMEOUT миллисекунд или нет (0)
uint8_t		mb_slave_addr		= 3;		// Modbus-адрес по-умолчанию
// uint8_t		DI_prev_state		= OFF;		// Предыдущее состояние дискретных входов
// uint8_t		DI_curr_state		= OFF;		// Текущее состояние дискретных входов
// ================

// ==== Arrays:
// uint16_t	enc_rec_1[150]		= {0};		// Массив значений с энкодера.
// uint16_t	enc_rec_2[150]		= {0};		// Массив значений с энкодера.

uint8_t currentDetected[3] = {0};
uint8_t arcDetected[3] = {0};
//uint8_t		phase_zero_1[phase_quantity]= {0};	// Для osc_buffer_1: номер осциллограммы в массиве, на которой ток упал до значения current_minimum и ниже.
uint16_t	ISR_arcPoint[phase_quantity]= {0};	// Для osc_buffer_2: номер осциллограммы в массиве, на которой ток упал до значения current_minimum и ниже.
uint8_t		lastOperation = 0;
uint8_t     ISR_timeShiftOff[phase_quantity] = {0};
uint8_t     ISR_timeShiftOn[phase_quantity] = {0};
//uint16_t	own_off_time_ISR[phase_quantity] = {0};	// Время полного срабатывания выключателя на выключение, мс
//uint16_t	own_on_time_ISR[phase_quantity] = {0};	    // Время полного собственного срабатывания выключателя на включение, мс
uint16_t	full_off_time_ISR[phase_quantity] = {0};			// Время собственного срабатывания выключателя на выключение, мс
uint16_t	full_on_time_ISR[phase_quantity] = {0};			// Время собственного срабатывания выключателя на включение, мс

uint16_t		fin_Captured[phase_quantity] = {0};
uint16_t		start_Captured[phase_quantity] = {0};
uint32_t currentTime = 0;

uint32_t heartBeatTimer = 0;
uint32_t  switchBounceTime = 0;
uint8_t switchBounceSilense = 200;

const uint8_t trendLimit = 5;
float RMS_Aver[phase_quantity][6] = {0};
uint16_t averPosition = 0;
float AVARAGED_RMS[phase_quantity] = {0};
float TREND[phase_quantity][trendLimit+1] = {0};
uint8_t tail = 0;
float PIDFILTER[4] = {0};
float const SMOOTHfilter[] = {0.03, 0.03, 0.03};
float const AGGRESSfilter[] = {0.5, 0.5, 0.5};
float filteredRMS[phase_quantity] = {0};





//static uint32_t g_seq_counter = 1;   // персистентность не нужна сейчас

// --- UART1 quiet mode ---
static uint8_t uart1_ucsr1b_saved = 0;
static uint8_t uart1_quiet_active = 0;

uint16_t	ISR_arc_lifetime[phase_quantity] = {0};// 13, 14, 15 - Время горения дуги во время последнего отключения, мс.
uint16_t	switchCurrent_Amp[phase_quantity] = {0};
// 19, 20, 21 - Выработанный электрический ресурс во время последнего отключения, кА^2 * с.
float	last_kA2s[phase_quantity] = {0};

uint8_t osc_Point= 0;
uint16_t fullTimeOffWarn_ISR;
uint16_t fullTimeOffAlarm_ISR;
uint16_t ownTimeOffWarn_ISR;
uint16_t ownTimeOffAlarm_ISR;

uint16_t fullTimeOnWarn_ISR;
uint16_t fullTimeOnAlarm_ISR;
uint16_t ownTimeOnWarn_ISR;
uint16_t ownTimeOnAlarm_ISR;

uint8_t auxType = 3;

uint8_t RMS_Zero[phase_quantity];


unsigned long timeToGetVal = 0;

unsigned long switchCoolDownStartTime = 0; // Засекаем время нахождения выключателя в конечном положении.
uint8_t switchCoolDownTime = 100; // уставка времени выключателя в конечном положении - против дребезга.
//uint8_t switchBlocked = false;  // Флаг разрешения учета положения выключателя.


static breakerState CB_State[phase_quantity] = { cb_NA, cb_NA, cb_NA };
// breakerState CB_State2 = cb_NA;
// breakerState CB_State3 = cb_NA;


unsigned long ISR_Timer_A = 0; // засекаем скорости переключения
unsigned long ISR_Timer_B = 0; // засекаем скорости переключения
unsigned long ISR_Timer_C = 0; // засекаем скорости переключения

volatile uint16_t complexSP = 0;
uint32_t ISR_microTimer = 0;   // глобально
volatile uint32_t ticks_now = 0;


uint8_t manualMode = 0;


uint8_t strikeData = 1;
uint16_t strikeLostData = 0;

uint64_t current_A_buf = 0;
uint64_t current_B_buf = 0;
uint64_t current_C_buf = 0;

float float_RMS_A = 0;
float float_RMS_B = 0;
float float_RMS_C = 0;
uint64_t qRMS_A_buf_ADC = 0;
uint64_t qRMS_B_buf_ADC = 0;
uint64_t qRMS_C_buf_ADC = 0;

uint8_t quickRMScnt = 0;


uint16_t idle_ct_meas_period = 0;		// Счётчик кол-ва замеров тока в дежурном режиме. Нужен для отсчёта периода в одну длину волны (20 мс при 50 Гц).
uint8_t encCounter = 0;
uint8_t encCallCounter = 0;

uint16_t micro40timer = 0;
uint16_t microtimer = 0;

#define	ADC_MID		2048
/** @brief Текущее "сырое" состояние 6 концевиков (после инверсии pull-up и учёта auxType). */
static volatile uint8_t contacts_state_raw = 0;

/** @brief Получить "сырой" байт состояния концевиков для внешних модулей (Modbus и т.п.). */
static inline uint8_t contacts_get_raw_state(void) { return contacts_state_raw; }


uint8_t partialElectricalSwitch = 0;
enum
{
	A_Fail,
	B_Fail,
	C_Fail,
	part_Phase_Switch_Bit,
	last_Direction,
	ownTimeOff_Warn_Bit,
	ownTimeOff_Alarm_Bit,
	ownTimeOn_Warn_Bit,
	ownTimeOn_Alarm_Bit,
	fullTimeOffWarn_Bit,
	fullTimeOffAlarm_Bit,
	fullTimeOnWarn_Bit,
	fullTimeOnAlarm_Bit,
};


uint16_t solenoidCurrOnBuf;
uint16_t solenoidCurrOff_1_Buf;
uint16_t solenoidCurrOff_2_Buf;

uint16_t solenoidCurrOn[3];
uint16_t solenoidCurrOff_1[3];
uint16_t solenoidCurrOff_2[3];

enum
{
	procDone,
	runProc,
};

/**
* @brief Беззнаковая разница времён в тиках (устойчива к переполнению uint32_t).
*/
static inline uint32_t ticks_diff_u32(uint32_t t, uint32_t t0)
{
	return (uint32_t)(t - t0); // модульная арифметика беззнаковая — ровно то, что нужно
}

/**
* @brief Минимум среди A/B/C, игнорируя нули (0 трактуем как «нет метки»).
* @return минимальное ненулевое значение; если все нули — вернёт 0.
*/
static inline uint32_t min_nonzero3_u32(uint32_t a, uint32_t b, uint32_t c)
{
	uint32_t m = 0;
	if (a) m = a;
	if (b && (!m || ticks_diff_u32(b, m) < 0x80000000UL)) m = (m ? (b < m ? b : m) : b);
	if (c && (!m || ticks_diff_u32(c, m) < 0x80000000UL)) m = (m ? (c < m ? c : m) : c);
	return m;
}



struct modbus_data{
	// 0 - Текущее состояние контактов (биты с 0 по 5), 0 - выкл, 1 - вкл.
	uint16_t	phase_contact_states;

	// 	1, 2, 3 - Счетчик выключений фазы при I ≤ 1.1 x Iном.
	// 	uint16_t	ph_A_less_11nom_cntr;
	// 	uint16_t	ph_B_less_11nom_cntr;
	// 	uint16_t	ph_C_less_11nom_cntr;
	//
	// 	// 4, 5, 6 - Счетчик выключений фазы при I > 1.1 x Iном.
	// 	uint16_t	ph_A_grt_11nom_cntr;
	// 	uint16_t	ph_B_grt_11nom_cntr;
	// 	uint16_t	ph_C_grt_11nom_cntr;

	// 7, 8, 9 - Действующее значение тока, А.
	float	RMS_A;
	float	RMS_B;
	float	RMS_C;

	// 10, 11, 12 - Общий отработанный ресурс, кА^2 * с.
	float	ph_A_kA2s;
	float	ph_B_kA2s;
	float	ph_C_kA2s;


	// 16, 17, 18 - Значение тока последнего отключения, А.



	// Счётчик операций с фазой
	uint16_t	power_OnOff_cntr_A;	 // включений
	uint16_t	power_OnOff_cntr_B;	 // выключений
	uint16_t	power_OnOff_cntr_C;	 // включений

	
	// Счётчик операций с фазой A
	uint16_t	power_On_cntr_A;	 // включений
	uint16_t	power_Off_cntr_A;	 // выключений
	
	// Счётчик операций с фазой B
	uint16_t	power_On_cntr_B;	 // включений
	uint16_t	power_Off_cntr_B;	 // выключений
	
	// Счётчик операций с фазой C
	uint16_t	power_On_cntr_C;	 // включений
	uint16_t	power_Off_cntr_C;	 // выключений
	//
	// 	// 22, 23, 24 - Счётчик операций с фазой (общее кол-во выключений).
	// 	uint16_t	ph_A_Off_cntr;
	// 	uint16_t	ph_B_Off_cntr;
	// 	uint16_t	ph_C_Off_cntr;


	// 25, 26, 27 - Механический износ контактов, %
	float		mechanical_wear_Perc_A;
	float		mechanical_wear_Perc_B;
	float		mechanical_wear_Perc_C;

	// 28, 29, 30 - Общий электрический износ контактов, %.
	float		ph_A_electrical_Perc;
	float		ph_B_electrical_Perc;
	float		ph_C_electrical_Perc;

	// 31, 32, 33 - Электрический износ за время последнего отключения, %.
	float		ph_A_last_elec_perc;
	float		ph_B_last_elec_perc;
	float		ph_C_last_elec_perc;


	
	uint16_t	short_Cnt_A;			// Счетчик КЗ фаза А
	uint16_t	short_Cnt_B;			// Счетчик КЗ фаза B
	uint16_t	short_Cnt_C;			// Счетчик КЗ фаза C
	
	uint16_t    minFailCurrent;
	
	float		overCurrentKoef;
	uint16_t	nominalCurrent_Amp;

} params;
// ================


/**
* @brief Обновить бит направления last_Direction в complexSP по breakerState.
* @param op  breakerState: cb_On_Off => бит=0; cb_Off_On => бит=1; прочие — без изменений.
*/
static inline void complex_set_last_dir_from_op(uint8_t op)
{
	if (op == cb_Off_On) {
		complexSP |=  (uint16_t)(1u << last_Direction);
		} else if (op == cb_On_Off) {
		complexSP &= (uint16_t)~(1u << last_Direction);
	}
}

uint8_t triggerRMScalc = 0;
uint8_t triggerRMSstep = 0;


// --- прототипы (чтобы можно было вызывать ниже по файлу) ---
static inline void osc_mark_status(uint8_t idx, uint8_t st);
//static bool     osc_find_oldest_ready(uint8_t* out_idx, osc_strike_header_t* out_hdr);
static void     kick_calculations(void);
static inline void rms_idle_accumulate_sample(int16_t sA, int16_t sB, int16_t sC);
static inline void adc_read_ct_triplet(int16_t* outA, int16_t* outB, int16_t* outC);
static inline void rms_idle_finalize_window(void);
static inline uint16_t fram_read_u16_be(uint16_t addr);
static inline int16_t osc_read_centered(uint8_t phase, uint16_t idx);
static inline void resetConsistency(uint8_t phase);
static inline uint8_t checkConsistency_RAM(uint8_t phase, uint16_t idx);
//static inline void osc_get_lengths(const osc_strike_header_t* hdr, uint16_t* nA, uint16_t* nB, uint16_t* nC, uint16_t* nMin);
// static inline int16_t osc_read_sample(uint8_t strike_idx, uint8_t phase, uint16_t i);
//static void osc_prepare_calc_indices_from_hdr(const osc_strike_header_t* hdr);
static inline int16_t osc_read_sample(uint8_t strike_idx, uint8_t phase, uint16_t i);
static inline void updateElectricalWear(void);
static inline uint8_t emu_contacts_make_state(uint32_t now_ticks);
static inline breakerState derive_last_operation(uint8_t prev_raw, breakerState cb_state_A);
static inline void dbg_tap_push(uint8_t ph, uint16_t code);
//static void emu_contacts_leave_mode(void);
static void EMU_init_EMU_mode(void);
//static void emu_contacts_force_state(uint8_t s_in);
static inline int16_t dbg_tap_get(uint8_t ph, uint16_t sample_i);
static inline void dbg_tap_reset(void);
static inline void write_last_direction_bit(uint8_t op);
static inline breakerState breaker_fsm_update(uint8_t on, uint8_t off, breakerState prev);
static inline uint8_t active_contact_is_on(breakerState s);
static inline void isrMeasurements();
static inline void initScoping(void);
static inline void finScoping(void);
static inline void check_Input_States(void);
static inline void ScopeCapture();
static uint8_t build_off_default_state(void);
static inline void emuFunctions();
static inline void switch_Finish_Timers();
static inline void osc_get_lengths(const osc_strike_header_t* hdr,
uint16_t* nA, uint16_t* nB, uint16_t* nC, uint16_t* nMin);

uint32_t dt = 0;
uint8_t startTimerTest = 0;

#define BOOT_START_ADDR_BYTES  0x1F000UL             // 128KB - 4KB
#define BOOT_START_ADDR_WORDS  (BOOT_START_ADDR_BYTES / 2)

typedef void (*boot_ptr_t)(void);
// typedef void (*AppPtr_t) (void);
// AppPtr_t GoToBootloader = (AppPtr_t)0x3F07;


int16_t	osc_buffer_1[4][phase_quantity];



/** @brief Одноразовая инициализация на старте захвата; сбрасывается в конце (COMPLETE). */
static uint8_t scope_pairpack_init_done = 0u;

/********************************  H E L P E R S  ******************************************/
/**
* @brief Упаковать два 12-битных значения в 3 байта (MSB-first).
*
* Схема:
*  b0 = a[11:4]
*  b1 = a[3:0]<<4 | b[11:8]
*  b2 = b[7:0]
*/
static inline void pack2x12_to3(uint8_t out3[3], uint16_t a12, uint16_t b12)
{
	out3[0] = (uint8_t)((a12 >> 4) & 0xFFu);
	out3[1] = (uint8_t)(((a12 & 0x0Fu) << 4) | ((b12 >> 8) & 0x0Fu));
	out3[2] = (uint8_t)(b12 & 0xFFu);
}

/**
* @brief Записать одиночный 12-битный сэмпл в 2 байта (BE-стиль 12→16, младшие 4 бита нули).
*
* Схема:
*  t0 = v[11:4]
*  t1 = v[3:0] << 4
*/
static inline void store_single12_to2(uint8_t out2[2], uint16_t v12)
{
	out2[0] = (uint8_t)((v12 >> 4) & 0xFFu);
	out2[1] = (uint8_t)((v12 & 0x0Fu) << 4);
}


/** @brief Преобразовать уже центрированный и подправленный код в Амперы. */
static inline float osc_sample_in_amps(uint8_t ph, uint16_t idx)
{
	/* dbg_ct_raw[ph][idx] уже: (raw - ADC12_MID - adcShift_codes) */
	return k[ph] * (float)dbg_ct_raw[ph][idx];


}
static inline uint8_t contacts_all_silent(void);
/*************************  C O N T A C T S   W A T C H D O G  *************************/
/**
* @brief Вотчдог контактов: вызывать из check_Input_States() после обновления phase_Contact_state[].
* @details
*  - Армится при первом переходе любой фазы из STANDBY (т.е. пойман старт коммутации).
*  - Если до таймаута не все (релевантные) фазы ушли в PH_EDGE_SILENT — принудительно переводит их в SILENT.
*  - Ставит флаг contact_wd_tripped=1 (TODO: твоя аварийная обработка).
*/
static inline void contacts_watchdog(void)
{
	/* Армирование: первая фаза пошла из STANDBY */
	if (!contact_wd_armed) {
		if (phase_Contact_state[PHASE_A] == PH_EDGE_STARTED ||
		phase_Contact_state[PHASE_B] == PH_EDGE_STARTED ||
		phase_Contact_state[PHASE_C] == PH_EDGE_STARTED)
		{
			contact_wd_armed = 1u;
			contact_wd_t0    = ISR_microTimer;
			contact_wd_tripped = 0;
		}
		return;
	}

	/* Если уже все SILENT — работа сделана, пусть арм останется до сброса в silence_timer_step() */
	if (contacts_all_silent()) return;

	/* Контроль времени */
	uint32_t dt = (uint32_t)(ISR_microTimer - contact_wd_t0);
	if (dt < (uint32_t)contactMaxWaitTime_ticks) return;

	/* Таймаут → принудительно переводим оставшиеся фазы в SILENT (аварийная ситуация) */
	if (auxType == 1u) {
		if (phase_Contact_state[PHASE_A] != PH_EDGE_SILENT) {
			phase_Contact_state[PHASE_A] = PH_EDGE_SILENT;
		}
		/* B/C зеркалятся по A логически; если есть физика — можешь дожимать и их. */
		} else {
		for (uint8_t ph = 0; ph < 3; ph++) {
			if (phase_Contact_state[ph] != PH_EDGE_SILENT) {
				phase_Contact_state[ph] = PH_EDGE_SILENT;
			}
		}
	}

	contact_wd_tripped = 1u;  /* TODO: лог/счётчик/флаг в регистре */
}


/****************************  У Т И Л И Т Ы  (локальная логика)  ****************************/
/**
* @brief Все актуальные контакты в SILENT?
* @return 1 — да; 0 — нет.
* @details При auxType==1 контролируем только фазу A. При auxType==3 — A/B/C.
*/
static inline uint8_t contacts_all_silent(void)
{
	if (auxType == 1u) {
		return (phase_Contact_state[PHASE_A] == PH_EDGE_SILENT) ? 1u : 0u;
	}
	return (phase_Contact_state[PHASE_A] == PH_EDGE_SILENT &&
	phase_Contact_state[PHASE_B] == PH_EDGE_SILENT &&
	phase_Contact_state[PHASE_C] == PH_EDGE_SILENT) ? 1u : 0u;
}

/**
* @brief Все фазы записи завершены (переведены в silentTime)?
*/
static inline uint8_t scopes_all_silent(void)
{
	return (scope_State[PHASE_A] == silentTime &&
	scope_State[PHASE_B] == silentTime &&
	scope_State[PHASE_C] == silentTime) ? 1u : 0u;
}



// Хелпер: текущее «всё тихо?»
static inline uint8_t IS_scope_and_contacts_complete(void)
{
	return (contacts_all_silent() && scopes_all_silent());
}



/**
* @brief Свести осциллограмму фазы к нулю «на месте»: raw(0..4095) → signed(−2048..+2047).
* @param ph  PHASE_A/B/C
* @return количество преобразованных отсчётов
* @note Однократно на цикл. Повторный вызов игнорируется до явного сброса флага.
*/
uint16_t osc_center_inplace_phase(uint8_t ph)
{
	
	if (ph > 2) return 0;

	int16_t phaseSh = 0;

	if (ph == PHASE_A) 	phaseSh = cts.adcShift_A;

	if (ph == PHASE_B) 	phaseSh = cts.adcShift_B;
	
	if (ph == PHASE_C) 	phaseSh = cts.adcShift_C;

	/* ВНИМАНИЕ: dbg_ct_raw[ph][i] должен быть типа int16_t,
	чтобы отрицательные значения сохранялись корректно. */
	uint16_t n = dbg_written[ph];
	if (n > PHASE_SAMPLES_MAX) n = PHASE_SAMPLES_MAX;

	for (uint16_t i = 0; i < n; i++) {
		/* берём 12 бит исходника, вычитаем середину */
		int16_t raw12 = (int16_t)(dbg_ct_raw[ph][i] & 0x0FFF);
		
		
		dbg_ct_raw[ph][i] = (int16_t)(raw12 - (int16_t)ADC12_MID + (int16_t)phaseSh - 128);
		
	}
	return n;
}

/**
* @brief Свести к нулю все фазы, у которых есть данные.
*/
void osc_center_inplace_all(void)
{
	(void)osc_center_inplace_phase(PHASE_A);
	(void)osc_center_inplace_phase(PHASE_B);
	(void)osc_center_inplace_phase(PHASE_C);
}






/**
* @brief Вотчдог осциллографирования: от первого старта записи до полного завершения.
* @details
*  - Армится при первом уходе любой фазы записи из standingBy.
*  - Если до таймаута не все фазы стали silentTime — принудительно закрывает их и
*    помечает запись как COMPLETE (чтобы телеметрия была консистентной).
*/
static inline void scope_watchdog(void)
{
	/* Армирование: любая фаза начала запись */
	if (!scope_wd_armed) {
		if (scope_State[PHASE_A] != standingBy ||
		scope_State[PHASE_B] != standingBy ||
		scope_State[PHASE_C] != standingBy)
		{
			scope_wd_armed = 1u;
			scope_wd_t0    = ISR_microTimer;
		}
		return;
	}

	/* Уже все тихие? — ничего делать не нужно */
	if (scope_State[PHASE_A] == silentTime &&
	scope_State[PHASE_B] == silentTime &&
	scope_State[PHASE_C] == silentTime)
	{
		return;
	}

	/* Контроль времени */
	uint32_t dt = (uint32_t)(ISR_microTimer - scope_wd_t0);
	if (dt < (uint32_t)scopeMaxWaitTime_ticks) return;

	/* Таймаут → принудительно завершаем запись по всем незавершённым фазам */
	for (uint8_t ph = 0; ph < 3; ph++) {
		if (scope_State[ph] != silentTime) {
			scope_State[ph] = silentTime;
		}
	}
	// fram_write_byte(7500, 33);
	scope_wd_tripped      = 1u;      /* TODO: аварийная обработка/лог при желании */
	cts.recording_status  = COMPLETE; /* телеметрия; логика на нём не завязана */
}


//
//
// static void EMU_EXIT_EMU_mode(void)
// {
// 	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
// 	{
// 		/* 1) Логическое состояние фаз: по умолчанию OFF */
// 		for (uint8_t ph = 0; ph < 3; ph++) {
// 			CB_State[ph]         = cb_Off;
//
// 		}
//
// 		/* 2) Синхронизация «сырых» битов (prev/curr) с устойчивым OFF-состоянием */
// 		const uint8_t s = build_off_default_state();  /* формирует маску CB*_OFF */
// 		prev_state = s;
// 		curr_state = s;
//
// 		/* 3) Глобальные статусы ожидания */
// 		positionChanged      = standingBy;
// 		cts.recording_status = 0;               /* IDLE */
//
// 		/* 4) Режим эмуляции включён, но сценарий (переключение) пока не идёт */
// 		EMU_mode_Flag       = 1;
// 		emulate_contacts    = 1;
// 		emulate_adc_wave    = 1;
// 		emu_scenario_active = 0;
// 		emu_contacts_t0_ticks = 0;
//
// 		/* 5) Служебные таймеры/флаги (если используются) — в ноль/разармить */
// 		scope_wd_armed      = 0;
// 		contact_wd_armed    = 0;
// 		GIS_Switched_Finished = 0;
// 		scope_wd_tripped    = 0;
// 		contact_wd_tripped  = 0;
// 	}
// }



/**
* @brief Войти в режим эмуляции доп-контактов и привести систему в консистентное исходное состояние.
* @details
*  - Источник контактов переключается на эмулятор (emulate_contacts=1).
*  - Базовое устойчивое положение фаз — OFF (emu_active_is_on[]=0).
*  - Сырые биты prev/curr синхронизируются с устойчивым OFF, чтобы детектор не поймал ложный фронт.
*  - Все фазовые статусы переведены в ожидание; глобальные флаги к standingBy.
*  - Сценарий эмуляции выключен (emu_scenario_active=0), т.к. «тишина» до явного старта.
*/
static void EMU_init_EMU_mode(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		/* 1) Логическое состояние фаз: по умолчанию OFF */
		for (uint8_t ph = 0; ph < 3; ph++) {
			CB_State[ph]         = cb_Off;
			phase_Contact_state[ph]  = PH_EDGE_STANDBY;
			scope_State[ph]         = standingBy;
			Scope_written_points[ph]= 0;
			emu_active_is_on[ph]    = 0;        /* устойчиво: OFF => ON=0, OFF=1 */
		}

		/* 2) Синхронизация «сырых» битов (prev/curr) с устойчивым OFF-состоянием */
		const uint8_t s = build_off_default_state();  /* формирует маску CB*_OFF */
		prev_state = s;
		curr_state = s;

		/* 3) Глобальные статусы ожидания */
		positionChanged      = standingBy;
		cts.recording_status = 0;               /* IDLE */

		/* 4) Режим эмуляции включён, но сценарий (переключение) пока не идёт */
		EMU_mode_Flag       = 1;
		emulate_contacts    = 1;
		emulate_adc_wave    = 1;
		emu_scenario_active = 0;
		emu_contacts_t0_ticks = 0;

		/* 5) Служебные таймеры/флаги (если используются) — в ноль/разармить */
		scope_wd_armed      = 0;
		contact_wd_armed    = 0;
		GIS_Switched_Finished = 0;
		scope_wd_tripped    = 0;
		contact_wd_tripped  = 0;
	}
}

/**
* @brief Старт сценария эмуляции одной коммутации (switch) с базой времени t0_ticks.
* @param t0_ticks Снимок «тик-таймера» (обычно atomic_u32_read(&ISR_microTimer))
* @details
*  - Делается атомарно, чтобы ISR не увидел «полуперестроенное» состояние.
*  - Сначала переводим все фазовые статусы в ожидание, затем выставляем t0 и только после — включаем сценарий.
*  - Устойчивое исходное положение (emu_active_is_on[]) НЕ трогаем здесь; оно меняется в Fin().
*/
static void EMU_Start_switching(uint32_t t0_ticks)
{
	if (!(emulate_contacts && EMU_mode_Flag)) return;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		/* 1) На время подготовки сценарий выключен → генератор отдаёт устойчивые 10/01 */
		emu_scenario_active = 0;

		/* 2) Фазовые статусы/счётчики — в ожидание нового цикла */
		for (uint8_t ph = 0; ph < 3; ph++) {
			phase_Contact_state[ph]  = PH_EDGE_STANDBY;
			scope_State[ph]          = standingBy;
			Scope_written_points[ph] = 0;
		}
		positionChanged = standingBy;
		cts.recording_status = 0;   /* IDLE */

		/* 3) База времени сценария */
		emu_contacts_t0_ticks = t0_ticks;

		/* 4) Включаем сценарий: с этого момента emu_contacts_make_state() начнёт «рисовать» профиль */
		emu_scenario_active   = 1;
	}
}

/**
* @brief Финализация после завершения переключения (реальное или эмулированное).
* @details
*  Вызывать один раз после истечения окна тишины (когда событие готовности зафиксировано).
*  Делает два слоя работ:
*   1) ОБЩЕЕ: возвращает систему в ожидание новой коммутации (reset фазовых статусов/счётчиков).
*   2) ЭМУЛЯТОР (только если EMU_mode_Flag==1): гасит сценарий, фиксирует новое устойчивое
*      положение фаз (ON↔OFF) и синхронизирует RAW-слепок prev/curr с этим положением.
*
*  Примечание:
*   - Все групповые изменения выполняются атомарно, чтобы ISR не увидел промежуточных состояний.
*   - Флаги *_wd_tripped намеренно не очищаются здесь — оставляются для логов/диагностики.
*/
static void switchingFinished(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		positionChanged = silentTime;
		cts.recording_status = COMPLETE;   // оставил как в твоём старте; убери, если не надо

		uart1_quiet_leave();
		Serial1.begin(9600, SERIAL_8N1);
		modbus.init(9600);
		
		
		/* ==== 1) ОБЩЕЕ: вернуть систему к ожиданию новой коммутации ==== */
		for (uint8_t ph = 0; ph < 3; ph++) {
			scope_State[ph]          = standingBy;
			Scope_written_points[ph] = 0;
			phase_Contact_state[ph]  = PH_EDGE_STANDBY;   // разрешаем детектору поймать следующий старт
		}

		cts.recording_status = 0;         // IDLE
		positionChanged = standingBy;

		// Снять арминги вотчдогов (при новой коммутации поднимутся заново)
		contact_wd_armed = 0;
		scope_wd_armed   = 0;

		/* ==== 2) ЭМУЛЯТОР: доп. действия, только если режим эмуляции включён ==== */
		if (EMU_mode_Flag)
		{
			// 2.1) Глушим сценарий: эмулятор снова отдаёт устойчивые (10/01), никаких 00
			emu_scenario_active = 0;

			// 2.2) Принять новое устойчивое положение фаз (после успешной коммутации): ON↔OFF
			for (uint8_t ph = 0; ph < 3; ph++) {
				emu_active_is_on[ph] = (uint8_t)!emu_active_is_on[ph];
			}

			// 2.3) Синхронизировать RAW-слепок prev/curr с новым устойчивым состоянием
			uint8_t s = 0;
			for (uint8_t ph = 0; ph < 3; ph++) {
				const uint8_t on  = (emu_active_is_on[ph] ? 1u : 0u);
				const uint8_t off = (emu_active_is_on[ph] ? 0u : 1u);
				switch (ph) {
					case 0: if (on)  s |= CB1_ON;  if (off) s |= CB1_OFF; break; // A
					case 1: if (on)  s |= CB2_ON;  if (off) s |= CB2_OFF; break; // B
					case 2: if (on)  s |= CB3_ON;  if (off) s |= CB3_OFF; break; // C
				}
			}
			prev_state = s;
			curr_state = s;
		}

		/* Если используешь флаг события окончания (например, GIS_Switched_Finished),
		его удобно сбросить здесь же после обработки:
		GIS_Switched_Finished = 0u;
		*/
	}
}













static inline void jump_to_bootloader(void)
{
	cli();

	// Глушим периферию (по минимуму — UART/SPI/TWI; добавь своё при необходимости)
	#ifdef UCSR0B
	UCSR0B = 0;
	#endif
	#ifdef UCSR1B
	UCSR1B = 0;
	#endif
	#ifdef SPCR
	SPCR = 0;
	#endif
	#ifdef TWCR
	TWCR = 0;
	#endif

	// Выключаем watchdog, чтобы бут не получил немедленный reset
	wdt_disable();

	// Перенос векторной таблицы в boot-секцию (если бут включает прерывания — это критично)
	#ifdef IVCE
	MCUCR = _BV(IVCE);       // разрешить изменение IVSEL на 4 такта
	MCUCR = _BV(IVSEL);      // векторы теперь из boot-секции
	#endif

	// По ABI GCC: r1 должен быть 0
	asm volatile ("clr __zero_reg__");

	// Готовим стек (на всякий случай)
	#ifdef RAMEND
	SP = RAMEND;
	#endif

	// Прыжок
	((boot_ptr_t)BOOT_START_ADDR_WORDS)();
}



typedef union
{
	float val;
	uint8_t bytes[4];
} floatval;

enum
{
	collectRMSvalues,
	getRMSvalues
};


uint16_t	Emul_SwitchTime = 0;
uint16_t	Emul_SwitchTime_Delay = 350;


uint8_t shootBreakerFlag = 0;




unsigned long timeItTake = 9000;  // буфферная переменная чтоб засечь время выполнения функций.
uint8_t calcSteps = 0;  // показывает текущий шаг в вычислениях и записи осциллограмм в Eeprom и SD карту
uint8_t transferDataToLCP = 0;


static volatile uint16_t dbg_dt_ticks = 0;




// void moveFramToBuf()
// {
// 	if (moveFRAM_Buffer!=0)
// 	putDataToBuffers();
//
// }

/** Пользовательский выбор фильтра */
/** Пользовательский выбор фильтра */
typedef enum {
	F_BY_PASS = 0,           /**< без фильтрации */
	F_MED3_LPF,              /**< медиана(3) + однополюсный НЧ */
	F_SG_11P2,               /**< Savitzky–Golay окно 11, полином 2 */
	F_SINE_RECON_50HZ,       /**< реконструкция «идеального» 50 Гц (жёсткая) */

	F_DESPIKE_RUN = 4,       /**< деспайк по длительности: |x|>T и длина<L → интерполяция */
	F_HAMPEL,                /**< хэмпелевский фильтр (медиана+MAD) */
	F_SLEW_LIMIT,            /**< ограничитель скорости изменения |Δx|≤dv_max */
	F_THRESH_PERSIST_ZERO,   /**< порог+персистентность: короткие вспышки выше T → ноль */
	F_PROFILE_P1             /**< профиль: DESPIKE→MED3+LPF (рекомендованный базовый) */
} filter_mode_t;


filter_mode_t noiseCancellMode = F_BY_PASS;


// ==== Functions:
int main(void){

	//	enc_rec_2[0] = 0;						// Исключительно для выделения места под массив
	//	enc_ptr16 = &enc_abs_status;
	//	enc_ptr8 = (uint8_t *)enc_ptr16;
	
	//	_delay_ms(1000);
	//	while (millis() < 1000){};

	cts.gpio_init();
	cts.spi_init();
	cts.active_buffer_id	= FIRST;
	//	cts.adc_switch[] = {ON, OFF, ON, OFF, ON, OFF};
	// 	cts.adc_switch[ADC1] = ON;
	// 	cts.adc_switch[ADC2] = OFF;
	// 	cts.adc_switch[ADC3] = ON;
	// 	cts.adc_switch[ADC4] = OFF;
	// 	cts.adc_switch[ADC5] = ON;
	// 	cts.adc_switch[ADC6] = OFF;
	//	cts.flush_buffer((uint8_t *) cts.overflowOscBuffer, sizeof(cts.overflowOscBuffer));
	//	cts.flush_buffer((uint8_t *) cts.osc_buffer_2, sizeof(cts.osc_buffer_2));
	//	cts.flush_buffer((uint8_t *) phase_zero_1, sizeof(phase_zero_1));
	cts.flush_buffer((uint8_t *) ISR_arcPoint, sizeof(ISR_arcPoint));
	void *ptr1 = &params;
	cts.flush_buffer((uint8_t *) ptr1, sizeof(modbus_data));

	loadSettings();
	gpio_init();							// Инициализация пинов
	comm_init();


	fram_init();
	// 	fram_write_enable();


	millis_init();

	timer.init();							// Инициализация Timer2 на 200 мкс
	timer.start();							// Запуск таймера на 200 мкс
	sei();									// Глобальное разрешение прерываний

	cts.recording_status = IDLE;		// Флаг окончания непрерывной записи пачки осциллограмм


	//	DI_curr_state = check_DI_status();		// Опрос текущего состояния дискретных входов - концевики положения выключателя
	//	DI_prev_state = DI_curr_state;			// Текущее и предыдущее на начальный момент равны.
	
	prev_state = ~PIN_DI;
	
	// Фаза A (1)
	if ((prev_state & CB1_OFF) != OFF)
	CB_State[PHASE_A] = cb_Off;
	else if ((prev_state & CB1_ON) == ON)
	CB_State[PHASE_A] = cb_On;
	else
	CB_State[PHASE_A] = cb_NA;


	// Фаза B (2)
	if ((prev_state & CB2_OFF) != OFF)
	CB_State[PHASE_B] = cb_Off;
	else if ((prev_state & CB2_ON) == ON)
	CB_State[PHASE_B] = cb_On;
	else
	CB_State[PHASE_B] = cb_NA;

	// Фаза C (3)
	if ((prev_state & CB3_OFF) != OFF)
	CB_State[PHASE_C] = cb_Off;
	else if ((prev_state & CB3_ON) == ON)
	CB_State[PHASE_C] = cb_On;
	else
	CB_State[PHASE_C] = cb_NA;








	
	//	prev_state = curr_state;
	/*
	

	// 	cts.oscillograms[0][PHASE_A]	= 0xaf;
	// 	cts.oscillograms[1][PHASE_A]	= 0xa1;
	// 	cts.oscillograms[0][PHASE_B]	= 0xbf;
	// 	cts.oscillograms[1][PHASE_B]	= 0xb1;
	// 	cts.oscillograms[0][PHASE_C]	= 0xcf;
	// 	cts.oscillograms[1][PHASE_C]	= 0xc1;
	*/
	/*
	cts.osc_buffer_1[3][PHASE_A]	= 0xaa;
	cts.osc_buffer_1[2][PHASE_B]	= 0xbb;
	cts.osc_buffer_1[1][PHASE_C]	= 0xcc;

	cts.osc_buffer_2[1][PHASE_A]	= 0xa1;
	cts.osc_buffer_2[2][PHASE_B]	= 0xb1;
	cts.osc_buffer_2[3][PHASE_C]	= 0xc1;
	*/
	
	PORT_LED_OK |= (1<<LED_OK); // LED ON
	wdt_enable(WDTO_2S);

	while (true)
	{
		loop();
		wdt_reset();						// Сброс WatchDog
	};
	return 0;
}

unsigned long rebootLCT = 0;
unsigned long ms100Cycle = 0;
//

uint8_t init_Emu_Mode_Flag = 0;
uint8_t exit_Emu_Mode_Flag = 0;

void loop()
{
	currentTime = millis();
	update_isr_microtimer();
	
	// START For debug!
	// 	strikeData = runProc;
	// 	currentTime++;
	// 	ISR_microTimer++;
	// FIN For debug!
	
	emuFunctions();

	
	if (positionChanged == standingBy)
	{
		modbus.update(currentTime);
		
		if (currentTime - heartBeatTimer > 500)
		{
			heartBeat++;
			heartBeatTimer = currentTime;
			modbus_timeout_check();										// Цикл (без прерываний) на 500 мс
		}
		
		if (rebootLCT && (currentTime - rebootLCT > 50))
		{
			rebootLCT = 0;
			jump_to_bootloader();
		}
	}
	calculations();
	
	// Вызываем обработку Modbus
	isrMeasurements();
	
}

void loadSettings()
{
	// 	uint16_t WordOfData;
	// 	WordOfData = eeprom_read_word((uint16_t*)46);
	// 	saveFloat(46, 255);

	//	mb_slave_addr = 3;
	
	params.ph_A_kA2s = loadFloat(10,0);			// 0
	params.ph_B_kA2s = loadFloat(14,0);			// 0
	params.ph_C_kA2s = loadFloat(18,0);			// 0
	
	
	
	
	
	
	max_electrical_wear = eeprom_read_word((uint16_t*)28);		// 850
	max_mechanical_wear = eeprom_read_word((uint16_t*)30);		// 5000
	
	
	
	
	ISR_timeShiftOff[PHASE_A] =  eeprom_read_word((uint16_t*)32);	// 128	//1 = 200 us
	ISR_timeShiftOff[PHASE_B] =  eeprom_read_word((uint16_t*)34);	// 128		//1 = 200 us
	ISR_timeShiftOff[PHASE_C] =  eeprom_read_word((uint16_t*)36);	// 128	//1 = 200 us
	


	cts.adcShift_A = eeprom_read_byte((uint8_t*)38);	// 128
	cts.adcShift_B = eeprom_read_byte((uint8_t*)39);	// 128
	cts.adcShift_C = eeprom_read_byte((uint8_t*)40);	// 128
	
	
	cts.gainAdcShift_A = eeprom_read_byte((uint8_t*)41);	// 128
	cts.gainAdcShift_B = eeprom_read_byte((uint8_t*)42);	// 128
	cts.gainAdcShift_C = eeprom_read_byte((uint8_t*)43);	// 128
	

	
	
	cts.amplification = eeprom_read_byte((uint8_t*)44);		// 0
	
	AI_TypeByte = eeprom_read_byte((uint8_t*)45);

	if (cts.amplification == 1)
	{
		set_pin_high(PORT_ADC_CT1_GAIN, ADC_CT1_GAIN);
		set_pin_high(PORT_ADC_CT2_GAIN, ADC_CT2_GAIN);
		set_pin_high(PORT_ADC_CT3_GAIN, ADC_CT3_GAIN);
	}
	else
	{
		set_pin_low(PORT_ADC_CT1_GAIN, ADC_CT1_GAIN);
		set_pin_low(PORT_ADC_CT2_GAIN, ADC_CT2_GAIN);
		set_pin_low(PORT_ADC_CT3_GAIN, ADC_CT3_GAIN);
	}
	
	params.minFailCurrent = eeprom_read_byte((uint8_t*)68);	// 40
	params.overCurrentKoef = loadFloat(69, 1.1);			// 1.1

	//params.power_Off_cntr = eeprom_read_word((uint16_t*)73);
	// 	params.ph_B_Off_cntr = eeprom_read_word((uint16_t*)74);
	// 	params.ph_C_Off_cntr = eeprom_read_word((uint16_t*)75);
	
	params.short_Cnt_A = eeprom_read_word((uint16_t*)74);
	params.short_Cnt_B = eeprom_read_word((uint16_t*)74);
	params.short_Cnt_B = eeprom_read_word((uint16_t*)78);
	
	params.nominalCurrent_Amp = eeprom_read_word((uint16_t*)80);  // 3150
	
	// 	fullTimeOffWarn_ISR = eeprom_read_word((uint16_t*)82);				// 200us
	// 	fullTimeOffAlarm_ISR = eeprom_read_word((uint16_t*)84);				// 200us
	// 	ownTimeOffWarn_ISR = eeprom_read_word((uint16_t*)86);				// 200us
	// 	ownTimeOffAlarm_ISR = eeprom_read_word((uint16_t*)88);				// 200us
	
	auxType = eeprom_read_byte((uint8_t*)90);				// 1
	
	
	min_Arc_Current = eeprom_read_word((uint16_t*)91);		// 20
	
	
	params.ph_A_electrical_Perc = 1.0 * params.ph_A_kA2s * 100.0 / max_electrical_wear;
	params.ph_B_electrical_Perc = 1.0 * params.ph_B_kA2s * 100.0 / max_electrical_wear;
	params.ph_C_electrical_Perc = 1.0 * params.ph_C_kA2s * 100.0 / max_electrical_wear;
	
	
	
	RMS_Zero[PHASE_A] = eeprom_read_byte((uint8_t*)93);		// 128
	RMS_Zero[PHASE_B] = eeprom_read_byte((uint8_t*)94);		// 128
	RMS_Zero[PHASE_C] = eeprom_read_byte((uint8_t*)95);		// 128
	
	
	ISR_timeShiftOn[PHASE_A] =  eeprom_read_byte((uint8_t*)96);	// 128 //1 = 200 us
	ISR_timeShiftOn[PHASE_B] =  eeprom_read_byte((uint8_t*)97);	// 128 //1 = 200 us
	ISR_timeShiftOn[PHASE_C] =  eeprom_read_byte((uint8_t*)98);	// 128 //1 = 200 us
	
	switchBounceSilense = eeprom_read_byte((uint8_t*)99);	// 99
	
	mb_slave_addr = eeprom_read_byte((uint8_t*)100); // нет
	
	if (mb_slave_addr <1 || mb_slave_addr>254)		 //
	mb_slave_addr = 1;


	
	ownTimeOffWarn_ISR = eeprom_read_word((uint16_t*)101);		// 34 ms
	ownTimeOffAlarm_ISR = eeprom_read_word((uint16_t*)103);		// 40 ms
	fullTimeOffWarn_ISR = eeprom_read_word((uint16_t*)105);		// 65 ms
	fullTimeOffAlarm_ISR = eeprom_read_word((uint16_t*)107);	// 70 ms
	fullTimeOnWarn_ISR = eeprom_read_word((uint16_t*)109);		// 80 ms
	fullTimeOnAlarm_ISR = eeprom_read_word((uint16_t*)111);		// 90 ms
	ownTimeOnWarn_ISR = eeprom_read_word((uint16_t*)113);		// 80 ms
	ownTimeOnAlarm_ISR = eeprom_read_word((uint16_t*)115);		// 90 ms
	
	

	for (uint8_t i = 1; i <= NUM_CHANNELS; i++)   // Загрузка данных кривой y = kx - b;
	calibrFromEEPROM(i);




	
	params.power_Off_cntr_A = eeprom_read_word((uint16_t*)202);	// 0
	params.power_On_cntr_A = eeprom_read_word((uint16_t*)204);		// 0
	params.power_Off_cntr_B = eeprom_read_word((uint16_t*)206);
	params.power_On_cntr_B = eeprom_read_word((uint16_t*)208);	// 0
	params.power_Off_cntr_C = eeprom_read_word((uint16_t*)210);		// 0
	params.power_On_cntr_C = eeprom_read_word((uint16_t*)212);
	
	params.power_OnOff_cntr_A = params.power_Off_cntr_A + params.power_On_cntr_A;
	params.power_OnOff_cntr_B = params.power_Off_cntr_B + params.power_On_cntr_B;
	params.power_OnOff_cntr_C = params.power_Off_cntr_C + params.power_On_cntr_C;
	
	//	params.mechanical_wear_Perc_A = 1.0 * (params.power_OnOff_cntr_A + params.power_OnOff_cntr_B) * 50.0 / max_mechanical_wear;
	params.mechanical_wear_Perc_A =	(float)params.power_OnOff_cntr_A * 100.0f/ (float)max_mechanical_wear;
	params.mechanical_wear_Perc_B =	(float)params.power_OnOff_cntr_B * 100.0f/ (float)max_mechanical_wear;
	params.mechanical_wear_Perc_C =	(float)params.power_OnOff_cntr_C * 100.0f/ (float)max_mechanical_wear;


	uint16_t eepromValue_ = eeprom_read_word((uint16_t*)220);

	if (eepromValue_ >= F_BY_PASS && eepromValue_ <= F_PROFILE_P1)
	noiseCancellMode = (filter_mode_t)eepromValue_;
	else
	noiseCancellMode = F_BY_PASS; // дефолт или игнорировать
	

	arc_consistency_len = eeprom_read_word((uint16_t*)222);


	for (uint8_t i = 0; i < phase_quantity; i++)   // Загрузка данных кривой y = kx - b;
	{
		on_contact_lag_ticks[i] = eeprom_read_byte((uint8_t*)226+i);   /* лаг доп.контакта на ВКЛ, тики (по фазам) */
		off_contact_lag_ticks[i] = eeprom_read_byte((uint8_t*)229+i);  /* ранний уход доп.контакта на ВЫКЛ, тики (по фазам) */
	}

	
}

uint8_t currentBuffer = 0;
uint8_t nextBuffer = 0;

uint8_t loadDefaultsDone = 0;

void loadDefaults()
{
	// 	uint16_t WordOfData;
	// 	WordOfData = eeprom_read_word((uint16_t*)46);
	// 	saveFloat(46, 255);
	
	
	saveFloat(10, 0);
	saveFloat(14, 0);
	saveFloat(18, 0);
	
	params.ph_A_kA2s = loadFloat(10,0);			// 0
	params.ph_B_kA2s = loadFloat(14,0);			// 0
	params.ph_C_kA2s = loadFloat(18,0);			// 0
	
	
	


	eeprom_write_word((uint16_t*)28,850);		// 850
	eeprom_write_word((uint16_t*)30,5000);		// 5000

	max_electrical_wear = eeprom_read_word((uint16_t*)28);		// 850
	max_mechanical_wear = eeprom_read_word((uint16_t*)30);		// 5000

	eeprom_write_word((uint16_t*)32,128);
	eeprom_write_word((uint16_t*)34,128);
	eeprom_write_word((uint16_t*)36,128);
	
	ISR_timeShiftOff[PHASE_A] =  eeprom_read_word((uint16_t*)32);	// 128	//1 = 200 us
	ISR_timeShiftOff[PHASE_B] =  eeprom_read_word((uint16_t*)34);	// 128		//1 = 200 us
	ISR_timeShiftOff[PHASE_C] =  eeprom_read_word((uint16_t*)36);	// 128	//1 = 200 us
	


	eeprom_write_byte((uint8_t*)38,128);	// 128
	eeprom_write_byte((uint8_t*)39,128);	// 128
	eeprom_write_byte((uint8_t*)40,128);	// 128


	cts.adcShift_A = eeprom_read_byte((uint8_t*)38);	// 128
	cts.adcShift_B = eeprom_read_byte((uint8_t*)39);	// 128
	cts.adcShift_C = eeprom_read_byte((uint8_t*)40);	// 128
	
	
	eeprom_write_byte((uint8_t*)41,128);	// 128
	eeprom_write_byte((uint8_t*)42,128);	// 128
	eeprom_write_byte((uint8_t*)43,128);	// 128
	
	
	cts.gainAdcShift_A = eeprom_read_byte((uint8_t*)41);	// 128
	cts.gainAdcShift_B = eeprom_read_byte((uint8_t*)42);	// 128
	cts.gainAdcShift_C = eeprom_read_byte((uint8_t*)43);	// 128
	
	eeprom_write_byte((uint8_t*)44,0);	// 128
	cts.amplification = eeprom_read_byte((uint8_t*)44);		// 0
	
	eeprom_write_byte((uint8_t*)45,0);	// 128
	AI_TypeByte = eeprom_read_byte((uint8_t*)45);
	
	
	if (cts.amplification == 1)
	{
		set_pin_high(PORT_ADC_CT1_GAIN, ADC_CT1_GAIN);
		set_pin_high(PORT_ADC_CT2_GAIN, ADC_CT2_GAIN);
		set_pin_high(PORT_ADC_CT3_GAIN, ADC_CT3_GAIN);
	}
	else
	{
		set_pin_low(PORT_ADC_CT1_GAIN, ADC_CT1_GAIN);
		set_pin_low(PORT_ADC_CT2_GAIN, ADC_CT2_GAIN);
		set_pin_low(PORT_ADC_CT3_GAIN, ADC_CT3_GAIN);
	}
	
	eeprom_write_byte((uint8_t*)68,40);	// 40
	saveFloat(69, 1.1);			// 1.1
	
	params.minFailCurrent = eeprom_read_byte((uint8_t*)68);	// 40
	params.overCurrentKoef = loadFloat(69, 1.1);			// 1.1

	//params.power_Off_cntr = eeprom_read_word((uint16_t*)73);
	// 	params.ph_B_Off_cntr = eeprom_read_word((uint16_t*)74);
	// 	params.ph_C_Off_cntr = eeprom_read_word((uint16_t*)75);
	
	eeprom_write_word((uint16_t*)74,0);
	eeprom_write_word((uint16_t*)76,0);
	eeprom_write_word((uint16_t*)78,0);
	
	
	params.short_Cnt_A = eeprom_read_word((uint16_t*)78);
	params.short_Cnt_B = eeprom_read_word((uint16_t*)76);
	params.short_Cnt_C = eeprom_read_word((uint16_t*)78);
	
	eeprom_write_word((uint16_t*)80,3150);  // 3150
	params.nominalCurrent_Amp = eeprom_read_word((uint16_t*)80);  // 3150
	
	
	eeprom_write_byte((uint8_t*)90,1);				// 1
	auxType = eeprom_read_byte((uint8_t*)90);				// 1
	
	
	eeprom_write_word((uint16_t*)91,20);		// 20
	min_Arc_Current = eeprom_read_word((uint16_t*)91);		// 20
	
	//	params.mechanical_wear_Perc_A = 1.0 * (params.power_OnOff_cntr_A + params.power_OnOff_cntr_B) * 50.0 / max_mechanical_wear;
	params.mechanical_wear_Perc_A =	(float)params.power_OnOff_cntr_A * 100.0f/ (float)max_mechanical_wear;
	params.mechanical_wear_Perc_B =	(float)params.power_OnOff_cntr_B * 100.0f/ (float)max_mechanical_wear;
	params.mechanical_wear_Perc_C =	(float)params.power_OnOff_cntr_C * 100.0f/ (float)max_mechanical_wear;
	
	
	params.ph_A_electrical_Perc = 1.0 * params.ph_A_kA2s * 100.0 / max_electrical_wear;
	params.ph_B_electrical_Perc = 1.0 * params.ph_B_kA2s * 100.0 / max_electrical_wear;
	params.ph_C_electrical_Perc = 1.0 * params.ph_C_kA2s * 100.0 / max_electrical_wear;
	
	
	
	
	eeprom_write_byte((uint8_t*)93, 128);		// 128
	eeprom_write_byte((uint8_t*)94, 128);		// 128
	eeprom_write_byte((uint8_t*)95, 128);		// 128
	
	RMS_Zero[PHASE_A] = eeprom_read_byte((uint8_t*)93);		// 128
	RMS_Zero[PHASE_B] = eeprom_read_byte((uint8_t*)94);		// 128
	RMS_Zero[PHASE_C] = eeprom_read_byte((uint8_t*)95);		// 128
	
	
	
	eeprom_write_byte((uint8_t*)96, 128);	// 128 //1 = 200 us
	eeprom_write_byte((uint8_t*)97, 128);	// 128 //1 = 200 us
	eeprom_write_byte((uint8_t*)98, 128);	// 128 //1 = 200 us
	
	ISR_timeShiftOn[PHASE_A] =  eeprom_read_byte((uint8_t*)96);	// 128 //1 = 200 us
	ISR_timeShiftOn[PHASE_B] =  eeprom_read_byte((uint8_t*)97);	// 128 //1 = 200 us
	ISR_timeShiftOn[PHASE_C] =  eeprom_read_byte((uint8_t*)98);	// 128 //1 = 200 us
	
	
	
	eeprom_write_byte((uint8_t*)99,100);	// 100
	switchBounceSilense = eeprom_read_byte((uint8_t*)99);	// 100
	
	
	eeprom_write_word((uint16_t*)101,166);		// 34 ms
	eeprom_write_word((uint16_t*)103,196);		// 40 ms
	eeprom_write_word((uint16_t*)105,318);		// 65 ms
	eeprom_write_word((uint16_t*)107,343);		// 70 ms
	eeprom_write_word((uint16_t*)109,392);		// 80 ms
	eeprom_write_word((uint16_t*)111,441);		// 90 ms
	eeprom_write_word((uint16_t*)113,392);		// 80 ms
	eeprom_write_word((uint16_t*)115,441);		// 90 ms

	
	
	ownTimeOffWarn_ISR = eeprom_read_word((uint16_t*)101);		// 34 ms
	ownTimeOffAlarm_ISR = eeprom_read_word((uint16_t*)103);		// 40 ms
	fullTimeOffWarn_ISR = eeprom_read_word((uint16_t*)105);		// 65 ms
	fullTimeOffAlarm_ISR = eeprom_read_word((uint16_t*)107);	// 70 ms
	fullTimeOnWarn_ISR = eeprom_read_word((uint16_t*)109);		// 80 ms
	fullTimeOnAlarm_ISR = eeprom_read_word((uint16_t*)111);		// 90 ms
	ownTimeOnWarn_ISR = eeprom_read_word((uint16_t*)113);		// 80 ms
	ownTimeOnAlarm_ISR = eeprom_read_word((uint16_t*)115);		// 90 ms

	for (uint8_t pointer_ = 1; pointer_ <= NUM_CHANNELS; pointer_++ )
	{
		calibr_X[2*pointer_ - 1] = 0;
		calibr_Y[2*pointer_ - 1] = 0;
		
		calibr_X[2*pointer_] = 10;
		calibr_Y[2*pointer_] = 10;
	}


	for (uint8_t j = 1; j <= NUM_CHANNELS; j++ )
	calibrToEEPROM(j);
	
	
	eeprom_write_word((uint16_t*)202,0);		// 34 ms
	eeprom_write_word((uint16_t*)204,0);		// 40 ms
	eeprom_write_word((uint16_t*)206,0);		// 65 ms
	eeprom_write_word((uint16_t*)208,0);		// 70 ms
	eeprom_write_word((uint16_t*)210,0);		// 80 ms
	eeprom_write_word((uint16_t*)212,0);		// 90 ms
	

	params.power_Off_cntr_A = eeprom_read_word((uint16_t*)202);	// 0
	params.power_On_cntr_A = eeprom_read_word((uint16_t*)204);		// 0
	params.power_Off_cntr_B = eeprom_read_word((uint16_t*)206);
	params.power_On_cntr_B = eeprom_read_word((uint16_t*)208);	// 0
	params.power_Off_cntr_C = eeprom_read_word((uint16_t*)210);		// 0
	params.power_On_cntr_C = eeprom_read_word((uint16_t*)212);

	params.power_OnOff_cntr_A = params.power_Off_cntr_A + params.power_On_cntr_A;
	params.power_OnOff_cntr_B = params.power_Off_cntr_B + params.power_On_cntr_B;
	params.power_OnOff_cntr_C = params.power_Off_cntr_C + params.power_On_cntr_C;

	//	params.mechanical_wear_Perc_A = 1.0 * (params.power_OnOff_cntr_A + params.power_OnOff_cntr_B) * 50.0 / max_mechanical_wear;
	params.mechanical_wear_Perc_A =	(float)params.power_OnOff_cntr_A * 100.0f/ (float)max_mechanical_wear;
	params.mechanical_wear_Perc_B =	(float)params.power_OnOff_cntr_B * 100.0f/ (float)max_mechanical_wear;
	params.mechanical_wear_Perc_C =	(float)params.power_OnOff_cntr_C * 100.0f/ (float)max_mechanical_wear;



	eeprom_write_word((uint16_t*)220,0);		// 90 ms
	noiseCancellMode = (filter_mode_t)eeprom_read_word((uint16_t*)220);


	eeprom_write_word((uint16_t*)222,6);		// 90 ms
	arc_consistency_len = eeprom_read_word((uint16_t*)222);



	
	
	for (uint8_t i = 0; i < phase_quantity; i++)   // Загрузка данных кривой y = kx - b;
	{
		eeprom_write_byte((uint8_t*)226+i,0);	// 100
		eeprom_write_byte((uint8_t*)229+i,0);	// 100
	}
	
	for (uint8_t i = 0; i < phase_quantity; i++)   // Загрузка данных кривой y = kx - b;
	{
		on_contact_lag_ticks[i] = eeprom_read_byte((uint8_t*)226+i);   /* лаг доп.контакта на ВКЛ, тики (по фазам) */
		off_contact_lag_ticks[i] = eeprom_read_byte((uint8_t*)229+i);  /* ранний уход доп.контакта на ВЫКЛ, тики (по фазам) */
	}
	

	loadDefaultsDone = 1;

}

uint8_t calcSwitchCurrentFlag = 0;



uint8_t calcRunCnt = 0;



void apply_filter_all(filter_mode_t mode);

void calculations()
{
	
	if ((transferDataToLCP == 0) && (positionChanged == standingBy)) // очищаем флаг новых данных в буфере
	{
		//// fram_write_byte(7500, 29);
		// ВЫКЛЮЧИТЬ буферный LED (active-HIGH)
		PORT_LED_BUF &= ~(1u << LED_BUF);
		

	}
	if (startCalculations != calcStarted)
	return;
	
	
	
	switch (calcSteps)
	{
		case 0:
		
		quickRMScnt = 0; // Сбрасываем счетчик быстрого RMS. То есть стартуем один замер RMS на 200 точек
		calcRunCnt++;

		osc_center_inplace_all();
		// ищем время окончания горения дуги
		
		
		apply_filter_all(noiseCancellMode);



		// fram_write_byte(7500, 20);
		finding_Current_Start_Point(); //done
		
		// fram_write_byte(7500, 21);
		// >>>>>>>>>>>>>>>>		// Ф-ция вычисляет полное время выключателя:  !!!!!!!!!!!!!!!!!!!!!!!
		calcFullTime(); //done
		
		// fram_write_byte(7500, 22);
		// 13, 14, 15 - Время горения дуги во время последнего отключения, мс.
		calcArcLifetime(); //done

		// fram_write_byte(7500, 23);
		calcSwitchCurrent(); //done

		// Поиск короткого замыкания

		// fram_write_byte(7500, 24);
		findShortCircuit();
		
		// fram_write_byte(7500, 25);
		// 19, 20, 21 - Отработанный ресурс во время последнего отключения, кА^2 * с.
		calcLast_kA2s(); //done
		
		
		// fram_write_byte(7500, 26);
		calcTimeStates();
		
		updateElectricalWear(); //done
		// 		// 10, 11, 12 - Суммарный отработанный электрический ресурс фазы, кА^2 * с.
		// 		params.ph_A_kA2s = params.ph_A_kA2s + last_kA2s[PHASE_A];
		// 		params.ph_B_kA2s = params.ph_B_kA2s + last_kA2s[PHASE_B];
		// 		params.ph_C_kA2s = params.ph_C_kA2s + last_kA2s[PHASE_C];
		// 		// 31, 32, 33 - Электрический износ за время последнего отключения, %.
		// 		params.ph_A_last_elec_perc = last_kA2s[PHASE_A] * 100.0 / max_electrical_wear;
		// 		params.ph_B_last_elec_perc = last_kA2s[PHASE_B] * 100.0 / max_electrical_wear;
		// 		params.ph_C_last_elec_perc = last_kA2s[PHASE_C] * 100.0 / max_electrical_wear;
		// 		// 28, 29, 30 - Суммарный электрический износ контактов фазы, %.
		// 		params.ph_A_electrical_Perc = params.ph_A_electrical_Perc + params.ph_A_last_elec_perc;
		// 		params.ph_B_electrical_Perc = params.ph_B_electrical_Perc + params.ph_B_last_elec_perc;
		// 		params.ph_C_electrical_Perc = params.ph_C_electrical_Perc + params.ph_C_last_elec_perc;
		//
		
		//		// fram_write_byte(7500, 27);
		calcSteps++;
		break;
		
		
		case 1:   // провести процессы медленной записи
		
		saveFloat(10, params.ph_A_kA2s);
		calcSteps++;
		break;
		
		case 2:   // провести процессы медленной записи
		
		
		saveFloat(14, params.ph_B_kA2s);
		calcSteps++;
		
		break;
		
		case 3:   // провести процессы медленной записи
		
		saveFloat(18, params.ph_C_kA2s);
		calcSteps++;
		break;

		case 4:   // провести процессы медленной записи
		
		eeprom_write_word((uint16_t*)22, params.power_OnOff_cntr_A);
		calcSteps++;
		break;
		
		case 5:   // провести процессы медленной записи
		
		eeprom_write_word((uint16_t*)24, params.power_OnOff_cntr_B);
		eeprom_write_word((uint16_t*)26, params.power_OnOff_cntr_C);
		calcSteps++;
		break;
		
		
		
		case 6:   // провести процессы медленной записи
		
		calcSteps++;
		transferDataToLCP = 1;
		startCalculations = calcFinished;
		// fram_write_byte(7500, 27);
		// ВКЛЮЧИТЬ буферный LED (active-HIGH)
		PORT_LED_BUF |=  (1u << LED_BUF);
		calcSteps = 0;
		
		break;
		
		
		
		case 7:   // Поднять флаг передачи данных в LCP для последующей записи на карту SD осциллограмм с временем и датой
		
		calcSteps++;
		break;
		
		case 8:   // Поднять флаг передачи данных в LCP для последующей записи на карту SD осциллограмм с временем и датой
		
		if (transferDataToLCP == 0) // очищаем флаг новых данных в буфере
		{
			//// fram_write_byte(7500, 29);
			// ВЫКЛЮЧИТЬ буферный LED (active-HIGH)
			PORT_LED_BUF &= ~(1u << LED_BUF);
		}
		
		
		break;
		
		default:

		//	calcSteps = 0;

		// 		// пометить секцию как обработанную (или EMPTY, если хочешь освобождать слоты)
		// 		if (g_proc_idx != 0xFF) {
		// 			osc_mark_status(g_proc_idx, OSC_STAT_PROCESSED);  // или OSC_STAT_EMPTY
		// 			g_proc_idx = 0xFF;
		// 		}

		// расчёты завершены — освободить «машину»

		break;
		
	}



}


/**
* @brief Обновляет накопленный ресурс и проценты электрического износа.
*
* last_kA2s[PHASE_*] — ресурс за последний удар по фазам (кА²·с)
* max_electrical_wear — ресурс контакта на 100% (кА²·с)
*
* На выходе:
*  - params.ph_*_kA2s            += last_kA2s[*]
*  - params.ph_*_last_elec_perc   = last_kA2s[*] / max_electrical_wear * 100
*  - params.ph_*_electrical_Perc  = params.ph_*_kA2s / max_electrical_wear * 100   // <-- КЛЮЧЕВОЕ ИЗМЕНЕНИЕ
*/
static inline void updateElectricalWear(void)
{
	const uint8_t nPhases = phase_quantity;

	// защита от деления на ноль — при 0 просто выходим без изменений
	if (max_electrical_wear <= 0.0f) {
		return;
	}
	const float wear_den = max_electrical_wear;

	for (uint8_t ph = 0; ph < nPhases; ph++)
	{
		// 1) Накопление ресурса (кА²·с) — отрицательные не копим
		float last = last_kA2s[ph];
		if (last < 0.0f) last = 0.0f;

		switch (ph)
		{
			case PHASE_A: params.ph_A_kA2s += last; break;
			case PHASE_B: params.ph_B_kA2s += last; break;
			case PHASE_C: params.ph_C_kA2s += last; break;
			default: break;
		}

		// 2) Процент за последний удар (информационно)
		float perc_last = (last / wear_den) * 100.0f;
		if (perc_last < 0.0f) perc_last = 0.0f;

		switch (ph)
		{
			case PHASE_A:
			params.ph_A_last_elec_perc = perc_last;
			// 3) Итоговый процент — из накопленного ресурса
			params.ph_A_electrical_Perc = (params.ph_A_kA2s / wear_den) * 100.0f;
			break;

			case PHASE_B:
			params.ph_B_last_elec_perc = perc_last;
			params.ph_B_electrical_Perc = (params.ph_B_kA2s / wear_den) * 100.0f;
			break;

			case PHASE_C:
			params.ph_C_last_elec_perc = perc_last;
			params.ph_C_electrical_Perc = (params.ph_C_kA2s / wear_den) * 100.0f;
			break;

			default: break;
		}
	}

	// Опционально «крышевать» итог сверху (например, 100%):
	// params.ph_A_electrical_Perc = fminf(params.ph_A_electrical_Perc, 100.0f);
	// params.ph_B_electrical_Perc = fminf(params.ph_B_electrical_Perc, 100.0f);
	// params.ph_C_electrical_Perc = fminf(params.ph_C_electrical_Perc, 100.0f);

	// Если при auxType==1 нужно зеркалить A→B/C — делайте это в месте формирования last_kA2s,
	// чтобы здесь логика была единая и без дубликатов.
}





// Ф-ция настраивает необходимые выводы МК на нужные режимы работы (вход или выход):
void gpio_init(void){


	// Current transformers pins

	set_pin_input(DDR_ADC_CT1, ADC_CT1);
	set_pin_input(DDR_ADC_CT2, ADC_CT2);
	set_pin_input(DDR_ADC_CT3, ADC_CT3);

	set_pin_output(DDR_ADC_CT1_GAIN, ADC_CT1_GAIN);
	set_pin_output(DDR_ADC_CT2_GAIN, ADC_CT2_GAIN);
	set_pin_output(DDR_ADC_CT3_GAIN, ADC_CT3_GAIN);

	pullup_off(PORT_ADC_CT1, ADC_CT1);
	pullup_off(PORT_ADC_CT2, ADC_CT2);
	pullup_off(PORT_ADC_CT3, ADC_CT3);
	
	
	// Current coils pins
	set_pin_input(DDR_ADC_A1, ADC_A1);
	set_pin_input(DDR_ADC_A2, ADC_A2);
	set_pin_input(DDR_ADC_A3, ADC_A3);
	
	set_pin_input(DDR_ADC_A4, ADC_A4);
	set_pin_input(DDR_ADC_A5, ADC_A5);
	set_pin_input(DDR_ADC_A6, ADC_A6);
	
	set_pin_input(DDR_ADC_A7, ADC_A7);
	set_pin_input(DDR_ADC_A8, ADC_A8);
	//	set_pin_input(DDR_ADC_A9, ADC_A9);

	pullup_off(PORT_ADC_A1, ADC_A1);
	pullup_off(PORT_ADC_A2, ADC_A2);
	pullup_off(PORT_ADC_A3, ADC_A3);
	
	pullup_off(PORT_ADC_A4, ADC_A4);
	pullup_off(PORT_ADC_A5, ADC_A5);
	pullup_off(PORT_ADC_A6, ADC_A6);
	pullup_off(PORT_ADC_A7, ADC_A7);
	pullup_off(PORT_ADC_A8, ADC_A8);
	//	pullup_off(PORT_ADC_A9, ADC_A9);

	// 	set_pin_high(PORT_ADC_CT1_GAIN, ADC_CT1_GAIN);
	// 	set_pin_high(PORT_ADC_CT2_GAIN, ADC_CT2_GAIN);
	// 	set_pin_high(PORT_ADC_CT3_GAIN, ADC_CT3_GAIN);
	// 	cts.amplification = 1;
	
	// 	set_pin_low(PORT_ADC_CT1_GAIN, ADC_CT1_GAIN);
	// 	set_pin_low(PORT_ADC_CT2_GAIN, ADC_CT2_GAIN);
	// 	set_pin_low(PORT_ADC_CT3_GAIN, ADC_CT3_GAIN);
	// 	cts.amplification = 0;
	
	
	
	//	FRAM START
	// + Hardware SPI Section:
	{
		set_pin_output(DDR_MOSI, MOSI);
		set_pin_input(DDR_MISO, MISO);
		set_pin_output(DDR_SCK, SCK);
		set_pin_output(DDR_SS, SS);
		set_pin_output(DDR_FRAM_CS, FRAM_CS);

		set_pin_low(PORT_MOSI, MOSI);
		pullup_off(PORT_MISO, MISO);
		set_pin_low(PORT_SCK, SCK);
		set_pin_high(PORT_SS, SS);
		set_pin_high(PORT_FRAM_CS, FRAM_CS);
	}
	//	FRAM FIN
	
	
	set_pin_output(DDR_ADC_CS, ADC_CS);
	set_pin_output(DDR_ADC_CLK, ADC_CLK);

	set_pin_high(PORT_ADC_CS, ADC_CS);		// !!!!!
	set_pin_high(PORT_ADC_CLK, ADC_CLK);		// !!!!!


	/*
	DDR_TX_EN  |= (1<<TX_EN);		// Pin is OUTPUT
	PORT_TX_EN &= ~(1<< TX_EN); 	// Pin is LOW (enable RX mode)
	*/
	
	// Задаём пины как выходы
	DDR_LED_OK  |= (1<< LED_OK);
	DDR_LED_BUF |= (1<< LED_BUF);
	DDR_LED_X2X |= (1<< LED_X2X);

	// Гасим все светодиоды
	PORT_LED_OK  &= ~(1<< LED_OK);
	PORT_LED_BUF &= ~(1<< LED_BUF);
	PORT_LED_X2X &= ~(1<< LED_X2X);


	//	PORT_LED |= (1<< LED_PLC_OK);		// LED is ON
	//	PORT_LED |= (1<< LED_SD_OK);		// LED is ON

	// Digital Inputs (DI) enable:
	// 	DDR_DI_1 &= ~(1<<DI_1);			// Pins are INPUT
	// 	DDR_DI_2 &= ~(1<<DI_2);
	// 	DDR_DI_3 &= ~(1<<DI_3);
	// 	DDR_DI_4 &= ~(1<<DI_4);
	// 	DDR_DI_5 &= ~(1<<DI_5);
	// 	DDR_DI_6 &= ~(1<<DI_6);
	//
	// 	// Digital Inputs (DI) enable:
	// 	PORT_DI_1 |= (1<<DI_1);			// Pins are pulled-up
	// 	PORT_DI_2 |= (1<<DI_2);
	// 	PORT_DI_3 |= (1<<DI_3);
	// 	PORT_DI_4 |= (1<<DI_4);
	// 	PORT_DI_5 |= (1<<DI_5);
	// 	PORT_DI_6 |= (1<<DI_6);
	//
	// 	// Abs Encoder Inputs enable:
	// 	DDR_ENC	= 0b00000000;			// Pins are INPUT
	// 	PORT_ENC= 0b00000000;			// Pins are PULL-UP
	// 	DDRA &= ~(1<<PINA0);
	// 	DDRA &= ~(1<<PINA1);
	// 	PORTA &= ~(1<<PINE0);
	// 	PORTA &= ~(1<<PINE1);
	//
}


// Ф-ция настраивает необходимые параметры протокола Modbus и сопутствующих подсистем (UART):
void comm_init(void){
	//	uint32_t	baud_;
	//	uint8_t		parity_ = 0;
	//
	//	baud_ =  getBaud(baudRate);						// из Eeprom ранее взяли байт. Байт превращаем в baud rate
	//
	//	if (parity == 1)								// Четность - none или Even, можно добавить и Odd
	//	parity_ = SERIAL_8E1;
	//	else
	//	parity_ = SERIAL_8N1;

	Serial1.begin(9600, SERIAL_8N1);

	modbus.init(9600);								// Инициируем Modbus
	modbus.setDelegate(modbusProceed);				// To run library function from main loop
	modbus.slave_id = mb_slave_addr;				// Задание Slave адреса модуля
	
}


// Проверка состояния связи по протоколу Modbus:
void modbus_timeout_check()
{
	if (currentTime - reset_wd_timer >= CONN_TIMEOUT) x2x_state = 0;	// Watchdog - таймаут по времени - нет входящих пакетов по Rs485
	
	if (x2x_state == 0) PORT_LED_X2X ^= (1<< LED_X2X);					// Если входящих на адрес X2X нет, то мигаем X2X-светодиодом
}

static inline void mb_write_float(int *regs, int base, float v)
{
	regs[0] = convertFloat(v, 0);
	regs[1] = convertFloat(v, 1);
}

static inline float ticks_to_us(uint32_t ticks)
{
	return (float)ticks * (float)SAMPLE_PERIOD_US;   // мкс
}

static inline float ticks_to_ms(uint32_t ticks)
{
	return (float)ticks * (float)SAMPLE_PERIOD_US / 1000.0f; // мс
}

static inline float peak_codes_to_amp(uint8_t phase, uint16_t peak_codes)
{
	return (float)peak_codes * k[phase];   // А
}



// Выбрать индекс "видимой" секции для Modbus.
// Простой policy: если есть активная к расчёту – её; иначе последняя завершённая (lastStrike-1).
// Прототипы FRAM (если уже видны — дубли не вставлять)
extern void     fram_read_array(uint16_t addr, uint8_t* dst, uint16_t length);

// Чтение одного байта из FRAM
static inline uint8_t fram_read_u8(uint16_t addr)
{
	uint8_t val;
	fram_read_array(addr, &val, 1);
	return val;
}


// Выбрать индекс секции для выдачи по Modbus и прочитать её заголовок.
// Политика: если есть секция в PROCESSING — показываем её; иначе последняя завершённая (lastStrike-1).
// static bool osc_pick_view_idx(uint8_t* out_idx, osc_strike_header_t* out_hdr)
// {
// 	uint8_t idx;
//
// 	if (g_proc_idx != 0xFF) {
// 		idx = g_proc_idx;
// 		} else {
// 		idx = (uint8_t)((lastStrike + STRIKES_MAX - 1u) % STRIKES_MAX);
// 	}
//
// 	const uint16_t base = STRIKE_BASE_ADDR(idx);
// 	uint8_t status = fram_read_u8((uint16_t)(base + OSC_HDR_STATUS_OFFS));
//
// 	if (status == OSC_STAT_EMPTY) {
// 		return false; // показывать нечего
// 	}
//
// 	uint8_t raw[FRAM_HDR_SIZE];
// 	fram_read_array(base, raw, FRAM_HDR_SIZE);
//
// 	const osc_strike_header_t* hdr = (const osc_strike_header_t*)raw;
// 	if (out_idx) *out_idx = idx;
// 	if (out_hdr) *out_hdr = *hdr;
//
// 	return true;
// }

//
// // Прочитать один сэмпл из FRAM → ток в деци-амперах (Q0.1), saturate к int16.
// static int16_t fram_sample_to_dA(uint8_t idx, const osc_strike_header_t *hdr, uint8_t phase, uint16_t sample_i)
// {
// 	// границы
// 	if (phase > 2) return 0;
// 	uint16_t written = hdr->written[phase];
// 	if (sample_i >= written) return 0;
//
// 	const uint16_t base = STRIKE_BASE_ADDR(idx);
// 	const uint16_t addr = PHASE_SAMPLE_ADDR(base, phase, sample_i);
// 	uint16_t codes = fram_read_u16_be(addr);     // сырое 0..4095 (или 0..N)
//
// 	// калибровка к амперам: A = k[phase]*codes + b[phase] (если b не нужен — поставь 0.f)
// 	float A = k[phase] * (float)codes; // + b[phase];
//
// 	// в деци-амперы
// 	int32_t dA = (int32_t)(A * 10.0f + (A >= 0 ? 0.5f : -0.5f));
// 	if (dA >  32767) dA =  32767;
// 	if (dA < -32768) dA = -32768;
// 	return (int16_t)dA;
// }

/* утилита: выдать слово HI/LO из uint32 */
static inline uint16_t reg_word_u32(uint32_t v, uint8_t hi)
{
	return hi ? (uint16_t)(v >> 16) : (uint16_t)(v & 0xFFFFu);
}

int64_t checkRMS = 0;

uint16_t ISR_us = OSC_REC_PERIOD * averPoints;
float sec2us = 1000.0/OSC_REC_PERIOD;

uint8_t testFRAM = 0;


// Выдача данных по протоколу Modbus:
void modbusProceed(int16_t *regs, uint16_t addr_, uint16_t reg_qty_, int16_t Value_)
{
	uint8_t	writeReg = 0;
	
	if (modbus.func == FC_WRITE_REGS || modbus.func == FC_WRITE_REG) writeReg = 1;

	if (x2x_state == 0)
	{
		x2x_state = 1;					// Connection is established
		PORT_LED_X2X |= (1<< LED_X2X);	// Turn corresponding LED on
	}

	reset_wd_timer = currentTime;

	for (uint8_t i = 0; i <= reg_qty_ - 1; i++){
		switch (addr_)
		{


			case 0:	regs[i] = curr_state; break;
			
			case 1: // 1 - состояние контактов, ошибок, последней операции и неполнофазной работы.
			
			regs[i] = complexSP;
			break;
			
			
			// 7, 8, 9 - Действующее значение тока RMS, А.
			case 2: regs[i] = convertFloat(params.RMS_A,0); break;
			case 3: regs[i] = convertFloat(params.RMS_A,1); break;

			case 4: regs[i] = convertFloat(params.RMS_B,0); break;
			case 5: regs[i] = convertFloat(params.RMS_B,1); break;

			case 6: regs[i] = convertFloat(params.RMS_C,0); break;
			case 7: regs[i] = convertFloat(params.RMS_C,1); break;
			

			// 8-13 - Общий отработанный ресурс, кА^2 * с.
			
			case 8:	regs[i] = convertFloat(params.ph_A_kA2s, 0); break;
			case 9:	regs[i] = convertFloat(params.ph_A_kA2s, 1); break;
			
			case 10: regs[i] = convertFloat(params.ph_B_kA2s, 0); break;
			case 11: regs[i] = convertFloat(params.ph_B_kA2s, 1); break;
			
			case 12: regs[i] = convertFloat(params.ph_C_kA2s, 0); break;
			case 13: regs[i] = convertFloat(params.ph_C_kA2s, 1); break;


			// 14 - 19 - Время горения дуги во время последнего отключения, мкс.
			case 14: regs[i] = convertFloat(ISR_arc_lifetime[PHASE_A],0); break;//convertFloat(ticks_to_us(ISR_arc_lifetime[PHASE_A]),0); break;
			case 15: regs[i] = convertFloat(ISR_arc_lifetime[PHASE_A],1); break;

			case 16: regs[i] = convertFloat(ISR_arc_lifetime[PHASE_B],0); break;
			case 17: regs[i] = convertFloat(ISR_arc_lifetime[PHASE_B],1); break;

			case 18: regs[i] = convertFloat(ISR_arc_lifetime[PHASE_C],0); break;
			case 19: regs[i] = convertFloat(ISR_arc_lifetime[PHASE_C],1); break;



			// 20 - 25  - Значение тока последнего отключения, А.
			
			case 20: regs[i] = convertFloat(peak_codes_to_amp(PHASE_A, switchCurrent_Amp[PHASE_A]),0); break;
			case 21: regs[i] = convertFloat(peak_codes_to_amp(PHASE_A, switchCurrent_Amp[PHASE_A]),1); break;

			case 22: regs[i] = convertFloat(peak_codes_to_amp(PHASE_B, switchCurrent_Amp[PHASE_B]),0); break;
			case 23: regs[i] = convertFloat(peak_codes_to_amp(PHASE_B, switchCurrent_Amp[PHASE_B]),1); break;

			case 24: regs[i] = convertFloat(peak_codes_to_amp(PHASE_C, switchCurrent_Amp[PHASE_C]),0); break;
			case 25: regs[i] = convertFloat(peak_codes_to_amp(PHASE_C, switchCurrent_Amp[PHASE_C]),1); break;

			
			

			// 26 - 31 - Выработанный электрический ресурс во время последнего отключения, кА^2 * с.
			case 26:
			regs[i] = convertFloat(last_kA2s[PHASE_A], 0);
			break;
			
			case 27:
			regs[i] = convertFloat(last_kA2s[PHASE_A], 1);
			break;
			
			case 28:
			regs[i] = convertFloat(last_kA2s[PHASE_B], 0);
			break;
			
			case 29:
			regs[i] = convertFloat(last_kA2s[PHASE_B], 1);
			break;
			
			case 30:
			regs[i] = convertFloat(last_kA2s[PHASE_C], 0);
			break;
			
			case 31:
			regs[i] = convertFloat(last_kA2s[PHASE_C], 1);
			break;
			
			// 32 - 37 - Счётчик операций с фазой кол-во включений
			case 32:
			regs[i] = convertFloat(params.power_OnOff_cntr_A, 0);
			break;
			
			case 33:
			regs[i] = convertFloat(params.power_OnOff_cntr_A, 1);
			break;
			
			case 34:
			regs[i] = convertFloat(params.power_OnOff_cntr_B, 0);
			break;
			
			case 35:
			regs[i] = convertFloat(params.power_OnOff_cntr_B, 1);
			break;
			
			case 36:
			regs[i] = convertFloat(params.power_OnOff_cntr_C, 0);
			break;
			
			case 37:
			regs[i] = convertFloat(params.power_OnOff_cntr_C, 1);
			break;
			
			
			// 38 - 43 - Механический износ контактов, %.!
			case 38:
			regs[i] = convertFloat(params.mechanical_wear_Perc_A, 0);
			break;
			
			case 39:
			regs[i] = convertFloat(params.mechanical_wear_Perc_A, 1);
			break;
			
			case 40:
			regs[i] = convertFloat(params.mechanical_wear_Perc_B, 0);
			break;
			
			case 41:
			regs[i] = convertFloat(params.mechanical_wear_Perc_B, 1);
			break;
			
			case 42:
			regs[i] = convertFloat(params.mechanical_wear_Perc_C, 0);
			break;
			
			case 43:
			regs[i] = convertFloat(params.mechanical_wear_Perc_C, 1);
			break;
			
			

			// 44 - 49 - Общий электрический износ контактов, %.

			case 44:
			regs[i] = convertFloat(params.ph_A_electrical_Perc, 0);
			break;
			
			case 45:
			regs[i] = convertFloat(params.ph_A_electrical_Perc, 1);
			break;
			
			case 46:
			regs[i] = convertFloat(params.ph_B_electrical_Perc, 0);
			break;
			
			case 47:
			regs[i] = convertFloat(params.ph_B_electrical_Perc, 1);
			break;
			
			case 48:
			regs[i] = convertFloat(params.ph_C_electrical_Perc, 0);
			break;
			
			case 49:
			regs[i] = convertFloat(params.ph_C_electrical_Perc, 1);
			break;
			
			

			// 31, 32, 33 - Электрический износ за время последнего отключения, %.
			case 50:
			regs[i] = convertFloat(params.ph_A_last_elec_perc, 0);
			break;
			
			case 51:
			regs[i] = convertFloat(params.ph_A_last_elec_perc, 1);
			break;
			
			case 52:
			regs[i] = convertFloat(params.ph_B_last_elec_perc, 0);
			break;
			
			case 53:
			regs[i] = convertFloat(params.ph_B_last_elec_perc, 1);
			break;
			
			case 54:
			regs[i] = convertFloat(params.ph_C_last_elec_perc, 0);
			break;
			
			case 55:
			regs[i] = convertFloat(params.ph_C_last_elec_perc, 1);
			break;
			
			
			// 68 - 73 - Полное время срабатывания выключателя, мс
			

			case 56: regs[i] = convertFloat(full_off_time_ISR[PHASE_A],0); break;
			case 57: regs[i] = convertFloat(full_off_time_ISR[PHASE_A],1); break;

			case 58: regs[i] = convertFloat(full_on_time_ISR[PHASE_A],0); break;
			case 59: regs[i] = convertFloat(full_on_time_ISR[PHASE_A],1); break;

			case 60: regs[i] = convertFloat(full_off_time_ISR[PHASE_B], 0);	break;
			case 61: regs[i] = convertFloat(full_off_time_ISR[PHASE_B], 1);	break;
			
			case 62: regs[i] = convertFloat(full_on_time_ISR[PHASE_B], 0); break;
			case 63: regs[i] = convertFloat(full_on_time_ISR[PHASE_B], 1); break;
			
			case 64: regs[i] = convertFloat(full_off_time_ISR[PHASE_C], 0);	break;
			case 65: regs[i] = convertFloat(full_off_time_ISR[PHASE_C], 1);	break;
			
			case 66: regs[i] = convertFloat(full_on_time_ISR[PHASE_C], 0); break;
			case 67: regs[i] = convertFloat(full_on_time_ISR[PHASE_C], 1); break;
			
			// Температура

			case 68:
			regs[i] = convertFloat(0, 0);
			break;
			
			case 69:
			regs[i] = convertFloat(0, 1);
			break;
			
			// количество включений при КЗ

			case 70:
			regs[i] = convertFloat(params.short_Cnt_A, 0);
			break;
			
			case 71:
			regs[i] = convertFloat(params.short_Cnt_A, 1);
			break;
			
			// количество выключений при КЗ

			case 72:
			regs[i] = convertFloat(params.short_Cnt_B, 0);
			break;
			
			case 73:
			regs[i] = convertFloat(params.short_Cnt_B, 1);
			break;
			
			// количество включений

			case 74:
			regs[i] = convertFloat(params.short_Cnt_C, 0);
			break;
			
			case 75:
			regs[i] = convertFloat(params.short_Cnt_C, 1);
			break;
			
			//	количество отключений
			
			case 76:
			regs[i] = convertFloat(params.power_OnOff_cntr_A, 0);
			break;
			
			case 77:
			regs[i] = convertFloat(params.power_OnOff_cntr_A, 1);
			break;

			//	Собственное время отключения ВВ превысило уставку
			
			

			case 78:
			regs[i] = convertFloat(0, 0);
			break;
			
			case 79:
			regs[i] = convertFloat(0, 1);
			break;
			
			
			case 80:
			regs[i] = convertFloat(0, 0);
			break;
			
			case 81:
			regs[i] = convertFloat(0, 1);
			break;
			
			
			//	Полное время отключения ВВ превысило уставку
			
			
			case 82:
			regs[i] = convertFloat(bitRead(complexSP, fullTimeOffAlarm_Bit), 0);
			break;
			
			case 83:
			regs[i] = convertFloat(bitRead(complexSP, fullTimeOffAlarm_Bit), 1);
			break;
			
			//	Полное время отключения ВВ превысило уставку

			case 84:
			regs[i] = convertFloat(bitRead(complexSP, fullTimeOnAlarm_Bit), 0);
			break;
			
			case 85:
			regs[i] = convertFloat(bitRead(complexSP, fullTimeOnAlarm_Bit), 1);
			break;



			case 86:
			{
				// Check if the corresponding bit in AI_TypeByte is set (1)
				if (AI_TypeByte & 0b00000001)
				regs[i] = convertFloat((cts.solenoidCurrOn_RAW - b[SOL_ON]) * k[SOL_ON], 0);
				else
				regs[i] = convertFloat((solenoidCurrOn[CALC] - b[SOL_ON]) * k[SOL_ON], 0);
			}
			break;
			
			case 87:
			{
				// Check if the corresponding bit in AI_TypeByte is set (1)
				if (AI_TypeByte & 0b00000001)
				regs[i] = convertFloat((cts.solenoidCurrOn_RAW - b[SOL_ON]) * k[SOL_ON], 1);
				else
				regs[i] = convertFloat((solenoidCurrOn[CALC] - b[SOL_ON]) * k[SOL_ON], 1);
			}
			break;
			
			
			case 88:
			{
				// Check if the corresponding bit in AI_TypeByte is set (1)
				if (AI_TypeByte & 0b00000010)
				regs[i] = convertFloat((cts.solenoidCurrOff_1_RAW - b[SOL_OFF_1]) * k[SOL_OFF_1], 0);
				else
				regs[i] = convertFloat((solenoidCurrOff_1[CALC] - b[SOL_OFF_1]) * k[SOL_OFF_1], 0);
			}
			break;
			
			case 89:
			{
				// Check if the corresponding bit in AI_TypeByte is set (1)
				if (AI_TypeByte & 0b00000010)
				regs[i] = convertFloat((cts.solenoidCurrOff_1_RAW - b[SOL_OFF_1]) * k[SOL_OFF_1], 1);
				else
				regs[i] = convertFloat((solenoidCurrOff_1[CALC] - b[SOL_OFF_1]) * k[SOL_OFF_1], 1);
			}
			break;
			
			
			case 90:
			{
				// Check if the corresponding bit in AI_TypeByte is set (1)
				if (AI_TypeByte & 0b00000100)
				regs[i] = convertFloat((cts.solenoidCurrOff_2_RAW - b[SOL_OFF_2]) * k[SOL_OFF_1], 0);
				else
				regs[i] = convertFloat((solenoidCurrOff_2[CALC] - b[SOL_OFF_2]) * k[SOL_OFF_1], 0);
			}
			break;
			
			case 91:
			{
				// Check if the corresponding bit in AI_TypeByte is set (1)
				if (AI_TypeByte & 0b00000100)
				regs[i] = convertFloat((cts.solenoidCurrOff_2_RAW - b[SOL_OFF_2]) * k[SOL_OFF_2], 1);
				else
				regs[i] = convertFloat((solenoidCurrOff_2[CALC] - b[SOL_OFF_2]) * k[SOL_OFF_2], 1);
			}
			break;
			// 			case 86:
			// 			regs[i] = convertFloat((solenoidCurrOn[CALC] - b[SOL_ON]) * k[SOL_ON], 0);
			// 			break;
			//
			// 			case 87:
			// 			regs[i] = convertFloat((solenoidCurrOn[CALC] - b[SOL_ON]) * k[SOL_ON], 1);
			// 			break;
			//
			//
			// 			case 88:
			// 			regs[i] = convertFloat((solenoidCurrOff_1[CALC] - b[SOL_OFF_1]) * k[SOL_OFF_1], 0);
			// 			break;
			//
			// 			case 89:
			// 			regs[i] = convertFloat((solenoidCurrOff_1[CALC] - b[SOL_OFF_1])  * k[SOL_OFF_1], 1);
			// 			break;
			//
			//
			// 			case 90:
			// 			regs[i] = convertFloat((solenoidCurrOff_2[CALC] - b[SOL_OFF_2]) * k[SOL_OFF_2], 0);
			// 			break;
			//
			// 			case 91:
			// 			regs[i] = convertFloat((solenoidCurrOff_2[CALC] - b[SOL_OFF_2]) * k[SOL_OFF_2], 1);
			// 			break;
			
			
			
			case 92:
			regs[i] = contacts_state_raw; //convertFloat((solenoidCurrOff_2[CALC] - b[SOL_OFF_2]) * k[SOL_OFF_2], 0);
			break;
			
			case 93:
			regs[i] = prev_state; //convertFloat((solenoidCurrOff_2[CALC] - b[SOL_OFF_2]) * k[SOL_OFF_2], 1);
			break;
			
			
			// Slave address:
			case 100:
			if (writeReg){
				
				if (Value_ >0 && Value_<254)
				mb_slave_addr = Value_;
				else
				mb_slave_addr = 1;
				eeprom_write_byte((uint8_t*)100,mb_slave_addr);
				modbus.slave_id = mb_slave_addr;
			};
			regs[i] = mb_slave_addr;
			break;

			
			
			
			
			
			
			// 56 - 61 - Собственное время срабатывания выключателя, мс
			
			// 			case 184:
			// 			regs[i] = convertFloat(full_off_time_ISR[PHASE_A], 0);
			// 			break;
			//
			// 			case 185:
			// 			regs[i] = convertFloat(full_off_time_ISR[PHASE_A], 1);
			// 			break;
			//
			// 			case 186:
			// 			regs[i] = convertFloat(full_on_time_ISR[PHASE_A], 0);
			// 			break;
			//
			// 			case 187:
			// 			regs[i] = convertFloat(full_on_time_ISR[PHASE_A], 1);
			// 			break;
			//
			// 			case 188:
			// 			regs[i] = convertFloat(full_off_time_ISR[PHASE_B], 0);
			// 			break;
			//
			// 			case 189:
			// 			regs[i] = convertFloat(full_off_time_ISR[PHASE_B], 1);
			// 			break;
			//
			//
			// 			case 190:
			// 			regs[i] = convertFloat(full_on_time_ISR[PHASE_B], 0);
			// 			break;
			//
			// 			case 191:
			// 			regs[i] = convertFloat(full_on_time_ISR[PHASE_B], 1);
			// 			break;
			//
			// 			case 192:
			// 			regs[i] = convertFloat(full_off_time_ISR[PHASE_C], 0);
			// 			break;
			//
			// 			case 193:
			// 			regs[i] = convertFloat(full_off_time_ISR[PHASE_C], 1);
			// 			break;
			//
			// 			case 194:
			// 			regs[i] = convertFloat(full_on_time_ISR[PHASE_C], 0);
			// 			break;
			//
			// 			case 195:
			// 			regs[i] = convertFloat(full_on_time_ISR[PHASE_C], 1);
			// 			break;
			//
			//
			

			/*
			case :
			regs[i] = ;
			break;
			*/
			case 198:
			{
				regs[i] = PHASE_SAMPLES_MAX; // Время одной точки
			}


			break;
			case 199:
			{
				regs[i] = SAMPLE_PERIOD_US; // Время одной точки
			}
			break;

			// ----- snb c0d3 b3g1n:
			// Buffer 1 begin:
			// 			case 200 ... 395:	// Осциллограммы фазы 'A'
			// 			regs[i] = cts.calcOscBuffer[addr_ - 200][PHASE_A];
			// 			break;
			//
			// 			case 400 ... 595:	// Осциллограммы фазы 'B'
			// 			regs[i] = cts.calcOscBuffer[addr_ - 400][PHASE_B];
			// 			break;
			//
			// 			case 600 ... 795:	// Осциллограммы фазы 'C'
			// 			regs[i] = cts.calcOscBuffer[addr_ - 600][PHASE_C];
			// 			break;
			//
			
			
			// 			case 800:	// Осциллограммы фазы 'C'
			// 			regs[i] = convertFloat(k[PHASE_A],0);
			// 			break;
			//
			// 			case 801:	// Осциллограммы фазы 'C'
			// 			regs[i] = convertFloat(k[PHASE_A],1);
			// 			break;
			//
			//
			// 			case 802:	// Осциллограммы фазы 'C'
			// 			regs[i] = convertFloat(k[PHASE_B],0);
			// 			break;
			//
			// 			case 803:	// Осциллограммы фазы 'C'
			// 			regs[i] = convertFloat(k[PHASE_B],1);
			// 			break;
			//
			// 			case 804:	// Осциллограммы фазы 'C'
			// 			regs[i] = convertFloat(k[PHASE_C],0);
			// 			break;
			//
			// 			case 805:	// Осциллограммы фазы 'C'
			// 			regs[i] = convertFloat(k[PHASE_C],1);
			// 			break;
			//
			
			
			
			
			
			
			case 850:	// Если в регистре 1 - есть новые данные для считывания. Если записать 0 в регистр, то при наличии ожидающего входного буффера - буффер будет отправлен в обработку.
			
			if (writeReg)
			transferDataToLCP = Value_;
			
			regs[i] = transferDataToLCP;
			break;
			
			case 851:	// сколько буфферов занято данными
			
			regs[i] = buffer_new_data_Tail;
			break;
			
			
			case 852:	// Запуск эмуляции коммутации
			
			if (writeReg)
			shootBreakerFlag = Value_;
			regs[i] = shootBreakerFlag;
			
			break;
			
			case 853:	// выбор ручного режима управления концевиками (force замена сигнала с физический концевиков)
			if (writeReg)
			manualMode = Value_;
			regs[i] = manualMode;
			
			break;
			
			case 854:	// текущий режим. 0 - режим ожидания коммутации. 1 - осциллографирование
			regs[i] = 0;
			break;
			
			case 855: // свободная оперативная память контроллера.
			regs[i] = FREE_RAM();
			break;
			
			
			case 856: // отладочный байт - для поиска зависания.
			
			regs[i] = fram_read_byte(7500);
			break;
			
			
			
			case 890 ... 899:	// сервисные регистры
			regs[i] = addr_ + 100;
			break;
			
			
			case 900:	// перезагрузить в загрузочный сектор
			
			if (writeReg)
			{
				rebootLCT = currentTime;
				regs[i] = 1;
			}
			else
			regs[i] = 0;
			
			break;
			
			case 990:
			
			regs[i] = CB_State[PHASE_A];
			
			break;
			
			case 991:
			
			regs[i] = CB_State[PHASE_B];
			
			break;
			
			case 992:
			
			regs[i] = CB_State[PHASE_C];
			
			break;
			
			case 993:
			
			regs[i] = lastOperation;
			
			break;
			
			
			case 994:
			
			regs[i] = osc_Point;
			break;
			
			
			
			case 996: // количество дребезга
			
			regs[i] = contactShakeCNT;
			
			break;
			
			case 997: // количество пропущенных точек в осциллограмме
			
			regs[i] = strikeLostData;  // Heart Bit
			
			break;
			
			case 998: // время блокировки дребезга контактов после коммутации. мс
			if (writeReg)
			{
				switchBounceSilense = Value_;
				eeprom_write_byte((uint8_t*)99, switchBounceSilense);
			}
			regs[i] = switchBounceSilense;  // Heart Bit
			
			break;
			
			break;
			
			case 999:  // Load default settings
			
			if (writeReg)
			{
				loadDefaults();
			}
			regs[i] = loadDefaultsDone;
			
			break;
			
			// Общий отработанный ресурс, кА^2 * с. Задание стартовых значений
			
			case 1000:
			
			if (writeReg)
			{
				eeprom_write_byte((byte*)12,(Value_ >> 8));
				eeprom_write_byte((byte*)13,(Value_ & 0x00FF));
			}
			
			regs[i] = convertFloat(params.ph_A_kA2s, 0);
			
			break;
			
			case 1001:
			
			if (writeReg)
			{
				eeprom_write_byte((byte*)10,(Value_ >> 8));
				eeprom_write_byte((byte*)11,(Value_ & 0x00FF));
				params.ph_A_kA2s = loadFloat(10, 0);
			}
			
			regs[i] = convertFloat(params.ph_A_kA2s, 1);

			break;
			
			
			case 1002:// Общий отработанный ресурс, кА^2 * с. Задание стартовых значений
			
			if (writeReg)
			{
				eeprom_write_byte((byte*)16,(Value_ >> 8));
				eeprom_write_byte((byte*)17,(Value_ & 0x00FF));
			}
			
			regs[i] = convertFloat(params.ph_B_kA2s, 0);
			
			break;
			
			case 1003:
			
			if (writeReg)
			{
				eeprom_write_byte((byte*)14,(Value_ >> 8));
				eeprom_write_byte((byte*)15,(Value_ & 0x00FF));
				params.ph_B_kA2s = loadFloat(14, 0);
			}
			
			regs[i] = convertFloat(params.ph_B_kA2s, 1);

			break;
			
			
			// Общий отработанный ресурс, кА^2 * с. Задание стартовых значений
			
			case 1004:
			
			if (writeReg)
			{
				eeprom_write_byte((byte*)20,(Value_ >> 8));
				eeprom_write_byte((byte*)21,(Value_ & 0x00FF));
			}
			
			regs[i] = convertFloat(params.ph_C_kA2s, 0);
			
			break;
			
			case 1005:
			
			if (writeReg)
			{
				eeprom_write_byte((byte*)18,(Value_ >> 8));
				eeprom_write_byte((byte*)19,(Value_ & 0x00FF));
				params.ph_C_kA2s = loadFloat(18,0);
			}
			
			regs[i] = convertFloat(params.ph_C_kA2s, 1);

			break;
			
			
			
			
			//  - Количество операций фаза А. Стартовые значения
			case 1010:
			if (writeReg)
			{
				params.power_Off_cntr_A = Value_;
				eeprom_write_word((uint16_t*)202, params.power_Off_cntr_A);
			}
			regs[i] = params.power_Off_cntr_A;
			
			break;
			
			//  - Количество включений.  Стартовые значения
			case 1011:
			if (writeReg)
			{
				params.power_On_cntr_A = Value_;
				eeprom_write_word((uint16_t*)204, params.power_On_cntr_A);
			}
			regs[i] = params.power_On_cntr_A;
			
			break;
			
			//  - Количество включений.  Стартовые значения
			case 1012:
			if (writeReg)
			{
				params.power_Off_cntr_B = Value_;
				eeprom_write_word((uint16_t*)206, params.power_Off_cntr_B);
			}
			regs[i] = params.power_Off_cntr_B;
			
			break;
			
			

			//  - Количество операций фаза А. Стартовые значения
			case 1013:
			if (writeReg)
			{
				params.power_On_cntr_B = Value_;
				eeprom_write_word((uint16_t*)208, params.power_On_cntr_B);
			}
			regs[i] = params.power_On_cntr_B;
			
			break;
			
			//  - Количество включений.  Стартовые значения
			case 1014:
			if (writeReg)
			{
				params.power_Off_cntr_C = Value_;
				eeprom_write_word((uint16_t*)210, params.power_Off_cntr_C);
			}
			regs[i] = params.power_Off_cntr_C;
			
			break;
			
			//  - Количество включений.  Стартовые значения
			case 1015:
			if (writeReg)
			{
				params.power_On_cntr_C = Value_;
				eeprom_write_word((uint16_t*)212, params.power_On_cntr_C);
			}
			regs[i] = params.power_On_cntr_C;
			
			break;
			
			case 1016: // Максимальный электрический износ. Уставка A, кА²·с
			if (writeReg)
			{
				max_electrical_wear = Value_;
				eeprom_write_word((uint16_t*)28, max_electrical_wear);
			}
			regs[i] = max_electrical_wear;
			
			break;
			
			case 1017:// Максимальный механический износ. Уставка
			if (writeReg)
			{
				max_mechanical_wear = Value_;
				eeprom_write_word((uint16_t*)30, max_mechanical_wear);
			}
			regs[i] = max_mechanical_wear;
			
			break;


			case 1018:
			if (writeReg)
			{
				ISR_timeShiftOff[PHASE_A] = Value_; // 1 = 200 us,
				eeprom_write_word((uint16_t*)32, ISR_timeShiftOff[PHASE_A]);
			}
			regs[i] = ISR_timeShiftOff[PHASE_A];
			
			break;
			
			case 1019:
			if (writeReg)
			{
				ISR_timeShiftOff[PHASE_B] = Value_;
				eeprom_write_word((uint16_t*)34, ISR_timeShiftOff[PHASE_B]);
			}
			regs[i] = ISR_timeShiftOff[PHASE_B];
			
			break;
			
			
			case 1020:
			if (writeReg)
			{
				ISR_timeShiftOff[PHASE_C] = Value_;
				eeprom_write_word((uint16_t*)36, ISR_timeShiftOff[PHASE_C]);
			}
			regs[i] = ISR_timeShiftOff[PHASE_C];
			
			break;
			
			
			case 1021:
			if (writeReg)
			{
				cts.adcShift_A = Value_;
				eeprom_write_byte((uint8_t*)38, cts.adcShift_A);
			}
			regs[i] = cts.adcShift_A;
			
			break;
			
			case 1022:
			if (writeReg)
			{
				cts.adcShift_B = Value_;
				eeprom_write_byte((uint8_t*)39, cts.adcShift_B);
			}
			regs[i] = cts.adcShift_B;
			
			break;
			
			
			case 1023:
			if (writeReg)
			{
				cts.adcShift_C = Value_;
				eeprom_write_byte((uint8_t*)40, cts.adcShift_C);
			}
			regs[i] = cts.adcShift_C;
			
			break;


			case 1024:
			if (writeReg)
			{
				cts.gainAdcShift_A = Value_;
				eeprom_write_byte((uint8_t*)41, cts.gainAdcShift_A);
			}
			regs[i] = cts.gainAdcShift_A;
			
			break;
			
			case 1025:
			if (writeReg)
			{
				cts.gainAdcShift_B = Value_;
				eeprom_write_byte((uint8_t*)42, cts.gainAdcShift_B);
			}
			regs[i] = cts.gainAdcShift_B;
			
			break;
			
			
			case 1026:
			if (writeReg)
			{
				cts.gainAdcShift_C = Value_;
				eeprom_write_byte((uint8_t*)43, cts.gainAdcShift_C);
			}
			regs[i] = cts.gainAdcShift_C;
			
			break;

			
			case 1027:
			if (writeReg)
			{
				eeprom_write_byte((uint8_t*)45,Value_);	// 128
				AI_TypeByte = Value_;
			}
			regs[i] = AI_TypeByte;
			break;

			
			case 1039:
			
			if (writeReg)
			{
				params.short_Cnt_A = Value_;
				eeprom_write_word((uint16_t*)74,params.short_Cnt_A);
			}
			
			regs[i] = params.short_Cnt_A;
			
			break;
			
			
			case 1040:
			
			if (writeReg)
			{
				params.short_Cnt_B = Value_;
				eeprom_write_word((uint16_t*)76,params.short_Cnt_B);
			}
			
			regs[i] = params.short_Cnt_B;
			
			break;
			
			case 1041:
			
			if (writeReg)
			{
				params.short_Cnt_C = Value_;
				eeprom_write_word((uint16_t*)78,params.short_Cnt_C);
			}
			
			regs[i] = params.short_Cnt_C;
			
			break;
			
			case 1042:
			if (writeReg)
			{
				eeprom_write_byte((uint8_t*)68,Value_ );
				params.minFailCurrent = Value_;
			}
			regs[i] = params.minFailCurrent;
			break;
			

			case 1043:
			
			if (writeReg)
			{
				eeprom_write_byte((uint8_t*)71,(Value_ >> 8));
				eeprom_write_byte((uint8_t*)72,(Value_ & 0x00FF));
			}
			
			regs[i] = convertFloat(params.overCurrentKoef, 0);
			
			break;
			
			case 1044:
			
			if (writeReg)
			{
				eeprom_write_byte((uint8_t*)69,(Value_ >> 8));
				eeprom_write_byte((uint8_t*)70,(Value_ & 0x00FF));
				params.overCurrentKoef = loadFloat(69, 1.0);
			}
			
			regs[i] = convertFloat(params.overCurrentKoef, 1);
			

			break;
			
			
			
			case 1045:
			
			if (writeReg)
			{
				params.nominalCurrent_Amp = Value_;
				eeprom_write_word((uint16_t*)80, params.nominalCurrent_Amp);
			}
			regs[i] = params.nominalCurrent_Amp;
			
			break;
			
			// 			case 1046:
			// 			if (writeReg)
			// 			{
			// 				eeprom_write_word((uint16_t*)82,Value_);
			// 				fullTimeOffWarn_ISR = Value_;
			// 			}
			// 			regs[i] = fullTimeOffWarn_ISR;
			// 			break;
			//
			//
			// 			case 1047:
			// 			if (writeReg)
			// 			{
			// 				eeprom_write_word((uint16_t*)84,Value_);
			// 				fullTimeOffAlarm_ISR = Value_;
			// 			}
			// 			regs[i] = fullTimeOffAlarm_ISR;
			// 			break;
			//
			// 			case 1048:
			// 			if (writeReg)
			// 			{
			// 				eeprom_write_word((uint16_t*)86,Value_);
			// 				ownTimeOffWarn_ISR = Value_;
			// 			}
			// 			regs[i] = ownTimeOffWarn_ISR;
			// 			break;
			//
			// 			case 1049:
			// 			if (writeReg)
			// 			{
			// 				eeprom_write_word((uint16_t*)88,Value_);
			// 				ownTimeOffAlarm_ISR = Value_;
			// 			}
			// 			regs[i] = ownTimeOffAlarm_ISR;
			// 			break;

			
			
			case 1050:
			
			if (writeReg)
			{
				if (Value_ == 1)
				{
					eeprom_write_byte((uint8_t*)44,1);
					set_pin_high(PORT_ADC_CT1_GAIN, ADC_CT1_GAIN);
					set_pin_high(PORT_ADC_CT2_GAIN, ADC_CT2_GAIN);
					set_pin_high(PORT_ADC_CT3_GAIN, ADC_CT3_GAIN);
					cts.amplification = 1;
				}
				else
				{
					eeprom_write_byte((uint8_t*)44,0);
					set_pin_low(PORT_ADC_CT1_GAIN, ADC_CT1_GAIN);
					set_pin_low(PORT_ADC_CT2_GAIN, ADC_CT2_GAIN);
					set_pin_low(PORT_ADC_CT3_GAIN, ADC_CT3_GAIN);
					cts.amplification = 0;
				}
			}
			
			regs[i] = cts.amplification;
			
			break;
			
			
			
			case 1051:
			if (writeReg)
			{
				eeprom_write_byte((uint8_t*)90,Value_);
				auxType = Value_;
			}
			regs[i] = auxType;
			break;
			
			
			
			case 1052:
			if (writeReg)
			{
				eeprom_write_word((uint16_t*)91,Value_);
				min_Arc_Current = Value_;
			}
			regs[i] = min_Arc_Current;
			break;
			
			
			
			case 1053:
			if (writeReg)
			{
				eeprom_write_byte((uint8_t*)93,Value_);
				RMS_Zero[PHASE_A] = Value_;
			}
			regs[i] = RMS_Zero[PHASE_A];
			break;
			
			case 1054:
			if (writeReg)
			{
				eeprom_write_byte((uint8_t*)94,Value_);
				RMS_Zero[PHASE_B] = Value_;
			}
			regs[i] = RMS_Zero[PHASE_B];
			break;
			
			case 1055:
			if (writeReg)
			{
				eeprom_write_byte((uint8_t*)95,Value_);
				RMS_Zero[PHASE_C] = Value_;
			}
			regs[i] = RMS_Zero[PHASE_C];
			break;
			

			case 1056:
			if (writeReg)
			{
				ISR_timeShiftOn[PHASE_A] = Value_; // 1 = 200 us,
				eeprom_write_byte((uint8_t*)96, ISR_timeShiftOn[PHASE_A]);
			}
			regs[i] = ISR_timeShiftOn[PHASE_A];
			
			break;
			
			case 1057:
			if (writeReg)
			{
				ISR_timeShiftOn[PHASE_B] = Value_;
				eeprom_write_byte((uint8_t*)97, ISR_timeShiftOn[PHASE_B]);
			}
			regs[i] = ISR_timeShiftOn[PHASE_B];
			
			break;
			
			
			case 1058:
			if (writeReg)
			{
				ISR_timeShiftOn[PHASE_C] = Value_;
				eeprom_write_byte((uint8_t*)98, ISR_timeShiftOn[PHASE_C]);
			}
			regs[i] = ISR_timeShiftOn[PHASE_C];
			
			break;
			
			case 1060:
			if (writeReg)
			{
				ownTimeOffWarn_ISR = Value_;
				eeprom_write_word((uint16_t*)101, ownTimeOffWarn_ISR);
			}
			regs[i] = ownTimeOffWarn_ISR;
			
			break;
			
			case 1061:
			if (writeReg)
			{
				ownTimeOffAlarm_ISR = Value_;
				eeprom_write_word((uint16_t*)103, ownTimeOffAlarm_ISR);
			}
			regs[i] = ownTimeOffAlarm_ISR;
			
			break;
			
			case 1062:
			if (writeReg)
			{
				fullTimeOffWarn_ISR = Value_;
				eeprom_write_word((uint16_t*)105, fullTimeOffWarn_ISR);
			}
			regs[i] = fullTimeOffWarn_ISR;
			
			break;
			
			case 1063:
			if (writeReg)
			{
				fullTimeOffAlarm_ISR = Value_;
				eeprom_write_word((uint16_t*)107, fullTimeOffAlarm_ISR);
			}
			regs[i] = fullTimeOffAlarm_ISR;
			
			break;
			
			case 1064:
			if (writeReg)
			{
				fullTimeOnWarn_ISR = Value_;
				eeprom_write_word((uint16_t*)109, fullTimeOnWarn_ISR);
			}
			regs[i] = fullTimeOnWarn_ISR;
			
			break;
			
			case 1065:
			if (writeReg)
			{
				fullTimeOnAlarm_ISR = Value_;
				eeprom_write_word((uint16_t*)111, fullTimeOnAlarm_ISR);
			}
			regs[i] = fullTimeOnAlarm_ISR;
			
			break;
			
			case 1066:
			if (writeReg)
			{
				ownTimeOnWarn_ISR = Value_;
				eeprom_write_word((uint16_t*)113, ownTimeOnWarn_ISR);
			}
			regs[i] = ownTimeOnWarn_ISR;
			
			break;
			
			case 1067:
			if (writeReg)
			{
				ownTimeOnAlarm_ISR = Value_;
				eeprom_write_word((uint16_t*)115, ownTimeOnAlarm_ISR);
			}
			regs[i] = ownTimeOnAlarm_ISR;
			
			break;
			
			
			case 1068:
			if (writeReg)
			{
				//eeprom_write_byte((uint8_t*)95,Value_);
				Emul_SwitchTime_Delay = (uint16_t)Value_/OSC_REC_PERIOD;
			}
			regs[i] = Emul_SwitchTime_Delay*OSC_REC_PERIOD;
			break;
			
			case 1069 ... 1071:
			
			regs[i] = arcDetected[addr_ - 1069];
			break;
			

			case 1100:
			{
				if (writeReg)
				{
					ownTimeOnAlarm_ISR = Value_;
				}
				regs[i] = Value_;
			}
			break;


			
			
			case 1201 ... 1218:

			regs[i] = calibr_X[addr_ - 1200];

			break;


			
			case 1301 ... 1318:

			regs[i] = calibr_Y[addr_ - 1300];

			break;
			
			
			case 1401 ... 1418:

			if (writeReg)
			calibr_X[addr_ - 1400] = Value_;
			regs[i] = calibr_X[addr_ - 1400];

			break;
			

			case 1501 ... 1518:
			
			if (writeReg)
			calibr_Y[addr_ - 1500] = Value_;
			regs[i] = calibr_Y[addr_ - 1500];

			break;

			
			// 			case 1124:
			//
			// 			regs[i] = cts.osc_buffer_1[0][PHASE_A] - ADC_MID - 128 + cts.adcShift_A; // sqrt(sqrForRMS(cts.osc_buffer_1,PHASE_A,cts.adcShift_A));
			//
			// 			break;
			//
			// 			case 1125:
			// 			{
			// 				regs[i] = static_cast<int64_t>(cts.osc_buffer_1[0][PHASE_A]);
			//
			// 			}
			// 			break;
			//
			// 			case 1126:
			// 			{
			// 				checkRMS = static_cast<int64_t>(cts.osc_buffer_1[0][PHASE_A]) - ADC_MID - 128 + cts.adcShift_A;
			// 				// curr_ - ADC_MID + shift_
			// 				regs[i] = checkRMS;
			//
			// 			}
			// 			break;
			//
			// 			case 1128:
			// 			{
			// 				checkRMS = static_cast<int64_t>(cts.osc_buffer_1[0][PHASE_A]) - 3500 + 128;
			// 				// curr_ - ADC_MID + shift_
			// 				int64_t result = checkRMS * checkRMS;
			//
			// 				regs[i] = sqrt(result);
			//
			// 			}
			// 			break;

			// 			case 1590:	// Осциллограммы фазы 'A'
			// 			regs[i] = cts.overflowOscBuffer[0][PHASE_A];
			// 			break;
			//
			// 			case 1591:	// Осциллограммы фазы 'A'
			// 			regs[i] = cts.overflowOscBuffer[0][PHASE_B];
			// 			break;
			//
			// 			case 1592:	// Осциллограммы фазы 'A'
			// 			regs[i] = cts.overflowOscBuffer[0][PHASE_C];
			// 			break;
			
			
			case 1599:

			if (writeReg)
			for (byte j = 1; j <= NUM_CHANNELS; j++ )
			calibrToEEPROM(j);


			break;
			


			// Buffer 2 begin:
			// 			case 1600 ... 1799:	// Осциллограммы фазы 'A'
			// 			regs[i] = cts.overflowOscBuffer[addr_ - 1600][PHASE_A];
			// 			break;
			// 			case 1800 ... 1999:	// Осциллограммы фазы 'B'
			// 			regs[i] = cts.osc_buffer_1[addr_ - 1800][PHASE_B];
			// 			break;
			// 			case 2000 ... 2199:	// Осциллограммы фазы 'C'
			// 			regs[i] = cts.osc_buffer_1[addr_ - 2000][PHASE_C];
			// 			break;
			
			// Buffer 2 end.
			
			
			case 1888:			// Номер активного в данный момент буфера
			regs[i] = heartBeat;
			break;
			

			case 1910:			// Номер активного в данный момент буфера
			regs[i] = cts.active_buffer_id;
			break;

			// 			case 1911:			// Очистка всех буферов, сброс флагов и т.д.
			// 			if (writeReg)
			// 			{
			// 				cts.flush_buffer((uint8_t *) cts.overflowOscBuffer, sizeof(cts.overflowOscBuffer));
			// 				cts.flush_buffer((uint8_t *) cts.calcOscBuffer, sizeof(cts.calcOscBuffer));
			//
			// 				// 				cts.adc_switch[ADC1] = ON;
			// 				// 				cts.adc_switch[ADC2] = OFF;
			// 				// 				cts.adc_switch[ADC3] = ON;
			// 				// 				cts.adc_switch[ADC4] = OFF;
			// 				// 				cts.adc_switch[ADC5] = ON;
			// 				// 				cts.adc_switch[ADC6] = OFF;
			// 				cts.active_buffer_id = FIRST;
			// 				cts.recording_status = INPROGRESS;
			// 				GLOBAL_MODE = IDLE;
			// 				// 				PORT_LED &= ~(1 << LED_PLC_OK);
			// 				// 				PORT_LED &= ~(1 << LED_SD_OK);
			// 				phase_zero_1[PHASE_A] = 0;
			// 				phase_zero_1[PHASE_B] = 0;
			// 				phase_zero_1[PHASE_C] = 0;
			// 				ISR_arcPoint[PHASE_A] = 0;
			// 				ISR_arcPoint[PHASE_B] = 0;
			// 				ISR_arcPoint[PHASE_C] = 0;
			// 				//	zero_is_found = false;
			// 			}
			// 			else
			// 			regs[i] = 0xFF;
			//
			// 			break;

			// 			case 1912:
			// 			regs[i] = GLOBAL_MODE;
			// 			break;
			//
			// 			case 1913:			// Время в секундах с момента старта
			// 			regs[i] = millis() / 1000;
			// 			break;

			// 			case 1912 ... 1917:	// Осциллограммы фазы 'A'
			// 			regs[i] = cts.adc_switch[addr_ - 1912];
			// 			break;
			//
			//
			// 			case 1920:
			// 			regs[i] = phase_zero_1[PHASE_A];
			// 			break;
			// 			case 1921:
			// 			regs[i] = phase_zero_1[PHASE_B];
			// 			break;
			// 			case 1922:
			// 			regs[i] = phase_zero_1[PHASE_C];
			// 			break;
			case 1923:
			regs[i] = ISR_arcPoint[PHASE_A];
			break;
			case 1924:
			regs[i] = ISR_arcPoint[PHASE_B];
			break;
			case 1925:
			regs[i] = ISR_arcPoint[PHASE_C];
			break;




			case 1926:
			regs[i] = ISR_arc_lifetime[PHASE_A];
			break;

			case 1927:
			regs[i] = ISR_arc_lifetime[PHASE_B];
			break;
			
			case 1928:
			regs[i] = ISR_arc_lifetime[PHASE_C];
			break;
			
			
			// 			case 1929 ... 1940:
			// 			if (writeReg)
			// 			{
			// 				tmp[addr_-1929] = Value_;
			// 			}
			// 			regs[i] = tmp[addr_-1929];
			// 			break;
			
			// 			case 1941:
			// 			{
			// 				regs[i] = cts.adc_ct_extract_single(tmp, ADC_CT_1);
			// 			}
			// 			break;
			//
			// 			case 1942:
			// 			// 			cts.adc_ct_rec(tmp);
			// 			regs[i] = cts.adc_ct_extract_single(tmp, ADC_CT_2);
			// 			break;
			//
			// 			case 1943:
			// 			// 			cts.adc_ct_rec(tmp);
			// 			regs[i] = cts.adc_ct_extract_single(tmp, ADC_CT_3);
			// 			break;
			//
			//
			// 			case 1949:
			// 			cts.adc_ct_rec(tmp);
			// 			break;
			//
			// 			case 1950 ... 1961:
			// 			regs[i] = tmp[addr_ - 1950];
			// 			break;
			//
			
			// 			case 1989:
			// 			{
			// 				if (writeReg)
			// 				{
			// 					uint8_t data_[] = {90,91,92,93,94,95,96,97,98,99};
			// 					fram_write_array(10, data_, 10);
			// 				}
			//
			// 				regs[i] = 1;
			//
			// 			}
			//
			// 			break;
			//
			//
			//
			
			case 1981 ... 1988:
			
			regs[i] = cts.SOLarr[addr_-1981];
			break;
			
			
			case 1989:
			
			regs[i] = wearStrikes;
			
			break;
			
			
			case 1991:
			
			regs[i] = cts.solenoidCurrOn_RAW;
			
			break;
			
			case 1992:
			
			regs[i] = cts.solenoidCurrOff_1_RAW;
			
			break;
			
			case 1993:
			
			regs[i] = cts.solenoidCurrOff_2_RAW;
			
			break;
			
			
			
			
			
			case 1994:
			regs[i] = convertFloat((cts.solenoidCurrOn_RAW - b[SOL_ON]) * k[SOL_ON], 0);
			break;
			
			case 1995:
			regs[i] = convertFloat((cts.solenoidCurrOn_RAW - b[SOL_ON]) * k[SOL_ON], 1);
			break;
			
			case 1996:
			regs[i] = convertFloat((cts.solenoidCurrOff_1_RAW - b[SOL_OFF_1]) * k[SOL_OFF_1], 0);
			break;
			
			case 1997:
			regs[i] = convertFloat((cts.solenoidCurrOff_1_RAW - b[SOL_OFF_1]) * k[SOL_OFF_1], 1);
			break;
			
			case 1998:
			regs[i] = convertFloat((cts.solenoidCurrOff_2_RAW - b[SOL_OFF_2]) * k[SOL_OFF_2], 0);
			break;
			
			case 1999:
			regs[i] = convertFloat((cts.solenoidCurrOff_2_RAW - b[SOL_OFF_2]) * k[SOL_OFF_2], 1);
			break;
			
			
			
			

			case 2000:
			regs[i] = convertFloat(b[PHASE_A], 0);
			break;
			
			case 2001:
			regs[i] = convertFloat(b[PHASE_A], 1);
			break;
			
			case 2002:
			regs[i] = convertFloat(b[PHASE_B], 0);
			break;
			
			case 2003:
			regs[i] = convertFloat(b[PHASE_B], 1);
			break;
			
			case 2004:
			regs[i] = convertFloat(b[PHASE_C], 0);
			break;
			
			case 2005:
			regs[i] = convertFloat(b[PHASE_C], 1);
			break;
			
			
			
			
			
			
			
			// 				PHASE_A,
			// 				PHASE_B,
			// 				PHASE_C,
			// 				PHASE_A_GAIN,
			// 				PHASE_B_GAIN,
			// 				PHASE_C_GAIN,
			// 				SOL_ON,
			// 				SOL_OFF_1,
			// 				SOL_OFF_2,
			
			
			case 2010:
			regs[i] = convertFloat(k[PHASE_A], 0);
			break;
			
			case 2011:
			regs[i] = convertFloat(k[PHASE_A], 1);
			break;
			
			case 2012:
			regs[i] = convertFloat(k[PHASE_B], 0);
			break;
			
			case 2013:
			regs[i] = convertFloat(k[PHASE_B], 1);
			break;
			
			case 2014:
			regs[i] = convertFloat(k[PHASE_C], 0);
			break;
			
			case 2015:
			regs[i] = convertFloat(k[PHASE_C], 1);
			break;
			
			
			
			
			
			
			
			// ======================= EMULATOR CONFIG (Holding) =======================
			case 2100: // emulate_contacts: 0=реальные, 1=эмуляция
			{
				if (writeReg)
				{
					if (Value_ == 1)
					{
						init_Emu_Mode_Flag = 1;
					}
					else
					{
						;	//exit_Emu_Mode_Flag = 1;
					}
					
				}
				regs[i] = emulate_contacts;
			}
			break;
			

			// [2101] emulate_adc_wave: 0=реальный АЦП, 1=синус из эмулятора
			case 2101:
			{
				if (writeReg) { emulate_adc_wave = Value_ ; }
				regs[i] = emulate_adc_wave;
			}
			break;

			// [2102] emu_trigger: запись 1 -> выполнить один «удар»; 0 – ничего.
			// Прошивка сама вернёт его в 0 по завершении.
			case 2102:
			{
				if (writeReg)
				{
					emu_Start_switching_Flag = Value_;
				}
				regs[i] = emu_Start_switching_Flag;
			}
			break;

			case 2103:
			{
				//	if (writeReg) { lastOperation = (Value_ ? 1 : 0); }
				regs[i] = lastOperation;
			}
			break;
			
			// Прошивка сама вернёт его в 0 по завершении.
			case 2104:
			{
				//	if (writeReg) { emu_mode = (Value_ ? 1 : 0); }
				regs[i] = EMU_mode_Flag;
			}
			break;
			
			
			
			// Прошивка сама вернёт его в 0 по завершении.
			case 2105:
			{
				regs[i] = dbg_dt_ticks;
			}
			break;
			
			
			// Прошивка сама вернёт его в 0 по завершении.
			case 2109:
			{
				if (writeReg)
				{
					if (Value_ >= F_BY_PASS && Value_ <= F_PROFILE_P1)
					noiseCancellMode = (filter_mode_t)Value_;
					else
					noiseCancellMode = F_BY_PASS; // дефолт или игнорировать
					
					eeprom_write_word((uint16_t*)220,noiseCancellMode);
				}
				regs[i] = (int16_t)noiseCancellMode;

			}
			break;
			
			
			
			
			
			
			

			// ---------- [2110..2112] ЗАДЕРЖКИ СТАРТА (ticks, 16-бит) ----------
			case 2110: // A
			if (writeReg) { emu_contact_start_Delay_ticks	[PHASE_A] = (uint16_t)Value_; }
			regs[i] = emu_contact_start_Delay_ticks[PHASE_A];
			break;
			case 2111: // B
			if (writeReg) { emu_contact_start_Delay_ticks[PHASE_B] = (uint16_t)Value_; }
			regs[i] = emu_contact_start_Delay_ticks[PHASE_B];
			break;
			case 2112: // C
			if (writeReg) { emu_contact_start_Delay_ticks[PHASE_C] = (uint16_t)Value_; }
			regs[i] = emu_contact_start_Delay_ticks[PHASE_C];
			break;

			// ---------- [2120..2122] ДЛИТЕЛЬНОСТИ «РАЗРЫВА» (ticks, 16-бит) ----------
			case 2120: // A
			if (writeReg) { emu_contact_duration_ticks[PHASE_A] = (uint16_t)Value_; }
			regs[i] = emu_contact_duration_ticks[PHASE_A];
			break;
			case 2121: // B
			if (writeReg) { emu_contact_duration_ticks[PHASE_B] = (uint16_t)Value_; }
			regs[i] = emu_contact_duration_ticks[PHASE_B];
			break;
			case 2122: // C
			if (writeReg) { emu_contact_duration_ticks[PHASE_C] = (uint16_t)Value_; }
			regs[i] = emu_contact_duration_ticks[PHASE_C];
			break;



			// ---------- ПАРАМЕТРЫ СИНУСА (эмуляция АЦП) ----------
			// [2130] Частота сигнала, Гц (целое). На запись пересчитываем шаг фазы.
			// step = Hz * 65536 * SAMPLE_PERIOD_US / 1e6
			/**
			* @brief [2130] Частота синуса, centi-Hz (Hz*100) — 16-бит.
			* @details Тик дискретизации SAMPLE_PERIOD_US=100 мкс. Внутри хранится emu_phase_step (Q16),
			*          поэтому на запись пересчитываем: step = round( f_centi * 65536 * Ts_us / 1e8 ).
			*          На чтение отдаём f_centi = round( step * 1e8 / (65536 * Ts_us) ).
			*          Примеры: 50.00 Гц -> 5000; 49.90 Гц -> 4990.
			*/
			case 2130:
			{
				if (writeReg) {
					uint16_t f_centi = (uint16_t)Value_;       // Hz*100 от мастера
					// clamp по желанию (верх ограничения по протоколу/задаче)
					// if (f_centi > 50000) f_centi = 50000; // например, 500.00 Гц максимум

					// step = round( f_centi * 65536 * Ts / 1e8 )
					// используем 64-бит, чтобы не переполниться
					uint64_t num = (uint64_t)f_centi * 65536ull * (uint64_t)SAMPLE_PERIOD_US;
					emu_phase_step = (uint16_t)((num + 50000000ull) / 100000000ull); // +0.5 для округления
				}

				// Вернуть centi-Hz из текущего emu_phase_step:
				// f_centi = round( step * 1e8 / (65536 * Ts) )
				uint64_t num = (uint64_t)emu_phase_step * 100000000ull;
				uint32_t den = 65536ul * (uint32_t)SAMPLE_PERIOD_US; // при Ts=100 мкс => 6_553_600
				uint16_t f_centi = (uint16_t)((num + (den/2)) / den);
				regs[i] = f_centi;
			}
			break;

			// [2131] Амплитуда синуса (ADC12 пик относительно центра), напр. 0..2047
			case 2131:
			{
				if (writeReg) { emu_Sine_amp = Value_; }
				regs[i] = emu_Sine_amp;
			}
			break;

			// [2132] Начальная фаза синуса в градусах (0..359) на момент входа в BUSY
			case 2132:
			{
				if (writeReg) {
					uint16_t deg = (uint16_t)(Value_ % 360);
					emu_start_phase_u16 = (uint16_t)((uint32_t)deg * 65536ul / 360ul);
				}
				regs[i] = (uint16_t)(((uint32_t)emu_start_phase_u16 * 360ul + 32768ul) / 65536ul);
			}
			break;

			// [2133] Межфазный сдвиг (° 0..359). B=A+shift, C=A+2*shift
			case 2133:
			{
				if (writeReg) {
					uint16_t deg = (uint16_t)(Value_ % 360);
					emu_interphase_u16 = (uint16_t)((uint32_t)deg * 65536ul / 360ul);
				}
				regs[i] = (uint16_t)(((uint32_t)emu_interphase_u16 * 360ul + 32768ul) / 65536ul);
			}
			break;
			// ===================== END EMULATOR CONFIG (Holding) =====================


			// ---------- [2140..2142] ДУГА (ticks, 16-бит) ----------
			case 2140: // A
			if (writeReg) { emu_arc_extinguish_ticks[PHASE_A] = (uint16_t)Value_; }
			regs[i] = emu_arc_extinguish_ticks[PHASE_A];
			break;
			case 2141: // B
			if (writeReg) { emu_arc_extinguish_ticks[PHASE_B] = (uint16_t)Value_; }
			regs[i] = emu_arc_extinguish_ticks[PHASE_B];
			break;
			case 2142: // C
			if (writeReg) { emu_arc_extinguish_ticks[PHASE_C] = (uint16_t)Value_; }
			regs[i] = emu_arc_extinguish_ticks[PHASE_C];
			break;

			case 2150: regs[i] = g_phase_start_dt_ticks[PHASE_A]; break; // dt_A
			case 2151: regs[i] = g_phase_start_dt_ticks[PHASE_B]; break; // dt_B
			case 2152: regs[i] = g_phase_start_dt_ticks[PHASE_C]; break; // dt_C


			/* 2160..2165: время коммутации в тиках (1 тик = 100 мкс), uint32 -> два регистра */
			case 2160 ... 2165:
			{
				/* смещение внутри блока 2160..2165 */
				const uint16_t off = (uint16_t)(addr_ - 2160);

				/* фаза: 0..2 (A/B/C) */
				const uint8_t ph = (uint8_t)(off / 2u);         // 0: A, 1: B, 2: C

				/* 0 -> HI (2160,2162,2164), 1 -> LO (2161,2163,2165) */
				const uint8_t want_hi = (uint8_t)((off & 1u) == 0u);

				/* важно: mechSwitchTime хранить как int32_t/uint32_t в ТИКАХ */
				const uint32_t t = (uint32_t)mechSwitchTime[ph];

				regs[i] = reg_word_u32(t, want_hi);
			}
			break;
			
			case 2170: if (writeReg) g_noise_T_codes = (int16_t)Value_;
			regs[i] = (int16_t)g_noise_T_codes; break;

			case 2171: if (writeReg) g_noise_L_min   = (uint16_t)Value_;
			regs[i] = (int16_t)g_noise_L_min;   break;

			case 2172: if (writeReg) g_hampel_W      = (uint8_t)Value_;
			regs[i] = (int16_t)g_hampel_W;      break;

			case 2173: if (writeReg) { union { int16_t i; float f; } u; u.i=Value_; g_hampel_k = u.f; }
			// если удобнее — держи k*1000 в int16 и дели на 1000.0 при использовании
			regs[i] = (int16_t)(g_hampel_k); break;

			case 2174: if (writeReg) g_slew_dv_max   = (int16_t)Value_;
			regs[i] = (int16_t)g_slew_dv_max;   break;

			case 2175: if (writeReg) g_lpf_tau_us    = (uint16_t)Value_;
			regs[i] = (int16_t)g_lpf_tau_us;    break;


			case 2180:
			
			if (writeReg)
			{
				eeprom_write_word((uint16_t*)222,Value_);		// 90 ms
				arc_consistency_len = Value_;
			}

			regs[i] = arc_consistency_len;

			break;
			
			
			/* ... в обработчике регистров ... */
			case 2190 ... 2192:
			{
				uint8_t idx = (uint8_t)(addr_ - 2190u);               // 0..2
				if (writeReg) {
					uint8_t v = clamp_u16_to_u8(Value_);
					eeprom_write_byte((uint8_t*)(EEADDR_ON_LAG_BASE + idx), v);
					on_contact_lag_ticks[idx] = v;
				}
				regs[i] = on_contact_lag_ticks[idx];                   // если regs[i] — uint16_t, авторасширится
				break;
			}

			case 2193 ... 2195:
			{
				uint8_t idx = (uint8_t)(addr_ - 2193u);               // 0..2
				if (writeReg) {
					uint8_t v = clamp_u16_to_u8(Value_);
					eeprom_write_byte((uint8_t*)(EEADDR_OFF_LAG_BASE + idx), v);
					off_contact_lag_ticks[idx] = v;
				}
				regs[i] = off_contact_lag_ticks[idx];
				break;
			}





			case 2200:
			regs[i] = convertFloat(k[PHASE_A], 0);
			break;

			case 2201:
			regs[i] = convertFloat(k[PHASE_A], 1);
			break;

			case 2202:
			regs[i] = convertFloat(k[PHASE_B], 0);
			break;

			case 2203:
			regs[i] = convertFloat(k[PHASE_B], 1);
			break;

			case 2204:
			regs[i] = convertFloat(k[PHASE_C], 0);
			break;

			case 2205:
			regs[i] = convertFloat(k[PHASE_C], 1);
			break;



			case 2206:
			regs[i] = convertFloat(b[PHASE_A], 0);
			break;

			case 2207:
			regs[i] = convertFloat(b[PHASE_A], 1);
			break;

			case 2208:
			regs[i] = convertFloat(b[PHASE_B], 0);
			break;

			case 2209:
			regs[i] = convertFloat(b[PHASE_B], 1);
			break;

			case 2210:
			regs[i] = convertFloat(b[PHASE_C], 0);
			break;

			case 2211:
			regs[i] = convertFloat(b[PHASE_C], 1);
			break;





			
			case 3000 ... (3000 + PHASE_SAMPLES_MAX - 1):
			{
				uint16_t sample_i = (uint16_t)(addr_ - 3000);
				regs[i] = (uint16_t)dbg_tap_get(PHASE_A, sample_i);
			} break;
			
			case 3400 ... (3400 + PHASE_SAMPLES_MAX - 1):
			{
				uint16_t sample_i = (uint16_t)(addr_ - 3400);
				regs[i] = (uint16_t)dbg_tap_get(PHASE_B, sample_i);
			} break;
			
			case 3800 ... (3800 + PHASE_SAMPLES_MAX - 1):
			{
				uint16_t sample_i = (uint16_t)(addr_ - 3800);
				regs[i] = (uint16_t)dbg_tap_get(PHASE_C, sample_i);
			} break;
			
			
			
			


			case 5000:
			regs[i] = convertFloat(firmVersion, 0);
			break;
			
			case 5001:
			regs[i] = convertFloat(firmVersion, 1);
			break;
			
			
			case 5002:
			{
				if (writeReg) {
					dt = 0;
					startTimerTest = Value_;
				}
				regs[i] = startTimerTest;
			}
			break;
			
			case 5003:
			{
				
				regs[i] = dt/10;
			}
			break;
			
			
			

			case 5004:
			regs[i] = currentTime/1000;
			break;
			
			case 5005:
			regs[i] = ISR_microTimer/10000;
			break;
			
			
			
			case 5006:
			regs[i] = partialElectricalSwitch;
			break;
			
			case 5007:
			regs[i] = contact_wd_tripped;
			break;
			
			case 5008:
			regs[i] = calcRunCnt;
			break;
			
			
			case 5010:
			regs[i] = fullTimeOffWarnState;
			break;
			
			case 5011:
			regs[i] = fullTimeOffAlarmState;
			break;


			case 5012:
			regs[i] = fullTimeOnWarnState;
			break;
			
			case 5013:
			regs[i] = fullTimeOnAlarmState;
			break;
			
			
			
			// 			case 3000 ... 3399: {   // фаза A
			// 				uint8_t  view_idx;
			// 				osc_strike_header_t hdr;
			// 				if (!osc_pick_view_idx(&view_idx, &hdr)) {
			// 					regs[i] = 0;
			// 					break;
			// 				}
			// 				uint16_t sample_i = (uint16_t)(addr_ - 3000);
			// 				regs[i] = (int)fram_sample_to_dA(view_idx, &hdr, PHASE_A, sample_i);
			// 			} break;
			//
			// 			case 3400 ... 3799: {   // фаза B
			// 				uint8_t  view_idx;
			// 				osc_strike_header_t hdr;
			// 				if (!osc_pick_view_idx(&view_idx, &hdr)) {
			// 					regs[i] = 0;
			// 					break;
			// 				}
			// 				uint16_t sample_i = (uint16_t)(addr_ - 3400);
			// 				regs[i] = (int)fram_sample_to_dA(view_idx, &hdr, PHASE_B, sample_i);
			// 			} break;
			//
			// 			case 3800 ... 4199: {   // фаза C
			// 				uint8_t  view_idx;
			// 				osc_strike_header_t hdr;
			// 				if (!osc_pick_view_idx(&view_idx, &hdr)) {
			// 					regs[i] = 0;
			// 					break;
			// 				}
			// 				uint16_t sample_i = (uint16_t)(addr_ - 3800);
			// 				regs[i] = (int)fram_sample_to_dA(view_idx, &hdr, PHASE_C, sample_i);
			// 			} break;

			case 9990:
			regs[i] = convertFloat(firmVersion, 0);
			break;
			
			case 9991:
			regs[i] = convertFloat(firmVersion, 1);
			break;





			
			
			// 			case 2000 ... 6800:
			//
			// regs[i] = fram_read_byte(addr_-2000);
			//
			// 			break;
			
			
			
			// ----- snb c0d3 3nd.


			
			

			// 			// Baud rate:
			// 			case 101:
			// 			if (writeReg){
			// 			baudRate = Value_;
			// 			//EEPROM.write(101, baudRate);
			// 			};
			// 			regs[i] = baudRate;
			// 			break;
			//
			// 			// Parity:
			// 			case 102:
			// 			if (writeReg){
			// 			parity = Value_;
			// 			//EEPROM.write(102, parity);
			// 			};
			// 			regs[i] = parity;
			// 			break;
			//
			// 			// ???:
			// 			case 138:
			// 			if (writeReg && pinCode){
			// 			;
			// 			};
			// 			regs[i] = 1;
			// 			break;
			//
			
			
			// BootLoader:

			default:
			regs[i] = -1;     // reply -1, if option is not available.
			break;
		}

		addr_++;

	}
}


// Обработчик прерывания, в котором выбирается свободный буфер осциллограм
// и вызывается процедура его заполнения:



static inline void isrMeasurements()
{
	if (strikeData == runProc)
	{
		check_Input_States();   // Мониторинг входов
		ScopeCapture();			// Запись осциллограмм
		
		switch_Finish_Timers();
		
		if (positionChanged == standingBy)
		{
			RMS_calculations();
			
			cts.get_ct_values_in_idle_mode(osc_buffer_1);
			switch (++idle_ct_meas_period)											// Увеличиваем счётчик.
			{
				//case 2000:															// Если достигли количества отсчетов, то
				case 2000:
				{
					idle_ct_meas_period	= 0;								// обнуляем счётчик,
					
					getRMS(osc_buffer_1, getRMSvalues);
					averageIt();
					triggerRMScalc = 1;
					current_A_buf = 0;
					current_B_buf = 0;
					current_C_buf = 0;
					// 						if (calcSwitchCurrentFlag < 5)
					// 						calcSwitchCurrentFlag++;
					
				}
				break;
				
				default:
				{
					getRMS(osc_buffer_1, collectRMSvalues);
					//quickRMS(osc_buffer);

					if (solenoidCurrOnBuf < cts.solenoidCurrOn_RAW)
					solenoidCurrOnBuf = cts.solenoidCurrOn_RAW;
					
					if(solenoidCurrOff_1_Buf < cts.solenoidCurrOff_1_RAW)
					solenoidCurrOff_1_Buf = cts.solenoidCurrOff_1_RAW;
					
					if(solenoidCurrOff_2_Buf < cts.solenoidCurrOff_2_RAW)
					solenoidCurrOff_2_Buf = cts.solenoidCurrOff_2_RAW;
					
					
				}
				break;
			};
		}
		
		
		strikeData = procDone;
	}
}




void RMS_calculations()
{
	
	if (triggerRMScalc)
	{
		switch(triggerRMSstep)
		{
			case 0:
			{
				trendBuffer();
				triggerRMSstep++;
			}
			break;
			
			case 1:
			{
				PIDfilter();
				triggerRMSstep++;
			}
			break;
			
			case 2:
			{
				filteredRMS[PHASE_A] = PIDFILTER[0];
				filteredRMS[PHASE_B] = PIDFILTER[1];
				filteredRMS[PHASE_C] = PIDFILTER[2];
				
				// 				if (cts.amplification == 1)
				// 				{
				// 					params.RMS_A = filteredRMS[PHASE_A] * k[PHASE_A_GAIN];
				// 					params.RMS_B = filteredRMS[PHASE_B] * k[PHASE_B_GAIN];
				// 					params.RMS_C = filteredRMS[PHASE_C] * k[PHASE_C_GAIN];
				// 				}
				// 				else
				// 				{
				params.RMS_A = filteredRMS[PHASE_A] * k[PHASE_A];
				params.RMS_B = filteredRMS[PHASE_B] * k[PHASE_B];
				params.RMS_C = filteredRMS[PHASE_C] * k[PHASE_C];
				//	}
				triggerRMSstep = 0;
				triggerRMScalc = 0;
			}
			break;
		}
	}
	
	
	
}




ISR(TIMER2_COMP_vect)
{
	ticks_now++;

	if (strikeData == procDone)
	{
		strikeData = runProc;
	}
	else
	{
		if (cts.recording_status == INPROGRESS)
		strikeLostData++;
	}
}




// Ф-ция отслеживает значения с АЦП трансформаторов тока и дискретных входов.
// В нормальном режиме - раз в 500 мс, в режиме записи - каждые 200 мкс до заполнения буфера.
// Переключение производится по состоянию дискретных входов:


void sendByte(uint8_t x_)
{
	UCSR1A = UCSR1A | (1 << TXC1);
	uint8_t oldSREG = SREG;
	cli();
	PORTD = PORTD | transmitMode;                  // Перевод драйвера RS485 в режим передачи сообщений
	SREG = oldSREG;

	Serial1.write(x_);
}


/*******************************  Т А Й М Е Р - М А Ш И Н А  *************************/
/**
* @brief Главный «тонкий» координирующий цикл осциллографирования коммутации.
* @details
*  - Переход standingBy → startScope делает внешний детектор доп. контактов.
*  - На startScope: глушим UART, выполняем initScoping(), переводим в activeScope.
*  - В activeScope: регулярные вызовы record_scope() до COMPLETE.
*  - По COMPLETE: выполняем finScoping(), переводим в silentTime.
* @note Не смешивает статусы доп. контактов и статусы осциллографирования.
*/
/*******************************  S C O P E   C A P T U R E  ********************************/
/**
* @brief Тонкий координатор режима осциллографирования.
* @details
*  - Вход по positionChanged==startScope: initScoping(), INPROGRESS, → activeScope.
*  - В activeScope: record_scope(), параллельно — вотчдог осциллографирования.
*  - После COMPLETE: возвращаем UART/Modbus, positionChanged=silentTime.
*  - Таймер «тишины»: стартуем, когда контакты SILENT и все фазы записи silentTime; по истечении — глобальный сброс.
*/
/**
* @brief Главный цикл осциллографирования коммутации.
*/
static inline void ScopeCapture(void)
{
	/* Вход в запись: standingBy → startScope сделан детектором */
	if (positionChanged == startScope)
	{
		uart1_quiet_enter();
		// fram_write_byte(7500, 32);
		initScoping();                      /* локальная инициализация записи (без FRAM) */
		cts.recording_status = INPROGRESS;
		positionChanged      = activeScope;

		scope_wd_armed   = 0;               /* вотчдог осциллографирования переармится сам */
		scope_wd_tripped = 0;
		// fram_write_byte(7500, 40);
	}

	/* Основной цикл записи */
	if (positionChanged == activeScope)
	{
		if (cts.recording_status == INPROGRESS) {
			record_scope();                 /* вызывать с периодом дискретизации */
			scope_watchdog();               /* следим за таймаутом осциллографирования */
		}

		/* Больше НЕ полагаемся только на cts.COMPLETE.
		Разморозка интерфейсов делается при арминге таймера тишины. */
	}
}




/* ==================== П Л Е Й С Х О Л Д Е Р Ы  (наполнение позже) ==================== */

/**
* @brief Подготовка к осциллографированию коммутации.
* @details
*  Здесь соберём всё для старта: отметим t0, подготовим заголовок (auxType, lastOperation),
*  сбросим per-phase счётчики/флаги записи, при необходимости вызовем старые init-функции.
*  Ничего не делаю сейчас — только точка расширения.
*/
static inline void initScoping(void)
{
	
	/* Сброс прогресса по фазам */
	Scope_written_points[PHASE_A] = 0;
	Scope_written_points[PHASE_B] = 0;
	Scope_written_points[PHASE_C] = 0;

	scope_State[PHASE_A] = standingBy;
	scope_State[PHASE_B] = standingBy;
	scope_State[PHASE_C] = standingBy;

	g_hdr.written[PHASE_A] = 0;
	g_hdr.written[PHASE_B] = 0;
	g_hdr.written[PHASE_C] = 0;

	/* Стартовые фазы «синуса» для эмуляции (Q16) */
	emu_phase_acc_A = emu_start_phase_u16;
	emu_phase_acc_B = (uint16_t)(emu_start_phase_u16 + emu_interphase_u16);
	emu_phase_acc_C = (uint16_t)(emu_start_phase_u16 + (uint16_t)(2u * emu_interphase_u16));
	
	
	/* --- Индекс и сброс внутренних счётчиков --- */
	//	g_strike_index = (strike_index < STRIKES_MAX) ? strike_index : (STRIKES_MAX - 1);


	/* --- Сброс RAM-tap, чтобы 3000/3400/3800 начинались с нуля --- */
	dbg_tap_reset();

	/* --- Пишем заголовок в FRAM --- */
	//	const uint16_t base = STRIKE_BASE_ADDR(g_strike_index);
	//	fram_write_strike_header(base, &g_hdr);
	
	/* Оставляем до рефакторинга имен: вызов корректен инициализационно */
}

/**
* @brief Завершение осциллографирования коммутации.
* @details
*  Здесь соберём пост-обработку: запись статусов/заголовка в FRAM, перенос/пометки буферов,
*  обновление счётчиков коммутаций, обслуживание эмулятора (сброс триггера), и т.д.
*  Ничего не делаю сейчас — только точка расширения.
*/
static inline void finScoping(void)
{
	// TODO: перенести сюда:
	// - запись заголовка OSC_STAT_READY (fram_write_strike_header, fram_write_u8(...));
	// - moveFRAM_Buffer и buffer_new_data_Tail++;
	// - updateSwitchCnt(), calcSwitchTimes(), копирование токов соленоидов;
	// - osc_end_* (если решишь оставить);
	// - обслуживание эмулятора: EMU_strikeBreakerFin(), emu_trigger=0 (если нужно).
}





/**
* @brief Фиксация моментов СТАРТА (уже есть) и ФИНИША (новое) движения контактов в ТИКАХ таймера.
*        Старт берём из phase_first_edge_ticks[]. Здесь ловим ФИНИШ по переходу в устойчивое
*        конечное состояние: для cb_Off_On ждём cb_On, для cb_On_Off ждём cb_Off.
*        Для auxType==1 дублируем время финиша A -> B,C.
*/
void captureContactTimes(void)
{
	uint32_t t_now = ISR_microTimer;

	// сколько фаз реально отслеживаем (логика фаз остаётся общей)
	uint8_t nPhases = (auxType == 1) ? 1 : phase_quantity;

	for (uint8_t ph = 0; ph < nPhases; ph++)
	{
		if (fin_edge_latched[ph]) continue; // финиш по фазе уже захвачен

		// Для операции «включить» ждём устойчивого cb_On, для «выключить» — устойчивого cb_Off
		uint8_t needState = (lastOperation == cb_Off_On) ? cb_On : cb_Off;

		if ((uint8_t)CB_State[ph] == needState)
		{
			phase_fin_edge_ticks[ph] = t_now;   // ФИНИШ — прямо в тиках
			fin_edge_latched[ph]     = 1;
		}
	}

	// В случае одного концевика — как только фаза A зафиксировала финиш, копируем B,C
	if (auxType == 1)
	{
		if (fin_edge_latched[PHASE_A])
		{
			phase_fin_edge_ticks[PHASE_B] = phase_fin_edge_ticks[PHASE_A];
			phase_fin_edge_ticks[PHASE_C] = phase_fin_edge_ticks[PHASE_A];
			fin_edge_latched[PHASE_B]     = 1;
			fin_edge_latched[PHASE_C]     = 1;
		}
	}
}


void averageIt()
{
	float bufferA_ = 0;
	float bufferB_ = 0;
	float bufferC_ = 0;
	
	RMS_Aver[PHASE_A][averPosition] = float_RMS_A;
	RMS_Aver[PHASE_B][averPosition] = float_RMS_B;
	RMS_Aver[PHASE_C][averPosition] = float_RMS_C;
	
	averPosition++;
	
	if (averPosition > 4)
	averPosition = 0;
	
	for (uint8_t i=0; i<=4; i++)
	{
		bufferA_ +=  RMS_Aver[PHASE_A][i];
		bufferB_ +=  RMS_Aver[PHASE_B][i];
		bufferC_ += RMS_Aver[PHASE_C][i];
		
	}
	
	AVARAGED_RMS[PHASE_A] = bufferA_*0.2;
	AVARAGED_RMS[PHASE_B] = bufferB_*0.2;
	AVARAGED_RMS[PHASE_C] = bufferC_*0.2;

}


void trendBuffer()
{
	tail++;
	if (tail>trendLimit)
	tail = 0;

	for (uint8_t i = 0; i<=2; i++)
	{
		TREND[i][tail] = AVARAGED_RMS[i];
	}
	
}


void PIDfilter()
{
	float noiseMax = -1000.0;
	float noiseMin = 100000.0;
	float powerShift;

	for (uint8_t i = 0; i <= 2; i++)
	{
		noiseMax = -1000.0;
		noiseMin = 100000.0;
		
		for (uint8_t j = 0; j < 5; j++)
		{
			if (TREND[i][j] > noiseMax)
			noiseMax = TREND[i][j];
			if (TREND[i][j] < noiseMin)
			noiseMin = TREND[i][j];
		}

		noiseMin = noiseMin * 0.99;
		noiseMax = noiseMax * 1.01;


		if  (PIDFILTER[i] > noiseMin &&  noiseMax > PIDFILTER[i])
		powerShift = SMOOTHfilter[i];
		else
		powerShift = AGGRESSfilter[i];

		PIDFILTER[i] += powerShift * (1.0 * AVARAGED_RMS[i] - PIDFILTER[i]);
	}

}
/**
* @brief Обновить механические счётчики по фазам и пересчитать % износа.
*
* Предположения:
*  - Ресурс задан в ЦИКЛАХ (1 цикл = ВКЛ + ВЫКЛ).
*  - В конце коммутации известно lastOperation (cb_Off_On / cb_On_Off) и финальные CB_State[ph] (cb_On / cb_Off).
*  - Счётчики в params:
*      power_On_cntr_X, power_Off_cntr_X, power_OnOff_cntr_X
*      mechanical_wear_Perc_X
*
* Порядок:
*  1) Инкрементируем On/Off по факту финального состояния.
*  2) Считаем пофазно сумму On+Off с насыщением.
*  3) Пересчитываем процент расхода ресурса: wear% = (On+Off) * 100 / (2 * max_mechanical_wear).
*/
void updateSwitchCnt(void)
{
	const uint8_t nPhases = (uint8_t)phase_quantity;

	// Ссылки на счётчики
	volatile uint16_t* onCnt[3]    = { &params.power_On_cntr_A,    &params.power_On_cntr_B,    &params.power_On_cntr_C };
	volatile uint16_t* offCnt[3]   = { &params.power_Off_cntr_A,   &params.power_Off_cntr_B,   &params.power_Off_cntr_C };
	volatile uint16_t* onOffCnt[3] = { &params.power_OnOff_cntr_A, &params.power_OnOff_cntr_B, &params.power_OnOff_cntr_C };

	float* wearPerc[3]             = { &params.mechanical_wear_Perc_A,
		&params.mechanical_wear_Perc_B,
	&params.mechanical_wear_Perc_C };

	// 1) Инкремент раздельных счётчиков по факту успешного финального состояния
	const uint8_t is_on_cmd  = (lastOperation == cb_Off_On);
	const uint8_t is_off_cmd = (lastOperation == cb_On_Off);

	for (uint8_t ph = 0; ph < nPhases; ph++)
	{
		if (is_on_cmd  && (CB_State[ph] == cb_On))  { if (*onCnt[ph]  < 0xFFFFu) (*onCnt[ph])++;  }
		if (is_off_cmd && (CB_State[ph] == cb_Off)) { if (*offCnt[ph] < 0xFFFFu) (*offCnt[ph])++; }
	}


	if (is_off_cmd)
	{
		eeprom_write_word((uint16_t*)202, params.power_Off_cntr_A);
		eeprom_write_word((uint16_t*)206, params.power_Off_cntr_B);
		eeprom_write_word((uint16_t*)210, params.power_Off_cntr_C);
		wearStrikes++;
	}


	if (is_on_cmd)
	{
		eeprom_write_word((uint16_t*)204, params.power_On_cntr_A);
		eeprom_write_word((uint16_t*)208, params.power_On_cntr_B);
		eeprom_write_word((uint16_t*)212, params.power_On_cntr_C);
		wearStrikes++;
	}

	// 2) Сумма On+Off по фазам (с насыщением)
	for (uint8_t ph = 0; ph < nPhases; ph++)
	{
		uint32_t total = (uint32_t)(*onCnt[ph]) + (uint32_t)(*offCnt[ph]);
		*onOffCnt[ph] = (total > 0xFFFFu) ? 0xFFFFu : (uint16_t)total;
	}

	// 3) Перевод в проценты относительно ресурса в ЦИКЛАХ
	if (max_mechanical_wear > 0.0f)
	{
		const float den = max_mechanical_wear;
		for (uint8_t ph = 0; ph < nPhases; ph++)
		{
			float w = ((float)(*onOffCnt[ph]) * 100.0f) / den;
			if (w > 100.0f) w = 100.0f;           // «крышуем» сверху
			if (w < 0.0f)   w = 0.0f;             // защита от поднутрений
			*wearPerc[ph] = w;
		}
	}
	else
	{
		// Невалидный предел — обнуляем проценты
		for (uint8_t ph = 0; ph < nPhases; ph++)
		*wearPerc[ph] = 0.0f;
	}
	
	
}




uint8_t checkSwitchFin(void)
{
	// сколько фаз обрабатывать
	uint8_t nPhases = (auxType == 1) ? 1 : phase_quantity;
	bool allOk = true;

	for (uint8_t phase = 0; phase < nPhases; phase++)
	{
		if (lastOperation == cb_On_Off)
		{
			// ожидали, что фаза перешла в OFF
			if (CB_State[phase] != cb_Off)
			allOk = false;
		}
		else if (lastOperation == cb_Off_On)
		{
			// ожидали, что фаза перешла в ON
			if (CB_State[phase] != cb_On)
			allOk = false;
		}
	}

	// 1 — авария неполнофазной коммутации, 0 — всё ОК
	bitWrite(complexSP, part_Phase_Switch_Bit, allOk ? 1 : 0);
	return allOk ? 0 : 1;
}


// helper для gnu++98: считаем длительность по одной фазе в тиках
static inline uint32_t calc_phase_ticks(uint8_t ph)
{
	uint32_t t0 = phase_first_edge_ticks[ph];
	uint32_t t1 = phase_fin_edge_ticks[ph];
	if (t0 == 0u || t1 == 0u || t1 <= t0) return 0u;
	return (uint32_t)(t1 - t0);
}



/**
* @brief Рассчитать длительность движения контактов по фазам (в тиках таймера).
*        Берём разницу: finish_tick - start_tick.
*        Если чего-то нет (старт/финиш), пишем 0.
*        Для auxType==1 фазы B,C наследуют времена A.
*/
void calcSwitchTimes(void)
{
	// считаем A
	uint32_t dA = calc_phase_ticks(PHASE_A);
	uint32_t dB, dC;

	if (auxType != 1)
	{
		dB = calc_phase_ticks(PHASE_B);
		dC = calc_phase_ticks(PHASE_C);
	}
	else
	{
		// один концевик → B,C такие же как A
		dB = dA;
		dC = dA;
	}

	// записываем в твои массивы-результаты (в тиках)
	if (lastOperation == cb_Off_On)
	{
		full_on_time_ISR[PHASE_A] = dA;
		full_on_time_ISR[PHASE_B] = dB;
		full_on_time_ISR[PHASE_C] = dC;

		full_off_time_ISR[PHASE_A] = 0;
		full_off_time_ISR[PHASE_B] = 0;
		full_off_time_ISR[PHASE_C] = 0;
	}
	else // cb_On_Off
	{
		full_off_time_ISR[PHASE_A] = dA;
		full_off_time_ISR[PHASE_B] = dB;
		full_off_time_ISR[PHASE_C] = dC;

		full_on_time_ISR[PHASE_A] = 0;
		full_on_time_ISR[PHASE_B] = 0;
		full_on_time_ISR[PHASE_C] = 0;
	}
}



// Ф-ция проверяет состояние дискретных входов, и в случае срабатывания одного из них возвращает "ON", иначе - "OFF".
// uint8_t check_DI_status(void)
// {
// 	return ((params.phase_contact_states & 0b00111111) != 0) ? ON: OFF;			// Весь функционал ниже, но одной строкой.
// }
//   	uint8_t DI_status = PIN_DI;							// Read full PortA pin status. Pins are externaly pulled-up.
//   	DI_status = ~DI_status;								// Inverse the value
//   	DI_status &= 0b00111111;							// We need pins from PA0 to PA5 only.
//  	return (DI_status != 0) ? ON: OFF;					// If any bit is HIGH than return ON else return OFF;

// НЕАКТУАЛЬНО: переключает режим записи осциллограмм из режима следящего окна в режим полного заполнения буфера.
//	 	if (DI_status) ring_buffer_mode = FULL_FILLING;		// If any bit is HIGH than change the ring buffer mode to fill the buffer completely;


/*
cb_Off,
cb_Off_On,
cb_On,
cb_On_Off,
cb_NA
*/

/*==============================================================================
*  ОСНОВНАЯ ФУНКЦИЯ: ДЕТЕКЦИЯ НАЧАЛА/КОНЦА КОММУТАЦИИ ПО ФАЗАМ
*==============================================================================*/

/**
* @brief Отслеживание доп. контактов (концевиков) фаз A/B/C. Фиксация начала и завершения коммутации.
*
* Задачи:
*  1) Считать текущие RAW-данные входов:
*     - Реальный режим: RAW = (~PIN_DI) & 0x3F; при auxType==1 — зеркалировать B/C от A.
*     - Эмуляция: RAW формируется emu_contacts_make_state(t_rel).
*
*  2) Если RAW изменился (curr_state != prev_state), для каждой фазы:
*     - Определить, какой контакт активный (ON или OFF) по текущему логическому состоянию фазы CB_State[ph].
*     - Проверить изменение активного сигнала «с 1 на 0». Если оно произошло, и фаза была в PH_EDGE_STANDBY:
*         * записать phase_first_edge_ticks[ph] = ISR_microTimer;
*         * ph_edge_state[ph] = PH_EDGE_STARTED;
*         * если positionChanged == standingBy — поднять startScope,
*           определить lastOperation (cb_On_Off или cb_Off_On) и записать бит направления.
*
*  3) Независимо от п.2: для каждой фазы проверить устойчивое состояние (ровно один из битов ON/OFF = 1).
*     Если да, и ph_edge_state[ph] == PH_EDGE_STARTED:
*         * записать phase_fin_edge_ticks[ph] = ISR_microTimer;
*         * ph_edge_state[ph] = PH_EDGE_SILENT (локальная «тишина» по фазе до конца коммутации).
*
* Примечания:
*  - Функция НЕ запускает/НЕ останавливает осциллографирование — это делает внешний код по positionChanged.
*  - Индексы записи по фазам также НЕ изменяются здесь.
*/
/*==============================================================================
*  ОСНОВНАЯ ФУНКЦИЯ: ДЕТЕКЦИЯ НАЧАЛА/КОНЦА КОММУТАЦИИ ПО ФАЗАМ
*==============================================================================*/

/**
* @brief Отслеживание доп. контактов (концевиков) фаз A/B/C. Фиксация начала и завершения коммутации.
*
* Режим «тишины»:
*  - Если ph_edge_state[ph] == PH_EDGE_SILENT, фаза полностью игнорируется:
*    CB_State[ph] не обновляется, positionChanged не меняется, события не детектируются,
*    пока внешняя функция не вернёт фазу в PH_EDGE_STANDBY.
*/
uint16_t checkFunc = 0;

static inline void check_Input_States(void)
{
	
	//   uint8_t curr_state;
	checkFunc++;
	/* 1) RAW-данные: реальность vs эмуляция */
	if (!emulate_contacts)
	{
		/* Реальные входы: инвертируем pull-up и берём 6 младших бит */
		curr_state = (uint8_t)((~PIN_DI) & 0x3Fu);

		if (auxType == 1u)
		{
			/* Один блок концевиков: зеркалируем фазы B/C от A */
			const uint8_t a_on  = bitRead(curr_state, 0);
			const uint8_t a_off = bitRead(curr_state, 1);
			bitWrite(curr_state, 2, a_on);
			bitWrite(curr_state, 3, a_off);
			bitWrite(curr_state, 4, a_on);
			bitWrite(curr_state, 5, a_off);
		}
	}
	else
	{
		/* Эмуляция: формируем RAW по сценарию.
		Для простоты используем абсолютное время ISR_microTimer как t_rel. */
		curr_state = emu_contacts_make_state((uint32_t)ISR_microTimer);
	}

	/* 2) Начало коммутации: проверяем изменение активного сигнала (1 -> 0), кроме фаз в SILENT */
	if (curr_state != prev_state)
	{
		for (uint8_t ph = 0; ph < 3; ph++)
		{
			if (phase_Contact_state[ph] == PH_EDGE_SILENT) {
				/* фаза в «тишине» — полностью игнорируем */
				continue;
			}

			const uint8_t onMask  = (ph == PHASE_A ? CB1_ON  : (ph == PHASE_B ? CB2_ON  : CB3_ON));
			const uint8_t offMask = (ph == PHASE_A ? CB1_OFF : (ph == PHASE_B ? CB2_OFF : CB3_OFF));

			const uint8_t on_prev  = (prev_state & onMask)  ? 1u : 0u;
			const uint8_t off_prev = (prev_state & offMask) ? 1u : 0u;
			const uint8_t on_now   = (curr_state & onMask)  ? 1u : 0u;
			const uint8_t off_now  = (curr_state & offMask) ? 1u : 0u;

			/* Обновим логическое состояние фазы (для выбора активного контакта) — только если не SILENT */
			CB_State[ph] = breaker_fsm_update(on_now, off_now, CB_State[ph]);

			/* Какой контакт активный? */
			const uint8_t active_is_on = active_contact_is_on(CB_State[ph]);

			/* Было ли изменение активного сигнала с 1 -> 0? */
			const uint8_t active_prev = active_is_on ? on_prev  : off_prev;
			const uint8_t active_now  = active_is_on ? on_now   : off_now;

			if (active_prev == 1u && active_now == 0u && phase_Contact_state[ph] == PH_EDGE_STANDBY)
			{
				phase_first_edge_ticks[ph] = (uint16_t)ISR_microTimer;
				phase_Contact_state[ph]          = PH_EDGE_STARTED;

				/* Глобальный старт — только один раз на первую сработавшую фазу */
				if (positionChanged == standingBy) {
					lastOperation = active_is_on ? (uint8_t)cb_On_Off : (uint8_t)cb_Off_On;
					write_last_direction_bit(lastOperation);
					positionChanged = startScope;
				}
			}
		}
	}
	else {
		/* Нет изменений RAW — поддержим CB_State в актуальном виде, НО не трогаем фазы в SILENT */
		for (uint8_t ph = 0; ph < 3; ph++) {
			if (phase_Contact_state[ph] == PH_EDGE_SILENT) continue;

			const uint8_t onMask  = (ph == PHASE_A ? CB1_ON  : (ph == PHASE_B ? CB2_ON  : CB3_ON));
			const uint8_t offMask = (ph == PHASE_A ? CB1_OFF : (ph == PHASE_B ? CB2_OFF : CB3_OFF));
			const uint8_t on_now  = (curr_state & onMask)  ? 1u : 0u;
			const uint8_t off_now = (curr_state & offMask) ? 1u : 0u;
			CB_State[ph] = breaker_fsm_update(on_now, off_now, CB_State[ph]);
		}
	}

	/* 3) Завершение коммутации: устойчивое состояние (ровно один из битов = 1), кроме SILENT */
	for (uint8_t ph = 0; ph < 3; ph++)
	{
		if (phase_Contact_state[ph] != PH_EDGE_STARTED) continue; /* только для фаз, которые стартовали и ещё не в тишине */

		const uint8_t onMask  = (ph == PHASE_A ? CB1_ON  : (ph == PHASE_B ? CB2_ON  : CB3_ON));
		const uint8_t offMask = (ph == PHASE_A ? CB1_OFF : (ph == PHASE_B ? CB2_OFF : CB3_OFF));
		const uint8_t on_now  = (curr_state & onMask)  ? 1u : 0u;
		const uint8_t off_now = (curr_state & offMask) ? 1u : 0u;

		/* Устойчивое состояние пары ON/OFF: XOR == 1 */
		if ( (uint8_t)(on_now ^ off_now) == 1u )
		{
			phase_fin_edge_ticks[ph] = (uint16_t)ISR_microTimer;
			phase_Contact_state[ph]        = PH_EDGE_SILENT;

			/* Важно: зафиксируем конечное устойчивое логическое состояние
			и далее его НЕ будем менять, пока внешняя логика не вернёт STANDBY */
			CB_State[ph] = breaker_fsm_update(on_now, off_now, CB_State[ph]);
		}
	}

	/* 4) Обновить снимок RAW для следующего шага */
	if (curr_state != prev_state) {
		prev_state = curr_state;
	}
	
	
	contacts_watchdog();
}



// Ф-ция перебирает по очереди осциллограммы в массиве и ищет момент, когда значение тока горения дуги
// упадёт ниже определённого. Поиск производится по каждой из фаз. Найденные значения складываются
// в массив phase_zero.



/********************************* ARC CONSISTENCY + FINDING *********************************/

/**
* @brief Прочитать семпл фазы из FRAM и центрировать (вычесть калибровочный сдвиг ADC).
* @param phase  PHASE_A/B/C
* @param idx    индекс отсчёта в пределах записанных данных по фазе
* @return центрированное значение (int16)
*/
static inline int16_t osc_read_centered(uint8_t phase, uint16_t idx)
{
	if (phase > 2) return 0;
	uint16_t n = dbg_written[phase];
	if (n > PHASE_SAMPLES_MAX) n = PHASE_SAMPLES_MAX;
	if (idx >= n) return 0;
	return dbg_ct_raw[phase][idx];
}

/** Счётчики «последовательной консистентности» по фазам (раздельно для +порог/−порог) */
static uint16_t g_cons_pos[3] = {0,0,0};
static uint16_t g_cons_neg[3] = {0,0,0};
/**
* @brief Сбросить счётчики консистентности для выбранной фазы (RAM-вариант).
* @note Должны быть глобальные/статические счётчики g_cons_pos[3], g_cons_neg[3] (uint8_t).
*/
static inline void resetConsistency(uint8_t phase)
{
	g_cons_pos[phase] = 0;
	g_cons_neg[phase] = 0;
}


/*******************************  C O N S I S T E N C Y   C H E C K  ************************/
/**
* @brief Проверка консистентности дуги по RAM-буферу: N подряд точек |I| > min_Arc_Current.
* @param phase  PHASE_A/B/C
* @param idx    индекс сэмпла в dbg_ct_raw[phase][]
* @return 1 — условие выполнено, 0 — нет.
*
* Предполагается:
*  - dbg_ct_raw уже центрирован (int16_t, −2048..+2047).
*  - min_Arc_Current задан в кодах АЦП после центрирования (0..2047).
*  - Число подряд N ≤ PHASE_SAMPLES_MAX (например, 5..400).
*/
static inline uint8_t checkConsistency_RAM(uint8_t phase, uint16_t idx)
{
	const int16_t thr = (int16_t)min_Arc_Current;
	const uint16_t N  = (uint16_t)arc_consistency_len;   /* можно задавать в конфиге */
	
	const int16_t v = dbg_ct_raw[phase][idx];  /* центрированный сэмпл */

	if (v > thr) {
		if (g_cons_pos[phase] < PHASE_SAMPLES_MAX) g_cons_pos[phase]++;
		g_cons_neg[phase] = 0u;
		return (g_cons_pos[phase] >= N);
	}
	else if (v < -thr) {
		g_cons_pos[phase] = 0u;
		if (g_cons_neg[phase] < PHASE_SAMPLES_MAX) g_cons_neg[phase]++;
		return (g_cons_neg[phase] >= N);
	}
	else {
		g_cons_pos[phase] = 0u;
		g_cons_neg[phase] = 0u;
		return 0u;
	}
}

/********************************  A R C   F I N D E R  ( R A M )  **************************/
/**
* @brief Поиск момента окончания/начала дуги по каждой фазе (RAM-версия, без FRAM и CALC).
*
* Логика:
*  - Отключение (cb_On_Off): сканируем НАЗАД по диапазону [0 .. n-1].
*  - Включение  (cb_Off_On): сканируем ВПЕРЁД по диапазону [N-n .. N-1].
*  - Консистентность: N подряд точек |I| > min_Arc_Current (см. checkConsistency_RAM).
*  - Найденную точку сдвигаем на ±6 отсчётов (клиппим по границам диапазона).
*
* Выход:
*  - arcDetected[ph]   = 1/0
*  - ISR_arcPoint[ph]  = индекс точки (0..PHASE_SAMPLES_MAX-1) в пределах фактически записанной длины
*
* Требования:
*  - Вызывать ПОСЛЕ центрирования буфера (osc_center_inplace_phase) и ДО его очистки.
*/
void finding_Current_Start_Point(void)
{
	partialElectricalSwitch = 0;
	/* по умолчанию — «дуги нет» */
	for (uint8_t ph = 0; ph < 3; ph++)
	{
		currentDetected[ph]  = 0;
		ISR_arcPoint[ph] = 0;
		resetConsistency(ph);
	}

	/* сколько фаз реально анализируем */
	const uint8_t nPhases = phase_quantity;
	const uint16_t Nmax = PHASE_SAMPLES_MAX;

	for (uint8_t ph = 0; ph < nPhases; ph++)
	{
		/* фактическая длина записи по фазе (из RAM-вьювера); 0 → пропускаем */
		uint16_t n = dbg_written[ph];
		if (n == 0) { currentDetected[ph] = 0; ISR_arcPoint[ph] = 0; continue; }
		if (n > Nmax) n = Nmax;

		const uint8_t is_off = (lastOperation == cb_On_Off);  /* 1=отключение, 0=включение */

		if (is_off)
		{
			/* ── ОТКЛЮЧЕНИЕ: валидный диапазон [0 .. n-1], сканируем НАЗАД ── */
			const uint16_t first = 0u;
			const uint16_t last  = (uint16_t)(n - 1u);

			uint16_t found = 0;
			uint8_t  ok    = 0;

			for (int32_t idx = (int32_t)last; idx >= (int32_t)first; --idx)
			{
				if (checkConsistency_RAM(ph, (uint16_t)idx)) {
					ok = 1u;
					/* «чуть после» детекта (в сторону тишины): +6, но не дальше last */
					int32_t cand = idx + arc_consistency_len;
					if (cand > (int32_t)last) cand = (int32_t)last;
					found = (uint16_t)cand;
					break;
				}
			}

			currentDetected[ph]  = ok;
			ISR_arcPoint[ph] = ok ? found : first;
			
			if (ISR_arcPoint[ph] > latestDetectedCurrent )
			partialElectricalSwitch = 1;
		}
		else
		{
			/* ── ВКЛЮЧЕНИЕ: пишем в хвост, валидный диапазон [N-n .. N-1], сканируем ВПЕРЁД ── */
			const uint16_t first = (uint16_t)(Nmax - n);
			const uint16_t last  = (uint16_t)(Nmax - 1u);

			uint16_t found = last;  /* по умолчанию — «в конце» валидного окна */
			uint8_t  ok    = 0;

			for (uint16_t idx = first; idx <= last; idx++)
			{
				if (checkConsistency_RAM(ph, idx)) {
					ok = 1u;
					/* «чуть раньше» детекта (в сторону тишины): −6, но не ниже first */
					int32_t cand = (int32_t)idx - arc_consistency_len;
					if (cand < (int32_t)first) cand = (int32_t)first;
					found = (uint16_t)cand;
					break;
				}
			}

			currentDetected[ph]  = ok;
			ISR_arcPoint[ph] = found;
			
			if ((ISR_arcPoint[ph] > latestDetectedCurrent) || (currentDetected[ph] == 0))
			partialElectricalSwitch = 1;
			
		}
		

	}
	
	//	bitWrite(complexSP, part_Phase_Switch_Bit, partialElectricalSwitch || contact_wd_tripped);
	bitWrite(complexSP, part_Phase_Switch_Bit, contact_wd_tripped);
	
	
}
/********************************  F U L L   T I M E   ( R A M )  ***************************/
/**
* @brief Полное время от начала движения активного контакта до события тока (в ОТСЧЁТАХ RAM) по каждой фазе.
*
* Опора:
*  - Старт RAM-записи по фазе совпадает с моментом начала движения контакта → индекс 0.
*  - ISR_arcPoint[ph] — индекс точки появления (включение) или исчезновения (отключение) тока.
*  - dbg_written[ph]  — реально записанное число отсчётов по фазе (0..PHASE_SAMPLES_MAX).
*
* Расчёт:
*  ticks = clamp(ISR_arcPoint[ph], 0 .. nPh-1).
*
* Результат:
*  - при отключении (cb_On_Off) → full_off_time_ISR[ph] = ticks;  (full_on_time_ISR[ph] не трогаем)
*  - при включении  (cb_Off_On) → full_on_time_ISR[ph]  = ticks;  (full_off_time_ISR[ph] не трогаем)
*
* Примечания:
*  - Ничего не зеркалим при auxType==1 — фазы независимы.
*  - Время в микросекундах: time_us = ticks * SAMPLE_PERIOD_US (вне этой функции).
*/
static inline uint16_t clamp_u16_u16(uint16_t v, uint16_t lo, uint16_t hi)
{
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

/**
* @brief Расчёт «полного времени» ВКЛ/ВЫКЛ по фазам в отсчётах RAM-окна
*        с учётом лагов вспомогательных контактов.
*
* @details
* Обозначения:
*  - arc_idx  — индекс появления (ВКЛ) / исчезновения (ВЫКЛ) тока.
*  - fin_idx  — индекс «контакт в конечном положении» (по вспомогательному).
*  - on_contact_lag_ticks[ph]  — запаздывание вспомогательного на ВКЛ (тики).
*  - off_contact_lag_ticks[ph] — «ранний» уход вспомогательного на ВЫКЛ (тики).
*
* Логика:
*  - ВКЛ (lastOperation != cb_On_Off):
*      fin_idx_corr = clamp( fin_idx - on_contact_lag_ticks[ph], 0 .. nPh-1 )
*      full_on_time_ISR[ph] = min( arc_idx, fin_idx_corr )
*      ⇒ Даже если ток не найден или найден позже контакта, результат не превышает
*         реальный приход контакта, скорректированный на запаздывание доп.контакта.
*
*  - ВЫКЛ (lastOperation == cb_On_Off):
*      arc_idx_corr = clamp( arc_idx - off_contact_lag_ticks[ph], 0 .. nPh-1 )
*      full_off_time_ISR[ph] = arc_idx_corr
*      ⇒ Первые «off_contact_lag_ticks[ph]» тиков после старта считаем лагом доп.контакта
*         и не относим к дуге, чтобы не ловить ложные срабатывания.
*
* Защиты:
*  - Все индексы зажаты в [0 .. nPh-1].
*  - При fin_rel < 0 → fin_idx = 0.
*  - При dbg_written[ph] == 0 — фаза пропускается (значения не трогаются).
*
* @pre
*  - ISR_arcPoint[ph], phase_first_edge_ticks[ph], phase_fin_edge_ticks[ph] валидны.
*  - dbg_written[ph] ∈ [0 .. PHASE_SAMPLES_MAX].
*  - on_contact_lag_ticks[ph] и off_contact_lag_ticks[ph] заданы (в тиках, ≥ 0).
*
* @post
*  - full_on_time_ISR[ph], full_off_time_ISR[ph] обновлены.
*/



void calcFullTime(void)
{
	const uint8_t nPhases = (uint8_t)phase_quantity;
	const uint8_t is_off  = (lastOperation == cb_On_Off); /* 1=отключение, 0=включение */

	for (uint8_t ph = 0; ph < nPhases; ph++)
	{
		/* 1) Фактическая длина записи по фазе */
		uint16_t nPh = dbg_written[ph];
		if (nPh == 0u) continue;                         /* нет данных — ничего не меняем */
		if (nPh > PHASE_SAMPLES_MAX) nPh = PHASE_SAMPLES_MAX;
		const uint16_t nPh_1 = (uint16_t)(nPh - 1u);

		/* 2) Индекс дуги (появление для ВКЛ / исчезновение для ВЫКЛ) */
		uint16_t arc_idx = ISR_arcPoint[ph];
		if (arc_idx > nPh_1) arc_idx = nPh_1;

		/* 3) Индекс «контакт в конечном положении» (по вспомогательному) */
		int32_t fin_rel = (int32_t)phase_fin_edge_ticks[ph] - (int32_t)phase_first_edge_ticks[ph];
		if (fin_rel < 0) fin_rel = 0;
		uint16_t fin_idx = (uint16_t)fin_rel;
		if (fin_idx > nPh_1) fin_idx = nPh_1;

		if (is_off)
		{
			/* ВЫКЛ: игнорируем ранний уход доп.контакта в пределах off_contact_lag_ticks[ph] */
			uint16_t lag_off = off_contact_lag_ticks[ph];
			uint16_t arc_idx_corr = (arc_idx > lag_off) ? (uint16_t)(arc_idx - lag_off) : 0u;
			if (arc_idx_corr > nPh_1) arc_idx_corr = nPh_1;

			full_off_time_ISR[ph] = arc_idx_corr;
		}
		else
		{
			/* ВКЛ: сдвигаем приход контакта «раньше» на запаздывание доп.контакта */
			uint16_t lag_on = on_contact_lag_ticks[ph];
			uint16_t fin_idx_corr = (fin_idx > lag_on) ? (uint16_t)(fin_idx - lag_on) : 0u;
			if (fin_idx_corr > nPh_1) fin_idx_corr = nPh_1;

			/* Не превышать момент прихода контакта (уже скорректированный на лаг) */
			const uint16_t ticks_on = (arc_idx < fin_idx_corr) ? arc_idx : fin_idx_corr;
			full_on_time_ISR[ph] = ticks_on;
		}
	}
}

//
// void calcFullTime(void)
// {
// 	const uint8_t nPhases = (uint8_t)phase_quantity;
// 	const uint8_t is_off  = (lastOperation == cb_On_Off); /* 1=отключение, 0=включение */
//
// 	for (uint8_t ph = 0; ph < nPhases; ph++)
// 	{
// 		/* сколько реально записано по фазе — если 0, пропускаем (ничего не меняем) */
// 		uint16_t nPh = dbg_written[ph];
// 		if (nPh == 0u) continue;
//
// 		if (nPh > PHASE_SAMPLES_MAX) nPh = PHASE_SAMPLES_MAX;
//
// 		/* clamp точки дуги в пределах фактически записанного окна [0..nPh-1] */
// 		uint16_t arc = ISR_arcPoint[ph];
// 		if (arc >= nPh) arc = (uint16_t)(nPh - 1u);
//
// 		/* ticks от «нуля» до дуги = arc */
// 		const uint16_t ticks = arc;
//
// 		if (is_off) {
// 			/* Отключение: время до исчезновения тока */
// 			full_off_time_ISR[ph] = ticks;
// 			} else {
// 			/* Включение: время до появления тока */
// 			full_on_time_ISR[ph]  = ticks;
// 		}
// 	}
// }

/********************************  A R C   L I F E T I M E   ( R A M )  *********************/
/**
* @brief Время горения дуги по фазам (в ОТСЧЁТАХ, RAM-режим).
*
* Определения:
*  - Старт окна RAM по фазе = 0 (момент начала движения контакта).
*  - ISR_arcPoint[ph] — индекс появления тока (ВКЛ) или исчезновения тока (ВЫКЛ).
*  - fin_idx — индекс момента «контакт в конечном положении»:
*      fin_idx = clamp( phase_fin_edge_ticks[ph] - phase_first_edge_ticks[ph], 0 .. nPh-1 ).
*
* Расчёт:
*  - cb_Off_On (ВКЛ):  lifetime = max(0, fin_idx - arc_idx).
*  - cb_On_Off (ВЫКЛ): lifetime = arc_idx.
*
* Примечания:
*  - nPh = dbg_written[ph] (фактически записанные точки). Если nPh==0 — фазу пропускаем.
*  - Никаких зеркалирований: каждая фаза считается отдельно.
*  - Пересчёт в мкс делается вне функции: time_us = lifetime * SAMPLE_PERIOD_US.
*/
static inline uint16_t clamp_u16_range(int32_t v, uint16_t lo, uint16_t hi)
{
	if (v < (int32_t)lo) return lo;
	if (v > (int32_t)hi) return hi;
	return (uint16_t)v;
}

/**
* @brief Расчёт времени горения дуги по фазам в отсчётах RAM-окна с учётом лагов доп.контактов.
*
* @details
* Определения (после коррекции лагов):
* - ВЫКЛ (lastOperation == cb_On_Off): дуга = [0 .. arc_idx_corr], где
*       arc_idx_corr = clamp( ISR_arcPoint[ph] - off_contact_lag_ticks[ph], 0 .. nPh-1 ).
*   Ранний уход доп.контакта не считаем дугой.
*
* - ВКЛ  (lastOperation != cb_On_Off): дуга = [arc_idx .. fin_idx_corr), где
*       fin_idx_corr = clamp( fin_idx - on_contact_lag_ticks[ph], 0 .. nPh-1 ).
*   Если arc_idx >= fin_idx_corr — дуги нет.
*
* Защиты:
* - Все индексы зажаты в [0 .. nPh-1].
* - При отсутствии данных (dbg_written[ph] == 0) — фаза пропускается.
* - При расхождении меток (fin_rel < 0) — fin_idx = 0.
*
* @pre
*  - ISR_arcPoint[ph] — индекс появления/исчезновения тока.
*  - phase_first_edge_ticks[ph], phase_fin_edge_ticks[ph] относятся к одному окну.
*  - on_contact_lag_ticks[ph], off_contact_lag_ticks[ph] заданы (≥0).
*
* @post
*  - ISR_arc_lifetime[ph] — длительность дуги (в тиках) с учётом лагов.
*  - mechSwitchTime[ph]   — сырое fin_rel (в тиках) для диагностики (без коррекции).
*/
void calcArcLifetime(void)
{
	const uint8_t nPhases = (uint8_t)phase_quantity;
	const uint8_t is_off  = (lastOperation == cb_On_Off); /* 1=ВЫКЛ, 0=ВКЛ */

	for (uint8_t ph = 0; ph < nPhases; ph++)
	{
		/* 1) Проверка наличия данных и ограничение длины окна */
		uint16_t nPh = dbg_written[ph];
		if (nPh == 0u) {
			ISR_arc_lifetime[ph] = 0u;
			mechSwitchTime[ph]   = 0;
			continue;
		}
		if (nPh > PHASE_SAMPLES_MAX) nPh = PHASE_SAMPLES_MAX;
		const uint16_t nPh_1 = (uint16_t)(nPh - 1u);

		/* 2) Базовый индекс дуги (исчезновение для OFF / появление для ON) */
		uint16_t arc_idx = ISR_arcPoint[ph];
		if (arc_idx > nPh_1) arc_idx = nPh_1;

		/* 3) Индекс «контакт в конечном положении» в координатах RAM-окна (сырой) */
		int32_t fin_rel = (int32_t)phase_fin_edge_ticks[ph] - (int32_t)phase_first_edge_ticks[ph];
		mechSwitchTime[ph] = fin_rel;                 /* сохраняем сырое значение для телеметрии */
		if (fin_rel < 0) fin_rel = 0;
		uint16_t fin_idx = (uint16_t)fin_rel;
		if (fin_idx > nPh_1) fin_idx = nPh_1;

		/* 4) Коррекция лагов */
		uint16_t lifetime = 0u;

		if (is_off) {
			/* ВЫКЛ: ранний уход доп.контакта вычитаем из arc_idx */
			uint16_t lag_off = off_contact_lag_ticks[ph];
			uint16_t arc_idx_corr = (arc_idx > lag_off) ? (uint16_t)(arc_idx - lag_off) : 0u;
			if (arc_idx_corr > nPh_1) arc_idx_corr = nPh_1;

			/* Дуга от начала до скорректированного исчезновения тока */
			lifetime = arc_idx_corr;
			} else {
			/* ВКЛ: доп.контакт запаздывает — сдвигаем приход «раньше» */
			uint16_t lag_on = on_contact_lag_ticks[ph];
			uint16_t fin_idx_corr = (fin_idx > lag_on) ? (uint16_t)(fin_idx - lag_on) : 0u;
			if (fin_idx_corr > nPh_1) fin_idx_corr = nPh_1;

			/* Дуга только пока контакт «в пути» */
			lifetime = (arc_idx < fin_idx_corr) ? (uint16_t)(fin_idx_corr - arc_idx) : 0u;
		}

		ISR_arc_lifetime[ph] = lifetime;
		
		if (ISR_arc_lifetime[ph] != 0)
		arcDetected[ph] = 1;
		else
		arcDetected[ph] = 0;
	}
}


/**
* @brief Комплексные флаги WARN/ALARM по временам ON/OFF для текущей коммутации (в тиках).
* @details
*  - Смотрим только ту половину, которая соответствует lastOperationBuf[CALC]:
*      cb_On_Off → считаем OFF-пороги; cb_Off_On → считаем ON-пороги.
*  - Агрегация по фазам: если хотя бы у одной фазы сработал критерий — флаг=1.
*  - Пороговая логика:
*      WARN:  own ∈ [ownWarn; ownAlarm)  и/или  full ∈ [fullWarn; fullAlarm)
*      ALARM: own ≥ ownAlarm             и/или  full ≥ fullAlarm
*/
void calcTimeStates(void)
{
	const uint8_t nPhases = phase_quantity;

	//	uint8_t ownTimeOffWarnState  = 0, ownTimeOffAlarmState  = 0;
	fullTimeOffWarnState = 0, fullTimeOffAlarmState = 0;

	//	uint8_t ownTimeOnWarnState   = 0, ownTimeOnAlarmState   = 0;
	fullTimeOnWarnState  = 0, fullTimeOnAlarmState  = 0;

	const uint8_t is_off = (lastOperation == cb_On_Off) ? 1u : 0u;

	for (uint8_t ph = 0; ph < nPhases; ph++)
	{
		//	const uint16_t own_off  = own_off_time_ISR[ph];
		const uint16_t full_off = full_off_time_ISR[ph];
		//	const uint16_t own_on   = own_on_time_ISR[ph];
		const uint16_t full_on  = full_on_time_ISR[ph];

		if (is_off)
		{
			/* --- OFF-пороговая проверка --- */
			//	if (own_off  >= ownTimeOffWarn_ISR  && own_off  < ownTimeOffAlarm_ISR)  ownTimeOffWarnState  = 1;
			//	if (own_off  >= ownTimeOffAlarm_ISR)                                    ownTimeOffAlarmState = 1;

			if (full_off >= fullTimeOffWarn_ISR && full_off < fullTimeOffAlarm_ISR) fullTimeOffWarnState = 1;
			if (full_off >= fullTimeOffAlarm_ISR)                                   fullTimeOffAlarmState= 1;
		}
		else
		{
			/* --- ON-пороговая проверка --- */
			//	if (own_on   >= ownTimeOnWarn_ISR   && own_on   < ownTimeOnAlarm_ISR)   ownTimeOnWarnState   = 1;
			//	if (own_on   >= ownTimeOnAlarm_ISR)                                     ownTimeOnAlarmState  = 1;

			if (full_on  >= fullTimeOnWarn_ISR  && full_on  < fullTimeOnAlarm_ISR)  fullTimeOnWarnState  = 1;
			if (full_on  >= fullTimeOnAlarm_ISR)                                    fullTimeOnAlarmState = 1;
		}
	}

	/* Выставляем биты комплексных состояний.
	Примечание: при ALARM соответствующий WARN тоже может остаться =1 (как сейчас).
	Если нужно «ALARM гасит WARN», можно поменять логику здесь. */
	//	bitWrite(complexSP, ownTimeOff_Warn_Bit,   ownTimeOffWarnState);
	//	bitWrite(complexSP, ownTimeOff_Alarm_Bit,  ownTimeOffAlarmState);

	//	bitWrite(complexSP, ownTimeOn_Warn_Bit,    ownTimeOnWarnState);
	//	bitWrite(complexSP, ownTimeOn_Alarm_Bit,   ownTimeOnAlarmState);

	bitWrite(complexSP, fullTimeOffWarn_Bit,   fullTimeOffWarnState);
	bitWrite(complexSP, fullTimeOffAlarm_Bit,  fullTimeOffAlarmState);

	bitWrite(complexSP, fullTimeOnWarn_Bit,    fullTimeOnWarnState);
	bitWrite(complexSP, fullTimeOnAlarm_Bit,   fullTimeOnAlarmState);
}




#define UINT16_MAX 65535
/**
* @brief Максимальный ток коммутации по фазам (пик в КОДАХ АЦП после центрирования).
* @details
*  - Берём фактическую длину каждой фазы (RAM: dbg_written[] через osc_get_lengths()).
*  - Читаем центрированные отсчёты (−2048..+2047) через osc_read_centered().
*  - Берём модуль и фиксируем максимум.
*  - Результат: switchCurrent_Amp[ph] = max(|I[k]|) (в кодах АЦП).
*    При необходимости RMS ≈ max/1.4142; физ. ток через калибровочные коэффициенты.
*/
void calcSwitchCurrent(void)
{
	uint16_t nA, nB, nC, nMin;
	osc_get_lengths(&g_proc_hdr, &nA, &nB, &nC, &nMin);

	const uint8_t nPhases = phase_quantity;

	for (uint8_t ph = 0; ph < nPhases; ph++)
	{
		const uint16_t nPh = (ph == PHASE_A ? nA : (ph == PHASE_B ? nB : nC));
		int16_t maxAbs = 0;

		for (uint16_t i = 0; i < nPh; i++)
		{
			int16_t s = osc_read_centered(ph, i);   // уже центрировано (−2048..+2047)

			// Безопасный модуль (защита на случай INT16_MIN)
			int16_t a = (s >= 0) ? s : (int16_t)(-s == s ? 32767 : -s);

			if (a > maxAbs) maxAbs = a;
		}

		switchCurrent_Amp[ph] = (uint16_t)maxAbs;
	}

	/* При auxType==1 оставляем отдельные значения по фазам (токи могут отличаться).
	Если потребуется жёстко синхронизировать B/C по A — раскомментируй:
	// if (auxType == 1u) {
	//     switchCurrent_Amp[PHASE_B] = switchCurrent_Amp[PHASE_A];
	//     switchCurrent_Amp[PHASE_C] = switchCurrent_Amp[PHASE_A];
	// }
	*/
}

/********************************  k A ^ 2 s   ( R A M )  **********************************/
/**
* @brief Расчёт ∫I^2 dt за последнюю коммутацию (kA²·s) — RAM-режим.
*
* Интервал интегрирования (индексы RAM):
*  - cb_Off_On (ВКЛ):  [arc_idx .. fin_idx)   — от появления тока до конечного положения контакта.
*  - cb_On_Off (ВЫКЛ): [0 .. arc_idx)         — от начала движения до исчезновения тока.
*
* Где:
*  - arc_idx = clamp(ISR_arcPoint[ph], 0 .. nPh-1)
*  - fin_idx = clamp(phase_fin_edge_ticks[ph] - phase_first_edge_ticks[ph], 0 .. nPh-1)
*  - nPh     = dbg_written[ph] (0..PHASE_SAMPLES_MAX)
*
* Единицы:
*  - I в амперах: I_A = amps_per_code[ph] * (osc_read_centered(ph,i) - adcShift_ph)
*  - dt = SAMPLE_PERIOD_US * 1e-6 (секунда)
*  - Выход: kA²·s = (A²·s) / 1e6
*
* Замечания:
*  - Если i0>=i1 или nPh==0 — энергия = 0.
*  - Никаких зеркалирований по auxType: считаем каждую фазу отдельно.
*/
void calcLast_kA2s(void)
{
	const uint8_t nPhases = (uint8_t)phase_quantity;
	const float dt = (float)SAMPLE_PERIOD_US * 1e-6f;  /* шаг интегрирования, секунды */

	for (uint8_t ph = 0; ph < nPhases; ph++)
	{
		float energy_A2s = 0.0f;

		/* длина фактически записанного окна */
		uint16_t nPh = dbg_written[ph];
		if (nPh == 0u) { last_kA2s[ph] = 0.0f; continue; }
		if (nPh > PHASE_SAMPLES_MAX) nPh = PHASE_SAMPLES_MAX;

		/* индексы характерных событий, с клипом в [0..nPh-1] */
		uint16_t arc_idx = ISR_arcPoint[ph];
		if (arc_idx >= nPh) arc_idx = (uint16_t)(nPh - 1u);

		int32_t fin_rel = (int32_t)phase_fin_edge_ticks[ph] - (int32_t)phase_first_edge_ticks[ph];
		if (fin_rel < 0) fin_rel = 0;
		uint16_t fin_idx = (uint16_t)fin_rel;
		if (fin_idx >= nPh) fin_idx = (uint16_t)(nPh - 1u);

		/* выбрать интервал интегрирования по направлению */
		uint16_t i0, i1;
		if (lastOperation == cb_Off_On) {
			/* ВКЛ: от появления тока до конечного положения контакта */
			i0 = arc_idx;
			i1 = fin_idx;
			} else {
			/* ВЫКЛ: от старта движения (индекс 0) до исчезновения тока */
			i0 = 0u;
			i1 = arc_idx;
		}

		/* полуинтервал [i0, i1) в пределах записанного окна */
		if (i1 > nPh) i1 = nPh;
		if (i0 >= i1) { last_kA2s[ph] = 0.0f; continue; }

		/* остаточный DC-сдвиг (в кодах) и коэффициент «коды → Амперы» */
		//         const int16_t adcShift = (ph==PHASE_A ? cts.adcShift_A :
		//                                (ph==PHASE_B ? cts.adcShift_B : cts.adcShift_C));
		//       const float amps_per_code = k[ph]; /* твой коэффициент пересчёта в Амперы */

		/* интегрирование I^2 dt */
		for (uint16_t i = i0; i < i1; i++) {
			const float I_A = osc_sample_in_amps(ph, i);

			energy_A2s += (I_A * I_A) * dt;                                   /* A²·s */
		}

		last_kA2s[ph] = energy_A2s * 1e-6f;  /* A²·s → kA²·s */
	}
}



/**
* @brief Расчёт ∫I^2 dt за последнюю коммутацию (kA²·s) — RAM-режим.
* @details В RAM запись фазы начинается с индекса 0; окно интегрирования берём в пределах [0..nPh-1].
*          Сэмплы читаем центрированными (osc_read_centered), при необходимости вычитаем остаточный adcShift_*.
*/
// void calcLast_kA2s(void)
// {
// 	const uint8_t nPhases = (auxType == 1u) ? 1u : (uint8_t)phase_quantity;
//
// 	uint16_t nA, nB, nC, nMin;
// 	osc_get_lengths(&g_proc_hdr, &nA, &nB, &nC, &nMin);   // RAM: dbg_written[ph]
//
// 	const float dt = (float)SAMPLE_PERIOD_US * 1e-6f;     // шаг интегрирования (с)
//
// 	for (uint8_t ph = 0; ph < nPhases; ph++)
// 	{
// 		float energy_A2s = 0.0f;
//
// 		if (ISR_arc_lifetime[ph] == 0u) {                 // дуги нет → 0
// 			last_kA2s[ph] = 0.0f;
// 			continue;
// 		}
//
// 		const uint16_t nPh = (ph==PHASE_A ? nA : (ph==PHASE_B ? nB : nC));
// 		if (nPh == 0u) { last_kA2s[ph] = 0.0f; continue; }
//
// 		const uint16_t st0 = 0u;
// 		const uint16_t fn0 = (uint16_t)(nPh - 1u);
//
// 		uint16_t arc = ISR_arcPoint[ph];
// 		if (arc > fn0) arc = fn0;
//
// 		uint16_t i0, i1; // полузакрытый [i0, i1)
//
// 		if (lastOperationBuf[CALC] == cb_On_Off) {
// 			// ОТКЛЮЧЕНИЕ: старт = st0 + delayOff; финиш = min(fn0, arc + PAD_OFF)
// 			int32_t start_i32 = (int32_t)st0 + (int32_t)ISR_timeShiftOff[ph];
// 			i0 = clamp_u16_range(start_i32, st0, fn0);
//
// 			uint16_t arc_end = (uint16_t)((arc + ARC_PAD_TICKS_OFF <= fn0) ? (arc + ARC_PAD_TICKS_OFF) : fn0);
// 			i1 = arc_end;
// 			} else {
// 			// ВКЛЮЧЕНИЕ: старт = max(st0, arc - delayOn); финиш = min(fn0, arc + PAD_ON)
// 			int32_t start_i32 = (int32_t)arc - (int32_t)ISR_timeShiftOn[ph];
// 			i0 = clamp_u16_range(start_i32, st0, fn0);
//
// 			uint16_t arc_end = (uint16_t)((arc + ARC_PAD_TICKS_ON <= fn0) ? (arc + ARC_PAD_TICKS_ON) : fn0);
// 			i1 = arc_end;
// 		}
//
// 		if (i1 > nPh) i1 = nPh;          // делаем [i0, i1) в пределах записанного
// 		if (i0 >= i1) { last_kA2s[ph] = 0.0f; continue; }
//
// 		// остаточный DC-сдвиг (в кодах) и коэффициент «коды → Амперы»
// 		const int16_t adcShift = (ph==PHASE_A ? cts.adcShift_A :
// 		(ph==PHASE_B ? cts.adcShift_B : cts.adcShift_C));
// 		const float kA_per_code = k[ph];
//
// 		for (uint16_t i = i0; i < i1; i++) {
// 			// центрированный код → коррекция остаточного смещения → Амперы
// 			int16_t x = (int16_t)(osc_read_centered(ph, i) - adcShift);
// 			float I_A = kA_per_code * (float)x;
// 			energy_A2s += (I_A * I_A) * dt;
// 		}
//
// 		last_kA2s[ph] = energy_A2s * 1e-6f;  // A²·s → kA²·s
// 	}
//
// 	// при auxType==1 оставляем независимые значения по фазам (токи могут отличаться)
// 	// если нужно — можно синхронизировать B/C по A
// }




/**
* @brief Перевод порога тока из ампер в «коды АЦП» (для центрированного сигнала).
*        Если I = k*x + b и ты используешь b, см. вариант с b ниже.
* @param ph        PHASE_A/B/C
* @param I_thresh  порог в амперах
* @return порог в кодах (модуль), для сравнения с switchCurrent_Amp[ph]
*/
static inline uint16_t overcurrent_threshold_in_codes(uint8_t ph, float I_thresh)
{
	float kA_per_code = k[ph];              // А/код
	if (kA_per_code <= 1e-9f) return 0xFFFF; // защита от деления на ноль

	float x = I_thresh / kA_per_code;       // коды
	if (x < 0) x = -x;
	if (x > 32767.f) x = 32767.f;           // ограничим до int16
	return (uint16_t)(x + 0.5f);            // округление
}

/**
* @brief Детект КЗ по пику тока. Сравнение в «кодах АЦП».
*        switchCurrent_Amp[ph] — пик |x| в кодах (после центрирования),
*        I_thr = I_nom * K_over → переводим в коды и сравниваем.
*/
void findShortCircuit(void)
{
	const float I_thr = params.nominalCurrent_Amp * params.overCurrentKoef;

	bool changed = false;

	for (uint8_t ph = 0; ph < phase_quantity; ph++)
	{
		uint16_t thr_codes = overcurrent_threshold_in_codes(ph, I_thr);
		uint16_t peak_codes = (uint16_t)switchCurrent_Amp[ph];

		if (peak_codes > thr_codes)
		{
			switch (ph)
			{
				case PHASE_A: params.short_Cnt_A++; break;
				case PHASE_B: params.short_Cnt_B++; break;
				case PHASE_C: params.short_Cnt_C++; break;
			}
			changed = true;
		}
	}

	// Чтобы не убивать EEPROM, лучше писать на «медленном шаге».
	// Если надо прямо здесь — оставь так, но за один проход:
	if (changed) {
		eeprom_write_word((uint16_t*)74, params.short_Cnt_A);
		eeprom_write_word((uint16_t*)76, params.short_Cnt_B);
		eeprom_write_word((uint16_t*)78, params.short_Cnt_C);
	}
}







void getRMS(int16_t osc_buffer[][phase_quantity], uint8_t command_)
{
	
	if (command_ == 0)
	{
		// 		if (cts.amplification == 1)
		// 		{
		// 			current_A_buf += sqrForRMS(osc_buffer,PHASE_A,cts.gainAdcShift_A);
		// 			current_B_buf += sqrForRMS(osc_buffer,PHASE_B,cts.gainAdcShift_B);
		// 			current_C_buf += sqrForRMS(osc_buffer,PHASE_C,cts.gainAdcShift_C);
		// 		}
		// 		else
		//		{
		current_A_buf += sqrForRMS(osc_buffer,PHASE_A,cts.adcShift_A);
		current_B_buf += sqrForRMS(osc_buffer,PHASE_B,cts.adcShift_B);
		current_C_buf += sqrForRMS(osc_buffer,PHASE_C,cts.adcShift_C);
		//		}
	}
	else
	{
		int16_t shiftA = (int16_t)RMS_Zero[PHASE_A] - 128;
		int16_t shiftB = (int16_t)RMS_Zero[PHASE_B] - 128;
		int16_t shiftC = (int16_t)RMS_Zero[PHASE_C] - 128;

		float_RMS_A = sqrtf(current_A_buf * 0.0005f) + shiftA;
		float_RMS_B = sqrtf(current_B_buf * 0.0005f) + shiftB;
		float_RMS_C = sqrtf(current_C_buf * 0.0005f) + shiftC;

		
		if (float_RMS_A < 0)
		float_RMS_A = 0;
		
		if (float_RMS_B < 0)
		float_RMS_B = 0;
		
		if (float_RMS_C < 0)
		float_RMS_C = 0;
		
	}
}

/**
* @brief Квадрат центрированного значения для RMS с учётом фазового сдвига.
* @param osc_buffer  буфер сырых кодов [sample][phase]
* @param phase_      индекс фазы (0..2)
* @param shift_      калибровочный сдвиг, uint8_t 0..255 => signed offset = shift_-128
* @return (x - ADC_MID + shift_signed)^2, тип int64_t
*/
int64_t sqrForRMS(int16_t osc_buffer[][phase_quantity], uint8_t phase_, uint8_t shift_)
{
	// 0..255 -> -128..+127
	const int16_t shift_signed = (int16_t)shift_ - 128;

	// читаем текущий код; если это сырые 12 бит 0..4095, тип int16_t ок
	const int32_t x = (int32_t)osc_buffer[0][phase_];

	// убедись, что ADC_MID совместим по типу (лучше (int32_t)ADC_MID)
	const int32_t diff = x - (int32_t)ADC_MID + (int32_t)shift_signed;

	// квадрат в 64 бит во избежание переполнения
	return (int64_t)diff * (int64_t)diff;
}


// void getRMS(int16_t osc_buffer[][phase_quantity], uint8_t command_)
// {
// 	if (command_ == 0) {
// 		// Calculate squared sums with proper scaling to prevent overflow
// 		if (cts.amplification == 1) {
// 			current_A_buf += sqrForRMS(osc_buffer, PHASE_A, cts.gainAdcShift_A);
// 			current_B_buf += sqrForRMS(osc_buffer, PHASE_B, cts.gainAdcShift_B);
// 			current_C_buf += sqrForRMS(osc_buffer, PHASE_C, cts.gainAdcShift_C);
// 			} else {
// 			current_A_buf += sqrForRMS(osc_buffer, PHASE_A, cts.adcShift_A);
// 			current_B_buf += sqrForRMS(osc_buffer, PHASE_B, cts.adcShift_B);
// 			current_C_buf += sqrForRMS(osc_buffer, PHASE_C, cts.adcShift_C);
// 		}
// 		} else {
// 		// Calculate the RMS values with scaling and overflow checks
// 		float float_RMS_A = sqrt(current_A_buf * k_scaling_factor) - 128 + RMS_Zero[PHASE_A];
// 		float float_RMS_B = sqrt(current_B_buf * k_scaling_factor) - 128 + RMS_Zero[PHASE_B];
// 		float float_RMS_C = sqrt(current_C_buf * k_scaling_factor) - 128 + RMS_Zero[PHASE_C];
//
// 		// Ensure RMS values are non-negative
// 		float_RMS_A = (float_RMS_A < 0) ? 0 : float_RMS_A;
// 		float_RMS_B = (float_RMS_B < 0) ? 0 : float_RMS_B;
// 		float_RMS_C = (float_RMS_C < 0) ? 0 : float_RMS_C;
//
// 		// Reset buffers after calculating RMS
// 		current_A_buf = 0;
// 		current_B_buf = 0;
// 		current_C_buf = 0;
// 	}
// }
//
// int64_t sqrForRMS(int16_t osc_buffer[][phase_quantity], uint8_t phase_, uint8_t shift_)
//  {
// 	int64_t curr_ = static_cast<int64_t>(osc_buffer[0][phase_]);  // Ensure sufficient range for calculation
// 	int64_t adjusted_value = curr_ - ADC_MID - 128 + shift_;  // Adjusted value around zero
//
// 	// Squaring the adjusted value safely within int64_t range
// 	uint64_t result = static_cast<uint64_t>(adjusted_value) * static_cast<uint64_t>(adjusted_value);
// 	return result;
// }


int convertFloat(float flt_, uint8_t oreder_)
{
	union {
		float flt;
		int32_t i32;
		uint8_t bytes[4];
	} u;
	
	u.flt = flt_;
	
	return (u.bytes[1 + 2 * oreder_] << 8) | u.bytes[2 * oreder_];
}


float loadFloat(int addr_, float default_)
{
	floatval castFloat_;
	uint8_t error_ = 0;
	
	for (uint8_t i=0; i<=3; i++)
	{
		castFloat_.bytes[3-i] = eeprom_read_byte((uint8_t*)(addr_+i));
		if (eeprom_read_byte((uint8_t*)(addr_+i)) == 0xFF)
		error_++;
	}
	if (error_ == 4)
	return default_;
	
	return castFloat_.val;
}

void saveFloat(int addr_, float Value_)
{
	//	uint8_t bytes[4]
	
	uint32_t intValue; // = *reinterpret_cast<uint32_t*>(&Value_);
	memcpy(&intValue, &Value_, sizeof(uint32_t));
	
	eeprom_write_byte((uint8_t*)addr_,((intValue >> 24) & 0xFF));
	eeprom_write_byte((uint8_t*)addr_+1,((intValue >> 16) & 0xFF));
	eeprom_write_byte((uint8_t*)addr_+2,((intValue >> 8) & 0xFF));
	eeprom_write_byte((uint8_t*)addr_+3,(intValue & 0xFF));
}


uint16_t myAbs(int16_t x) {
	return (x < 0) ? -x : x;
}

uint16_t calcOwnTime(unsigned long cbTimer_X)
{
	unsigned long timerBuf_;
	timerBuf_ = ISR_microTimer - cbTimer_X;
	
	if (timerBuf_ > 600)
	return 0;
	else
	return timerBuf_;
}









// START  Analizing sine and Arc signal from scope

// check if a value exceeds the noise window but returns to within the noise range in the next measurements.

/*
void filterNoise(int16_t* buffer, uint16_t size, uint16_t noiseWindow, uint8_t returnThreshold) {
for (uint16_t i = 0; i < size; ++i) {
if (abs(buffer[i]) > noiseWindow) {
// Check the next returnThreshold measurements
bool returnsToNoise = false;
for (uint8_t j = 1; j <= returnThreshold && (i + j) < size; ++j) {
if (abs(buffer[i + j]) <= noiseWindow) {
returnsToNoise = true;
break;
}
}

// If it returns to the noise window, replace the peak with 0
if (returnsToNoise) {
buffer[i] = 0;
}
}
}
}

// function to reduce the impact of noise


void smoothSignal(int16_t* buffer, uint16_t length, uint8_t windowSize) {
for (byte i = 0; i < length - windowSize + 1; i++) {
float sum = 0.0;
for (byte j = 0; j < windowSize; j++) {
sum += buffer[i + j];
}
buffer[i] = sum / windowSize;
}
}



// check if the signal is trending in a stable direction within a noise tolerance window
bool isSignalTrending(int16_t* buffer, uint16_t size, uint16_t trendLength, uint8_t noiseWindow) {
if (size < trendLength) {
return false;  // Not enough data points to determine a trend
}

int16_t trend = 0;

for (uint16_t i = 0; i < trendLength - 1; ++i) {
int16_t diff = buffer[i + 1] - buffer[i];

// Ignore small changes within the noise window
if (abs(diff) > noiseWindow) {
if (diff > 0) {
trend += 1;
} else {
trend -= 1;
}
}
}

// Check if the trend is consistent
return abs(trend) >= (trendLength - 1);
}


*/

// FIN  Analizing sine and Arc signal from scope

// ======== EOF ========





void fram_init(void)
{
	bit_set(SPSR, SPI2X);
	bit_set(SPCR, MSTR);
	bit_set(SPCR, SPE);
}
//
// u8 fram_cmd(u8 command)
// {
// 	FRAM_CS_ON;
// 	SPDR = command;
// 	WAIT_SPI_TRANSFER;
// 	SPDR = 0;
// 	WAIT_SPI_TRANSFER;
// 	FRAM_CS_OFF;
// 	return SPDR;
// }

//
void fram_write_byte(u16 addr, u8 data)
{
	// Enable write
	FRAM_CS_ON;               // Pull CS low to start communication
	SPDR = FRAM_CMD_WREN;  // Send WRITE enable flag
	WAIT_SPI_TRANSFER_B;
	FRAM_CS_OFF;


	FRAM_CS_ON;
	SPDR = FRAM_CMD_WRITE;  // Send WRITE command
	WAIT_SPI_TRANSFER_B;

	SPDR = (u8)(addr >> 8);  // Send high byte of address
	WAIT_SPI_TRANSFER_B;

	SPDR = (u8)(addr & 0xFF);  // Send low byte of address
	WAIT_SPI_TRANSFER_B;

	// Send the data byte
	SPDR = data;
	WAIT_SPI_TRANSFER_B;

	FRAM_CS_OFF;  // Pull CS high to end communication
}

u8 fram_read_byte(u16 addr)
{


	FRAM_CS_ON;  // Pull CS low to start communication


	// Send the READ command (0x03)


	SPDR = FRAM_CMD_READ;


	WAIT_SPI_TRANSFER_B;  // Wait for transmission to complete
	// Send the 16-bit address (high byte first)


	SPDR = (u8)(addr >> 8);  // Send high byte of address

	WAIT_SPI_TRANSFER_B;


	SPDR = (u8)(addr & 0xFF);  // Send low byte of address


	WAIT_SPI_TRANSFER_B;
	// Receive the data byte

	SPDR = 0x00;  // Send dummy byte to initiate SPI clock

	WAIT_SPI_TRANSFER_B;
	FRAM_CS_OFF;  // Pull CS high to end communication
	return SPDR;  // Return the read byte

}




void fram_write_array(u16 addr, u8* data, u16 length)
{
	// Enable write
	FRAM_CS_ON;               // Pull CS low to start communication
	SPDR = FRAM_CMD_WREN;     // Send WRITE enable command
	WAIT_SPI_TRANSFER;
	FRAM_CS_OFF;

	// Start writing data
	FRAM_CS_ON;
	SPDR = FRAM_CMD_WRITE;    // Send WRITE command
	WAIT_SPI_TRANSFER;
	
	SPDR = (u8)(addr >> 8);   // Send high byte of address
	WAIT_SPI_TRANSFER;

	SPDR = (u8)(addr & 0xFF); // Send low byte of address
	WAIT_SPI_TRANSFER;

	// Send the data bytes
	for (u16 i = 0; i < length; i++) {
		SPDR = data[i];       // Send each byte in the array
		WAIT_SPI_TRANSFER;
	}

	FRAM_CS_OFF;  // Pull CS high to end communication
}


void fram_read_array(u16 addr, u8* data, u16 length)
{
	FRAM_CS_ON;                // Pull CS low to start communication

	SPDR = FRAM_CMD_READ;      // Send READ command
	WAIT_SPI_TRANSFER;

	SPDR = (u8)(addr >> 8);    // Send high byte of address
	WAIT_SPI_TRANSFER;

	SPDR = (u8)(addr & 0xFF);  // Send low byte of address
	WAIT_SPI_TRANSFER;

	// Read the data bytes
	for (u16 i = 0; i < length; i++) {
		SPDR = 0x00;           // Send dummy byte to initiate SPI clock
		WAIT_SPI_TRANSFER;
		data[i] = SPDR;        // Store the received byte in the array
	}

	FRAM_CS_OFF;               // Pull CS high to end communication
}


int16_t getSingleFramValue(uint16_t address_, uint8_t channel_)
{
	uint8_t adc_data[12];
	fram_read_array(address_*12, adc_data, 12); // Read 12 bytes from FRAM

	// Extract and accumulate values for PHASE_A, PHASE_B, and PHASE_C
	return cts.adc_ct_extract_single(adc_data, channel_);
}



void process_adc_data(int16_t osc_buffer_[][3], uint16_t measurements_num_RAW, uint8_t averPoints_)
{
	uint8_t adc_data[12];
	int16_t average_sum[3] = {0, 0, 0};  // To accumulate sums for averaging
	uint8_t measurement_count = 0;

	for (uint16_t i = 0; i < measurements_num_RAW; i++)
	{
		fram_read_array(i*12, adc_data, 12); // Read 12 bytes from FRAM

		// Extract and accumulate values for PHASE_A, PHASE_B, and PHASE_C
		average_sum[PHASE_A] += cts.adc_ct_extract_single(adc_data, ADC1);
		average_sum[PHASE_B] += cts.adc_ct_extract_single(adc_data, ADC2);
		average_sum[PHASE_C] += cts.adc_ct_extract_single(adc_data, ADC3);

		measurement_count++;

		if (measurement_count == averPoints_)  // After 3 measurements
		{
			// Calculate the average and store it in osc_buffer_2
			osc_buffer_[i/averPoints_][PHASE_A] = average_sum[PHASE_A] / averPoints_;
			osc_buffer_[i/averPoints_][PHASE_B] = average_sum[PHASE_B] / averPoints_;
			osc_buffer_[i/averPoints_][PHASE_C] = average_sum[PHASE_C] / averPoints_;

			// Reset for the next group of 3 measurements
			average_sum[PHASE_A] = 0;
			average_sum[PHASE_B] = 0;
			average_sum[PHASE_C] = 0;
			measurement_count = 0;
			wdt_reset();
		}
	}
}


/**
* @brief Определение состояния выключателя по паре битов (ON/OFF).
*
* Логика:
* - ON=1, OFF=0  → cb_On
* - ON=0, OFF=1  → cb_Off
* - ON=0, OFF=0  → "в движении", но только если раньше было уверенно cb_On/cb_Off
* - ON=1, OFF=1  → невалидно, возвращаем прошлое состояние без изменений
*
* @param curr_state      "Сырой" байт дискретных входов (после инверсии pull-up)
* @param Switch_ON_mask  маска бита "ON" для фазы
* @param Switch_OFF_mask маска бита "OFF" для фазы
* @param CB_State_       предыдущее подтверждённое состояние фазы
* @return breakerState
*/
breakerState getBreakerState(uint8_t curr_state,
uint8_t Switch_ON_mask,
uint8_t Switch_OFF_mask,
breakerState CB_State_)
{
	const uint8_t on  = (curr_state & Switch_ON_mask)  ? 1u : 0u;
	const uint8_t off = (curr_state & Switch_OFF_mask) ? 1u : 0u;

	if ( on && !off ) {
		if (!emulate_contacts) {               // дребезг — только в реале
			//switchBlocked = true;
			// switchBounceTime = currentTime;    // мс
		}
		return cb_On;
	}
	if ( !on && off ) {
		if (!emulate_contacts) {
			//switchBlocked = true;
			// switchBounceTime = currentTime;    // мс
		}
		return cb_Off;
	}

	if (!on && !off) {
		if (CB_State_ == cb_On)  return cb_On_Off;
		if (CB_State_ == cb_Off) return cb_Off_On;
		return CB_State_;
	}

	// оба 1 → невалидно
	return CB_State_;
}





void calibrToEEPROM(uint8_t channel_)
{
	k[channel_-1] = 1.0 * (calibr_Y[2*channel_] - calibr_Y[2*channel_ - 1]) / (calibr_X[2*channel_] - calibr_X[2*channel_ - 1]);
	b[channel_-1] = (k[channel_-1] * calibr_X[2*channel_ - 1] - calibr_Y[2*channel_ - 1]) / k[channel_-1];
	// ************ Запись значений во внутреннюю память контроллера ******************************************
	
	eeprom_write_word((uint16_t*)EEPROM_Start_Adress[channel_], calibr_X[2*channel_]);
	eeprom_write_word((uint16_t*)(EEPROM_Start_Adress[channel_] + 2), calibr_Y[2*channel_]);
	
	eeprom_write_word((uint16_t*)(EEPROM_Start_Adress[channel_] + 4), calibr_X[2*channel_-1]);
	eeprom_write_word((uint16_t*)(EEPROM_Start_Adress[channel_] + 6), calibr_Y[2*channel_-1]);
}


void calibrFromEEPROM(uint8_t pointer)   // Загрузка данных кривой y = kx - b;
{
	calibr_X[2*pointer] = eeprom_read_word((uint16_t*)EEPROM_Start_Adress[pointer]);
	calibr_Y[2*pointer] = eeprom_read_word((uint16_t*)(EEPROM_Start_Adress[pointer] + 2));
	
	calibr_X[2*pointer-1] = eeprom_read_word((uint16_t*)(EEPROM_Start_Adress[pointer] + 4));
	calibr_Y[2*pointer-1] = eeprom_read_word((uint16_t*)(EEPROM_Start_Adress[pointer] + 6));
	
	k[pointer-1] = 1.0 * (calibr_Y[2*pointer] - calibr_Y[2*pointer - 1]) / (calibr_X[2*pointer] - calibr_X[2*pointer - 1]);
	b[pointer-1] = (k[pointer-1] * calibr_X[2*pointer - 1] - calibr_Y[2*pointer - 1]) / k[pointer-1];
}



void update_isr_microtimer(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		ISR_microTimer = ticks_now;
	}
}


void uart1_quiet_enter(void)
{
	if (uart1_quiet_active) return;
	uart1_ucsr1b_saved = UCSR1B;
	UCSR1B &= (uint8_t)~((1<<RXCIE1)|(1<<TXCIE1)|(1<<UDRIE1)|(1<<RXEN1)|(1<<TXEN1));
	uart1_quiet_active = 1;
}

void uart1_quiet_leave(void)
{
	if (!uart1_quiet_active) return;
	UCSR1B = uart1_ucsr1b_saved;
	uart1_quiet_active = 0;
	
}




// ========= Хелперы FRAM (используют твою fram_write_array) =========

static inline void fram_write_u8(uint16_t addr, uint8_t v) {
	fram_write_array(addr, &v, 1);
}

static inline void fram_write_u16_be(uint16_t addr, uint16_t v) {
	uint8_t two[2] = { (uint8_t)(v>>8), (uint8_t)(v&0xFF) };
	fram_write_array(addr, two, 2);
}
static inline void fram_write_strike_header(uint16_t strike_base, const osc_strike_header_t* h) {
	fram_write_array((uint16_t)(strike_base + 0), (uint8_t*)h, sizeof(osc_strike_header_t));
}


/**
* @brief Инициализация секции удара: заполняем заголовок и сбрасываем счётчики.
*/
void begin_strike_capture(uint8_t strike_index,
uint32_t t0_ticks,
uint8_t  aux_type,
uint8_t  last_op)
{
	g_strike_index = (strike_index < STRIKES_MAX) ? strike_index : (STRIKES_MAX-1);

	Scope_written_points[0]=Scope_written_points[1]=Scope_written_points[2]=0;


	g_hdr.version               = OSC_HDR_VERSION;
	g_hdr.auxType               = aux_type;
	g_hdr.lastOperation         = last_op;
	g_hdr.reserved0             = 0;
	g_hdr.sample_period_us      = SAMPLE_PERIOD_US;
	g_hdr.samples_max_per_phase = PHASE_SAMPLES_MAX;
	g_hdr.t0_ticks              = t0_ticks;

	// метки первого разрыва ты ведёшь в phase_first_edge_ticks[]
	g_hdr.start_tick[0] = phase_first_edge_ticks[PHASE_A];
	g_hdr.start_tick[1] = phase_first_edge_ticks[PHASE_B];
	g_hdr.start_tick[2] = phase_first_edge_ticks[PHASE_C];

	g_hdr.written[0] = g_hdr.written[1] = g_hdr.written[2] = 0;
	g_hdr.reserved1  = 0;

	// Записать заголовок в FRAM
	fram_write_strike_header(STRIKE_BASE_ADDR(g_strike_index), &g_hdr);


}


// Быстрый скейлинг LUT → 12 бит
static inline uint16_t lut_to_adc12(uint16_t lutv, uint16_t amp)
{
	// lutv в 0..65535; переводим в [-amp .. +amp] и прибавляем MID
	// Δ = (lutv - 32768) ~ [-32768..32767] → умножаем на amp и делим на 32768
	int32_t delta = (int32_t)lutv - 32768;
	int32_t swing = (delta * (int32_t)amp) / 32768;
	int32_t v = (int32_t)ADC12_MID + swing;
	if (v < 0) v = 0;
	if (v > 4095) v = 4095;
	return (uint16_t)v;
}

/*******************************  E M U   S I N E   S A M P L E  ****************************/
/**
* @brief Сформировать 12-битный код «синуса» для одной фазы (эмуляция АЦП).
* @param ph   PHASE_A/B/C
* @param acc  фазовый аккумулятор (Q16), инкрементируется на emu_phase_step
* @return 0..4095, центр LUT в ADC12_MID (2048).
* @note Никаких окон/задержек здесь нет — решение «синус или midpoint» принимается в record_scope().
*/
static inline uint16_t emu_make_ct_value(uint8_t ph, uint16_t acc)
{
	(void)ph; /* на случай, если дальнейшая логика не будет зависеть от ph */

	const uint8_t  idx   = (uint8_t)(acc >> 8);
	const uint16_t lut12 = pgm_read_word(&sin_lut_256[idx]);   /* 0..4095, mid=2048 */

	int16_t  s12 = (int16_t)lut12 - (int16_t)ADC12_MID;        /* [-2048..+2047] */
	uint16_t amp = emu_Sine_amp;                               /* [0..2047] */
	if (amp > 2047u) amp = 2047u;

	int32_t v = (int32_t)ADC12_MID + (((int32_t)s12 * (int32_t)amp) >> 11);
	if (v < 0) v = 0; else if (v > 4095) v = 4095;
	return (uint16_t)v;
}



/****************************  A D C   /   E M U   R E A D E R  ****************************/
/**
* @brief Прочитать три «прямых» значения CT (12 бит) для фаз A/B/C.
* @details
*  - Эмуляция: генерируется синус по фазовым аккумуляторам (окна/midpoint решаются в record_scope()).
*  - Реальный режим: бит-бэнг чтение 3×12 бит с параллельной шины (три младших бита PIN_ADC_CT).
*  - dbg_dt_ticks = разница тиков между вызовами (ожидается 1 при Ts=100 мкс).
*
* @param[out] ct0 Фаза A (12-бит).
* @param[out] ct1 Фаза B (12-бит).
* @param[out] ct2 Фаза C (12-бит).
*/
static inline void osc_adc_ct_read_raw12(uint16_t* ct0, uint16_t* ct1, uint16_t* ct2)

{

	if (!ct0 || !ct1 || !ct2) return;


	/* ── Реальный АЦП: 3 dummy-такта + 12 значимых бит, MSB-first ── */
	uint16_t a = 0u, b = 0u, c = 0u;

	ADC_CS_LOW;

	/* «Пустые» такты перед выдачей первого значимого бита (оставляем 3, как подтверждено) */
	ADC_CLOCK; ADC_CLOCK; ADC_CLOCK;

	/* 12 значимых бит: на каждый такт один сэмпл PIN_ADC_CT & 0x07 */
	for (uint8_t bit = 0; bit < 12; bit++) {
		ADC_CLOCK;                                   /* фронт такта → обновление выходов АЦП */
		const uint8_t pins = (uint8_t)(PIN_ADC_CT & 0x07u);  /* bit0=A, bit1=B, bit2=C */
		a = (uint16_t)((a << 1) | ((pins >> 0) & 0x01u));
		b = (uint16_t)((b << 1) | ((pins >> 1) & 0x01u));
		c = (uint16_t)((c << 1) | ((pins >> 2) & 0x01u));
	}

	ADC_CLOCK;          /* завершающий такт — по твоей схеме полезен; оставить симметрию */
	ADC_CS_HIGH;

	*ct0 = (uint16_t)(a & 0x0FFFu);
	*ct1 = (uint16_t)(b & 0x0FFFu);
	*ct2 = (uint16_t)(c & 0x0FFFu);
}



//
//
// /********************************  S C O P E   R E C O R D  ********************************/
// /**
// * @brief Один шаг записи осциллограмм по фазам A/B/C (строго с периодом дискретизации).
// * @details
// *  - Разрешение записи фазы выдаёт детектор концевиков: PH_EDGE_STARTED → scope_State[ph] = startScope.
// *  - Первый реально записанный сэмпл по фазе переводит её в activeScope.
// *  - Записываем ровно PHASE_SAMPLES_MAX точек по каждой фазе.
// *    * Если emulate_adc_wave=1: первые emu_arc_extinguish_ticks[ph] — «синус», далее — midpoint (ADC12_MID).
// *      При нуле длительности — всегда midpoint (без «бесконечных окон»).
// *    * Если emulate_adc_wave=0 (реальный АЦП): пишем полученный код как есть.
// *  - Когда все три фазы в silentTime — ставим COMPLETE и пишем заголовок.
// * @note Не смешивает статусы концевиков и статусы записи. «Синус vs midpoint» решается здесь, а не в генераторе.
// */
// void record_scope(void)
// {
// 	if (cts.recording_status == COMPLETE) return;
//
// 	/* 1) Считать текущие «прямые» значения CT A/B/C (эмуляция или реальный АЦП) */
// 	uint16_t ctA = 0u, ctB = 0u, ctC = 0u;
// 	osc_adc_ct_read_raw12(&ctA, &ctB, &ctC);
//
// 	/* 2) Перевод пер-фазных статусов записи по событию концевиков
// 	PH_EDGE_STARTED -> scope_State[ph] = startScope
// 	*/
// 	for (uint8_t ph = 0; ph < 3; ph++) {
// 		if (scope_State[ph] == standingBy && phase_Contact_state[ph] == PH_EDGE_STARTED) {
// 			scope_State[ph] = startScope;
//
// 			/* (Опционально для отчётности) проставим стартовые метки в заголовок:
// 			при auxType==1 и реальных контактах выровняем B/C по A */
// 			if (g_hdr.auxType == 1u && !(EMU_mode_Flag && emulate_contacts)) {
// 				if (ph == PHASE_A) {
// 					const uint16_t tA = phase_first_edge_ticks[PHASE_A];
// 					g_hdr.start_tick[PHASE_A] = tA;
// 					g_hdr.start_tick[PHASE_B] = tA;
// 					g_hdr.start_tick[PHASE_C] = tA;
//
// 					/* Синхронно разрешим B/C (если ещё ждали) */
// 					if (scope_State[PHASE_B] == standingBy) scope_State[PHASE_B] = startScope;
// 					if (scope_State[PHASE_C] == standingBy) scope_State[PHASE_C] = startScope;
// 				}
// 				} else {
// 				/* Три пары концевиков или эмуляция контактов: независимые старты */
// 				g_hdr.start_tick[ph] = phase_first_edge_ticks[ph];
// 			}
// 		}
// 	}
//
// 	/* 3) Запись сэмплов по тем фазам, которым запись разрешена (startScope/activeScope) */
// 	const uint16_t base = STRIKE_BASE_ADDR(g_strike_index);
//
// 	struct { uint16_t v; uint8_t ph; } pack[3] = {
// 	{ ctA, PHASE_A }, { ctB, PHASE_B }, { ctC, PHASE_C }
// };
//
// for (uint8_t i = 0; i < 3; i++) {
// 	const uint8_t  ph  = pack[i].ph;
// 	const uint16_t v12 = (uint16_t)(pack[i].v & 0x0FFFu);
//
// 	if (scope_State[ph] == startScope || scope_State[ph] == activeScope)
// 	{
// 		/* Выберем, что писать: синус (в эмуляции) или midpoint после заданной длительности */
// 		uint16_t sample_to_write = v12;
//
// 		if (EMU_mode_Flag && emulate_adc_wave) {
// 			const uint16_t n_written = Scope_written_points[ph];
// 			const uint16_t n_arc     = emu_arc_extinguish_ticks[ph];
//
// 			/* Нулевая длительность → всегда midpoint (никакого «бесконечного окна») */
// 			const uint8_t in_arc_window = (n_arc != 0u) && (n_written < n_arc);
// 			if (!in_arc_window) {
// 				sample_to_write = (uint16_t)ADC12_MID;
// 			}
// 		}
//
// 		/* Отладочный тап — тем же темпом, что и запись в FRAM */
// 		dbg_tap_push(ph, sample_to_write);
//
// 		/* Запись в FRAM (или в SRAM-буфер, если так устроено phase_data_offset/FRAM_HDR_SIZE) */
// 		if (Scope_written_points[ph] < PHASE_SAMPLES_MAX) {
// 			const uint16_t off  = (uint16_t)(phase_data_offset(ph) +
// 			(uint16_t)Scope_written_points[ph] * SAMPLE_BYTES_PER_PHASE);
// 			const uint16_t addr = (uint16_t)(base + FRAM_HDR_SIZE + off);
//
// 			fram_write_u16_be(addr, sample_to_write);
//
// 			Scope_written_points[ph]++;
// 			g_hdr.written[ph] = Scope_written_points[ph];
//
// 			/* Первый фактический сэмпл → activeScope */
// 			if (Scope_written_points[ph] == 1u && scope_State[ph] == startScope) {
// 				scope_State[ph] = activeScope;
// 			}
//
// 			/* Достигли лимита — фаза готова */
// 			if (Scope_written_points[ph] >= PHASE_SAMPLES_MAX) {
// 				scope_State[ph] = silentTime;
// 			}
// 			} else {
// 			/* Перестраховка от выходов за границы */
// 			scope_State[ph] = silentTime;
// 		}
// 	}
// }
//
// /* 4) Все три фазы завершили запись? → финализация заголовка и статус COMPLETE */
// if (scope_State[PHASE_A] == silentTime &&
// scope_State[PHASE_B] == silentTime &&
// scope_State[PHASE_C] == silentTime)
// {
// 	g_hdr.status = OSC_STAT_READY;
// 	calc_phase_start_deltas_ticks();                    /* если нужно, оставим для отчётности */
//
// 	fram_write_strike_header(base, &g_hdr);
// 	fram_write_u8((uint16_t)(base + OSC_HDR_STATUS_OFFS), OSC_STAT_READY);
//
// 	cts.recording_status = COMPLETE;
// }
// }


/********************************  S C O P E   R E C O R D  ********************************/
/* ====== Быстрый RAM-only путь: AЦП → SRAM, без эмуляции/FRAM/направлений ====== */

/// Рабочие указатели записи по фазам
static int16_t* wr_ptr[3];


/// Маска активных фаз: бит0=A, бит1=B, бит2=C (1 = писать)
static uint8_t active_mask;

/// Сколько фаз уже завершены (0..3)
static uint8_t phases_done;


/********************************  R A M   W R I T E R  ************************************/
/**
* @brief Старт записи по фазе: подготовить RAM-указатель и включить обратный отсчёт.
* @details
*  dbg_written[ph] временно хранит «сколько ОСТАЛОСЬ записать» (обратный счёт).
*  Финальная конверсия в «сколько ЗАПИСАНО» делается после завершения всех фаз.
*/
static inline void scope_phase_start(uint8_t ph)
{
	wr_ptr[ph]       = &dbg_ct_raw[ph][0];   // пишем с начала буфера
	dbg_written[ph]  = PHASE_SAMPLES_MAX;    // ОБРАТНЫЙ отсчёт (остаток)
	active_mask     |= (uint8_t)(1u << ph);  // фаза активна
}

/**
* @brief Записать один 12-битный сэмпл в RAM (если фаза активна). Быстрый путь.
* @param ph  0..2
* @param v12 0..4095 (12 бит)
* @return true, если фаза завершила запись на этом сэмпле
*/
static inline bool scope_try_write(uint8_t ph, uint16_t v12)
{
	const uint8_t bit = (uint8_t)(1u << ph);
	if ((active_mask & bit) == 0u) return false;  // фаза неактивна

	*wr_ptr[ph]++ = (int16_t)(v12 & 0x0FFFu);

	// Обратный счёт: уменьшили «сколько осталось»
	if (--dbg_written[ph] == 0u) {
		active_mask &= (uint8_t)~bit;   // отключить фазу
		phases_done++;                  // учесть завершение
		return true;
	}
	return false;
}

/**
* @brief Быстрый регистратор осциллограмм: реальный АЦП → RAM, жёстко 400 сэмплов/фазу.
*
* @details
* - Фаза переходит в запись при PH_EDGE_STARTED.
* - Пишем подряд в dbg_ct_raw[ph][0..399].
* - Как только все 3 фазы добили лимит — ставим COMPLETE и дергаем хвостовой таймер.
*/
void record_scope(void)
{
	if (cts.recording_status == COMPLETE) return;

	// Ленивый сброс при первом заходе
	if (!scope_pairpack_init_done) {
		active_mask = 0u;
		phases_done = 0u;
		scope_pairpack_init_done = 1u;
	}

	// 1) Считать три кода АЦП (реальные, без эмуляции)
	uint16_t ctA = 0u, ctB = 0u, ctC = 0u;
	osc_adc_ct_read_raw12(&ctA, &ctB, &ctC);

	// 2) Перевести фазы из standby в запись по фронту (один раз на фазу)
	if (scope_State[PHASE_A] == standingBy && phase_Contact_state[PHASE_A] == PH_EDGE_STARTED) {
		scope_State[PHASE_A] = activeScope;
		scope_phase_start(PHASE_A);
	}
	if (scope_State[PHASE_B] == standingBy && phase_Contact_state[PHASE_B] == PH_EDGE_STARTED) {
		scope_State[PHASE_B] = activeScope;
		scope_phase_start(PHASE_B);
	}
	if (scope_State[PHASE_C] == standingBy && phase_Contact_state[PHASE_C] == PH_EDGE_STARTED) {
		scope_State[PHASE_C] = activeScope;
		scope_phase_start(PHASE_C);
	}

	// 3) Запись (минимум ветвлений)
	// При первом фактическом сэмпле переводим startScope -> activeScope (для совместимости флагов)
	if (scope_State[PHASE_A] == activeScope)
	{
		if (scope_try_write(PHASE_A, ctA)) { scope_State[PHASE_A] = silentTime; }
	}
	if (scope_State[PHASE_B] == activeScope) {
		if (scope_try_write(PHASE_B, ctB)) { scope_State[PHASE_B] = silentTime; }
	}
	if (scope_State[PHASE_C] == activeScope) {
		if (scope_try_write(PHASE_C, ctC)) { scope_State[PHASE_C] = silentTime; }
	}

	// 4) Все три фазы завершили?
	if (phases_done >= phase_quantity) {
		cts.recording_status = COMPLETE;     // «удар» закончился
		activateSwitchFinTimer();            // хвостовой таймер
		/* ── Конверсия обратного счёта в «сколько ЗАПИСАНО» ──
		* Здесь dbg_written[ph] пока хранит «сколько ОСТАЛОСЬ».
		* Пересчитываем в «сколько ЗАПИСАНО» и (по желанию) дублируем в заголовок.
		*/
		for (uint8_t ph = 0; ph < 3; ph++) {
			const uint16_t written = (uint16_t)(PHASE_SAMPLES_MAX - dbg_written[ph]);
			dbg_written[ph]        = written;          // теперь это «сколько реально записано»
			g_hdr.written[ph]      = written;          // если заголовок используется — обновим
		}

		scope_pairpack_init_done = 0u;       // готовность к новому циклу
	}
}




/********************************  S C O P E   R E C O R D  ********************************/
/**
* @brief Один шаг записи осциллограмм по фазам A/B/C (строго с периодом дискретизации).
* @details
*  - Разрешение записи фазы выдаёт детектор концевиков: PH_EDGE_STARTED → scope_State[ph] = startScope.
*  - Первый реально записанный сэмпл по фазе переводит её в activeScope.
*  - Записываем ровно PHASE_SAMPLES_MAX точек по каждой фазе.
*    * Если emulate_adc_wave=1: первые emu_arc_extinguish_ticks[ph] — «синус», далее — midpoint (ADC12_MID).
*      При нуле длительности — всегда midpoint (без «бесконечных окон»).
*    * Если emulate_adc_wave=0 (реальный АЦП): пишем полученный код как есть.
*  - Ориентация буфера по времени всегда слева-направо (индекс 0 → начало событий):
*    * При размыкании (lastOperation == cb_On_Off): пишем «вперёд» (0..N-1): синус → midpoint.
*    * При замыкании (lastOperation == cb_Off_On): пишем «с хвоста» (N-1..0), чтобы получить midpoint → синус.
*  - Когда все три фазы в silentTime — ставим COMPLETE и пишем заголовок.
* @note Никаких окон/задержек в генераторе нет — решение «синус или midpoint» принимается здесь.
*/
// 	void record_scope(void)
// 	{
// 		if (cts.recording_status == COMPLETE) return;
//
// 		/* 1) Считать текущие «прямые» значения CT A/B/C (эмуляция или реальный АЦП) */
// 		uint16_t ctA = 0u, ctB = 0u, ctC = 0u;
// 		osc_adc_ct_read_raw12(&ctA, &ctB, &ctC);
//
// 		/* 2) Перевод пер-фазных статусов записи по событию концевиков:
// 		PH_EDGE_STARTED -> scope_State[ph] = startScope */
// 		for (uint8_t ph = 0; ph < 3; ph++) {
// 			if (scope_State[ph] == standingBy && phase_Contact_state[ph] == PH_EDGE_STARTED) {
// 				scope_State[ph] = startScope;
//
// 				/* (Опционально для отчётности) проставим стартовые метки в заголовок:
// 				при auxType==1 и реальных контактах выровняем B/C по A */
// 				if (g_hdr.auxType == 1u && !(EMU_mode_Flag && emulate_contacts)) {
// 					if (ph == PHASE_A) {
// 						const uint16_t tA = phase_first_edge_ticks[PHASE_A];
// 						g_hdr.start_tick[PHASE_A] = tA;
// 						g_hdr.start_tick[PHASE_B] = tA;
// 						g_hdr.start_tick[PHASE_C] = tA;
//
// 						/* Синхронно разрешим B/C (если ещё ждали) */
// 						if (scope_State[PHASE_B] == standingBy) scope_State[PHASE_B] = startScope;
// 						if (scope_State[PHASE_C] == standingBy) scope_State[PHASE_C] = startScope;
// 					}
// 					} else {
// 					/* Три пары концевиков или эмуляция контактов: независимые старты */
// 					g_hdr.start_tick[ph] = phase_first_edge_ticks[ph];
// 				}
// 			}
// 		}
//
// 		/* 3) Запись сэмплов по тем фазам, которым запись разрешена (startScope/activeScope) */
// 		const uint16_t base = STRIKE_BASE_ADDR(g_strike_index);
//
// 		struct { uint16_t v; uint8_t ph; } pack[3] = {
// 		{ ctA, PHASE_A }, { ctB, PHASE_B }, { ctC, PHASE_C }
// 	};
//
// 	/* Направление коммutaции: для cb_Off_On пишем «с хвоста», иначе как обычно «вперёд» */
// 	const uint8_t reverse_write = (lastOperation == cb_Off_On) ? 1u : 0u;
//
// 	for (uint8_t i = 0; i < 3; i++) {
// 		const uint8_t  ph  = pack[i].ph;
// 		const uint16_t v12 = (uint16_t)(pack[i].v & 0x0FFFu);
//
// 		if (scope_State[ph] == startScope || scope_State[ph] == activeScope)
// 		{
// 			/* Выберем, что писать: синус (в эмуляции) или midpoint после заданной длительности */
// 			uint16_t sample_to_write = v12;
//
// 			if (EMU_mode_Flag && emulate_adc_wave) {
// 				const uint16_t n_written = Scope_written_points[ph];
//
// 				uint16_t n_arc = reverse_write
// 				? (PHASE_SAMPLES_MAX - emu_arc_extinguish_ticks[ph])
// 				: emu_arc_extinguish_ticks[ph];
//
//
// 				/* Нулевая длительность → всегда midpoint (никакого «бесконечного окна») */
// 				const uint8_t in_arc_window = (n_arc != 0u) && (n_written < n_arc);
// 				if (!in_arc_window) {
// 					sample_to_write = (uint16_t)ADC12_MID;
// 				}
// 			}
//
// 			/* Отладочный тап — тем же темпом, что и запись в FRAM */
// 			dbg_tap_push(ph, sample_to_write);
//
// 			/* Запись в FRAM (или в SRAM-буфер, если так устроено phase_data_offset/FRAM_HDR_SIZE) */
// 			if (Scope_written_points[ph] < PHASE_SAMPLES_MAX) {
// 				const uint16_t n_written = Scope_written_points[ph];
//
// 				/* Физический индекс записи с учётом направления коммутации */
// 				const uint16_t idx = reverse_write
// 				? (uint16_t)(PHASE_SAMPLES_MAX - 1u - n_written)   /* замыкание: хвост → голова */
// 				: n_written;                                       /* размыкание: как было */
//
// 				const uint16_t off  = (uint16_t)(phase_data_offset(ph) +
// 				(uint16_t)idx * SAMPLE_BYTES_PER_PHASE);
// 				const uint16_t addr = (uint16_t)(base + FRAM_HDR_SIZE + off);
//
// 				//			fram_write_u16_be(addr, sample_to_write);
//
// 				Scope_written_points[ph]++;
// 				g_hdr.written[ph] = Scope_written_points[ph];
//
// 				/* Первый фактический сэмпл → activeScope */
// 				if (Scope_written_points[ph] == 1u && scope_State[ph] == startScope) {
// 					scope_State[ph] = activeScope;
// 				}
//
// 				/* Достигли лимита — фаза готова */
// 				if (Scope_written_points[ph] >= PHASE_SAMPLES_MAX) {
// 					scope_State[ph] = silentTime;
// 					// fram_write_byte(7500, 41);
// 				}
// 				} else {
// 				/* Перестраховка от выходов за границы */
// 				scope_State[ph] = silentTime;
//
// 			}
// 		}
// 	}
//
// 	/* 4) Все три фазы завершили запись? → финализация заголовка и статус COMPLETE */
// 	if (scope_State[PHASE_A] == silentTime &&
// 	scope_State[PHASE_B] == silentTime &&
// 	scope_State[PHASE_C] == silentTime)
// 	{
// 		g_hdr.status = OSC_STAT_READY;
// 		calc_phase_start_deltas_ticks();                    /* если нужно, оставим для отчётности */
//
// 		fram_write_strike_header(base, &g_hdr);
// 		fram_write_u8((uint16_t)(base + OSC_HDR_STATUS_OFFS), OSC_STAT_READY);
//
// 		cts.recording_status = COMPLETE;
// 	}
// 	}



/**
* @brief Финализация секции удара: пишем финальный заголовок.
*/
void osc_end_strike_capture(void)
{
	const uint16_t base = STRIKE_BASE_ADDR(g_strike_index);

	fram_write_strike_header(base, &g_hdr);
	fram_write_u8((uint16_t)(base + OSC_HDR_STATUS_OFFS), OSC_STAT_READY);

	// крутим ТОЛЬКО свой внутренний индекс FRAM
	g_strike_index = (uint8_t)((g_strike_index + 1u) % STRIKES_MAX);
}


#define FRAM_SUPER_BASE   (uint16_t)(FRAM_OSC_BASE + STRIKES_MAX * STRIKE_SECTION_BYTES)
// Суперблок займёт 16 байт, у тебя остаётся запас до 8 КБ.

typedef struct __attribute__((packed)) {
	uint8_t  magic;     // 0xA5
	uint8_t  version;   // 1
	uint8_t  head;      // куда писать следующий удар [0..2]
	uint8_t  tail;      // откуда читать следующий к обработке [0..2]
	uint8_t  count;     // сколько READY/PROCESSING/PROCESSED внутри (0..3), если считаем «занятость»
	uint8_t  policy;    // 0=overwrite oldest when full, 1=drop new (или игнор)
	uint8_t  reserved[2];
	uint32_t next_seq;  // следующий seq для записи
	uint32_t flags;     // на будущее
} osc_ring_super_t;

static osc_ring_super_t g_ring;


static inline void ring_load(void)
{
	fram_read_array(FRAM_SUPER_BASE, (uint8_t*)&g_ring, sizeof(g_ring));
	if (g_ring.magic != 0xA5 || g_ring.version != 1 || g_ring.head >= STRIKES_MAX || g_ring.tail >= STRIKES_MAX) {
		// инициализация по умолчанию
		g_ring.magic    = 0xA5;
		g_ring.version  = 1;
		g_ring.head     = 0;
		g_ring.tail     = 0;
		g_ring.count    = 0;
		g_ring.policy   = 0;   // overwrite oldest
		g_ring.next_seq = 1;
		g_ring.flags    = 0;
		fram_write_array(FRAM_SUPER_BASE, (uint8_t*)&g_ring, sizeof(g_ring));
	}
}

static inline void ring_store(void)
{
	fram_write_array(FRAM_SUPER_BASE, (uint8_t*)&g_ring, sizeof(g_ring));
}


// --- чтение заголовка секции ---
static inline void fram_read_strike_header(uint16_t base, osc_strike_header_t* h)
{
	fram_read_array(base, (uint8_t*)h, sizeof(*h));
}


// --- смещения внутри секции ---
#define PHASE_BYTES_PER_BLOCK   (uint16_t)(PHASE_SAMPLES_MAX * 2u)
#define PHASE_BLOCK_OFFSET(ph)  (uint16_t)(FRAM_HDR_SIZE + (ph) * PHASE_BYTES_PER_BLOCK)
#define PHASE_SAMPLE_ADDR(base, ph, i) \
(uint16_t)((base) + PHASE_BLOCK_OFFSET(ph) + ((i) << 1))



/****************************  К И К Е Р   Р А С Ч Ё Т О В  ****************************/
/**
* @brief «Пнуть» расчёты, если данные готовы и мы не в записи. RAM-вариант без FRAM/очередей.
* @details
*  - Стартуем только если: !startCalculations, positionChanged != activeScope, calc_ready_flag=1.
*  - Индексы/заголовки не трогаем (один SRAM-буфер). Дальше пусть твоя машина расчётов читает dbg_tap_*.
*/
static inline void kick_calculations(void)
{

	/* Бронировка «задачи» в RAM: дальше твоя машина расчётов заберёт dbg_tap_* */

	//transferDataToLCP = 1;
	startCalculations = calcStarted;
	/* При желании — сбросить счётчик шагов расчётов, заполнить служебные поля и т.п. */
}

//
// static void osc_prepare_calc_indices_from_hdr(const osc_strike_header_t* hdr)
// {
// 	const uint32_t t0      = hdr->t0_ticks;
// 	const uint32_t sp_us   = hdr->sample_period_us;
//
// 	for (uint8_t ph = 0; ph < 3; ph++)
// 	{
// 		const uint32_t st_ticks = hdr->start_tick[ph];
// 		const uint16_t nWritten = hdr->written[ph];
//
// 		// если старта не было — индекс старта = 0
// 		uint16_t start_idx = 0;
// 		if (st_ticks > 0 && sp_us > 0) {
// 			// индекс = round( (st_ticks - t0) * TICK_US / sp_us )
// 			int32_t dt_ticks = (int32_t)(st_ticks - t0);
// 			if (dt_ticks < 0) dt_ticks = 0;
// 			uint32_t num = (uint32_t)dt_ticks * (uint32_t)TICK_US + (sp_us/2); // округление
// 			uint32_t idx = num / (uint32_t)sp_us;
// 			if (idx >= nWritten) idx = (nWritten ? (nWritten-1) : 0);
// 			start_idx = (uint16_t)idx;
// 		}
//
// 		uint16_t fin_idx = (nWritten ? (nWritten-1) : 0); // если фин-тик не сохраняли
//
// 		ContactStartTimePoint[ph][CALC] = start_idx;
// 		ContactFinTimePoint[ph][CALC]   = fin_idx;
// 	}
// }

static inline uint16_t clamp_u16(uint32_t v, uint16_t hi)
{
	return (v > hi) ? hi : (uint16_t)v;
}




// Возвращает true и индекс секции [0..2], если нашли READY; иначе false.
// static bool osc_find_oldest_ready(uint8_t* out_idx, osc_strike_header_t* out_hdr)
// {
// 	bool found = false;
// 	uint32_t best_seq = 0xFFFFFFFFul;
// 	uint8_t  best_idx = 0;
//
// 	for (uint8_t i = 0; i < STRIKES_MAX; i++) {
// 		const uint16_t base = STRIKE_BASE_ADDR(i);
// 		osc_strike_header_t h;
// 		fram_read_strike_header(base, &h);
// 		if (h.status == OSC_STAT_READY) {
// 			if (!found || h.seq < best_seq) {
// 				best_seq = h.seq;
// 				best_idx = i;
// 				*out_hdr = h;
// 				found = true;
// 			}
// 		}
// 	}
// 	if (found) *out_idx = best_idx;
// 	return found;
// }

// Пометить секцию одним байтом (PROCESSING/PROCESSED/EMPTY)
static inline void osc_mark_status(uint8_t idx, uint8_t st)
{
	const uint16_t base = STRIKE_BASE_ADDR(idx);
	fram_write_u8((uint16_t)(base + OSC_HDR_STATUS_OFFS), st);
}


/**
* @brief Вернуть длины трасс по фазам из RAM-буферов.
* @param hdr  (не используется в RAM, оставлен для совместимости)
* @param nA,nB,nC,nMin выходные длины (сатурированы PHASE_SAMPLES_MAX)
*/
static inline void osc_get_lengths(const osc_strike_header_t* hdr,
uint16_t* nA, uint16_t* nB, uint16_t* nC, uint16_t* nMin)
{
	(void)hdr;
	uint16_t a = dbg_written[0];
	uint16_t b = dbg_written[1];
	uint16_t c = dbg_written[2];

	if (a > PHASE_SAMPLES_MAX) a = PHASE_SAMPLES_MAX;
	if (b > PHASE_SAMPLES_MAX) b = PHASE_SAMPLES_MAX;
	if (c > PHASE_SAMPLES_MAX) c = PHASE_SAMPLES_MAX;

	if (nA) *nA = a;
	if (nB) *nB = b;
	if (nC) *nC = c;

	if (nMin) {
		uint16_t ab = (a < b) ? a : b;
		*nMin = (ab < c) ? ab : c;
	}
}



static inline uint16_t fram_read_u16_be(uint16_t addr)
{
	uint8_t b[2];
	fram_read_array(addr, b, 2);
	return (uint16_t)((((uint16_t)b[0]) << 8) | b[1]);
}

// Удобный «аксессор»: прочитать семпл фазы из FRAM как int16
static inline int16_t osc_read_sample(uint8_t strike_idx, uint8_t phase, uint16_t i)
{
	const uint16_t base = STRIKE_BASE_ADDR(strike_idx);
	const uint16_t addr = PHASE_SAMPLE_ADDR(base, phase, i);
	return (int16_t)fram_read_u16_be(addr);
}


/// Целочисленный корень для 32-бит (достаточно точен для RMS)
static uint16_t isqrt_u32(uint32_t x)
{
	uint32_t op = x;
	uint32_t res = 0;
	uint32_t one = 1UL << 30; // самая высокая степень 4, <= 2^30

	while (one > op) one >>= 2;
	while (one != 0) {
		if (op >= res + one) {
			op  -= res + one;
			res  = res +  2 * one;
		}
		res >>= 1;
		one >>= 2;
	}
	return (uint16_t)res;
}


/// Считать три 12-бит значения CT (A,B,C). Возвращаем как int16 (расширено).
static inline void adc_read_ct_triplet(int16_t* outA, int16_t* outB, int16_t* outC)
{
	// твой существующий низкоуровневый вызов:
	// adc_ct_rec(buf_ct); -> но нам не нужен buf_ct с россыпью, если у тебя уже есть быстрый извлекатель.
	// Если проще, оставь adc_ct_rec(buf_ct) + adc_ct_extract_single(...)

	uint16_t ctA, ctB, ctC;
	osc_adc_ct_read_raw12(&ctA, &ctB, &ctC); // если этой функции нет — используй твою пару adc_ct_rec + extract
	*outA = (int16_t)ctA;
	*outB = (int16_t)ctB;
	*outC = (int16_t)ctC;
}


/// Прочитать подряд окно семплов фазы в небольшой локальный буфер dst.
/// count ДОЛЖЕН быть небольшим (например, 5..64), чтобы не раздувать SRAM.
static inline void osc_read_window(uint8_t strike_idx, uint8_t phase,
uint16_t start, uint16_t count, int16_t* dst)
{
	const uint16_t base = STRIKE_BASE_ADDR(strike_idx);
	for (uint16_t k = 0; k < count; k++) {
		uint16_t i = (uint16_t)(start + k);
		dst[k] = (int16_t)fram_read_u16_be(PHASE_SAMPLE_ADDR(base, phase, i));
	}
}


/// Коллбек для синхронного прохода трёх фаз: (idx, sA, sB, sC)
typedef void (*osc_iter_cb3_t)(uint16_t, int16_t, int16_t, int16_t);

/// Пройти «в ногу» по трём фазам до nMin, вызывая cb(i, sA, sB, sC).
static inline void osc_iterate_min_aligned(uint8_t strike_idx,
const osc_strike_header_t* hdr,
osc_iter_cb3_t cb)
{
	uint16_t nA, nB, nC, nMin;
	osc_get_lengths(hdr, &nA, &nB, &nC, &nMin);
	for (uint16_t i = 0; i < nMin; i++) {
		int16_t sA = osc_read_sample(strike_idx, PHASE_A, i);
		int16_t sB = osc_read_sample(strike_idx, PHASE_B, i);
		int16_t sC = osc_read_sample(strike_idx, PHASE_C, i);
		cb(i, sA, sB, sC);
	}
}



/** @brief Утилиты проверки состояния выключателя по enum. */
static inline uint8_t bs_is_on(breakerState s)      { return (s == cb_On); }
static inline uint8_t bs_is_off(breakerState s)     { return (s == cb_Off); }
static inline uint8_t bs_is_moving(breakerState s)  { return (s == cb_On_Off) || (s == cb_Off_On); }

/**
* @brief Построить "сырое" состояние для OFF-всех фаз (ON=0, OFF=1) с учётом auxType.
* @return байт curr/prev_state с выставленными OFF-битами.
*/
static uint8_t build_off_default_state(void)
{
	uint8_t s = 0;
	s |= CB1_OFF;  // A OFF
	s |= CB2_OFF;  // B OFF
	s |= CB3_OFF;  // C OFF
	return s;
}


/** @brief Безопасная разность тиков для uint16_t (учёт переполнения). */
static inline uint16_t ticks_diff_u16(uint16_t now, uint16_t then)
{
	return (uint16_t)(now - then);
}

/**
* @brief Получить пары ON/OFF до и после движения для выбранного направления.
*/
static inline void pair_before_after(breakerState dir,
uint8_t* on_before, uint8_t* off_before,
uint8_t* on_after,  uint8_t* off_after)
{
	if (dir == cb_On_Off) {
		// было ON(1/0) → станет OFF(0/1)
		*on_before = 1; *off_before = 0;
		*on_after  = 0; *off_after  = 1;
		} else {
		// было OFF(0/1) → станет ON(1/0)
		*on_before = 0; *off_before = 1;
		*on_after  = 1; *off_after  = 0;
	}
}




/// @brief Добавить один сэмпл на фазу в RMS-аккумулятор (IDLE)
static inline void rms_idle_accumulate_sample(int16_t sA, int16_t sB, int16_t sC)
{
	// центрирование (убираем DC — твоими калибровками)
	int32_t a = (int32_t)sA - (int32_t)cts.adcShift_A;
	int32_t b = (int32_t)sB - (int32_t)cts.adcShift_B;
	int32_t c = (int32_t)sC - (int32_t)cts.adcShift_C;

	// Квадраты: на AVR 32-бит достаточно при реальном масштабе (если вдруг опасаешься насыщения — можно ограничивать |a|/|b|/|c|)
	rms_sumsq[PHASE_A] += (uint32_t)(a * a);
	rms_sumsq[PHASE_B] += (uint32_t)(b * b);
	rms_sumsq[PHASE_C] += (uint32_t)(c * c);

	rms_count++;
}

/// @brief Финализировать окно RMS: посчитать RMS по каждой фазе и обнулить аккумуляторы.
/// Результат пишется в float_RMS_A/B/C (как у тебя) и далее в averageIt().
static inline void rms_idle_finalize_window(void)
{
	if (rms_count == 0) return;

	// RMS = sqrt( sumSq / N )
	uint32_t divA = rms_sumsq[PHASE_A] / rms_count;
	uint32_t divB = rms_sumsq[PHASE_B] / rms_count;
	uint32_t divC = rms_sumsq[PHASE_C] / rms_count;

	uint16_t rA = isqrt_u32(divA);
	uint16_t rB = isqrt_u32(divB);
	uint16_t rC = isqrt_u32(divC);

	// применяем твои поправки по нулям:
	float_RMS_A = (float)rA - 128.0f + (float)RMS_Zero[PHASE_A];
	float_RMS_B = (float)rB - 128.0f + (float)RMS_Zero[PHASE_B];
	float_RMS_C = (float)rC - 128.0f + (float)RMS_Zero[PHASE_C];

	if (float_RMS_A < 0) float_RMS_A = 0;
	if (float_RMS_B < 0) float_RMS_B = 0;
	if (float_RMS_C < 0) float_RMS_C = 0;

	// обнулить окно
	rms_sumsq[PHASE_A] = rms_sumsq[PHASE_B] = rms_sumsq[PHASE_C] = 0;
	rms_count = 0;

	// сглаживание (оставляем твою averageIt())
	averageIt();
	triggerRMScalc = 1;
}



static inline uint8_t make_bits_from_CB(uint8_t phase, uint8_t was_on)
{
	const uint8_t onMask  = (phase == PHASE_A ? CB1_ON  : (phase == PHASE_B ? CB2_ON  : CB3_ON));
	const uint8_t offMask = (phase == PHASE_A ? CB1_OFF : (phase == PHASE_B ? CB2_OFF : CB3_OFF));
	return was_on ? onMask : offMask;
}






/**
* @brief Выход из режима эмуляции: прочитать реальные входы, нормализовать пары.
*        Если пара невалидна (оба 0 или оба 1) — принять OFF по умолчанию.
*        Синхронизировать CB_State[*] и prev_state без генерации событий.
* @note  Подавляет обработку изменений на одну итерацию.
*/
// static void emu_contacts_leave_mode(void)
// {
// 	// 1) Прочтём реальные входы (инверсия из-за pull-up), зеркалим при auxType==1
// 	uint8_t s = (uint8_t)((~PIN_DI) & 0x3Fu);
// 	if (auxType == 1) {
// 		// B.ON = A.ON, B.OFF = A.OFF; C.ON = A.ON, C.OFF = A.OFF
// 		bitWrite(s, 2, bitRead(s, 0));
// 		bitWrite(s, 3, bitRead(s, 1));
// 		bitWrite(s, 4, bitRead(s, 0));
// 		bitWrite(s, 5, bitRead(s, 1));
// 	}
//
// 	// 2) Нормализация по фазам: валидная пара = ровно один бит (ON^OFF) == 1
// 	//    Невалидно → OFF по умолчанию.
// 	for (uint8_t ph = 0; ph < phase_quantity; ph++) {
// 		const uint8_t onMask  = (ph == PHASE_A ? CB1_ON  : (ph == PHASE_B ? CB2_ON  : CB3_ON));
// 		const uint8_t offMask = (ph == PHASE_A ? CB1_OFF : (ph == PHASE_B ? CB2_OFF : CB3_OFF));
//
// 		const uint8_t on_now  = (s & onMask)  ? 1u : 0u;
// 		const uint8_t off_now = (s & offMask) ? 1u : 0u;
//
// 		if ((on_now ^ off_now) == 0u) {
// 			// Невалидно (оба 0 или оба 1) → принудительно OFF
// 			s &= (uint8_t)~onMask;  // ON=0
// 			s |= offMask;           // OFF=1
// 			CB_State[ph] = cb_Off;
// 			} else {
// 			// Валидно → назначаем состояние напрямую (без getBreakerState)
// 			CB_State[ph] = on_now ? cb_On : cb_Off;
// 		}
//
// 		//phase_edge_latched[ph] = 0;
// 	}
//
// 	// 3) Зафиксировать "сырое" состояние как предыдущее
// 	prev_state = s;
//
// 	// 4) Подавить фиксацию "движения" на ближайшей итерации
// 	//	suppress_edges = 1;
// 	//	switchBlocked = true;
// 	// switchBounceTime = currentTime;
// 	contacts_state_raw = prev_state;
//
// }


/**
* @brief Принудительно задать "сырое" состояние концевиков (только в эмуляции).
*        Нормализует пары по фазам: валидно "ровно один активен"; при невалидности → OFF.
*        Обновляет CB_State[*], prev_state и публикует contacts_state_raw.
*        Подавляет фиксацию событий на одну итерацию.
* @param s_in Маска 6 младших бит: [C.OFF C.ON B.OFF B.ON A.OFF A.ON]
*/
// static void emu_contacts_force_state(uint8_t s_in)
// {
// 	// 1) Маскируем до 6 бит
// 	uint8_t s = (uint8_t)(s_in & 0x3Fu);
//
// 	// 2) При auxType==1 зеркалим A -> B,C
// 	if (auxType == 1) {
// 		bitWrite(s, 2, bitRead(s, 0)); // B.ON = A.ON
// 		bitWrite(s, 3, bitRead(s, 1)); // B.OFF = A.OFF
// 		bitWrite(s, 4, bitRead(s, 0)); // C.ON = A.ON
// 		bitWrite(s, 5, bitRead(s, 1)); // C.OFF = A.OFF
// 	}
//
// 	// 3) Нормализация по фазам (невалидно → OFF)
// 	for (uint8_t ph = 0; ph < phase_quantity; ph++) {
// 		const uint8_t onMask  = (ph == PHASE_A ? CB1_ON  : (ph == PHASE_B ? CB2_ON  : CB3_ON));
// 		const uint8_t offMask = (ph == PHASE_A ? CB1_OFF : (ph == PHASE_B ? CB2_OFF : CB3_OFF));
// 		const uint8_t on_now  = (s & onMask)  ? 1u : 0u;
// 		const uint8_t off_now = (s & offMask) ? 1u : 0u;
//
// 		if ((on_now ^ off_now) == 0u) {
// 			// оба 0 или оба 1 → OFF по умолчанию
// 			s &= (uint8_t)~onMask; // ON=0
// 			s |= offMask;          // OFF=1
// 			CB_State[ph] = cb_Off;
// 			} else {
// 			CB_State[ph] = on_now ? cb_On : cb_Off;
// 		}
// 		//phase_edge_latched[ph] = 0;
// 	}
//
// 	// 4) Синхронизация "сырого" и "предыдущего"
// 	prev_state = s;
// 	contacts_state_raw = s;
//
// 	// 5) Подавим фиксацию событий на одну итерацию
// 	//	suppress_edges = 1;
// 	//	switchBlocked = true;
// 	// switchBounceTime = currentTime;
// }



/**
* @brief Определить направление (ON→OFF или OFF→ON) по прошлому сырому состоянию.
* @param prev_raw 6 младших битов: A{ON,OFF}, B{ON,OFF}, C{ON,OFF}
* @param cb_stateA Подтверждённое состояние фазы A (фолбэк)
* @return cb_On_Off (идём к отключению) или cb_Off_On (идём к включению)
*/
static inline enum breakerState derive_last_operation(uint8_t prev_raw,
enum breakerState cb_stateA)
{
	const uint8_t a_on  = (prev_raw & CB1_ON)  ? 1u : 0u;
	const uint8_t a_off = (prev_raw & CB1_OFF) ? 1u : 0u;

	if (a_on && !a_off)  return cb_On_Off; // было ON → идём в OFF
	if (!a_on && a_off)  return cb_Off_On; // было OFF → идём в ON
	return (cb_stateA == cb_On || cb_stateA == cb_On_Off) ? cb_On_Off : cb_Off_On;
}



/**
* @brief Эмуляция: зафиксировать t0 и направление при внешнем триггере.
*        НЕ поднимает positionChanged — это сделает детектор, когда биты реально "пойдут".
*/



/********************************* DEBUG TAP (RAW12) *********************************/

/**
* @brief Сбросить «просвет»: обнулить счётчики/буферы.
*/
static inline void dbg_tap_reset(void)
{
	for (uint8_t ph=0; ph<3; ph++) {
		dbg_written[ph] = 0;
		//	for (uint16_t i=0; i<PHASE_SAMPLES_MAX; i++) dbg_ct_raw[ph][i] = 0;
	}
}

/*******************************  D B G   T A P   B U F F E R  ******************************/
/**
* @brief Записать один сырой код в SRAM-буфер выбранной фазы (эмулятор осциллограмм).
* @details
*  - Логика индекса такая же, как в record_scope() при записи в FRAM:
*      * Размыкание (lastOperation == cb_On_Off): индекс = dbg_written[ph] (0..N-1).
*      * Замыкание  (lastOperation == cb_Off_On): индекс = N-1-dbg_written[ph] (хвост→голова).
*  - Счётчик dbg_written[ph] остаётся «логическим» количеством записанных точек.
*  - Переполнения не допускаются: при w >= PHASE_SAMPLES_MAX запись не выполняется.
* @param ph   PHASE_A/B/C (0..2)
* @param code 12-битный код (0..4095)
*/
static inline void dbg_tap_push(uint8_t ph, uint16_t code)
{
	if (!dbg_tap_enable) return;
	if (ph > 2) return;

	uint16_t w = dbg_written[ph];
	if (w >= PHASE_SAMPLES_MAX) return;

	/* Определяем, нужно ли разворачивать индекс (как в record_scope) */
	const uint8_t reverse_write = (lastOperation == cb_Off_On) ? 1u : 0u;

	/* Физический индекс в SRAM-буфере отрисовки */
	const uint16_t idx = reverse_write
	? (uint16_t)(PHASE_SAMPLES_MAX - 1u - w)   /* замыкание: хвост → голова */
	: w;                                       /* размыкание: как было */

	dbg_ct_raw[ph][idx] = (int16_t)(code & 0x0FFFu);
	dbg_written[ph] = (uint16_t)(w + 1u);
}


/**
* @brief Получить сырой код из RAM-буфера для Modbus-вьювера.
* @param ph       фаза
* @param sample_i индекс сэмпла (0..PHASE_SAMPLES_MAX-1)
* @return 12-битный код (0..4095) или 0, если индекс за пределами записанного.
*/
static inline int16_t dbg_tap_get(uint8_t ph, uint16_t sample_i)
{
	if (ph > 2) return 0;
	if (sample_i >= dbg_written[ph]) return 0;
	return dbg_ct_raw[ph][sample_i]; // УЖЕ центрировано и signed
}
/********************************* END DEBUG TAP *************************************/




// /**
// * @brief Посчитать пофазные дельты старта и сохранить в g_phase_start_dt_ticks[3].
// * @details База:
// *          - если реальные концевики и auxType==1 → база=tA;
// *          - иначе (эмуляция или три блока) → база = min_nonzero(A,B,C).
// *          Если у фазы нет метки старта (0) — кладём 0xFFFF.
// */
// static void calc_phase_start_deltas_ticks(void)
// {
// 	const uint32_t tA = g_hdr.start_tick[PHASE_A];
// 	const uint32_t tB = g_hdr.start_tick[PHASE_B];
// 	const uint32_t tC = g_hdr.start_tick[PHASE_C];
//
// 	if (tA == 0u) { // нет метки A — считаем, что данных нет
// 		g_phase_start_dt_ticks[PHASE_A] = 0xFFFFu;
// 		g_phase_start_dt_ticks[PHASE_B] = 0xFFFFu;
// 		g_phase_start_dt_ticks[PHASE_C] = 0xFFFFu;
// 		return;
// 	}
//
// 	g_phase_start_dt_ticks[PHASE_A] = 0u;
// 	g_phase_start_dt_ticks[PHASE_B] = (tB ? (uint16_t)ticks_diff_u32(tB, tA) : 0xFFFFu);
// 	g_phase_start_dt_ticks[PHASE_C] = (tC ? (uint16_t)ticks_diff_u32(tC, tA) : 0xFFFFu);
// }
//


/*==============================================================================
*  ВНУТРЕННИЕ ХЕЛПЕРЫ ДЛЯ FSM
*==============================================================================*/

/**
* @brief Выбрать «активный» контакт для фазы по текущему логическому состоянию.
* @return 1 — активен ON (отслеживаем изменение ON: 1→0); 0 — активен OFF (отслеживаем OFF: 1→0).
*/
static inline uint8_t active_contact_is_on(breakerState s)
{
	switch (s) {
		case cb_On:
		case cb_On_Off: return 1u;  /* были во включённом — активен ON */
		case cb_Off:
		case cb_Off_On:
		case cb_NA:
		default:        return 0u;  /* были в выключенном/неопределённом — активен OFF */
	}
}




/**
* @brief Обновить логическое состояние выключателя по фазе на основе пары RAW-битов.
* @param on   текущий RAW-бит ON (0/1)
* @param off  текущий RAW-бит OFF (0/1)
* @param prev предыдущее логическое состояние (для выбора направления в переходе 00)
*/
static inline breakerState breaker_fsm_update(uint8_t on, uint8_t off, breakerState prev)
{
	if (on && !off)  return cb_On;    /* валидно: включён */
	if (!on && off)  return cb_Off;   /* валидно: выключен */

	/* переход/невалидно (00 или 11) — определяем направление по прошлому устойчивому состоянию */
	if (!on && !off) {
		if (prev == cb_On  || prev == cb_On_Off)  return cb_On_Off;
		if (prev == cb_Off || prev == cb_Off_On)  return cb_Off_On;
		return cb_NA;
	}
	/* on && off == 1 → невалидно: сохраняем предыдущее, если оно определено */
	return (prev == cb_On || prev == cb_Off || prev == cb_On_Off || prev == cb_Off_On) ? prev : cb_NA;
}

/**
* @brief Записать бит «направление коммутации» в @ref complexSP (cb_Off_On => 1, cb_On_Off => 0).
*/
static inline void write_last_direction_bit(uint8_t op)
{
	if (op == (uint8_t)cb_Off_On) {
		complexSP |=  (uint16_t)(1u << last_Direction);
		} else {
		complexSP &= (uint16_t)~(1u << last_Direction);
	}
}



/**
* @brief Сформировать RAW-состояние доп-контактов на момент текущего времени.
* @param ticks_now Абсолютное текущее время (ISR_microTimer).
* @return 6 младших бит: A{ON,OFF}, B{ON,OFF}, C{ON,OFF}.
*
* Алгоритм: t_rel = (uint16_t)(ticks_now - emu_contacts_t0_ticks),
* далее сравниваем t_rel с delay и delay+duration по каждой фазе.
*/
static uint8_t emu_contacts_make_state(uint32_t ticks_now)
{
	uint8_t s = 0;

	// Если сценарий не активен — вернуть исходные устойчивые состояния
	if (!emu_scenario_active) {
		for (uint8_t ph = 0; ph < 3; ph++) {
			const uint8_t on  = (emu_active_is_on[ph] ? 1u : 0u);
			const uint8_t off = (emu_active_is_on[ph] ? 0u : 1u);
			if (ph == 0) { if (on) s |= CB1_ON;  if (off) s |= CB1_OFF; }
			else if (ph==1){ if (on) s |= CB2_ON;  if (off) s |= CB2_OFF; }
			else          { if (on) s |= CB3_ON;  if (off) s |= CB3_OFF; }
		}
		return s;
	}

	// Относительное время от момента триггера
	const uint16_t t_rel = (uint16_t)(ticks_now - emu_contacts_t0_ticks);

	// Пофазное формирование
	for (uint8_t ph = 0; ph < 3; ph++)
	{
		const uint16_t delay = emu_contact_start_Delay_ticks[ph];
		const uint16_t dur   = emu_contact_duration_ticks[ph];
		const uint16_t tfin  = (uint16_t)(delay + dur);

		uint8_t on  = 0u;
		uint8_t off = 0u;

		if (t_rel < delay) {
			// до старта: исходное устойчивое
			if (emu_active_is_on[ph]) { on = 1u; off = 0u; }
			else                      { on = 0u; off = 1u; }
		}
		else if (t_rel < tfin) {
			// переходная зона
			on = 0u; off = 0u;
		}
		else {
			// после завершения: противоположное устойчивое
			if (emu_active_is_on[ph]) { on = 0u; off = 1u; }
			else                      { on = 1u; off = 0u; }
		}

		switch (ph) {
			case 0: if (on) s |= CB1_ON;  if (off) s |= CB1_OFF;  break; // A
			case 1: if (on) s |= CB2_ON;  if (off) s |= CB2_OFF;  break; // B
			case 2: if (on) s |= CB3_ON;  if (off) s |= CB3_OFF;  break; // C
		}
	}

	return s;
}

/**
* @brief Записать бит «направление коммутации» в @ref complexSP (cb_Off_On => 1, cb_On_Off => 0).
*/
static inline void emuFunctions()
{
	if (init_Emu_Mode_Flag)
	{
		EMU_init_EMU_mode();   // вход в эмуляцию (инициализация t0, prev_state, CB_State)
		
		init_Emu_Mode_Flag = 0;
	}

	if (exit_Emu_Mode_Flag)
	{
		jump_to_bootloader();
		//EMU_EXIT_EMU_mode();   // вход в эмуляцию (инициализация t0, prev_state, CB_State)
		exit_Emu_Mode_Flag = 0;
	}




	if (emu_Start_switching_Flag)
	{
		emu_Start_switching_Flag = 0;
		
		if (startCalculations == calcStandBy)
		EMU_Start_switching(ISR_microTimer);
	}
	
	//
	// 	if (startTimerTest)
	// 	{
	// 		timer2_selftest_1s();
	// 	}
}



static inline void silence_sm_tick(void)
{
	switch (s_state)
	{
		case SS_STANDBY:
		// намеренно пусто: без твоей команды SM не стартует
		break;

		case SS_WAIT_SILENT:
		if (IS_scope_and_contacts_complete())
		s_state = SS_INIT;
		break;

		case SS_INIT:
		// --- одноразовый вход, твой исходный код ---
		s_t0_ticks = ISR_microTimer;

		// порядок как у тебя

		s_state = SS_RUNNING;
		break;

		case SS_RUNNING:
		{
			const uint32_t dt = (uint32_t)(ISR_microTimer - s_t0_ticks);  // корректно при переполнении
			if (dt >= (uint32_t)silenceTime_ticks)
			s_state = SS_DONE;
		}
		break;

		case SS_DONE:
		// --- одноразовый вход ---
		updateSwitchCnt();
		kick_calculations();     // идемпотентная

		GIS_Switched_Finished = 1;   //
		s_state = SS_STANDBY;        // полный сброс: без твоей команды не перезапустится
		break;
	}
}

static inline void switch_Finish_Timers(void)
{
	silence_sm_tick();   // <-- вся логика таймера теперь здесь

	if (GIS_Switched_Finished && (startCalculations == calcFinished))
	{
		switchingFinished();
		GIS_Switched_Finished = 0;
		startCalculations = calcStandBy;
	}
}

//
//
// /** @brief Проверка частоты Timer2: за 1000 мс должно набежать 10000 тиков. */
// static void timer2_selftest_1s(void)
// {
// 	static uint32_t last_ms = 0, last_ticks = 0;
// 	if (millis() - last_ms >= 1000)
// 	{
// 		uint32_t now_ticks = ISR_microTimer;
// 		dt = now_ticks - last_ticks;
// 		last_ticks = now_ticks;
// 		last_ms += 1000;
// 		startTimerTest = 0; // test fin
// 	}
// }


/**
* @brief Обработать одно окно данных: аппроксимация синусом 50 Гц и запись результата на место.
*
* Модель: x[n] ≈ DC + a*sin(ωn) + b*cos(ωn), ω = 2π·f0/Fs.
* Коэффициенты оцениваются по методу НСК (эквивалент DFT по одному бину).
* Син/кос генерируются итеративно: s_{n+1} = s_n·cΔ + c_n·sΔ, c_{n+1} = c_n·cΔ − s_n·sΔ,
* где cΔ = cos(ω), sΔ = sin(ω).
*
* @param x            [in/out] указатель на массив int16_t (центрированные коды), длина ≥ N
* @param N            число отсчётов в окне
* @param Fs_Hz        частота дискретизации, Гц (например, 10000.0f)
* @param f0_Hz        частота синуса, Гц (обычно 50.0f)
* @param snr_thresh   порог «наличия сигнала» по отношению энергии синуса к общей, 0..1 (например, 0.2f)
* @param zero_if_absent  если true и сигнала нет → окно гасим в 0; если false — оставляем исходник
*
* @return Оценённая амплитуда A в «кодах АЦП» (пик), полезно для диагностики.
*/
static float filter50_reconstruct_window_inplace(int16_t *x,
uint16_t N,
float Fs_Hz,
float f0_Hz,
float snr_thresh,
uint8_t zero_if_absent)
{
	if (N == 0) return 0.0f;

	const float omega = 2.0f * (float)M_PI * f0_Hz / Fs_Hz;
	const float cD = cosf(omega);
	const float sD = sinf(omega);

	/* 1) DC и энергия */
	int32_t sum = 0;
	float   Etot = 0.0f;
	for (uint16_t n = 0; n < N; n++) {
		const int16_t xn = x[n];
		sum  += (int32_t)xn;
		Etot += (float)xn * (float)xn;
	}
	const float DC = (float)sum / (float)N;

	/* 2) Проекции на sin/cos (итеративный генератор) */
	float s = 0.0f, c = 1.0f;   // sin(0)=0, cos(0)=1
	float Sacc = 0.0f, Cacc = 0.0f;
	for (uint16_t n = 0; n < N; n++) {
		const float xn = (float)x[n] - DC;   // удаляем DC перед проекцией
		Sacc += xn * s;
		Cacc += xn * c;
		/* шаг вращения */
		const float s_next = s * cD + c * sD;
		const float c_next = c * cD - s * sD;
		s = s_next; c = c_next;
	}

	/* 3) Коэффициенты НСК (аппроксимации) и амплитуда */
	const float scale = 2.0f / (float)N;
	const float a = scale * Sacc;   // при sin
	const float b = scale * Cacc;   // при cos
	const float A = sqrtf(a*a + b*b);   // пик в «кодах»

	/* 4) Оценка "насколько это синус" — доля энергии синуса в общей */
	// Энергия синуса на окне примерно N * (A^2)/2 (для нулевого DC)
	const float Esine = (float)N * (A * A) * 0.5f;
	const float ratio = (Etot > 1.0f) ? (Esine / Etot) : 0.0f;

	/* 5) Решение: синус есть? */
	const uint8_t present = (ratio >= snr_thresh);

	/* 6) Реконструкция или гашение */
	if (present) {
		/* второй проход: сгенерировать sin/cos заново и записать сглаженный сигнал */
		s = 0.0f; c = 1.0f;
		for (uint16_t n = 0; n < N; n++) {
			const float xhat = DC + a * s + b * c;
			/* сатурация в int16_t */
			int32_t yi = (int32_t)lrintf(xhat);
			if (yi < -32768) yi = -32768;
			if (yi >  32767) yi =  32767;
			x[n] = (int16_t)yi;

			const float s_next = s * cD + c * sD;
			const float c_next = c * cD - s * sD;
			s = s_next; c = c_next;
		}
		} else if (zero_if_absent) {
		for (uint16_t n = 0; n < N; n++) x[n] = 0;
		} else {
		/* оставляем как есть */
	}

	return A;
}

/**
* @brief Сгладить всю фазу «идеальным» 50 Гц по окнам 2 периода. Пишет результат в dbg_ct_raw[ph][].
*
* @param ph            0/1/2 (PHASE_A/B/C)
* @param zero_if_absent 1 — если синуса нет, окно обнуляется; 0 — оставляем исходные значения
* @param snr_thresh     порог доли энергии 50 Гц (0..1). Типично 0.15..0.3.
* @return Средняя амплитуда по обработанным окнам (в кодах), можно логировать.
*
* @note Требует, чтобы данные были центрированы заранее (−2048..+2047).
*/
float filter50_phase_inplace(uint8_t ph, uint8_t zero_if_absent, float snr_thresh)
{
	if (ph >= phase_quantity) return 0.0f;

	/* длина доступной записи */
	uint16_t Ntot = dbg_written[ph];
	if (Ntot > PHASE_SAMPLES_MAX) Ntot = PHASE_SAMPLES_MAX;

	/* параметры окна: 2 периода 50 Гц при текущей Fs */
	const float  Fs = 1e6f / (float)SAMPLE_PERIOD_US;         // 10000.0f
	const float  f0 = (float)EMU_Sine_HZ;                     // 50.0f
	uint16_t Nw = (uint16_t)lrintf( 2.0f * Fs / f0 );         // ≈400
	if (Nw == 0) Nw = 1;

	float Aavg = 0.0f;
	uint16_t nwin = 0;

	/* пройти по окнам без доп. памяти */
	uint16_t pos = 0;
	while (pos < Ntot) {
		const uint16_t chunk = (uint16_t)((Ntot - pos) >= Nw ? Nw : (Ntot - pos));
		float A = filter50_reconstruct_window_inplace(&dbg_ct_raw[ph][pos],
		chunk, Fs, f0,
		snr_thresh, zero_if_absent);
		Aavg += A;
		nwin++;
		pos  += chunk;
	}

	return (nwin ? (Aavg / (float)nwin) : 0.0f);
}

/**
* @brief Сгладить все фазы. Должно вызываться после osc_center_inplace_all().
*/
void filter50_all_phases(void)
{
	/* пример: если синуса нет — обнулять окна; порог SNR=0.2 (20% энергии в 50 Гц) */
	(void)filter50_phase_inplace(0, 1u, 0.20f);
	(void)filter50_phase_inplace(1, 1u, 0.20f);
	(void)filter50_phase_inplace(2, 1u, 0.20f);
}






/* ====================== F1: МЕДИАНА(3) + ОДНОПОЛЮСНЫЙ НЧ ====================== */

/**
* @brief Возвратить медиану трёх целых.
*/
static inline int16_t median3(int16_t a, int16_t b, int16_t c)
{
	// без ветвления тяжеловато; сделаем коротко и быстро
	if (a > b) { int16_t t = a; a = b; b = t; }
	if (b > c) { int16_t t = b; b = c; c = t; }
	if (a > b) { int16_t t = a; a = b; b = t; }
	return b;
}

/**
* @brief Однополюсный НЧ (экспоненциальное сглаживание) с постоянной времени τ.
* y[n] = y[n-1] + α*(x[n]-y[n-1]), α = Ts/(τ+Ts)
* @param x  новый отсчёт (int16)
* @param y  предыдущее значение фильтра (float)
* @param alpha  коэффициент (0..1)
* @return новое значение фильтра (float)
*/
static inline float lpf1_step(int16_t x, float y, float alpha)
{
	return y + alpha * ((float)x - y);
}

/**
* @brief F1: медиана(3) → однополюсный НЧ. Пишет на место. Память O(1).
* @param ph          0/1/2
* @param tau_us      τ в микросекундах (напр., 500..1000)
* @note Задержка по фазе ≈ ~0.2–0.6 мс (медиана) + зависящая от τ; 50 Гц почти не страдает.
*/
void filter_med3_lpf_phase_inplace(uint8_t ph, uint32_t tau_us)
{
	if (ph >= 3) return;

	uint16_t N = dbg_written[ph];
	if (N > PHASE_SAMPLES_MAX) N = PHASE_SAMPLES_MAX;
	if (N < 3) return;

	const float Ts_us = (float)SAMPLE_PERIOD_US;
	const float alpha = Ts_us / ( (float)tau_us + Ts_us );  // 0..1

	int16_t *x = &dbg_ct_raw[ph][0];

	// Скользящая медиана(3) с записью на позицию n-1 (чтобы не портить окно)
	int16_t x0 = x[0];
	int16_t x1 = x[1];
	float y = (float)x0; // начальное для НЧ

	for (uint16_t n = 2; n < N; n++) {
		int16_t x2 = x[n];
		int16_t med = median3(x0, x1, x2);    // медиана окна [n-2,n-1,n]

		// шаг НЧ по медианированному отсчёту
		y = lpf1_step(med, y, alpha);

		// пишем на место с задержкой на один сэмпл (n-1)
		int32_t yi = (int32_t)lrintf(y);
		if (yi < -32768) yi = -32768;
		if (yi >  32767) yi =  32767;
		x[n-1] = (int16_t)yi;

		// сдвиг окна
		x0 = x1; x1 = x2;
	}

	// крайние точки: можно просто дублировать соседние
	x[0]     = x[1];
	x[N-1]   = x[N-2];
}

/* =================== F2: SAVITZKY–GOLAY (окно 11, полином 2) =================== */

/**
* @brief F2: Savitzky–Golay фильтр (окно 11, полином 2), запись на место.
* @details Нормированные коэффициенты: [-36,9,44,69,84,89,84,69,44,9,-36]/429
*          Центрированное окно: выход выравнивается по центру (задержка 5 отсчётов).
* @note Требуется N ≥ 11. На краях — «хвосты» дублируем.
*/
void filter_sg11p2_phase_inplace(uint8_t ph)
{
	if (ph >= 3) return;

	uint16_t N = dbg_written[ph];
	if (N > PHASE_SAMPLES_MAX) N = PHASE_SAMPLES_MAX;
	if (N < 11) return;

	static const int16_t c[11] = { -36, 9, 44, 69, 84, 89, 84, 69, 44, 9, -36 };
	const int32_t denom = 429;

	int16_t *x = &dbg_ct_raw[ph][0];

	// Буфер из 11 значений в стеке
	int16_t w[11];
	// Инициализация окна для позиции k=5 (центр)
	for (uint8_t i = 0; i < 11; i++) {
		w[i] = x[i];
	}

	// Обработка центральной части: пишем на позицию (n-5)
	for (uint16_t n = 10; n < N; n++) {
		// свёртка
		int32_t acc = 0;
		for (uint8_t i = 0; i < 11; i++) acc += (int32_t)c[i] * (int32_t)w[i];
		int32_t yi = acc / denom;
		if (yi < -32768) yi = -32768;
		if (yi >  32767) yi =  32767;
		x[n-5] = (int16_t)yi;

		// сдвинуть окно: w[0] выкинуть, добавить x[n+1] (если есть)
		// сначала сдвиг
		for (uint8_t i = 0; i < 10; i++) w[i] = w[i+1];
		// затем новая точка
		if (n+1 < N) w[10] = x[n+1];
	}

	// Хвосты (края): просто подтянуть к ближайшим отфильтрованным
	for (uint8_t k = 0; k < 5; k++) x[k]     = x[5];
	for (uint8_t k = 0; k < 5; k++) x[N-1-k] = x[N-6];
}




/* =================== 1) DESPIKE ПО ДЛИТЕЛЬНОСТИ (RUN-LENGTH) =================== */
/**
* @brief Удалить короткие выбросы |x| > T длительностью < L: заменить линейной интерполяцией.
* @param ph  фаза 0/1/2
* @param T   порог по модулю (коды), напр. min_Arc_Current в кодах
* @param L   минимальная длительность «настоящего» события (сэмплы), напр. 8..16
*/
void filter_despike_runlength_phase_inplace(uint8_t ph, int16_t T, uint16_t L)
{
	if (ph > 2) return;
	uint16_t N = dbg_written[ph];
	if (N > PHASE_SAMPLES_MAX) N = PHASE_SAMPLES_MAX;

	int16_t *x = &dbg_ct_raw[ph][0];
	uint16_t i = 0;

	while (i < N) {
		// ищем начало выброса
		if ((i < N) && ( (x[i] >  T) || (x[i] < -T) )) {
			uint16_t start = i;
			// считаем длину подряд |x| > T
			while (i < N && ( (x[i] >  T) || (x[i] < -T) )) i++;
			uint16_t end = i;                 // [start .. end-1]
			uint16_t len = (uint16_t)(end - start);

			if (len < L) {
				// короткий выброс → интерполяция
				int16_t y0 = (start > 0) ? x[start-1] : 0;
				int16_t y1 = (end < N)   ? x[end]     : y0;
				// линейный мост
				for (uint16_t k = 0; k < len; k++) {
					int32_t num = (int32_t)(y0) * (int32_t)(len - k) + (int32_t)(y1) * (int32_t)k;
					x[start + k] = (int16_t)(num / (int32_t)len);
				}
			}
			// иначе оставляем как есть (это «настоящий» участок)
			} else {
			i++;
		}
	}
}

/* ====================== 2) ХЭМПЕЛЕВСКИЙ ФИЛЬТР (Hampel) ======================= */
/**
* @brief Hampel-фильтр: окно W (нечёт), выбросы > k*MAD заменяются медианой.
* @param ph  фаза 0/1/2
* @param W   размер окна (odd), обычно 7..11
* @param k   множитель порога, обычно 3.0
* @note Реализация O(W) на точку, буфер W в стеке; для N~40k и W<=11 — нормально.
*/
static int16_t isort_pick_median(int16_t *w, uint8_t W) {
	// простой вставками, W<=11 — достаточно быстро
	for (uint8_t i = 1; i < W; i++) {
		int16_t key = w[i]; int8_t j = (int8_t)i - 1;
		while (j >= 0 && w[j] > key) { w[j+1] = w[j]; j--; }
		w[j+1] = key;
	}
	return w[W/2];
}
void filter_hampel_phase_inplace(uint8_t ph, uint8_t W, float k)
{
	if (ph > 2 || (W < 3) || !(W & 1)) return;
	uint16_t N = dbg_written[ph];
	if (N > PHASE_SAMPLES_MAX) N = PHASE_SAMPLES_MAX;
	if (N < W) return;

	int16_t *x = &dbg_ct_raw[ph][0];
	const uint8_t H = W/2;

	// рабочие окна в стеке
	int16_t win[21];     // максимум W=21
	int16_t absbuf[21];

	// копируем для первых W
	for (uint16_t n = H; n < (N - H); n++) {
		// собрать окно вокруг n
		for (uint8_t i = 0; i < W; i++) win[i] = x[n - H + i];
		// медиана
		int16_t wcpy[21];
		for (uint8_t i = 0; i < W; i++) wcpy[i] = win[i];
		int16_t med = isort_pick_median(wcpy, W);
		// MAD
		for (uint8_t i = 0; i < W; i++) absbuf[i] = (int16_t)abs((int)win[i] - (int)med);
		int16_t acpy[21];
		for (uint8_t i = 0; i < W; i++) acpy[i] = absbuf[i];
		int16_t mad = isort_pick_median(acpy, W);
		float thr = k * (float)mad * 1.4826f;  // масштаб под σ (для норм. шума)

		if (fabsf((float)x[n] - (float)med) > thr) {
			x[n] = med; // выброс → заменяем медианой
		}
	}
	// края просто подтягиваем к соседям
	for (uint8_t i = 0; i < H; i++) x[i] = x[H];
	for (uint8_t i = 0; i < H; i++) x[N-1-i] = x[N-1-H];
}

/* =================== 3) ОГРАНИЧИТЕЛЬ СКОРОСТИ ИЗМЕНЕНИЯ ====================== */
/**
* @brief Slew-rate limiter: ограничить |Δx| ≤ dv_max кодов/сэмпл.
* @param ph     фаза 0/1/2
* @param dv_max максимум приращения между соседними отсчётами
*/
void filter_slew_limit_phase_inplace(uint8_t ph, int16_t dv_max)
{
	if (ph > 2) return;
	uint16_t N = dbg_written[ph];
	if (N > PHASE_SAMPLES_MAX) N = PHASE_SAMPLES_MAX;
	if (N < 2) return;

	int16_t *x = &dbg_ct_raw[ph][0];
	int16_t y = x[0];

	for (uint16_t n = 1; n < N; n++) {
		int32_t d = (int32_t)x[n] - (int32_t)y;
		if (d >  dv_max) d =  dv_max;
		if (d < -dv_max) d = -dv_max;
		y = (int16_t)( (int32_t)y + d );
		x[n] = y;
	}
}

/* ========== 4) ПОРОГ + ПЕРСИСТЕНТНОСТЬ: «короткие вспышки» → ноль ============ */
/**
* @brief Если |x| > T держится < Lmin подряд — занулить такие точки (считать шумом).
* @param ph     фаза 0/1/2
* @param T      порог по модулю (коды)
* @param Lmin   минимальная длительность «настоящего» участка
*/
void filter_threshold_persistence_zero_phase_inplace(uint8_t ph, int16_t T, uint16_t Lmin)
{
	if (ph > 2) return;
	uint16_t N = dbg_written[ph];
	if (N > PHASE_SAMPLES_MAX) N = PHASE_SAMPLES_MAX;

	int16_t *x = &dbg_ct_raw[ph][0];
	uint16_t i = 0;

	while (i < N) {
		// пропускаем «тишину»
		while (i < N && abs((int)x[i]) <= T) i++;
		if (i >= N) break;
		uint16_t start = i;
		while (i < N && abs((int)x[i]) > T) i++;
		uint16_t end = i;
		uint16_t len = (uint16_t)(end - start);
		if (len < Lmin) {
			for (uint16_t k = start; k < end; k++) x[k] = 0;
		}
	}
}



/* ===================== ВЫЗОВ ПО РЕЖИМУ (переключатель) ====================== */

/**
* @brief Применить выбранный фильтр к фазе. Пишет на место.
* @param ph        0/1/2
* @param mode      выбор фильтра (см. filter_mode_t)
*/
/**
* @brief Применить выбранный фильтр к фазе. Пишет на место.
* @param ph   0/1/2
* @param mode см. filter_mode_t
*/
void apply_filter_phase(uint8_t ph, filter_mode_t mode)
{
	switch (mode) {
		case F_BY_PASS:
		return;

		case F_MED3_LPF:
		filter_med3_lpf_phase_inplace(ph, g_lpf_tau_us);
		break;

		case F_SG_11P2:
		filter_sg11p2_phase_inplace(ph);
		break;

		case F_SINE_RECON_50HZ:
		(void)filter50_phase_inplace(ph, /*zero_if_absent=*/0u, /*snr=*/0.20f);
		break;

		case F_DESPIKE_RUN:
		filter_despike_runlength_phase_inplace(ph,
		g_noise_T_codes,
		g_noise_L_min);
		break;

		case F_HAMPEL:
		/* гарантируем валидные параметры окна */
		g_hampel_W = clamp_odd_3_21(g_hampel_W);
		g_hampel_k = clamp_f(g_hampel_k, 0.5f, 10.0f);
		filter_hampel_phase_inplace(ph, g_hampel_W, g_hampel_k);
		break;

		case F_SLEW_LIMIT:
		g_slew_dv_max = clamp_i16(g_slew_dv_max, 1, 500);
		filter_slew_limit_phase_inplace(ph, g_slew_dv_max);
		break;

		case F_THRESH_PERSIST_ZERO:
		filter_threshold_persistence_zero_phase_inplace(ph,
		g_noise_T_codes,
		g_noise_L_min);
		break;

		case F_PROFILE_P1:
		filter_despike_runlength_phase_inplace(ph, g_noise_T_codes, g_noise_L_min);
		filter_med3_lpf_phase_inplace(ph, g_lpf_tau_us);
		break;
	}
}



/**
* @brief Применить фильтр ко всем фазам.
*/
void apply_filter_all(filter_mode_t mode)
{
	apply_filter_phase(0, mode);
	apply_filter_phase(1, mode);
	apply_filter_phase(2, mode);
}