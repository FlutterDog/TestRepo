#include "main.h"


IGAS_X2X_485		modbus;						// Modbus protocol routines.
u8 mb_addr			= 0;						// Адрес устройства на шине Modbus.
u16 mb_reg0			= 0;						// Modbus-регистр, хранящий состояние всех 16-и входов в бинарном (битовом) представлении.
u32 millis			= 0;						// Счётчик времени с момента старта устройства, мс.
u8 leds[INPUT_QTY]	= {0};						// Массив, хранящий состояние LED-индикаторов лицевой панели.

u32 current_time;								// Текущее время. Используется для расчёта временных интервалов.
u32 loop_500_start	= 0;						// Стартовое время цикла в 500 мс.
u32 reset_wd_timer;								// Сброс таймаута момента последнего принятого по RS485 пакета.
u8 x2x_state		= 0;						// Статус связи: были ли (1) входящие за последние CONN_TIMEOUT миллисекунд или нет (0).




/**
* @brief Время запроса перехода в bootloader, мс.
*
* @details
* Нулевое значение означает, что переход не запрошен.
*/
uint32_t rebootBOOT = 0UL;


	/**
	* @brief Задержка перед переходом в bootloader, мс.
	*
	* @details
	* Задержка даёт Modbus-ответу физически уйти в линию RS-485 перед
	* запретом прерываний и передачей управления bootloader.
	*/
	static const uint32_t BOOTLOADER_JUMP_DELAY_MS = 50UL;

//======== Перезагрузка в Bootloader ========
typedef void (*app_prt)(void);
app_prt GotoBootloader = (app_prt)0x3E00;	// 3F07
//===========================================


// Ф-ция инициализирует пины МК на ввод или вывод:
void init_gpio(void){
	// Modbus-адрес:
	set_pin_input(DDR_ADDR, PIN_ADDR0);	// Устанавливаем работу пина ADDR0 на вход.
	set_pin_input(DDR_ADDR, PIN_ADDR1);	// То же самое для остальных пинов ADDR.
	set_pin_input(DDR_ADDR, PIN_ADDR2);
	set_pin_input(DDR_ADDR, PIN_ADDR3);
	set_pin_input(DDR_ADDR, PIN_ADDR4);
	pullup_on(PORT_ADDR, PIN_ADDR0);	// Включаем внутреннюю подтяжку (pull-up), т.к. переключатели прижимают выход к земле.
	pullup_on(PORT_ADDR, PIN_ADDR1);	// То же самое для остальных пинов ADDR.
	pullup_on(PORT_ADDR, PIN_ADDR2);
	pullup_on(PORT_ADDR, PIN_ADDR3);
	pullup_on(PORT_ADDR, PIN_ADDR4);

	// RS-485:
	set_pin_input(DDR_485, RX);			// Устанавливаем работу пина RX на вход.
	set_pin_output(DDR_485, TX);		// Устанавливаем работу пина TX на выход.
	set_pin_output(DDR_485, TX_EN);		// Устанавливаем работу пина TX Enable на выход.
	pullup_off(PORT_485, RX);			// Отключаем внутреннюю подтяжку (pull-up), т.к. используется внешняя.
	set_pin_low(PORT_485, TX);			// Устанавливаем состояние линии TX в LOW.
	set_pin_low(PORT_485, TX_EN);		// По умолчанию передача не ведётся (линия TX Enable в состоянии LOW).

	// Сдвиговые регистры:
	set_pin_output(DDR_SR, SR_LATCH);	// Устанавливаем работу пина SR_LATCH на выход.
	set_pin_output(DDR_SR, SR_CLK);		// Устанавливаем работу пина SR_CLK на выход.
	set_pin_output(DDR_LED, SR_LED);	// Устанавливаем работу пина SR_LED на выход.
	set_pin_input(DDR_SR, SR_DI);		// Устанавливаем работу пина SR_DI на вход.

	set_pin_low(PORT_SR, SR_LATCH);		// Устанавливаем линию SR_LATCH в LOW.
	set_pin_low(PORT_SR, SR_CLK);		// Устанавливаем линию SR_CLK в LOW.
	set_pin_low(PORT_LED, SR_LED);		// Устанавливаем линию SR_LED в LOW.
	pullup_off(PORT_SR, SR_DI);			// Отключаем внутреннюю подтяжку (pull-up) для линии SR_DI.

	// Индикация:
	set_pin_output(DDR_LED, LED_X2X);	// Устанавливаем работу пина LED_X2X на выход.
	LED_X2X_OFF();						// Устанавливаем индикатор LED_X2X в состояние ВЫКЛ.
}



int mirrorBits(u8 value, u8 numBits) {
	int mirroredValue = 0;

	// Reverse the first 'numBits' bit
	
	for (int i = 0; i < numBits; i++)
	{
		if (bitRead(value,i))
		bitSet(mirroredValue,numBits-i-1);
	}

	return mirroredValue;
}


