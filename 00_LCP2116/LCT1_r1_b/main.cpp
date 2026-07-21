// ToDo

// Кольцевой буффер записи дуги. Необходимо для засекания дуги при включении и отключении
// Изменить логику работы с концевиками. ожидать концевики не по времени заполнения буффера, а по времени прихода сигнала с концевика или
// по таймауту.
// Сделать 3 буффера ОВО


#include "main.h"
#include <avr/eeprom.h>

// u16 buf_adc1[ADC_SMPLS] = {0};
// u16 buf_adc2[ADC_SMPLS] = {0};
// u16 buf_adc3[ADC_SMPLS] = {0};

// u16 buf_adc1[ADC_SMPLS] = {0};
// u16 buf_adc2[ADC_SMPLS] = {0};
// u16 buf_adc3[ADC_SMPLS] = {0};

u8 tmp[12];


// ==== Objects:
TIMER			timer;						// Timer for measurements
ADC_SPI			cts;						// Current Transformers
IGAS_X2X_485	modbus;						// Modbus protocol routines
// ================

// ==== Global variables:
uint8_t		GLOBAL_MODE		= IDLE;			// Флаг режима записи осциллограмм с ТТ: IDLE - дежурный режим, BUSY - идёт запись, FULL - оба буфера заполнены.
uint32_t	timer1_millis;



unsigned long totalSwitchTimer;
uint16_t	enc_abs_status;
uint8_t lastOperationBuf[3] = {0};
uint16_t lastOperationTimeBuf[3] = {0};


uint16_t	current_ph_A	= 0;			// Значение тока в дежурном режиме, А * 100. Фаза A
uint16_t	current_ph_B	= 0;			// Значение тока в дежурном режиме, А * 100. Фаза B
uint16_t	current_ph_C	= 0;			// Значение тока в дежурном режиме, А * 100. Фаза C

uint8_t		buffer_has_new_data = 0;	// Флаг обновления данных в буфере осциллограмм. Исп-ся для старта вычислений значений структуры modbus_data
uint8_t		startCalculations = false;      // Флаг - данные получены. запуск математической обработки
uint8_t		moveFRAM_Buffer = 0;      // Флаг - данные получены. запуск математической обработки
uint16_t	max_electrical_wear = 1240;
uint16_t	max_mechanical_wear = 5000;

uint16_t fullBraker = 0;

int16_t	min_Arc_Current		= 10;		// Минимальное значение тока горения дуги
//uint8_t		zero_is_found		= false;
uint8_t		positionChanged	= false;

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
uint8_t		phase_zero_1[phase_quantity]= {0};	// Для osc_buffer_1: номер осциллограммы в массиве, на которой ток упал до значения current_minimum и ниже.
uint8_t		ISR_arcPoint[phase_quantity]= {0};	// Для osc_buffer_2: номер осциллограммы в массиве, на которой ток упал до значения current_minimum и ниже.
uint8_t		lastOperation = 0;
uint8_t		lastOperationTime = 0;
uint8_t     ISR_timeShiftOff[phase_quantity] = {0};
uint8_t     ISR_timeShiftOn[phase_quantity] = {0};
uint16_t	own_off_time_ISR[phase_quantity] = {0};	// Время полного срабатывания выключателя на выключение, мс
uint16_t	own_on_time_ISR[phase_quantity] = {0};	    // Время полного собственного срабатывания выключателя на включение, мс
uint16_t	full_off_time_ISR[phase_quantity] = {0};			// Время собственного срабатывания выключателя на выключение, мс
uint16_t	full_on_time_ISR[phase_quantity] = {0};			// Время собственного срабатывания выключателя на включение, мс

uint8_t		off_on_Captured = 0;

unsigned long currentTime = 0;
uint16_t switchBounceTime = 0;
uint16_t switchBounceSilense = 10;

uint8_t arcDetected[3] = {0};

const uint8_t trendLimit = 5;
float RMS_Aver[phase_quantity][6] = {0};
uint8_t averPosition = 0;
float AVARAGED_RMS[phase_quantity] = {0};
float TREND[phase_quantity][trendLimit+1] = {0};
uint8_t tail = 0;
float PIDFILTER[4] = {0};
float const SMOOTHfilter[] = {0.03, 0.03, 0.03};
float const AGGRESSfilter[] = {0.5, 0.5, 0.5};
float filteredRMS[phase_quantity] = {0};


uint16_t	ISR_arc_lifetime[phase_quantity] = {0};// 13, 14, 15 - Время горения дуги во время последнего отключения, мс.
uint16_t	switchCurrent_Amp[phase_quantity] = {0};
float		k_current[phase_quantity] = {0};
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

uint8_t auxType;

uint8_t RMS_Zero[phase_quantity];

uint8_t BLOCK_GAIN = 0;


unsigned long timeToGetVal = 0;

unsigned long switchCoolDownStartTime = 0; // Засекаем время нахождения выключателя в конечном положении.
uint8_t switchCoolDownTime = 100; // уставка времени выключателя в конечном положении - против дребезга.
uint8_t switchBlocked = false;  // Флаг разрешения учета положения выключателя.


breakerState CB_State1 = cb_NA;
breakerState CB_State2 = cb_NA;
breakerState CB_State3 = cb_NA;


unsigned long ISR_Timer_A = 0; // засекаем скорости переключения
unsigned long ISR_Timer_B = 0; // засекаем скорости переключения
unsigned long ISR_Timer_C = 0; // засекаем скорости переключения

uint16_t complexSP = 0;
unsigned long ISR_microTimer = 0;

uint8_t manualMode = 0;


uint8_t strikeData = 1;
uint16_t strikeLostData = 0;

uint64_t current_A_buf = 0;
uint64_t current_B_buf = 0;
uint64_t current_C_buf = 0;
const float k_scaling_factor = 0.0005;  // Scaling factor for RMS calculation

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

#define	ADC_MID		0
uint8_t curr_state = 0;

enum
{
	ownTimeOff_Warn_Bit,
	ownTimeOff_Alarm_Bit,
	ownTimeOn_Warn_Bit,
	ownTimeOn_Alarm_Bit,
	fullTimeOffWarn_Bit,
	fullTimeOffAlarm_Bit,
	fullTimeOnWarn_Bit,
	fullTimeOnAlarm_Bit,
	A_Fail,
	B_Fail,
	C_Fail,
	part_Phase_Switch_Bit,
	last_Direction,
};



enum
{
	procDone,
	runProc,
};


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
	uint16_t	power_On_cntr;	 // включений
	uint16_t	power_Off_cntr;	 // выключений
	
	// 	uint16_t	ph_B_On_cntr;
	// 	uint16_t	ph_C_On_cntr;
	//
	// 	// 22, 23, 24 - Счётчик операций с фазой (общее кол-во выключений).
	// 	uint16_t	ph_A_Off_cntr;
	// 	uint16_t	ph_B_Off_cntr;
	// 	uint16_t	ph_C_Off_cntr;


	// 25, 26, 27 - Механический износ контактов, %
	float		mechanical_wear_Perc;

	// 28, 29, 30 - Общий электрический износ контактов, %.
	float		ph_A_electrical_Perc;
	float		ph_B_electrical_Perc;
	float		ph_C_electrical_Perc;

	// 31, 32, 33 - Электрический износ за время последнего отключения, %.
	float		ph_A_last_elec_perc;
	float		ph_B_last_elec_perc;
	float		ph_C_last_elec_perc;


	
	uint16_t	short_Cnt_On;			// Время собственного срабатывания выключателя на включение, мс
	uint16_t	short_Cnt_Off;			// Время собственного срабатывания выключателя на включение, мс

	float		ph_A_Gain_k_current;
	float		ph_B_Gain_k_current;
	float		ph_C_Gain_k_current;
	uint16_t    minFailCurrent;
	
	float		overCurrentKoef;
	uint16_t	nominalCurrent_Amp;

} params;
// ================


typedef void (*AppPtr_t) (void);
AppPtr_t GoToBootloader = (AppPtr_t)0x3F07;

uint8_t prev_state = 0;

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


uint16_t	AutoSwitchTime = 0;
uint16_t	AutoSwitchTime_Delay = 350;


uint8_t shootBreakerFlag = 0;




unsigned long timeItTake = 9000;  // буфферная переменная чтоб засечь время выполнения функций.
uint8_t calcSteps = 0;  // показывает текущий шаг в вычислениях и записи осциллограмм в Eeprom и SD карту
uint8_t transferDataToLCP = 0;

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
	cts.flush_buffer((uint8_t *) cts.overflowOscBuffer, sizeof(cts.overflowOscBuffer));
	//	cts.flush_buffer((uint8_t *) cts.osc_buffer_2, sizeof(cts.osc_buffer_2));
	cts.flush_buffer((uint8_t *) phase_zero_1, sizeof(phase_zero_1));
	cts.flush_buffer((uint8_t *) ISR_arcPoint, sizeof(ISR_arcPoint));
	void *ptr1 = &params;
	cts.flush_buffer((uint8_t *) ptr1, sizeof(modbus_data));

	loadSettings();
	gpio_init();							// Инициализация пинов
	comm_init();


	//	fram_init();
	// 	fram_write_enable();


	millis_init();

	timer.init();							// Инициализация Timer2 на 200 мкс
	timer.start();							// Запуск таймера на 200 мкс
	sei();									// Глобальное разрешение прерываний

	cts.recording_status = INPROGRESS;		// Флаг окончания непрерывной записи пачки осциллограмм


	//	DI_curr_state = check_DI_status();		// Опрос текущего состояния дискретных входов - концевики положения выключателя
	//	DI_prev_state = DI_curr_state;			// Текущее и предыдущее на начальный момент равны.
	
	curr_state = (~PIN_DI) & 0b00111111;			// Поскольку выходы - PULLED UP, то инвертируем состояние выключателей
	
	if (auxType == 1)
	{
		bitWrite(curr_state, 2, bitRead(curr_state, 0));  // в режиме одного концевика - дублируем первый концевик на остальные два
		bitWrite(curr_state, 3, bitRead(curr_state, 1));  // в режиме одного концевика - дублируем первый концевик на остальные два
		bitWrite(curr_state, 4, bitRead(curr_state, 0));  // в режиме одного концевика - дублируем первый концевик на остальные два
		bitWrite(curr_state, 5, bitRead(curr_state, 1));  // в режиме одного концевика - дублируем первый концевик на остальные два
	}
	prev_state = curr_state;
	
	if ((prev_state & CB1_OFF) != OFF)
	CB_State1 = cb_Off;
	else if ((prev_state & CB1_ON) == ON)
	CB_State1 = cb_On;
	else
	CB_State1 = cb_NA;

	
	

	
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

	cts.amplification = 1;

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
void loop()
{
	if (GLOBAL_MODE != BUSY)
	{
		currentTime = millis();
		
		if (rebootLCT && (currentTime - rebootLCT > 50))
		{
			cli();
			GoToBootloader();
		}


		// 		if (currentTime - ms100Cycle > 500)
		// 		{
		// 			ms100Cycle = currentTime;
		modbus_timeout_check();										// Цикл (без прерываний) на 500 мс
		
		//	timeItTake = micro40timer;
		
		//	}
		
		modbus.update(currentTime);											// Вызываем обработку Modbus
		moveFramToBuf();
		calculations();
	}
	
	isrMeasurements();
	RMS_calculations();
}