// Ф-ция считывает состояние DIP-переключателей и отбрасывает неиспользуемые биты (5-7), а также смотрит значение в EEPROM:
void get_modbus_addr(void){
	//	mb_addr = eeprom_read_byte(EEP_MB_ADDR);					// Считываем адрес из EEPROM (имеет приоритет перед DIP-задатчиком).
	//	if ((mb_addr != 0) && (mb_addr < 248)) return;				// Если в EEPROM записан корректный Modbus-адрес, то выходим из ф-ции.
	mb_addr = mirrorBits(~PIN_ADDR & ADDR_MASK, 5);			// Иначе - считываем установленное значение и отбрасываем неиспользуемые биты (5-7). Поскольку входы с внутренней подтяжкой, то инвертируем состояние порта.
	if (mb_addr < 1)
	mb_addr = 1;

}




// Ф-ция запускает Timer2 на срабатывание каждую миллисекунду (1/1000 сек.).
void init_millis(void){
	TCCR2A	= 0;
	TCCR2B	= 0;
	TCNT2	= 0;
	bit_set(TCCR2A, WGM21);					// Mode: CTC
	bits_set(TCCR2B, CS22, CS20);			// Prescaler: 128
	OCR2A	= 124;							// Freq: 1000 Hz (16000000/((124+1)*128))
	bit_set(TIMSK2, OCIE2A);				// Output Compare Match A Interrupt Enable
}

// Ф-ция увеличивает на единицу значение переменной millis при каждом страбатывании Timer2.
ISR (TIMER2_COMPA_vect){
	millis++;
};

// Ф-ция считывает состояние входов и проецирует его на массив состояния индикаторов leds[], попутно сохраняя эти состояния в соответствующей переменной (mb_reg0).
void get_input_status(void){
	mb_reg0 = 0;								// Сбрасываем переменную, хранящую в бинарном представлении состояние входов.
	sr_latch_high();							// Переводим "защёлку" SH/LD в HIGH для работы с 74HC165.
	sr_latch_165();								// "Замораживаем" состояние входов 74Н165 путём изменения сигнала SH/LD из HIGH в LOW и обратно.

	u8 i = INPUT_QTY;							// Организуем цикл по количеству входов.
	while (i--){
		mb_reg0 <<= 1;							// Сдвигаем даннные влево для установки значения состояния следующего входа.
		if (PIN_SR & SR_DI_MASK){				// Читаем состояние порта, маскируем ненужные биты. Если состояние текущего входа 74НС165 установлено в HIGH...
			leds[i] = ON;						// ...то включаем соотвествующий индикатор и...
			mb_reg0++;							// ...устанавливаем lsb переменной mb_reg0 в "1".
			} else {								// В противном случае...
			leds[i] = OFF;						// ...индикатор выключаем.
		};
		sr_clock();								// Подаём тактовый импульс.
	};

	sr_latch_low();								// Возвращаем сигнал SH/LD обратно в LOW.
}

// Ф-ция выводит состояние входов (массив leds[]) на индикаторную панель.
void set_status_leds(void){
	u8 i = INPUT_QTY;							// Организуем цикл по количеству входов.
	while (i--){
		if (leds[i]){							// Если текущий индикатор включен...
			sr_led_high();						// ...то переводим сигнал SR_LED в HIGH.
			} else {								// В противном случае...
			sr_led_low();						// ...устанавливаем сигнал SR_LED в LOW.
		};
		sr_clock();								// Подаём тактовый импульс.
	};

	sr_led_low();								// Принудительно устанавливаем сигнал SR_LED в LOW.
	sr_latch_595();								// Устанавливаем на выходах 74HC595 переданное состояние.
}

// Ф-ция преобразует массив leds[] для правильного отображения на индикаторной панели ("костыль").
void leds_rearrange(void){
	u8 tmp = 0;									// Временная переменная для переноса значений между элементами массива.
	for (u8 i = 0; i < 8; i++){					// Цикл на 1/2 размера массива.
		tmp = leds[i];							// Меняем местами...
		leds[i] = leds[i + 8];					// ...две половины...
		leds[i + 8] = tmp;						// ...массива leds[].
	};
}


//=======================  Для тестов  ============================
/*
// Ф-ция возвращает состояние 16-и входов.
u16 leds2mb_reg(void){
u16	mb_reg = 0;
for (u8 i = 0; i < INPUT_QTY; i++){
if (leds[i] == 1) mb_reg++;
mb_reg << 1;
};
return mb_reg;
};
*/
/*

// Ф-ция проверки работы макроопределений.
void tmp_macros_test(void){
sr_latch_595();
sr_clock();
}
*/

// Ф-ция зажигает верхний ряд светодиодов.
void leds_blinking(void){
	leds[0] = ON;
	leds[1] = ON;
	leds[8] = ON;
	leds[9] = ON;
	set_status_leds();
	_delay_ms(1500);
	leds[0] = OFF;
	leds[1] = OFF;
	leds[8] = OFF;
	leds[9] = OFF;
	set_status_leds();
}

//=================================================================


//======================  MODBUS BEGIN  ===========================