void loadSettings()
{
	// 	uint16_t WordOfData;
	// 	WordOfData = eeprom_read_word((uint16_t*)46);
	// 	saveFloat(46, 255);

	
	params.ph_A_kA2s = loadFloat(10,0);			// 0
	params.ph_B_kA2s = loadFloat(14,0);			// 0
	params.ph_C_kA2s = loadFloat(18,0);			// 0
	
	params.power_Off_cntr = eeprom_read_word((uint16_t*)22);	// 0
	params.power_On_cntr = eeprom_read_word((uint16_t*)24);		// 0
	// 	params.ph_C_On_cntr = eeprom_read_word((uint16_t*)26);
	//
	
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
	
	
	
	k_current[PHASE_A] = loadFloat(44, 1.0);	// 73,2
	k_current[PHASE_B] = loadFloat(48, 1.0);	// 73,2
	k_current[PHASE_C] = loadFloat(52, 1.0);	// 73,2
	
	params.ph_A_Gain_k_current = loadFloat(56, 1.0);	// 1
	params.ph_B_Gain_k_current = loadFloat(60, 1.0);	// 1
	params.ph_C_Gain_k_current = loadFloat(64, 1.0);	// 1
	
	params.minFailCurrent = eeprom_read_byte((uint8_t*)68);	// 40
	params.overCurrentKoef = loadFloat(69, 1.1);			// 1.1

	//params.power_Off_cntr = eeprom_read_word((uint16_t*)73);
	// 	params.ph_B_Off_cntr = eeprom_read_word((uint16_t*)74);
	// 	params.ph_C_Off_cntr = eeprom_read_word((uint16_t*)75);
	
	params.short_Cnt_Off = eeprom_read_word((uint16_t*)76);
	params.short_Cnt_On = eeprom_read_word((uint16_t*)78);
	
	params.nominalCurrent_Amp = eeprom_read_word((uint16_t*)80);  // 3150
	
	fullTimeOffWarn_ISR = eeprom_read_word((uint16_t*)82);				// 200us
	fullTimeOffAlarm_ISR = eeprom_read_word((uint16_t*)84);				// 200us
	// 	ownTimeOffWarn_ISR = eeprom_read_word((uint16_t*)86);				// 200us
	// 	ownTimeOffAlarm_ISR = eeprom_read_word((uint16_t*)88);				// 200us
	
	auxType = eeprom_read_byte((uint8_t*)90);				// 1
	
	
	min_Arc_Current = eeprom_read_word((uint16_t*)91);		// 20
	
	params.mechanical_wear_Perc = 1.0 * (params.power_On_cntr + params.power_Off_cntr) * 50.0 / max_mechanical_wear;
	
	
	
	params.ph_A_electrical_Perc = 1.0 * params.ph_A_kA2s * 100.0 / max_electrical_wear;
	params.ph_B_electrical_Perc = 1.0 * params.ph_B_kA2s * 100.0 / max_electrical_wear;
	params.ph_C_electrical_Perc = 1.0 * params.ph_C_kA2s * 100.0 / max_electrical_wear;
	
	
	
	RMS_Zero[PHASE_A] = eeprom_read_byte((uint8_t*)93);		// 128
	RMS_Zero[PHASE_B] = eeprom_read_byte((uint8_t*)94);		// 128
	RMS_Zero[PHASE_C] = eeprom_read_byte((uint8_t*)95);		// 128
	
	
	ISR_timeShiftOn[PHASE_A] =  eeprom_read_byte((uint8_t*)96);	// 128 //1 = 200 us
	ISR_timeShiftOn[PHASE_B] =  eeprom_read_byte((uint8_t*)97);	// 128 //1 = 200 us
	ISR_timeShiftOn[PHASE_C] =  eeprom_read_byte((uint8_t*)98);	// 128 //1 = 200 us
	
	switchBounceSilense = eeprom_read_byte((uint8_t*)99);	// 100
	
	mb_slave_addr = eeprom_read_byte((uint8_t*)100); // нет
	
	if (mb_slave_addr <1 || mb_slave_addr>254)		 //
	mb_slave_addr = 1;

	//	mb_slave_addr = 3;
	
	
	ownTimeOffWarn_ISR = eeprom_read_word((uint16_t*)101);		// 34 ms
	ownTimeOffAlarm_ISR = eeprom_read_word((uint16_t*)103);		// 40 ms
	fullTimeOffWarn_ISR = eeprom_read_word((uint16_t*)105);		// 65 ms
	fullTimeOffAlarm_ISR = eeprom_read_word((uint16_t*)107);	// 70 ms
	fullTimeOnWarn_ISR = eeprom_read_word((uint16_t*)109);		// 80 ms
	fullTimeOnAlarm_ISR = eeprom_read_word((uint16_t*)111);		// 90 ms
	ownTimeOnWarn_ISR = eeprom_read_word((uint16_t*)113);		// 80 ms
	ownTimeOnAlarm_ISR = eeprom_read_word((uint16_t*)115);		// 90 ms
	
	
	
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
	
	
	
	eeprom_write_word((uint16_t*)22,0);	// 0
	eeprom_write_word((uint16_t*)24,0);		// 0
	
	params.power_Off_cntr = eeprom_read_word((uint16_t*)22);	// 0
	params.power_On_cntr = eeprom_read_word((uint16_t*)24);		// 0
	// 	params.ph_C_On_cntr = eeprom_read_word((uint16_t*)26);
	//

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
	
	
	saveFloat(44, 73.2);	// 73,2
	saveFloat(48, 73.2);	// 73,2
	saveFloat(52, 73.2);	// 73,2
	
	
	k_current[PHASE_A] = loadFloat(44, 1.0);	// 73,2
	k_current[PHASE_B] = loadFloat(48, 1.0);	// 73,2
	k_current[PHASE_C] = loadFloat(52, 1.0);	// 73,2
	
	
	saveFloat(56, 2.2);	// 1
	saveFloat(60, 2.2);	// 1
	saveFloat(64, 2.2);	// 1
	
	params.ph_A_Gain_k_current = loadFloat(56, 1.0);	// 1
	params.ph_B_Gain_k_current = loadFloat(60, 1.0);	// 1
	params.ph_C_Gain_k_current = loadFloat(64, 1.0);	// 1
	
	
	eeprom_write_byte((uint8_t*)68,40);	// 40
	saveFloat(69, 1.1);			// 1.1
	
	params.minFailCurrent = eeprom_read_byte((uint8_t*)68);	// 40
	params.overCurrentKoef = loadFloat(69, 1.1);			// 1.1

	//params.power_Off_cntr = eeprom_read_word((uint16_t*)73);
	// 	params.ph_B_Off_cntr = eeprom_read_word((uint16_t*)74);
	// 	params.ph_C_Off_cntr = eeprom_read_word((uint16_t*)75);
	
	
	eeprom_write_word((uint16_t*)76,0);
	eeprom_write_word((uint16_t*)78,0);
	
	
	params.short_Cnt_Off = eeprom_read_word((uint16_t*)76);
	params.short_Cnt_On = eeprom_read_word((uint16_t*)78);
	
	
	eeprom_write_word((uint16_t*)80,3150);  // 3150
	params.nominalCurrent_Amp = eeprom_read_word((uint16_t*)80);  // 3150
	
	
	eeprom_write_byte((uint8_t*)90,1);				// 1
	auxType = eeprom_read_byte((uint8_t*)90);				// 1
	
	
	eeprom_write_word((uint16_t*)91,20);		// 20
	min_Arc_Current = eeprom_read_word((uint16_t*)91);		// 20
	
	params.mechanical_wear_Perc = 1.0 * (params.power_On_cntr + params.power_Off_cntr) * 50.0 / max_mechanical_wear;
	
	
	
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
	
	
	
	eeprom_write_byte((uint8_t*)99,100);					// 99
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
	loadDefaultsDone = 1;
	

}

uint8_t calcSwitchCurrentFlag = 0;



void moveFramToBuf()
{
	if (moveFRAM_Buffer!=0)
	{
		switch (buffer_has_new_data)
		{
			// 			case 2:
			// 			{
			// 				//process_adc_data(cts.osc_buffer_1, rawPoints, averPoints);  // извлечение ADC измерений из RAW данных и осреднение
			//
			// 				// Расчет тока. Режим замера во время коммутации - только без усиления.
			// 				for (int i = 0; i < osc_length; i++)
			// 				{
			// 					cts.osc_buffer_1[i][PHASE_A] = (cts.osc_buffer_1[i][PHASE_A] - ADC_MID - 128 + cts.adcShift_A);
			// 					cts.osc_buffer_1[i][PHASE_B] = (cts.osc_buffer_1[i][PHASE_B] - ADC_MID - 128 + cts.adcShift_B);
			// 					cts.osc_buffer_1[i][PHASE_C] = cts.osc_buffer_1[i][PHASE_C] - ADC_MID - 128 + cts.adcShift_C;
			// 				}
			// 				lastOperationBuf[1] = lastOperation;
			// 				// 				switchCurrent_Buf[PHASE_A][1] = params.RMS_A;
			// 				// 				switchCurrent_Buf[PHASE_B][1] = params.RMS_B;
			// 				// 				switchCurrent_Buf[PHASE_C][1] = params.RMS_C;
			//
			// 				moveFRAM_Buffer = 0;  // copy from FRAM to Buf done
			// 				if (startCalculations == false)  // if calculations not busy and we have data - start calculations
			// 				startCalculations = 1;
			//
			// 			}
			// 			break;
			
			
			
			
			case 1:
			{
				//	process_adc_data(cts.osc_buffer_2, rawPoints, averPoints);  // извлечение ADC измерений из RAW данных и осреднение

				memcpy(cts.calcOscBuffer, cts.overflowOscBuffer, sizeof(cts.overflowOscBuffer));
				// Расчет тока. Режим замера во время коммутации - только без усиления.
				
				shiftADC(); // Shift ADC to mid or 0
				
				lastOperationBuf[2] = lastOperationBuf[1];
				
				lastOperationTimeBuf[2] = lastOperationTimeBuf[1];
				
				// 				switchCurrent_Buf[PHASE_A][2] = params.RMS_A;
				// 				switchCurrent_Buf[PHASE_B][2] = params.RMS_B;
				// 				switchCurrent_Buf[PHASE_C][2] = params.RMS_C;
				
				moveFRAM_Buffer = 0;  // copy from FRAM to Buf done
				if (startCalculations == false)  // if calculations not busy and we have data - start calculations
				startCalculations = 1;
			}
			break;
			
			
			default:
			
			break;
			
			
		}

	}

	
}


void shiftADC()
{
	for (int i = 0; i < osc_length; i++)
	{
		if (BLOCK_GAIN == 1)
		{
			cts.calcOscBuffer[i][PHASE_A] = cts.calcOscBuffer[i][PHASE_A] - ADC_MID - 128 + cts.gainAdcShift_A;
			cts.calcOscBuffer[i][PHASE_B] = cts.calcOscBuffer[i][PHASE_B] - ADC_MID - 128 + cts.gainAdcShift_B;
			cts.calcOscBuffer[i][PHASE_C] = cts.calcOscBuffer[i][PHASE_C] - ADC_MID - 128 + cts.gainAdcShift_C;
		}
		else
		{
			cts.calcOscBuffer[i][PHASE_A] = cts.calcOscBuffer[i][PHASE_A] - ADC_MID - 128 + cts.adcShift_A;
			cts.calcOscBuffer[i][PHASE_B] = cts.calcOscBuffer[i][PHASE_B] - ADC_MID - 128 + cts.adcShift_B;
			cts.calcOscBuffer[i][PHASE_C] = cts.calcOscBuffer[i][PHASE_C] - ADC_MID - 128 + cts.adcShift_C;
		}
	}
}