// Выдача данных по протоколу Modbus:
void mb_proceed(s16 *regs, s16 start_reg, s16 regs_qty_, s16 Value_){
	u8	writeReg = 0;

	if (modbus.func == FC_WRITE_REGS || modbus.func == FC_WRITE_REG) writeReg = 1;

	if (x2x_state == IDLE){
		x2x_state = ACTIVE;					// Connection is established
		LED_X2X_ON();						// Turn corresponding LED on
	};

	reset_wd_timer = current_time;

	for (u8 i = 0; i < regs_qty_; i++){
		switch (start_reg){
			#include "mb_registers.h"
		};
		start_reg++;
	};
}

// Ф-ция настраивает необходимые параметры протокола Modbus и сопутствующих подсистем (UART):
void mb_comm_init(void){
	Serial.begin(BAUDRATE, SERIAL_8N1);
	modbus.init(BAUDRATE);						// Инициируем Modbus
	modbus.setDelegate(mb_proceed);				// To run library function from main loop
	//	mb_addr = 3;
	modbus.slave_id = mb_addr;					// Задание Slave адреса модуля
}

// Проверка состояния связи по протоколу Modbus:
void mb_timeout_check(){
	if (current_time - loop_500_start >= LOOP_TIME){
		loop_500_start = current_time;
		if (current_time - reset_wd_timer >= CONN_TIMEOUT) x2x_state = IDLE;	// Watchdog - таймаут по времени - нет входящих пакетов по Rs485
		if (x2x_state == IDLE) LED_X2X_TGL();					// Если входящих на адрес X2X нет, то мигаем X2X-светодиодом
	};
}

void mb_loop(void){
	current_time = millis;
	mb_timeout_check();										// Цикл (без прерываний) на 500 мс
	modbus.update();											// Вызываем обработку Modbus
}

/**
* @brief Стартовый адрес bootloader в байтах.
*
* @details
* Для ATmega328P значение 0x7000 соответствует boot-секции размером
* 4096 байт при верхней границе flash 0x7FFF.
*
* @warning
* Значение должно соответствовать фактическим fuse-настройкам BOOTSZ
* и сборке bootloader.
*/
static const uint16_t BOOT_START_ADDR_BYTES = 0x7000U;


/**
* @brief Стартовый адрес bootloader в словах flash.
*
* @details
* На AVR адреса переходов через function pointer задаются в словах flash,
* поэтому байтовый адрес делится на два.
*/
static const uint16_t BOOT_START_ADDR_WORDS =
static_cast<uint16_t>(BOOT_START_ADDR_BYTES / 2U);



/**
* @brief Тип указателя на функцию bootloader.
*/
typedef void (*boot_ptr_t)(void);


//======================  MODBUS END  =============================

// -------------------------------------------------------------------------
// Переход в bootloader
// -------------------------------------------------------------------------

/**
* @brief Передаёт управление в bootloader.
*/
void jump_to_bootloader(void)
{
	cli();

	wdt_disable();

	#ifdef UCSR0B
	UCSR0B = 0u;
	#endif

	#ifdef SPCR
	SPCR = 0u;
	#endif

	#ifdef TWCR
	TWCR = 0u;
	#endif

	/*
	* Восстановление нулевого регистра ABI AVR-GCC.
	*/
	asm volatile ("clr __zero_reg__");

	/*
	* Установка стека в конец SRAM.
	*/
	SP = RAMEND;

	/*
	* Перенос таблицы векторов прерываний в boot-секцию.
	* Последовательность IVCE -> IVSEL должна выполняться без задержки
	* между двумя записями.
	*/
	MCUCR = static_cast<uint8_t>(_BV(IVCE));
	MCUCR = static_cast<uint8_t>(_BV(IVSEL));

	/*
	* Прямой переход в bootloader.
	* Адрес задаётся в словах flash.
	*/
	reinterpret_cast<boot_ptr_t>(BOOT_START_ADDR_WORDS)();
}



int main(void){
	wdt_disable();
	init_gpio();								// Инициализация портов ввода-вывода.
	init_millis();								// Инициализация счётчика времени с момента запуска устройства.
	get_modbus_addr();							// Получение Modbus-адреса устройства.

	leds_blinking();							// Тестовая ф-ция. Зажигает 4 индикатора в верхнем ряду при старте устройства.

	mb_comm_init();								// Инициализация UART и параметров Modbus.
	sei();										// Глобальное разрешение прерываний. Запуск счётчика времени с момента запуска устройства.

	while (true){

		//	get_modbus_addr();						// Тестирование. Можно поиграться...
		//	modbus.slave_id = mb_addr;				// ...с задатчиком Modbus-адреса.

		get_input_status();						// Получение состояния входов.
		leds_rearrange();						// Преобразование массива состяний входов для правильного отображения на индикаторной панели.
		set_status_leds();						// Отображение на индикаторной панели состояния входов.
		mb_loop();								// Обработчик общения по протоколу Modbus.
		
		if ((rebootBOOT != 0UL) &&  (static_cast<uint32_t>(millis - rebootBOOT) > BOOTLOADER_JUMP_DELAY_MS))
		{
			rebootBOOT = 0UL;
			jump_to_bootloader();

			for (;;)
			{
				/*
				* Защитный цикл на случай возврата из bootloader.
				*/
			}
		}
		
		//		_delay_ms(100);							// Задержка 100 мс. Просто так.
	};
}