void calculations()
{
	
	if (startCalculations == false)
	return;
	
	
	switch (calcSteps)
	{
		case 0:
		/*
		if (calcSteps != 0)  // Если вычисления не закончены и буффер занят, пишем данные в следующий буффер и ставим его в очередь на обработку.
		{
		currentBuffer = 0;   // Дописать обработку - если текущий буфер не обратан - пишем в следующий
		nextBuffer = 0;
		}
		*/
		
		
		
		quickRMScnt = 0; // Сбрасываем счетчик быстрого RMS. То есть стартуем один замер RMS на 200 точек
		
		//		memset(cts.osc_buffer_2[currentBuffer], 0, sizeof(cts.osc_buffer_2[currentBuffer]));				  // Очищаем буфер для новых данных
		

		//	memset(cts.osc_buffer_1, 0, sizeof(cts.osc_buffer_1));				  // Очищаем буфер приемник для новых данных
		
		
		// 		memset(enc_rec_2, 0, sizeof(enc_rec_2));
		// 		memcpy(enc_rec_2, enc_rec_1, sizeof(enc_rec_1)); // Копируем данные из приемника в буффер для обработки.
		// 		memset(enc_rec_1, 0, sizeof(enc_rec_1));				  // Очищаем буфер приемник для новых данных
		
		//	memcpy(destination, source, sizeof(source));						// Копируем данные из приемника в буффер для обработки.


		// ищем время окончания горения дуги
		finding_arc_end();
		
		// >>>>>>>>>>>>>>>>		// Ф-ция вычисляет полное время выключателя:  !!!!!!!!!!!!!!!!!!!!!!!
		calcFullTime();
		
		
		// 13, 14, 15 - Время горения дуги во время последнего отключения, мс.
		calcArcLifetime(); // arc_lifetime[phase]

		calcSwitchCurrent();

		// Поиск короткого замыкания

		
		findShortCircuit();
		
		
		// 19, 20, 21 - Отработанный ресурс во время последнего отключения, кА^2 * с.
		calcLast_kA2s(); // last_kA2s[phase]
		
		calcTimeStates();
		
		// 10, 11, 12 - Суммарный отработанный электрический ресурс фазы, кА^2 * с.
		params.ph_A_kA2s = params.ph_A_kA2s + last_kA2s[PHASE_A];
		params.ph_B_kA2s = params.ph_B_kA2s + last_kA2s[PHASE_B];
		params.ph_C_kA2s = params.ph_C_kA2s + last_kA2s[PHASE_C];
		// 31, 32, 33 - Электрический износ за время последнего отключения, %.
		params.ph_A_last_elec_perc = last_kA2s[PHASE_A] * 100.0 / max_electrical_wear;
		params.ph_B_last_elec_perc = last_kA2s[PHASE_B] * 100.0 / max_electrical_wear;
		params.ph_C_last_elec_perc = last_kA2s[PHASE_C] * 100.0 / max_electrical_wear;
		// 28, 29, 30 - Суммарный электрический износ контактов фазы, %.
		params.ph_A_electrical_Perc = params.ph_A_electrical_Perc + params.ph_A_last_elec_perc;
		params.ph_B_electrical_Perc = params.ph_B_electrical_Perc + params.ph_B_last_elec_perc;
		params.ph_C_electrical_Perc = params.ph_C_electrical_Perc + params.ph_C_last_elec_perc;
		
		
		
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
		
		eeprom_write_word((uint16_t*)22, params.power_Off_cntr);
		calcSteps++;
		break;
		
		case 5:   // провести процессы медленной записи
		
		eeprom_write_word((uint16_t*)24, params.power_On_cntr);
		calcSteps++;
		break;
		
		
		
		case 6:   // провести процессы медленной записи
		
		calcSteps++;
		transferDataToLCP = 1;
		
		break;
		
		
		
		case 7:   // Поднять флаг передачи данных в LCP для последующей записи на карту SD осциллограмм с временем и датой
		
		// 		switch(lastOperationBuf[2])
		// 		{
		// 			case cb_On_Off:
		// 			{
		// 				if (CB_State1 != cb_Off)			// если концевик не соответствует последней операции - значит авария
		// 				bitWrite(complexSP,A_Fail,1);
		// 				else
		// 				bitWrite(complexSP,A_Fail,0);
		//
		// 				if (auxType != 1)
		// 				{
		// 					if (CB_State2 != cb_Off)			// если концевик не соответствует последней операции - значит авария
		// 					bitWrite(complexSP,B_Fail,1);
		// 					else
		// 					bitWrite(complexSP,B_Fail,0);
		// 				}
		//
		//
		// 				if (auxType != 1)
		// 				{
		// 					if (CB_State3 != cb_Off)			// если концевик не соответствует последней операции - значит авария
		// 					bitWrite(complexSP,C_Fail,1);
		// 					else
		// 					bitWrite(complexSP,C_Fail,0);
		// 				}
		//
		// 				if (qRMS_A_buf_ADC < params.minFailCurrent && qRMS_B_buf_ADC < params.minFailCurrent && qRMS_C_buf_ADC < params.minFailCurrent)
		// 				bitWrite(complexSP,part_Phase_Switch_Bit,0);
		// 				else
		// 				bitWrite(complexSP,part_Phase_Switch_Bit,1);
		//
		// 			}
		// 			break;
		//
		// 			case cb_Off_On:
		// 			{
		// 				if (CB_State1 != cb_On)			// если концевик не соответствует последней операции - значит авария
		// 				bitWrite(complexSP,A_Fail,1);
		// 				else
		// 				bitWrite(complexSP,A_Fail,0);
		//
		// 				if (auxType != 1)
		// 				{
		// 					if (CB_State2 != cb_On)			// если концевик не соответствует последней операции - значит авария
		// 					bitWrite(complexSP,B_Fail,1);
		// 					else
		// 					bitWrite(complexSP,B_Fail,0);
		// 				}
		//
		//
		// 				if (auxType != 1)
		// 				{
		// 					if (CB_State3 != cb_On)			// если концевик не соответствует последней операции - значит авария
		// 					bitWrite(complexSP,C_Fail,1);
		// 					else
		// 					bitWrite(complexSP,C_Fail,0);
		// 				}
		//
		//
		//
		// 				if (qRMS_A_buf_ADC > params.minFailCurrent && qRMS_B_buf_ADC > params.minFailCurrent && qRMS_C_buf_ADC > params.minFailCurrent)
		// 				bitWrite(complexSP,part_Phase_Switch_Bit,0);
		// 				else
		// 				bitWrite(complexSP,part_Phase_Switch_Bit,1);
		// 			}
		// 			break;
		// 		}
		
		calcSteps++;
		break;
		
		case 8:   // Поднять флаг передачи данных в LCP для последующей записи на карту SD осциллограмм с временем и датой
		
		if (transferDataToLCP == 0) // очищаем флаг новых данных в буфере
		{
			buffer_has_new_data = buffer_has_new_data - 1;	// последний буффер обработан - идем к предыдущему
			
			if (buffer_has_new_data == 1)
			{
				memcpy(cts.calcOscBuffer, cts.overflowOscBuffer, sizeof(cts.overflowOscBuffer)); // Если выясняется? что было 2 буффера одновременно, то мы копируем данные из буффера 1 (последнего буффера) в буффер 2 - буффер обработки
				shiftADC(); // Shift ADC to mid or 0
				lastOperationBuf[2] = lastOperationBuf[1];
				lastOperationTimeBuf[2] = lastOperationTimeBuf[1];
				// 			switchCurrent_Buf[PHASE_A][2] = switchCurrent_Buf[PHASE_A][1];
				// 			switchCurrent_Buf[PHASE_B][2] = switchCurrent_Buf[PHASE_B][1];
				// 			switchCurrent_Buf[PHASE_C][2] = switchCurrent_Buf[PHASE_C][1];
				// 			if ((buffer_has_new_data == 2) && (moveFRAM_Buffer == 1)) // Если у нас есть данные в буффере 2, которые мы скопировали выше, при этом поднят флаг что в FRAM тоже есть данные - значит возвращаем указатель на количество данных к обработке до 2
				// 			buffer_has_new_data = 2; // из запись FRAM снова пойдет
				
			}
			
			
			calcSteps++;
		}
		
		
		break;
		
		default:

		calcSteps = 0;    // когда все шаги пройдены - возвращаемся к нулевому шагу

		if (buffer_has_new_data) //  если в процессе мат обработки заполнился приемник новым событием, то
		startCalculations = true;		 // сразу стартуем еще одну мат обработку с копированием данных из премника в буфер.
		else
		startCalculations = false;

		break;
		
	}



}

// Ф-ция настраивает необходимые выводы МК на нужные режимы работы (вход или выход):
void gpio_init(void){


	DDR_485  |= (1<<PIN_RS_Enable);		// Pin is OUTPUT
	PORT_485 &= ~(1<< PIN_RS_Enable); 	// Pin is LOW (enable RX mode)

	// LEDs enable:
	DDR_LED |= (1<< LED_PLC_OK);		// Pins are OUTPUT
	DDR_LED |= (1<< LED_SD_OK);
	DDR_LED |= (1<< LED_X2X_OK);

	//	PORT_LED |= (1<< LED_PLC_OK);		// LED is ON
	//	PORT_LED |= (1<< LED_SD_OK);		// LED is ON

	// Digital Inputs (DI) enable:
	DDR_DI &= ~(1<<PIN_DI_1);			// Pins are INPUT
	DDR_DI &= ~(1<<PIN_DI_2);
	DDR_DI &= ~(1<<PIN_DI_3);
	DDR_DI &= ~(1<<PIN_DI_4);
	DDR_DI &= ~(1<<PIN_DI_5);
	DDR_DI &= ~(1<<PIN_DI_6);

	// Digital Inputs (DI) enable:
	PORT_DI |= (1<<PIN_DI_1);			// Pins are pulled-up
	PORT_DI |= (1<<PIN_DI_2);
	PORT_DI |= (1<<PIN_DI_3);
	PORT_DI |= (1<<PIN_DI_4);
	PORT_DI |= (1<<PIN_DI_5);
	PORT_DI |= (1<<PIN_DI_6);

	// Abs Encoder Inputs enable:
	DDR_ENC		= 0b00000000;			// Pins are INPUT
	PORT_ENC	= 0b00000000;			// Pins are PULL-UP
	DDRE &= ~(1<<PINE0);
	DDRE &= ~(1<<PINE1);
	PORTE &= ~(1<<PINE0);
	PORTE &= ~(1<<PINE1);

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
	if (currentTime - loop_500_start >= LOOP_TIME){
		loop_500_start = currentTime;
		if (currentTime - reset_wd_timer >= CONN_TIMEOUT) x2x_state = 0;	// Watchdog - таймаут по времени - нет входящих пакетов по Rs485
		if (x2x_state == 0) PORT_LED ^= (1<< LED_PLC_OK);					// Если входящих на адрес X2X нет, то мигаем X2X-светодиодом
	};
}

int64_t checkRMS = 0;

uint16_t ISR_us = OSC_REC_PERIOD;// * averPoints;
float sec2us = 1000.0/OSC_REC_PERIOD;

uint8_t testFRAM = 0;


// Выдача данных по протоколу Modbus:
void modbusProceed(int *regs, int addr_, int reg_qty_, int Value_)
{
	uint8_t	writeReg = 0;

	if (modbus.func == FC_WRITE_REGS || modbus.func == FC_WRITE_REG) writeReg = 1;

	if (x2x_state == 0){
		x2x_state = 1;					// Connection is established
		PORT_LED |= (1<< LED_PLC_OK);	// Turn corresponding LED on
	};

	reset_wd_timer = currentTime;

	for (uint8_t i = 0; i <= reg_qty_ - 1; i++){
		switch (addr_){

			case 0:  // 0 - Состояние концевиков вспомогательных контактов
			
			if (writeReg)
			{
				curr_state = Value_;
			}
			regs[i] = curr_state;
			break;
			
			
			case 1: // 1 - состояние контактов, ошибок, последней операции и неполнофазной работы.
			
			regs[i] = complexSP;
			break;
			
			
			// 7, 8, 9 - Действующее значение тока RMS, А.
			case 2:
			regs[i] = convertFloat(params.RMS_A,0);
			
			case 3:
			regs[i] = convertFloat(params.RMS_A,1);
			break;
			
			case 4:
			regs[i] = convertFloat(params.RMS_B,0);
			
			case 5:
			regs[i] = convertFloat(params.RMS_B,1);
			break;

			case 6:
			regs[i] = convertFloat(params.RMS_C,0);
			
			case 7:
			regs[i] = convertFloat(params.RMS_C,1);
			break;
			

			// 8-13 - Общий отработанный ресурс, кА^2 * с.
			
			case 8:
			regs[i] = convertFloat(params.ph_A_kA2s, 0);
			break;
			
			case 9:
			regs[i] = convertFloat(params.ph_A_kA2s, 1);
			break;

			case 10:
			regs[i] = convertFloat(params.ph_B_kA2s, 0);
			break;

			case 11:
			regs[i] = convertFloat(params.ph_B_kA2s, 1);
			break;

			case 12:
			regs[i] = convertFloat(params.ph_C_kA2s, 0);
			break;

			case 13:
			regs[i] = convertFloat(params.ph_C_kA2s, 1);
			break;


			// 14 - 19 - Время горения дуги во время последнего отключения, мкс.
			case 14:
			regs[i] = convertFloat(ISR_arc_lifetime[PHASE_A]*ISR_us, 0);
			break;

			case 15:
			regs[i] = convertFloat(ISR_arc_lifetime[PHASE_A]*ISR_us, 1);
			break;

			case 16:
			regs[i] = convertFloat(ISR_arc_lifetime[PHASE_B]*ISR_us, 0);
			break;

			case 17:
			regs[i] = convertFloat(ISR_arc_lifetime[PHASE_B]*ISR_us, 1);
			break;

			case 18:
			regs[i] = convertFloat(ISR_arc_lifetime[PHASE_C]*ISR_us, 0);
			break;

			case 19:
			regs[i] = convertFloat(ISR_arc_lifetime[PHASE_C]*ISR_us, 1);
			break;


			// 20 - 25  - Значение тока последнего отключения, А.
			
			case 20:
			regs[i] = convertFloat(switchCurrent_Amp[PHASE_A], 0);
			break;

			case 21:
			regs[i] = convertFloat(switchCurrent_Amp[PHASE_A], 1);
			break;

			case 22:
			regs[i] = convertFloat(switchCurrent_Amp[PHASE_B], 0);
			break;

			case 23:
			regs[i] = convertFloat(switchCurrent_Amp[PHASE_B], 1);
			break;

			case 24:
			regs[i] = convertFloat(switchCurrent_Amp[PHASE_C], 0);
			break;

			case 25:
			regs[i] = convertFloat(switchCurrent_Amp[PHASE_C], 1);
			break;
			
			

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
			regs[i] = convertFloat(params.power_On_cntr+params.power_Off_cntr, 0);
			break;
			
			case 33:
			regs[i] = convertFloat(params.power_On_cntr+params.power_Off_cntr, 1);
			break;
			
			case 34:
			regs[i] = convertFloat(params.power_On_cntr+params.power_Off_cntr, 0);
			break;
			
			case 35:
			regs[i] = convertFloat(params.power_On_cntr+params.power_Off_cntr, 1);
			break;
			
			case 36:
			regs[i] = convertFloat(params.power_On_cntr+params.power_Off_cntr, 0);
			break;
			
			case 37:
			regs[i] = convertFloat(params.power_On_cntr+params.power_Off_cntr, 1);
			break;
			
			
			// 38 - 43 - Механический износ контактов, %.!
			case 38:
			regs[i] = convertFloat(params.mechanical_wear_Perc, 0);
			break;
			
			case 39:
			regs[i] = convertFloat(params.mechanical_wear_Perc, 1);
			break;
			
			case 40:
			regs[i] = convertFloat(params.mechanical_wear_Perc, 0);
			break;
			
			case 41:
			regs[i] = convertFloat(params.mechanical_wear_Perc, 1);
			break;
			
			case 42:
			regs[i] = convertFloat(params.mechanical_wear_Perc, 0);
			break;
			
			case 43:
			regs[i] = convertFloat(params.mechanical_wear_Perc, 1);
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
			

			case 56:
			regs[i] = convertFloat(full_off_time_ISR[PHASE_A]*ISR_us, 0);
			break;
			
			case 57:
			regs[i] = convertFloat(full_off_time_ISR[PHASE_A]*ISR_us, 1);
			break;
			
			case 58:
			regs[i] = convertFloat(full_on_time_ISR[PHASE_A]*ISR_us, 0);
			break;
			
			case 59:
			regs[i] = convertFloat(full_on_time_ISR[PHASE_A]*ISR_us, 1);
			break;
			
			case 60:
			regs[i] = convertFloat(full_off_time_ISR[PHASE_B]*ISR_us, 0);
			break;
			
			case 61:
			regs[i] = convertFloat(full_off_time_ISR[PHASE_B]*ISR_us, 1);
			break;
			

			case 62:
			regs[i] = convertFloat(full_on_time_ISR[PHASE_B]*ISR_us, 0);
			break;
			
			case 63:
			regs[i] = convertFloat(full_on_time_ISR[PHASE_B]*ISR_us, 1);
			break;
			
			case 64:
			regs[i] = convertFloat(full_off_time_ISR[PHASE_C]*ISR_us, 0);
			break;
			
			case 65:
			regs[i] = convertFloat(full_off_time_ISR[PHASE_C]*ISR_us, 1);
			break;
			
			case 66:
			regs[i] = convertFloat(full_on_time_ISR[PHASE_C]*ISR_us, 0);
			break;
			
			case 67:
			regs[i] = convertFloat(full_on_time_ISR[PHASE_C]*ISR_us, 1);
			break;
			
			// Температура

			case 68:
			regs[i] = convertFloat(0, 0);
			break;
			
			case 69:
			regs[i] = convertFloat(0, 1);
			break;
			
			
			// количество включений при КЗ

			case 70:
			regs[i] = convertFloat(params.short_Cnt_On, 0);
			break;
			
			case 71:
			regs[i] = convertFloat(params.short_Cnt_On, 1);
			break;
			
			// количество выключений при КЗ

			case 72:
			regs[i] = convertFloat(params.short_Cnt_Off, 0);
			break;
			
			case 73:
			regs[i] = convertFloat(params.short_Cnt_Off, 1);
			break;
			
			// количество включений

			case 74:
			regs[i] = convertFloat(params.power_On_cntr, 0);
			break;
			
			case 75:
			regs[i] = convertFloat(params.power_On_cntr, 1);
			break;
			
			//	количество отключений
			
			case 76:
			regs[i] = convertFloat(params.power_Off_cntr, 0);
			break;
			
			case 77:
			regs[i] = convertFloat(params.power_Off_cntr, 1);
			break;

			//	Собственное время отключения ВВ превысило уставку
			
			

			case 78:
			regs[i] = convertFloat(1, 0);
			break;
			
			case 79:
			regs[i] = convertFloat(1, 1);
			break;
			
			//	Собственное время включения ВВ превысило уставку
			case 80:
			regs[i] = convertFloat(1, 0);
			break;
			
			case 81:
			regs[i] = convertFloat(1, 1);
			break;
			
			
			//	Полное время отключения ВВ превысило уставку
			
			case 82:
			regs[i] = convertFloat(1, 0);
			break;
			
			case 83:
			regs[i] = convertFloat(1, 1);
			break;
			
			//	Полное время отключения ВВ превысило уставку

			case 84:
			regs[i] = convertFloat(1, 0);
			break;
			
			case 85:
			regs[i] = convertFloat(1, 1);
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
			
			case 184:
			regs[i] = convertFloat(full_off_time_ISR[PHASE_A], 0);
			break;
			
			case 185:
			regs[i] = convertFloat(full_off_time_ISR[PHASE_A], 1);
			break;
			
			case 186:
			regs[i] = convertFloat(full_on_time_ISR[PHASE_A], 0);
			break;
			
			case 187:
			regs[i] = convertFloat(full_on_time_ISR[PHASE_A], 1);
			break;
			
			case 188:
			regs[i] = convertFloat(full_off_time_ISR[PHASE_B], 0);
			break;
			
			case 189:
			regs[i] = convertFloat(full_off_time_ISR[PHASE_B], 1);
			break;
			

			case 190:
			regs[i] = convertFloat(full_on_time_ISR[PHASE_B], 0);
			break;
			
			case 191:
			regs[i] = convertFloat(full_on_time_ISR[PHASE_B], 1);
			break;
			
			case 192:
			regs[i] = convertFloat(full_off_time_ISR[PHASE_C], 0);
			break;
			
			case 193:
			regs[i] = convertFloat(full_off_time_ISR[PHASE_C], 1);
			break;
			
			case 194:
			regs[i] = convertFloat(full_on_time_ISR[PHASE_C], 0);
			break;
			
			case 195:
			regs[i] = convertFloat(full_on_time_ISR[PHASE_C], 1);
			break;
			
			
			

			/*
			case :
			regs[i] = ;
			break;
			*/


			case 199:
			{
				regs[i] = 200; // Time
			}
			break;
			
			// ----- snb c0d3 b3g1n:
			// Buffer 1 begin:
			case 200 ... 395:	// Осциллограммы фазы 'A'
			regs[i] = cts.calcOscBuffer[addr_ - 200][PHASE_A] * k_current[PHASE_A];
			break;
			
			case 400 ... 595:	// Осциллограммы фазы 'B'
			regs[i] = cts.calcOscBuffer[addr_ - 400][PHASE_B] * k_current[PHASE_B];
			break;
			
			case 600 ... 795:	// Осциллограммы фазы 'C'
			regs[i] = cts.calcOscBuffer[addr_ - 600][PHASE_C] * k_current[PHASE_C];
			break;
			
			
			case 800:	// Осциллограммы фазы 'C'
			regs[i] = convertFloat(k_current[PHASE_A],0);
			break;
			
			case 801:	// Осциллограммы фазы 'C'
			regs[i] = convertFloat(k_current[PHASE_A],1);
			break;
			
			
			case 802:	// Осциллограммы фазы 'C'
			regs[i] = convertFloat(k_current[PHASE_B],0);
			break;
			
			case 803:	// Осциллограммы фазы 'C'
			regs[i] = convertFloat(k_current[PHASE_B],1);
			break;
			
			case 804:	// Осциллограммы фазы 'C'
			regs[i] = convertFloat(k_current[PHASE_C],0);
			break;
			
			case 805:	// Осциллограммы фазы 'C'
			regs[i] = convertFloat(k_current[PHASE_C],1);
			break;
			
			
			
			
			
			
			case 850:	// Если в регистре 1 - есть новые данные для считывания. Если записать 0 в регистр, то при наличии ожидающего входного буффера - буффер будет отправлен в обработку.
			
			if (writeReg)
			transferDataToLCP = Value_;
			
			regs[i] = transferDataToLCP;
			break;
			
			case 851:	// Если в регистре 1 - есть новые данные для считывания. Если записать 0 в регистр, то при наличии ожидающего входного буффера - буффер будет отправлен в обработку.
			
			regs[i] = buffer_has_new_data;
			break;
			
			
			case 852:	// Если в регистре 1 - есть новые данные для считывания. Если записать 0 в регистр, то при наличии ожидающего входного буффера - буффер будет отправлен в обработку.
			
			if (writeReg)
			shootBreakerFlag = Value_;
			regs[i] = shootBreakerFlag;
			
			break;
			
			case 853:	// Если в регистре 1 - есть новые данные для считывания. Если записать 0 в регистр, то при наличии ожидающего входного буффера - буффер будет отправлен в обработку.
			
			if (writeReg)
			manualMode = Value_;
			regs[i] = manualMode;
			
			break;
			
			case 854:	// Если в регистре 1 - есть новые данные для считывания. Если записать 0 в регистр, то при наличии ожидающего входного буффера - буффер будет отправлен в обработку.
			
			regs[i] = GLOBAL_MODE;
			break;
			

			case 855:	// Если в регистре 1 - есть новые данные для считывания. Если записать 0 в регистр, то при наличии ожидающего входного буффера - буффер будет отправлен в обработку.
			
			if (writeReg)
			BLOCK_GAIN = Value_;
			
			regs[i] = BLOCK_GAIN;
			break;
			
			
			
			case 890 ... 899:
			regs[i] = addr_ + 100;
			break;
			
			
			case 900:
			
			if (writeReg)
			{
				rebootLCT = currentTime;
				regs[i] = 1;
			}
			else
			regs[i] = 0;
			
			break;
			
			case 990:
			
			regs[i] = CB_State1;
			
			break;
			
			case 991:
			
			regs[i] = CB_State2;
			
			break;
			
			case 992:
			
			regs[i] = CB_State3;
			
			break;
			
			case 993:
			
			regs[i] = lastOperation;
			
			break;
			
			
			case 994:
			
			regs[i] = osc_Point;
			break;

			case 995:
			
			regs[i] = lastOperationTime;
			break;


			
			
			
			
			case 996:
			
			regs[i] = moveFRAM_Buffer;  // Heart Bit
			
			break;
			
			case 997:
			
			regs[i] = strikeLostData;  // Heart Bit
			
			break;
			
			case 998:
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
			
			// Общий отработанный ресурс, кА^2 * с.
			
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
			
			
			case 1002:
			
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

			//  - Количество включений
			case 1013:
			if (writeReg)
			{
				params.power_Off_cntr = Value_;
				eeprom_write_word((uint16_t*)22, params.power_Off_cntr);
			}
			regs[i] = params.power_Off_cntr;
			
			break;
			
			//  - Количество включений
			case 1014:
			if (writeReg)
			{
				params.power_On_cntr = Value_;
				eeprom_write_word((uint16_t*)24, params.power_On_cntr);
			}
			regs[i] = params.power_On_cntr;
			
			break;
			
			
			
			
			case 1016:
			if (writeReg)
			{
				max_electrical_wear = Value_;
				eeprom_write_word((uint16_t*)28, max_electrical_wear);
			}
			regs[i] = max_electrical_wear;
			
			break;
			
			case 1017:
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
				eeprom_write_byte((uint8_t*)46,(Value_ >> 8));
				eeprom_write_byte((uint8_t*)47,(Value_ & 0x00FF));
			}
			
			regs[i] = convertFloat(k_current[PHASE_A], 0);
			
			break;
			
			case 1028:
			
			if (writeReg)
			{
				eeprom_write_byte((uint8_t*)44,(Value_ >> 8));
				eeprom_write_byte((uint8_t*)45,(Value_ & 0x00FF));
				k_current[PHASE_A] = loadFloat(44, 1.0);
			}
			
			regs[i] = convertFloat(k_current[PHASE_A], 1);
			
			break;
			
			
			case 1029:
			
			if (writeReg)
			{
				eeprom_write_byte((uint8_t*)50,(Value_ >> 8));
				eeprom_write_byte((uint8_t*)51,(Value_ & 0x00FF));
			}
			
			regs[i] = convertFloat(k_current[PHASE_B], 0);
			
			break;
			
			case 1030:
			
			if (writeReg)
			{
				eeprom_write_byte((uint8_t*)48,(Value_ >> 8));
				eeprom_write_byte((uint8_t*)49,(Value_ & 0x00FF));
				k_current[PHASE_B] = loadFloat(48, 1.0);
			}
			
			regs[i] = convertFloat(k_current[PHASE_B], 1);
			
			break;
			
			
			
			case 1031:
			
			if (writeReg)
			{
				eeprom_write_byte((uint8_t*)54,(Value_ >> 8));
				eeprom_write_byte((uint8_t*)55,(Value_ & 0x00FF));
			}
			
			regs[i] = convertFloat(k_current[PHASE_C], 0);
			
			break;
			
			case 1032:
			
			if (writeReg)
			{
				eeprom_write_byte((uint8_t*)52,(Value_ >> 8));
				eeprom_write_byte((uint8_t*)53,(Value_ & 0x00FF));
				k_current[PHASE_C] = loadFloat(52, 1.0);
			}
			
			regs[i] = convertFloat(k_current[PHASE_C], 1);
			
			break;
			
			
			
			
			case 1033:
			
			if (writeReg)
			{
				eeprom_write_byte((uint8_t*)58,(Value_ >> 8));
				eeprom_write_byte((uint8_t*)59,(Value_ & 0x00FF));
			}
			
			regs[i] = convertFloat(params.ph_A_Gain_k_current, 0);
			
			break;
			
			case 1034:
			
			if (writeReg)
			{
				eeprom_write_byte((uint8_t*)56,(Value_ >> 8));
				eeprom_write_byte((uint8_t*)57,(Value_ & 0x00FF));
				params.ph_A_Gain_k_current = loadFloat(56, 1.0);
			}
			
			regs[i] = convertFloat(params.ph_A_Gain_k_current, 1);
			
			break;
			
			
			case 1035:
			
			if (writeReg)
			{
				eeprom_write_byte((uint8_t*)62,(Value_ >> 8));
				eeprom_write_byte((uint8_t*)63,(Value_ & 0x00FF));
			}
			
			regs[i] = convertFloat(params.ph_B_Gain_k_current, 0);
			
			break;
			
			case 1036:
			
			if (writeReg)
			{
				eeprom_write_byte((uint8_t*)60,(Value_ >> 8));
				eeprom_write_byte((uint8_t*)61,(Value_ & 0x00FF));
				params.ph_B_Gain_k_current = loadFloat(60, 1.0);
			}
			
			regs[i] = convertFloat(params.ph_B_Gain_k_current, 1);
			
			break;
			
			
			
			case 1037:
			
			if (writeReg)
			{
				eeprom_write_byte((uint8_t*)66,(Value_ >> 8));
				eeprom_write_byte((uint8_t*)67,(Value_ & 0x00FF));
			}
			
			regs[i] = convertFloat(params.ph_C_Gain_k_current, 0);
			
			break;
			
			case 1038:
			
			if (writeReg)
			{
				eeprom_write_byte((uint8_t*)64,(Value_ >> 8));
				eeprom_write_byte((uint8_t*)65,(Value_ & 0x00FF));
				params.ph_C_Gain_k_current = loadFloat(64, 1.0);
			}
			
			regs[i] = convertFloat(params.ph_C_Gain_k_current, 1);
			
			break;
			
			
			case 1040:
			
			if (writeReg)
			{
				params.short_Cnt_Off = Value_;
				eeprom_write_word((uint16_t*)76,params.short_Cnt_Off);
			}
			
			regs[i] = params.short_Cnt_Off;
			
			break;
			
			
			case 1041:
			
			if (writeReg)
			{
				params.short_Cnt_On = Value_;
				eeprom_write_word((uint16_t*)78,params.short_Cnt_On);
			}
			
			regs[i] = params.short_Cnt_On;
			
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
			
			case 1046:
			if (writeReg)
			{
				eeprom_write_word((uint16_t*)82,Value_);
				fullTimeOffWarn_ISR = Value_;
			}
			regs[i] = fullTimeOffWarn_ISR;
			break;
			
			
			case 1047:
			if (writeReg)
			{
				eeprom_write_word((uint16_t*)84,Value_);
				fullTimeOffAlarm_ISR = Value_;
			}
			regs[i] = fullTimeOffAlarm_ISR;
			break;
			
			// 			case 1048:
			// 			if (writeReg)
			// 			{
			// 				eeprom_write_word((uint16_t*)86,Value_);
			// 				ownTimeOffWarn_ISR = Value_;
			// 			}
			// 			regs[i] = ownTimeOffWarn_ISR;
			// 			break;
			
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
					// 					set_pin_high(PORT_ADC_CT1_GAIN, ADC_CT1_GAIN);
					// 					set_pin_high(PORT_ADC_CT2_GAIN, ADC_CT2_GAIN);
					// 					set_pin_high(PORT_ADC_CT3_GAIN, ADC_CT3_GAIN);
					cts.amplification = 1;
				}
				else
				{
					// 					set_pin_low(PORT_ADC_CT1_GAIN, ADC_CT1_GAIN);
					// 					set_pin_low(PORT_ADC_CT2_GAIN, ADC_CT2_GAIN);
					// 					set_pin_low(PORT_ADC_CT3_GAIN, ADC_CT3_GAIN);
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
				AutoSwitchTime_Delay = (uint16_t)Value_ / OSC_REC_PERIOD;
			}
			regs[i] = AutoSwitchTime_Delay * OSC_REC_PERIOD;
			break;
			
			case 1069 ... 1071:
			
			regs[i] = arcDetected[addr_ - 1069];
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

			case 1500:	// Осциллограммы фазы 'A'
			regs[i] = cts.overflowOscBuffer[0][PHASE_A];
			break;
			
			case 1501:	// Осциллограммы фазы 'A'
			regs[i] = cts.overflowOscBuffer[0][PHASE_B];
			break;
			
			case 1502:	// Осциллограммы фазы 'A'
			regs[i] = cts.overflowOscBuffer[0][PHASE_C];
			break;
			


			// Buffer 2 begin:
			case 1600 ... 1699:	// Осциллограммы фазы 'A'
			regs[i] = cts.overflowOscBuffer[addr_ - 1600][PHASE_A];
			break;
			case 1700 ... 1799:	// Осциллограммы фазы 'B'
			regs[i] = cts.overflowOscBuffer[addr_ - 1700][PHASE_B];
			break;
			case 1800 ... 1899:	// Осциллограммы фазы 'C'
			regs[i] = cts.overflowOscBuffer[addr_ - 1800][PHASE_C];
			break;
			
			// Buffer 2 end.

			case 1910:			// Номер активного в данный момент буфера
			regs[i] = cts.active_buffer_id;
			break;

			case 1911:			// Очистка всех буферов, сброс флагов и т.д.
			if (writeReg)
			{
				cts.flush_buffer((uint8_t *) cts.overflowOscBuffer, sizeof(cts.overflowOscBuffer));
				cts.flush_buffer((uint8_t *) cts.calcOscBuffer, sizeof(cts.calcOscBuffer));
				
				// 				cts.adc_switch[ADC1] = ON;
				// 				cts.adc_switch[ADC2] = OFF;
				// 				cts.adc_switch[ADC3] = ON;
				// 				cts.adc_switch[ADC4] = OFF;
				// 				cts.adc_switch[ADC5] = ON;
				// 				cts.adc_switch[ADC6] = OFF;
				cts.active_buffer_id = FIRST;
				cts.recording_status = INPROGRESS;
				GLOBAL_MODE = IDLE;
				// 				PORT_LED &= ~(1 << LED_PLC_OK);
				// 				PORT_LED &= ~(1 << LED_SD_OK);
				phase_zero_1[PHASE_A] = 0;
				phase_zero_1[PHASE_B] = 0;
				phase_zero_1[PHASE_C] = 0;
				ISR_arcPoint[PHASE_A] = 0;
				ISR_arcPoint[PHASE_B] = 0;
				ISR_arcPoint[PHASE_C] = 0;
				//	zero_is_found = false;
			}
			else
			regs[i] = 0xFF;
			
			break;

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
			
			
			case 1929 ... 1940:
			if (writeReg)
			{
				tmp[addr_-1929] = Value_;
			}
			regs[i] = tmp[addr_-1929];
			break;
			
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
			case 2000 ... 6800:
			
			regs[i] = fram_read_byte(addr_-2000);
			
			break;
			
			
			
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



void isrMeasurements()
{
	if (strikeData == runProc)
	{

		//	if (GLOBAL_MODE != BUSY)
		mechanical_wear_Perc_counting();
		
		timer_routines(cts.overflowOscBuffer);			// заполняем его значениями,
		
		strikeData = procDone;
	}

	
}

uint8_t triggerRMScalc = 0;
uint8_t triggerRMSstep = 0;

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
				
				if (cts.amplification == 1)
				{
					params.RMS_A = filteredRMS[PHASE_A] * params.ph_A_Gain_k_current;
					params.RMS_B = filteredRMS[PHASE_B] * params.ph_B_Gain_k_current;
					params.RMS_C = filteredRMS[PHASE_C] * params.ph_C_Gain_k_current;
				}
				else
				{
					params.RMS_A = filteredRMS[PHASE_A] * k_current[PHASE_A];
					params.RMS_B = filteredRMS[PHASE_B] * k_current[PHASE_B];
					params.RMS_C = filteredRMS[PHASE_C] * k_current[PHASE_C];
				}
				triggerRMSstep = 0;
				triggerRMScalc = 0;
			}
			break;
		}
	}
	
	
	
}




ISR(TIMER2_COMP_vect)
{
	//	micro40timer++;  // Every 40 ms ISR interrupt occur
	//	microtimer++;
	// 	if (microtimer >= 25000)
	// 	{
	// 		micro200timer++;
	// 		microtimer = 0;
	// 	}

	ISR_microTimer++;

	if (strikeData == procDone)
	{
		strikeData = runProc;
	}
	else
	{
		if (GLOBAL_MODE == BUSY)
		strikeLostData++;
	}
}


// Ф-ция отслеживает значения с АЦП трансформаторов тока и дискретных входов.
// В нормальном режиме - раз в 500 мс, в режиме записи - каждые 200 мкс до заполнения буфера.
// Переключение производится по состоянию дискретных входов:




void timer_routines(int16_t osc_buffer[][phase_quantity])
{
	//static	uint8_t	idle_ct_meas_period	= 0;		// Счётчик кол-ва замеров тока в дежурном режиме. Нужен для отсчёта периода в одну длину волны (20 мс при 50 Гц).

	//	enc_abs_status = (PINA & 0b11000000)<<8 | PINC;
	
	if (positionChanged)	// Если положение выключателя изменилось
	{
		Serial1.end();
		// переключаемся в режим "ЗАНЯТО",
		cts.recording_status = INPROGRESS;									// устанавливаем статус записи осциллограмм "В ПРОЦЕССЕ",
		positionChanged = 0;
		encCounter = 0;		 // обнуляем координату Х энкодера
		encCallCounter = ISR_ENCODER_FREQ;  // обнуляем счетчик частоты вызовов энкодера
		totalSwitchTimer = currentTime;
		ISR_microTimer = 0;
		AutoSwitchTime = 0;
		shootBreakerFlag = 3;
		GLOBAL_MODE = BUSY;
		//	DI_prev_state = DI_curr_state;
	};
	

	switch (GLOBAL_MODE)
	{
		case FULL:																	// Если режим - "ЗАПОЛНЕНО", то
		break;																	// выходим из процедуры.

		// DI @ 200 us + CTx3 @ 2 ms:
		case IDLE:
		{																	// Если режим - дежурный, то
			timer.isr_call_counter++;
			// увеличиваем счётчик вызовов.
			
			
			if (timer.isr_call_counter == ISR_CT_FREQ)	// Если пришло время обновления данных в дежурном режиме, то
			{
				
				timer.isr_call_counter = 0;											// сбрасываем счётчик вызовов,
				micro40timer = 0;
				
				cts.get_ct_values_in_idle_mode(osc_buffer);							// читаем данные с ТТ

				// Ищем максимальное значение тока и делим его на корень из двух (округлено до 1.41):
				switch (++idle_ct_meas_period)											// Увеличиваем счётчик.
				{
					//case 2000:															// Если достигли количества отсчетов, то
					case 2000:
					{
						idle_ct_meas_period	= 0;								// обнуляем счётчик,
						
						getRMS(osc_buffer, getRMSvalues);
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
						getRMS(osc_buffer, collectRMSvalues);
						//quickRMS(osc_buffer);
						
						
						
						/*
						if (osc_buffer[0][PHASE_A] > current_ph_A){						// Если текущее значение больше предыдущего, то
						current_ph_A = osc_buffer[0][PHASE_A];						// принимаем текущее за максимум.
						};
						if (osc_buffer[0][PHASE_B] > current_ph_B){						// То же самое для фазы B и...
						current_ph_B = osc_buffer[0][PHASE_B];
						};
						if (osc_buffer[0][PHASE_C] > current_ph_C)						// фазы C.
						{
						current_ph_C = osc_buffer[0][PHASE_C];
						};
						*/
					}
					break;
				};
			};

			
			// Проверяем дискретные входы. Реагируем только на нисходящие фронты:
			//	DI_curr_state = check_DI_status();										// Получаем текущий статус дискретных входов.
			//	if ((DI_prev_state == OFF) && (DI_curr_state == ON)){					// Если поймали восходящий фронт, то
			
			// Запоминаем текущее значение дискретных входов.
		}
		break;

		case BUSY:	// Fill the buffer: 3 x CT @ 200 us x 100 points				// Если режим - "ЗАНЯТО", то
		{
			if (cts.recording_status == INPROGRESS)
			cts.fill_ct_buffer_completely(osc_buffer, BLOCK_GAIN);		// заполняем буфер осциллограмм значениями.
			
			
			// Если выключатель включается, то ограничиваем осциллограмму концевиком включения, для корректного расчета дуги
			
			if (lastOperation == cb_Off_On && off_on_Captured == 0)  // идет включение
			{
				if (CB_State1 == cb_On) // Выключатель включился
				{
					//cts.recording_status = COMPLETE;
					lastOperationTimeBuf[1] = ISR_microTimer;
					
					//	full_on_time_ISR[PHASE_A] = ISR_microTimer; // / 3; //calcOwnTime(ISR_Timer_A);
					
					// 					if (auxType == 1)
					// 					{
					// 						full_on_time_ISR[PHASE_B] = full_on_time_ISR[PHASE_A];
					// 						full_on_time_ISR[PHASE_C] = full_on_time_ISR[PHASE_A];
					// 					}
					//
					off_on_Captured = 2;
				}
			}
			
			if (cts.recording_status == COMPLETE || (currentTime - totalSwitchTimer > 100))		// Если буфер заполнен полностью, то
			{
				
				GLOBAL_MODE = IDLE;
				off_on_Captured = 0;
				cts.recording_status = INPROGRESS;
				// переключаемся в дежурный режим и
				timer.isr_call_counter = 0;		// сбрасываем счётчик вызовов
				
				lastOperationBuf[1] = lastOperation;
				
				moveFRAM_Buffer = 1;       // Зажигаем флаг обработки
				// В буффере есть новые данные
				if (buffer_has_new_data < 2)
				buffer_has_new_data ++;
				
				checkSwitchFin(); // проверка на корректность операции. если это ошибочная операция - исключить расчеты. все приравнять к нулю
				
				updateSwitchCnt();
				
				checkSwitchRule();
				
				cts.adc_switch[ADC1] = ON;		// Вернуть настройки ADC из режима замыкания, если был, в обычный режим.
				cts.adc_switch[ADC2] = OFF;
				cts.adc_switch[ADC3] = ON;
				cts.adc_switch[ADC4] = OFF;
				cts.adc_switch[ADC5] = ON;
				cts.adc_switch[ADC6] = OFF;
				
				Serial1.begin(9600, SERIAL_8N1);
				

			}
		}
		break;
		
		default:
		break;
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



void updateSwitchCnt()
{
	// Механический счетчик включений
	if (lastOperation == cb_Off_On)
	{
		params.power_On_cntr++;
	}
	
	// Механический счетчик выключений
	if (lastOperation == cb_On_Off)
	{
		params.power_Off_cntr++;
	}
	
	params.mechanical_wear_Perc = 1.0 * (params.power_On_cntr + params.power_Off_cntr) * 50.0 / max_mechanical_wear;
	
}

uint8_t checkSwitchFin()
{
	
	switch(lastOperation)
	{
		case cb_On_Off:
		{
			if (auxType == 1)
			{
				if (CB_State1 == cb_Off)			// если концевик не соответствует последней операции - значит авария
				{
					bitWrite(complexSP,part_Phase_Switch_Bit,1);
					return 1;
				}
				else
				{
					bitWrite(complexSP,part_Phase_Switch_Bit,0);
					return 0;
				}
				
			}
			else
			{
				if (CB_State1 == cb_Off && CB_State2 == cb_Off && CB_State3 == cb_Off)
				{
					bitWrite(complexSP,part_Phase_Switch_Bit,1);
					return 1;
				}
				else
				{
					bitWrite(complexSP,part_Phase_Switch_Bit,0);
					return 0;
				}
			}
		}

		break;
		
		case cb_Off_On:
		{
			if (auxType == 1)
			{
				if (CB_State1 == cb_On)			// если концевик не соответствует последней операции - значит авария
				{
					bitWrite(complexSP,part_Phase_Switch_Bit,1);
					return 1;
				}
				else
				{
					bitWrite(complexSP,part_Phase_Switch_Bit,0);
					return 0;
				}
			}
			else
			{
				if (CB_State1 == cb_On && CB_State2 == cb_On && CB_State3 == cb_On)
				{
					bitWrite(complexSP,part_Phase_Switch_Bit,1);
					return 1;
				}
				else
				{
					bitWrite(complexSP,part_Phase_Switch_Bit,0);
					return 0;
				}
			}
		}
		break;
		
	}
	return 0;
}



void checkSwitchRule()
{
	if (lastOperationBuf[1] == cb_Off_On)		// Если время буфферизации измерений закончилось, а концевик все еще не сработал
	{
		if (CB_State1 != cb_On)
		full_on_time_ISR[PHASE_A] = 0;	// берем время окончания буфферизации. В новой версии переделать.
		
		if (auxType != 1)
		{
			if (CB_State2 != cb_On)
			full_on_time_ISR[PHASE_C] = 0;
			
			if (CB_State3 != cb_On)
			full_on_time_ISR[PHASE_C] = 0;
		}

	}
	else
	{
		if (CB_State1 != cb_Off)
		full_off_time_ISR[PHASE_A] = 0;	// берем время окончания буфферизации. В новой версии переделать.
		
		if (auxType != 1)
		{
			if (CB_State2 != cb_Off)
			full_off_time_ISR[PHASE_C] = 0;
			
			if (CB_State3 != cb_Off)
			full_off_time_ISR[PHASE_C] = 0;
		}
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




// Ф-ция отслеживает переключание контакта из ON в OFF и в этом случае увеличивает счётчик операций с фазой:
void mechanical_wear_Perc_counting(void){
	//static	uint8_t prev_state;		// Состояние контактов при предыдущем опросе
	//	uint8_t curr_state;		// Текущее состояние контактов


	if (manualMode == 0)
	{
		curr_state = (~PIN_DI) & 0b00111111;			// Поскольку выходы - PULLED UP, то инвертируем состояние выключателей
		
		if (auxType == 1)
		{
			bitWrite(curr_state, 2, bitRead(curr_state, 0));  // в режиме одного концевика - дублируем первый концевик на остальные два
			bitWrite(curr_state, 3, bitRead(curr_state, 1));  // в режиме одного концевика - дублируем первый концевик на остальные два
			bitWrite(curr_state, 4, bitRead(curr_state, 0));  // в режиме одного концевика - дублируем первый концевик на остальные два
			bitWrite(curr_state, 5, bitRead(curr_state, 1));  // в режиме одного концевика - дублируем первый концевик на остальные два
		}
	}
	else
	{
		// Start Automatic Test
		if (shootBreakerFlag == 3)
		if (((ISR_microTimer - AutoSwitchTime) > AutoSwitchTime_Delay) && curr_state == 0)
		{
			if (lastOperation == cb_Off_On)
			curr_state = CB1_ON;
			else
			curr_state = CB1_OFF;
			
			if (auxType == 1)
			{
				bitWrite(curr_state, 2, bitRead(curr_state, 0));  // в режиме одного концевика - дублируем первый концевик на остальные два
				bitWrite(curr_state, 3, bitRead(curr_state, 1));  // в режиме одного концевика - дублируем первый концевик на остальные два
				bitWrite(curr_state, 4, bitRead(curr_state, 0));  // в режиме одного концевика - дублируем первый концевик на остальные два
				bitWrite(curr_state, 5, bitRead(curr_state, 1));  // в режиме одного концевика - дублируем первый концевик на остальные два
			}
			
			shootBreakerFlag = 0;
		}
		
		if (shootBreakerFlag == 1)
		{
			shootBreakerFlag = 2;
			curr_state = 0;
		}
		
		
		
		
		// FIN Automatic Test
	}
	

	if ((ISR_microTimer - switchBounceTime) > switchBounceSilense)
	switchBlocked = false;
	
	if (switchBlocked == false) // дребезг контактов
	{
		if (curr_state != prev_state)
		{
			CB_State1 = getBreakerState(curr_state, CB1_ON, CB1_OFF, CB_State1);
			// 	CB_State2 = getBreakerState(curr_state, prev_state, CB2_ON, CB2_OFF);
			// 	CB_State3 = getBreakerState(curr_state, prev_state, CB3_ON, CB3_OFF);
			prev_state = curr_state;
		}
	}

	
	
	
	/*
	// Включение 1 Старт ++++++++++++++++++++++++
	//	if (switchBlocked == false)
	if (((curr_state & CB1_OFF) == OFF) && ((prev_state & CB1_OFF) == CB1_OFF) && (CB_State1 != cb_Off_On)) // Выключатель включается
	{	// Если поймали спад, то
	CB_State1 = cb_Off_On;  // Выключатель на пути к включению
	//		updContacts_ComplexSP(); //обновляем состояние вспомогательного блока контактов в комплексной переменной complexSP
	lastOperation = cb_Off_On;
	positionChanged = true;	// Выключатель начал движение - флаг для включения расчетов
	//ISR_Timer_A = ISR_microTimer;
	};
	
	if (((curr_state & CB1_ON) != OFF) && ((CB_State1 == cb_Off_On) || (CB_State1 == cb_NA))) // Выключатель включился
	{
	CB_State1 = cb_On;
	//		updContacts_ComplexSP(); //обновляем состояние вспомогательного блока контактов в комплексной переменной complexSP
	//own_on_time_X[PHASE_A] = ISR_microTimer; //calcOwnTime(ISR_Timer_A);
	//		switchCoolDownStartTime = currentTime;
	//	switchBlocked = true;
	}
	// Включение 1 Фин

	
	// Выключение 1 Старт
	//	if (switchBlocked == false)
	if (((curr_state & CB1_ON) == OFF) && ((prev_state & CB1_ON) != OFF) && (CB_State1 != cb_On_Off))  // Выключатель выключается
	{
	CB_State1 = cb_On_Off;  // Выключатель на пути к включению
	//		updContacts_ComplexSP(); //обновляем состояние вспомогательного блока контактов в комплексной переменной complexSP
	lastOperation = cb_On_Off;
	positionChanged = true;	// Выключатель начал движение - флаг для включения расчетов
	//ISR_Timer_A = ISR_microTimer;
	};
	
	if (((curr_state & CB1_OFF) != OFF)  && ((CB_State1 == cb_On_Off) || (CB_State1 == cb_NA))) // Выключатель выключился
	{
	own_off_time_X[PHASE_A] = ISR_microTimer; //calcOwnTime(ISR_Timer_A);
	CB_State1 = cb_Off;
	//	updContacts_ComplexSP(); //обновляем состояние вспомогательного блока контактов в комплексной переменной complexSP
	//		switchCoolDownStartTime = currentTime;
	//		switchBlocked = true;
	}
	
	
	// Выключение 1 Фин +++++++++++++++++++++++++++++++
	
	if (auxType != 1)
	{
	// Включение 2 Старт
	//		if (switchBlocked == false)
	if (((curr_state & CB2_OFF) == OFF) && ((prev_state & CB2_OFF) != OFF) && (CB_State2 != cb_Off_On)) // Выключатель включается
	{	// Если поймали спад, то
	//ISR_Timer_B = ISR_microTimer;
	CB_State2 = cb_Off_On;  // Выключатель на пути к включению
	//			updContacts_ComplexSP(); //обновляем состояние вспомогательного блока контактов в комплексной переменной complexSP
	lastOperation = cb_Off_On;
	positionChanged = true;	// Выключатель начал движение - флаг для включения расчетов
	};
	
	if (((curr_state & CB2_ON) != OFF) && ((CB_State2 == cb_Off_On) || (CB_State2 == cb_NA))) // Выключатель включился
	{
	//own_on_time_X[PHASE_B] = ISR_microTimer; //calcOwnTime(ISR_Timer_B);
	//			switchCoolDownStartTime = currentTime;
	//			switchBlocked = true;
	CB_State2 = cb_On;
	//		updContacts_ComplexSP(); //обновляем состояние вспомогательного блока контактов в комплексной переменной complexSP
	}
	
	// Включение 2 Фин
	
	// Выключение 2 Старт
	//		if (switchBlocked == false)
	if (((curr_state & CB2_ON) == OFF) && ((prev_state & CB2_ON) != OFF) && (CB_State2 != cb_On_Off))  // Выключатель выключается
	{
	//ISR_Timer_B = ISR_microTimer;
	CB_State2 = cb_On_Off;  // Выключатель на пути к включению
	//			updContacts_ComplexSP(); //обновляем состояние вспомогательного блока контактов в комплексной переменной complexSP
	lastOperation = cb_On_Off;
	positionChanged = true;	// Выключатель начал движение - флаг для включения расчетов
	
	};
	
	if (((curr_state & CB2_OFF) != OFF)  && ((CB_State2 == cb_On_Off) || (CB_State2 == cb_NA))) // Выключатель выключился
	{
	own_off_time_X[PHASE_B] = ISR_microTimer; //calcOwnTime(ISR_Timer_B);
	//			switchCoolDownStartTime = currentTime;
	//			switchBlocked = true;
	CB_State2 = cb_Off;
	//			updContacts_ComplexSP(); //обновляем состояние вспомогательного блока контактов в комплексной переменной complexSP
	}
	
	
	// Выключение 2 Фин ++++++++++++++++++++++++++++++


	// Включение 3 Старт ++++++++++++++++++++++++
	//		if (switchBlocked == false)
	if (((curr_state & CB3_OFF) == OFF) && ((prev_state & CB3_OFF) != OFF) && (CB_State3 != cb_Off_On)) // Выключатель включается
	{	// Если поймали спад, то															// увеличиваем счётчик операций.
	CB_State3 = cb_Off_On;  // Выключатель на пути к включению
	//			updContacts_ComplexSP(); //обновляем состояние вспомогательного блока контактов в комплексной переменной complexSP
	lastOperation = cb_Off_On;
	positionChanged = true;	// Выключатель начал движение - флаг для включения расчетов
	//ISR_Timer_C = ISR_microTimer;
	};

	if (((curr_state & CB3_ON) != OFF) && ((CB_State3 == cb_Off_On) || (CB_State3 == cb_NA))) // Выключатель включился
	{
	CB_State3 = cb_On;
	//			updContacts_ComplexSP(); //обновляем состояние вспомогательного блока контактов в комплексной переменной complexSP
	//own_on_time_X[PHASE_C] = ISR_microTimer; //calcOwnTime(ISR_Timer_C);
	//			switchCoolDownStartTime = currentTime;
	//			switchBlocked = true;
	}
	// Включение 3 Фин

	// Выключение 3 Старт
	//		if (switchBlocked == false)
	if (((curr_state & CB3_ON) == OFF) && ((prev_state & CB3_ON) != OFF) && (CB_State3 != cb_On_Off))  // Выключатель выключается
	{
	CB_State3 = cb_On_Off;  // Выключатель на пути к включению
	//			updContacts_ComplexSP(); //обновляем состояние вспомогательного блока контактов в комплексной переменной complexSP
	lastOperation = cb_On_Off;
	positionChanged = true;	// Выключатель начал движение - флаг для включения расчетов
	//ISR_Timer_C = ISR_microTimer;
	};

	if (((curr_state & CB3_OFF) != OFF)  && ((CB_State3 == cb_On_Off) || (CB_State3 == cb_NA))) // Выключатель выключился
	{
	own_off_time_X[PHASE_C] = ISR_microTimer; //calcOwnTime(ISR_Timer_C);
	CB_State3 = cb_Off;
	//			updContacts_ComplexSP(); //обновляем состояние вспомогательного блока контактов в комплексной переменной complexSP
	//			switchCoolDownStartTime = currentTime;
	//			switchBlocked = true;
	}
	}

	// Выключение 3 Фин
	*/

	
	//	params.phase_contact_states = curr_state;
	
}
/*
uint16_t findMinCurr(int8_t phase, int16_t buffer[][phase_quantity])
{
uint16_t minValue = 50000;
uint8_t osc_Point= 0;

for (osc_Point = osc_length - 1; osc_Point > 0; osc_Point--)		// Перебираем по очереди все осциллограммы, начиная с последней ('osc_length - 1', поскольку элементы массива считаются с нулевого).
{
if (myAbs(buffer[osc_Point][phase]) < minValue)
minValue = myAbs(buffer[osc_Point][phase]);							// ищем минимальное значение - синус поднимается от нуля при повышении тока через ТТ
}

if (2*minValue > 100)
return (2*minValue);
else
return 100;
}
*/
// Ф-ция перебирает по очереди осциллограммы в массиве и ищет момент, когда значение тока горения дуги
// упадёт ниже определённого. Поиск производится по каждой из фаз. Найденные значения складываются
// в массив phase_zero.

uint8_t consistencyPosit = 0;
uint8_t consistencyNegat = 0;



void finding_arc_end()
{
	uint8_t phase_ = 0;
	//	lastOperation[PHASE_B]
	osc_Point = 0;
	//	int16_t osc_value = 0;
	
	memset(arcDetected, 0, sizeof(arcDetected));
	memset(ISR_arcPoint, 0, sizeof(ISR_arcPoint));

	
	for (phase_ = 0; phase_ < phase_quantity; phase_++) // Перебираем по очереди все фазы.
	{
		
		consistencyPosit = 0;
		consistencyNegat = 0;
		
		if (lastOperationBuf[2] == cb_On_Off)
		{
			ISR_arcPoint[phase_] = 0;
			
			for (osc_Point = osc_length - 1; osc_Point > 0; osc_Point--)		// Перебираем по очереди все осциллограммы, начиная с последней ('osc_length - 1', поскольку элементы массива считаются с нулевого).
			{
				if (checkConsistensy(phase_))
				{
					arcDetected[phase_] = 1;

					if (osc_Point > 249)
					osc_Point = 249;
					
					ISR_arcPoint[phase_] = osc_Point+6;
					
					
					break;
				}
			}
		}
		else
		{
			
			if (lastOperationTimeBuf[2] > 0)
			ISR_arcPoint[phase_] = lastOperationTimeBuf[2] - 1;
			else
			ISR_arcPoint[phase_] = 0;
			
			for (osc_Point = 0; osc_Point < (lastOperationTimeBuf[2]); osc_Point++)		// Перебираем по очереди все осциллограммы, начиная с последней ('osc_length - 1', поскольку элементы массива считаются с нулевого).
			{
				if (checkConsistensy(phase_))
				{
					{
						arcDetected[phase_] = 1;
						
						if (osc_Point < 6)
						osc_Point = 6;
						
						
						ISR_arcPoint[phase_] = osc_Point - 6;							// точкой окончания горения дуги назначаем предыдущую сравненную (перебираем с конца массива!)
						break;														// и досрочно выходим из цикла.
					}											// и досрочно выходим из цикла.
				}
			}
		}
		
		// Следующая осциллограмма.
		// 		if (osc_Point == 0)				 // Если достигли самой первой осциллограммы и её значение по-прежнему ниже минимума,
		// 		arcTime[phase_] = 0;
	}
}



uint8_t checkConsistensy(uint8_t phase_)
{
	if (cts.calcOscBuffer[osc_Point][phase_] > min_Arc_Current ) 								// Если значение текущей осциллограммы больше минимального тока, то
	{
		consistencyPosit++;
		consistencyNegat = 0;
		if (consistencyPosit > 5)
		{
			return 1;														// и досрочно выходим из цикла.
		}
		
	}
	else
	{
		if (cts.calcOscBuffer[osc_Point][phase_] < (-min_Arc_Current) ) 								// Если значение текущей осциллограммы больше минимального тока, то
		{
			consistencyPosit = 0;
			consistencyNegat ++;
			if (consistencyNegat > 5)
			{
				return 1;// и досрочно выходим из цикла.
			}
			
		}
	}
	return 0;
}

void calcFullTime()
{
	//	int16_t timeBuffer = 0;

	for (uint8_t phase = 0; phase < phase_quantity; phase++) // Перебираем по очереди все фазы.
	{
		if (lastOperationBuf[2] == cb_On_Off)
		{
			full_off_time_ISR[phase] = ISR_arcPoint[phase];		// phase_zero[phase] + 1 ???
		}
		else
		{
			full_on_time_ISR[phase] = ISR_arcPoint[phase];		// phase_zero[phase] + 1 ???
		}
		//	full_on_time_ISR[phase] = ISR_arcPoint[phase];		// phase_zero[phase] + 1 ???
		
	}
	
}

// Ф-ция вычисляет время горения дуги во время последней коммутации:
void calcArcLifetime()
{
	int16_t arcBuffer = 0;
	
	
	for (uint8_t phase = 0; phase < phase_quantity; phase++) // Перебираем по очереди все фазы.
	{
		if (arcDetected[phase])
		{
			if (lastOperationBuf[2] == cb_On_Off)
			{
				arcBuffer = ((int16_t)ISR_arcPoint[phase] + 128 - ISR_timeShiftOff[phase]);
				if (arcBuffer < 0 )
				arcBuffer = 0;
				
				//	ISR_arc_lifetime[phase] = arcBuffer;		// phase_zero[phase] + 1 ???
				
			}
			else
			{
				arcBuffer = lastOperationTimeBuf[2] - 1 + 128 - ISR_timeShiftOn[phase] - ISR_arcPoint[phase];
				if (arcBuffer < 0 )
				arcBuffer = 0;
				
				//	ISR_arc_lifetime[phase] = arcBuffer;		// phase_zero[phase] + 1 ???
				
			}
		}
		ISR_arc_lifetime[phase] = arcBuffer;
		
		
	}
}


// Ф-ция вычисляет время горения дуги во время последней коммутации:
void calcTimeStates()
{
	uint8_t phase_quantity_;
	uint8_t ownTimeOffWarnState = 0;
	uint8_t ownTimeOffAlarmState = 0;
	
	uint8_t ownTimeOnWarnState = 0;
	uint8_t ownTimeOnAlarmState = 0;
	
	uint8_t fullTimeOffWarnState = 0;
	uint8_t fullTimeOffAlarmState = 0;
	
	uint8_t fullTimeOnWarnState = 0;
	uint8_t fullTimeOnAlarmState = 0;
	
	if (auxType == 1)
	{
		phase_quantity_ = 1;
	}
	else
	{
		phase_quantity_ = phase_quantity;
	}

	for (uint8_t phase = 0; phase < phase_quantity_; phase++) // Перебираем по очереди все фазы.
	{
		
		// Off times
		if (own_off_time_ISR[phase] > fullTimeOffWarn_ISR && own_off_time_ISR[phase] < fullTimeOffAlarm_ISR )
		fullTimeOffWarnState = 1;
		
		if (full_off_time_ISR[phase] > ownTimeOffWarn_ISR && full_off_time_ISR[phase] < ownTimeOffAlarm_ISR )
		ownTimeOffWarnState = 1;
		

		if (own_off_time_ISR[phase] > fullTimeOffAlarm_ISR)
		fullTimeOffAlarmState = 1;
		
		if (full_off_time_ISR[phase] > ownTimeOffAlarm_ISR)
		ownTimeOffAlarmState = 1;
		
		// On times
		if (own_on_time_ISR[phase] > fullTimeOnWarn_ISR && own_on_time_ISR[phase] < fullTimeOnAlarm_ISR )
		fullTimeOnWarnState = 1;
		
		if (full_on_time_ISR[phase] > ownTimeOnWarn_ISR && full_on_time_ISR[phase] < ownTimeOnAlarm_ISR )
		ownTimeOnWarn_ISR = 1;

		if (own_on_time_ISR[phase] > fullTimeOnAlarm_ISR)
		fullTimeOnAlarmState = 1;
		
		if (full_on_time_ISR[phase] > ownTimeOnAlarm_ISR)
		ownTimeOnAlarmState = 1;
	}

	bitWrite(complexSP,ownTimeOff_Warn_Bit,ownTimeOffWarnState);
	bitWrite(complexSP,ownTimeOff_Alarm_Bit,ownTimeOffAlarmState);
	bitWrite(complexSP,ownTimeOn_Warn_Bit,ownTimeOnWarnState);
	bitWrite(complexSP,ownTimeOn_Alarm_Bit,ownTimeOnAlarmState);
	bitWrite(complexSP,fullTimeOffWarn_Bit,fullTimeOffWarnState);
	bitWrite(complexSP,fullTimeOffAlarm_Bit,fullTimeOffAlarmState);
	bitWrite(complexSP,fullTimeOnWarn_Bit,fullTimeOnWarnState);
	bitWrite(complexSP,fullTimeOnAlarm_Bit,fullTimeOnAlarmState);
}





//
// 				for (uint8_t osc = 0; osc < osc_length; osc++)
// 				{
// 					current_Buf_RMS_A += sqrForRMS(cts.osc_buffer_2,PHASE_A,cts.adcShift_A);
// 					current_Buf_RMS_B += sqrForRMS(cts.osc_buffer_2,PHASE_B,cts.adcShift_B);
// 					current_Buf_RMS_C += sqrForRMS(cts.osc_buffer_2,PHASE_C,cts.adcShift_C);
// 				}
// 				switchCurrent_Amp[PHASE_A] = sqrt(current_Buf_RMS_A*0.0051) - 128 + RMS_ZeroA;
// 				switchCurrent_Amp[PHASE_B] = sqrt(current_Buf_RMS_B*0.0051) - 128 + RMS_ZeroB;
// 				switchCurrent_Amp[PHASE_C] = sqrt(current_Buf_RMS_C*0.0051) - 128 + RMS_ZeroC;
//



#define UINT16_MAX 65535
// Ф-ция ищет максимальное значение тока в массиве осциллограмм и делит его на корень из двух:
void calcSwitchCurrent()
{
	
	for (uint8_t phase_ = 0; phase_ < phase_quantity; phase_++) // Перебираем по очереди все фазы.
	{
		
		if (ISR_arc_lifetime[phase_] == 0)
		{
			switchCurrent_Amp[phase_] = 0;
		}
		else
		{
			if (lastOperationBuf[2] == cb_On_Off)
			{
				
				uint64_t current_RMS_Buf = 0;
				for (osc_Point = 0; osc_Point < ISR_arcPoint[phase_]; osc_Point++)
				{
					int32_t value = (int32_t) cts.calcOscBuffer[osc_Point][phase_];  // Ensure proper range
					current_RMS_Buf += (uint64_t)value * value;  // Cast to ensure no overflow
				}
				uint32_t result = (uint32_t)k_current[phase_] * (sqrt(current_RMS_Buf / ISR_arcPoint[phase_]) - 128 + RMS_Zero[phase_]);
				switchCurrent_Amp[phase_] = (result > UINT16_MAX) ? UINT16_MAX : (uint16_t)result;

				
			}
			else
			{
				uint64_t current_RMS_Buf = 0;
				for (osc_Point = ISR_arcPoint[phase_]; osc_Point <  (osc_length - 1); osc_Point++)		// Перебираем по очереди все осциллограммы, начиная с последней ('osc_length - 1', поскольку элементы массива считаются с нулевого).
				{
					int32_t value = (int32_t) cts.calcOscBuffer[osc_Point][phase_];  // Ensure proper range
					current_RMS_Buf += (uint64_t)value * value;  // Cast to ensure no overflow
				}
				uint32_t result = (uint32_t)k_current[phase_] * (sqrt(current_RMS_Buf / (osc_length - 1 - ISR_arcPoint[phase_])) - 128 + RMS_Zero[phase_]);
				switchCurrent_Amp[phase_] = (result > UINT16_MAX) ? UINT16_MAX : (uint16_t)result;
				
				//	switchCurrent_Amp[phase_] = k_current[phase_] * (sqrt(current_RMS_Buf / (osc_length - 1 - ISR_arcTime[phase_])) - 128 + RMS_Zero[phase_]);
			}
		}
		
		
	}
}




// Ф-ция суммирует квадраты тока с первой осциллограммы до точки окончания горения дуги:
void calcLast_kA2s()
{
	for (uint8_t phase = 0; phase < phase_quantity; phase++) // Перебираем по очереди все фазы.
	{
		float passed_resource = 0;
		
		if (ISR_arc_lifetime[phase] == 0)
		{
			last_kA2s[phase] = 0;
		}
		else
		{
			if (lastOperationBuf[2] == cb_On_Off)
			{
				uint16_t startCycleCnt = 0;
				
				if ((ISR_timeShiftOff[phase] - 128) < 0)
				startCycleCnt = 0;
				else
				startCycleCnt = ISR_timeShiftOff[phase] - 128;
				
				
				for (uint8_t osc = startCycleCnt; osc < ISR_arcPoint[phase]; osc++)
				passed_resource = passed_resource + ((1.0*cts.calcOscBuffer[osc][phase]*k_current[phase])*(1.0*cts.calcOscBuffer[osc][phase]*k_current[phase])) * OSC_REC_PERIOD;
			}
			else
			{
				for (uint8_t osc = ISR_arcPoint[phase]; osc < (lastOperationTimeBuf[2] - 1 + 128 - ISR_timeShiftOn[phase]); osc++)
				passed_resource = passed_resource + ((1.0*cts.calcOscBuffer[osc][phase]*k_current[phase])*(1.0*cts.calcOscBuffer[osc][phase]*k_current[phase])) * OSC_REC_PERIOD;
			}
			passed_resource = passed_resource * 0.001 * 0.001 * 0.001 * 0.001;
			last_kA2s[phase] = passed_resource;
		}
		
		
	}
}

void findShortCircuit()
{
	float overcurrent_ = params.nominalCurrent_Amp * params.overCurrentKoef;
	if (switchCurrent_Amp[PHASE_A] > overcurrent_ || switchCurrent_Amp[PHASE_B] > overcurrent_ || switchCurrent_Amp[PHASE_C] > overcurrent_)
	{
		if (lastOperationBuf[2] == cb_On_Off)
		{
			params.short_Cnt_Off++;
			eeprom_write_word((uint16_t*)76,params.short_Cnt_Off);
		}
		else
		{
			params.short_Cnt_On++;
			eeprom_write_word((uint16_t*)78,params.short_Cnt_On);
		}
		
	}
}


void updContacts_ComplexSP()
{
	int buffer_ = 0;

	for (int i = 0; i <= 5; i++)
	{
		if (bitRead(curr_state,i))
		bitSet(buffer_,5-i);
	}

	// Reverse the first 'numBits' bit
	complexSP &= ~(0b111111 << 0);
	// Set the first 6 bits of ComplexState to the first 6 bits of endEffector
	complexSP |= (buffer_ & 0b111111) << 0;

}





void getRMS(int16_t osc_buffer[][phase_quantity], uint8_t command_)
{
	
	if (command_ == 0)
	{
		if (cts.amplification == 1)
		{
			current_A_buf += sqrForRMS(osc_buffer,PHASE_A,cts.gainAdcShift_A);
			current_B_buf += sqrForRMS(osc_buffer,PHASE_B,cts.gainAdcShift_B);
			current_C_buf += sqrForRMS(osc_buffer,PHASE_C,cts.gainAdcShift_C);
		}
		else
		{
			current_A_buf += sqrForRMS(osc_buffer,PHASE_A,cts.adcShift_A);
			current_B_buf += sqrForRMS(osc_buffer,PHASE_B,cts.adcShift_B);
			current_C_buf += sqrForRMS(osc_buffer,PHASE_C,cts.adcShift_C);
		}
	}
	else
	{
		float_RMS_A = sqrt(current_A_buf*0.0005) - 128 + RMS_Zero[PHASE_A];
		float_RMS_B = sqrt(current_B_buf*0.0005) - 128 + RMS_Zero[PHASE_B];
		float_RMS_C = sqrt(current_C_buf*0.0005) - 128 + RMS_Zero[PHASE_C];
		
		if (float_RMS_A < 0)
		float_RMS_A = 0;
		
		if (float_RMS_B < 0)
		float_RMS_B = 0;
		
		if (float_RMS_C < 0)
		float_RMS_C = 0;
		
	}
}


int64_t sqrForRMS(int16_t osc_buffer[][phase_quantity], uint8_t phase_, uint8_t shift_)
{
	int64_t curr_ = static_cast<int64_t>(osc_buffer[0][phase_]);
	uint64_t result;
	
	
	result = (curr_ - ADC_MID - 128 + shift_)*(curr_ - ADC_MID - 128 + shift_);
	return result;
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

u8 fram_cmd(u8 command)
{
	FRAM_CS_ON;
	SPDR = command;
	WAIT_SPI_TRANSFER;
	SPDR = 0;
	WAIT_SPI_TRANSFER;
	FRAM_CS_OFF;
	return SPDR;
}


void fram_write_byte(u16 addr, u8 data)
{
	// Enable write
	FRAM_CS_ON;               // Pull CS low to start communication
	SPDR = FRAM_CMD_WREN;  // Send WRITE enable flag
	WAIT_SPI_TRANSFER;
	FRAM_CS_OFF;


	FRAM_CS_ON;
	SPDR = FRAM_CMD_WRITE;  // Send WRITE command
	WAIT_SPI_TRANSFER;
	
	SPDR = (u8)(addr >> 8);  // Send high byte of address
	WAIT_SPI_TRANSFER;

	SPDR = (u8)(addr & 0xFF);  // Send low byte of address
	WAIT_SPI_TRANSFER;

	// Send the data byte
	SPDR = data;
	WAIT_SPI_TRANSFER;

	FRAM_CS_OFF;  // Pull CS high to end communication
}

u8 fram_read_byte(u16 addr)
{

	FRAM_CS_ON;  // Pull CS low to start communication

	// Send the READ command (0x03)
	SPDR = FRAM_CMD_READ;
	WAIT_SPI_TRANSFER;  // Wait for transmission to complete

	// Send the 16-bit address (high byte first)
	SPDR = (u8)(addr >> 8);  // Send high byte of address
	WAIT_SPI_TRANSFER;

	SPDR = (u8)(addr & 0xFF);  // Send low byte of address
	WAIT_SPI_TRANSFER;

	// Receive the data byte
	SPDR = 0x00;  // Send dummy byte to initiate SPI clock
	WAIT_SPI_TRANSFER;
	
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



void process_adc_data(int16_t osc_buffer_[][3], uint16_t measurements_num_RAW, uint8_t averPoints_)
{
	uint8_t adc_data[12];
	uint16_t average_sum[3] = {0, 0, 0};  // To accumulate sums for averaging
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





breakerState getBreakerState(uint8_t curr_state, uint8_t open_mask, uint8_t closed_mask, breakerState CB_State_)
{
	//
	// 	if ((curr_state == 0b00000011) || (curr_state == 0b00111111))
	// 	{
	// 		if (fullBraker < 1000)
	// 		{
	// 			fullBraker++;
	// 		}
	// 	}
	
	// Check if the door is currently open
	if ((curr_state & open_mask) && !(curr_state & closed_mask))
	{
		//	ISR_microTimer
		switchBlocked = true;
		switchBounceTime = ISR_microTimer;
		
		return cb_On;  // Door is open
	}


	// Check if the door is currently closed
	if (!(curr_state & open_mask) && (curr_state & closed_mask))
	{
		
		// 		full_off_time_ISR[PHASE_A] = ISR_microTimer;
		// 		if (auxType == 1)
		// 		{
		// 			full_off_time_ISR[PHASE_B] = full_off_time_ISR[PHASE_A];
		// 			full_off_time_ISR[PHASE_C] = full_off_time_ISR[PHASE_A];
		// 		}
		
		
		switchBlocked = true;
		switchBounceTime = ISR_microTimer;
		return cb_Off;  // Door is closed
	}

	// Check if the door was open and is now closing (cb_On_Off)
	if (!(curr_state & open_mask) && !(curr_state & closed_mask))
	{
		if (CB_State_ == cb_On)
		{
			if (fullBraker < 1000)
			{
				fullBraker++;
			}
			
			positionChanged = true;
			lastOperation = cb_On_Off;
			
			
			
			bitWrite(complexSP,last_Direction,0);
			
			return cb_On_Off;  // Door was open, now closing
		}
		if (CB_State_ == cb_Off)
		{
			if (fullBraker < 1000)
			{
				fullBraker++;
			}
			
			positionChanged = true;
			lastOperation = cb_Off_On;
			
			
			bitWrite(complexSP,last_Direction,1);
			return cb_Off_On;  // Door was closed, now opening
		}
	}

	// If both bits are 0 and no transition detected, return cb_NA
	return CB_State_;
}



