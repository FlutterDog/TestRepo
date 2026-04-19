#include "IGAS_ADC_SPI.h"

// Буферы для хранения выборок АЦП:
u8	buf_ct[ADC_RES]	= {0};								// Буфер на 1 выборку с нужным разрешением АЦП.
u16	buf_ai[ADC_RES]	= {0};								// Двойной буфер (т.к. аналоговых входов 9, т.е. больше 8) на 1 выборку с нужным разрешением АЦП.
u16	buf_tmp[1]		= {0};


// Function sets up MCU SPI subsystem:
void ADC_SPI::spi_init(void){
	SPSR = 0b00000000;								// Reset register to zero
	SPCR = 0b00000000;								// Reset register to zero

	SPSR |= (1<<SPI2X);								// SPI2X mode and XTAL divider /4 (total divider is 2)
	SPCR &= ~(1<<SPR0);								// with XTAL = 16 MHz
	SPCR &= ~(1<<SPR1);								// give us 8 MHz SPI clock rate

	SPCR &= ~(1<<CPOL);								// SPI mode 0
	SPCR &= ~(1<<CPHA);								// --- // ---

	//	SPCR |= (1<<DORD);								// 0 = MSB first (default), 1 = LSB first

	SPCR |= (1<<MSTR);								// MASTER Mode
	SPCR |= (1<<SPE);								// Enable SPI interface
}


// Function sets Chip (Slave) Select (CS/SS) pins as OUTPUT and makes them HIGH:
void ADC_SPI::gpio_init(void){

	
}


// Function gets 12 bits of data from desired ADC and puts them in a 16 bit word (little-endian!!!):
void ADC_SPI::get_single_channel_data(uint8_t *data_address, uint16_t buf_ct16)
{
	// буффер для складывания полученных бит в байты измерений
	
	
	*(data_address + 1) = highByte(buf_ct16);	// Put received data in the second byte of data_address. Four lead bits aren't useful
	
	*(data_address) = lowByte(buf_ct16);						// Put received data in the first byte. All bits are useful.
	
}


// Function gets 12 bits of data from desired ADC and puts them in a 16 bit word (little-endian!!!):
void ADC_SPI::get_single_channel_dataLCT1(uint8_t *data_address, uint8_t adc_id)
{
	// Set desired ADC' CS pin to LOW:
	uint8_t current_cs_pin = 0;
	//uint16_t dataBuf = 0;
	if (adc_id == ADC3)
	{					// ADC3' CS pin is connected to PORT D
		PORTD &= ~(1<<PIN_ADC3_CS);
	}
	else
	{
		switch (adc_id)
		{
			case ADC1:
			current_cs_pin = PIN_ADC1_CS;
			break;
			case ADC2:
			current_cs_pin = PIN_ADC2_CS;
			break;
			case ADC4:
			current_cs_pin = PIN_ADC4_CS;
			break;
			case ADC5:
			current_cs_pin = PIN_ADC5_CS;
			break;
			case ADC6:
			current_cs_pin = PIN_ADC6_CS;
			break;
		};
		PORTB &= ~(1<<current_cs_pin);
	};

	SPDR = 0;									// Start SPI transfer by writing a value to the SPDR register
	while (!(SPSR & (1<<SPIF)));				// Wait until SPI transfer is complete
	/*	Little-endian, мать его!
	\/\/\/\/\/\/\/\/\/  */
	
	*(data_address + 1) = (SPDR & 0b00001111);	// Put received data in the second byte of data_address. Four lead bits aren't useful
	//dataBuf = (SPDR & 0b00001111) << 8;	// Put received data in the second byte of data_address. Four lead bits aren't useful
	SPDR = 0;
	while (!(SPSR & (1<<SPIF)));
	*(data_address) = SPDR;						// Put received data in the first byte. All bits are useful.
	//dataBuf = dataBuf | SPDR;						// Put received data in the first byte. All bits are useful.

	//data_address = dataBuf;
	// Set desired ADC' CS pin back to HIGH:
	if (adc_id == ADC3)
	{
		PORTD |= (1<<PIN_ADC3_CS);
	}
	else
	{
		PORTB |= (1<<current_cs_pin);
	};
}



// Function gets 12 bits of data from desired ADC and puts them in a 16 bit word (little-endian!!!):
uint16_t ADC_SPI::get_single_channel_dataX(uint8_t *data_address, uint8_t adc_id){
	// Set desired ADC' CS pin to LOW:
	uint8_t current_cs_pin = 0;
	uint16_t dataBuf = 0;
	if (adc_id == ADC3)
	{					// ADC3' CS pin is connected to PORT D
		PORTD &= ~(1<<PIN_ADC3_CS);
	}
	else
	{
		switch (adc_id)
		{
			case ADC1:
			current_cs_pin = PIN_ADC1_CS;
			break;
			case ADC2:
			current_cs_pin = PIN_ADC2_CS;
			break;
			case ADC4:
			current_cs_pin = PIN_ADC4_CS;
			break;
			case ADC5:
			current_cs_pin = PIN_ADC5_CS;
			break;
			case ADC6:
			current_cs_pin = PIN_ADC6_CS;
			break;
		};
		PORTB &= ~(1<<current_cs_pin);
	};

	SPDR = 0;									// Start SPI transfer by writing a value to the SPDR register
	while (!(SPSR & (1<<SPIF)));				// Wait until SPI transfer is complete
	/*	Little-endian, мать его!
	\/\/\/\/\/\/\/\/\/  */
	
	//	*(data_address + 1) = (SPDR & 0b00001111);	// Put received data in the second byte of data_address. Four lead bits aren't useful
	dataBuf = (SPDR & 0b00001111) << 8;	// Put received data in the second byte of data_address. Four lead bits aren't useful
	SPDR = 0;
	while (!(SPSR & (1<<SPIF)));
	//*(data_address) = SPDR;						// Put received data in the first byte. All bits are useful.
	dataBuf = dataBuf | SPDR;						// Put received data in the first byte. All bits are useful.

	//data_address = dataBuf;
	// Set desired ADC' CS pin back to HIGH:
	if (adc_id == ADC3)
	{
		PORTD |= (1<<PIN_ADC3_CS);
	}
	else
	{
		PORTB |= (1<<current_cs_pin);
	};
	
	return dataBuf;
}

// Ф-ция получает данные с 3-х ТТ нормального режима и складывает их в самую первую строку массива осциллограмм:
void ADC_SPI::get_ct_values_in_idle_mode(int16_t osc_buffer[][phase_quantity])
{
	if (recording_status == COMPLETE) return;


	adc_ct_rec(buf_ct);		// получили одно измерение россыпью бит


	get_single_channel_data((uint8_t*)&osc_buffer[0][PHASE_A], adc_ct_extract_single(buf_ct, ADC1));	// взяли из buf_ct данные и преобразовали в измерения для канала ADC_CT_1
	get_single_channel_data((uint8_t*)&osc_buffer[0][PHASE_B], adc_ct_extract_single(buf_ct, ADC2));	// взяли из buf_ct данные и преобразовали в измерения для канала
	get_single_channel_data((uint8_t*)&osc_buffer[0][PHASE_C], adc_ct_extract_single(buf_ct, ADC3));	// взяли из buf_ct данные и преобразовали в измерения для канала

	solenoidCurrOn_RAW = adc_ct_extract_single(buf_ct, ADC4);
	solenoidCurrOff_1_RAW = adc_ct_extract_single(buf_ct, ADC5);
	solenoidCurrOff_2_RAW = adc_ct_extract_single(buf_ct, ADC6);
	

	
	
}




// Ф-ция получает данные с 3-х АЦП нормального режима данные и каждый вызов последовательно
// складывает эти данные в массив осциллограмм.
// Если АЦП НР выйдет на максимум, то значения начнут считываться с парного ему АЦП КЗ,
// а в массиве switching_iteration запишется номер вызова, на котором произошло переключение.
// ВНИМАНИЕ! Обратного переключения с АЦП КЗ на АЦП НР НЕ ПРОИСХОДИТ!

uint16_t checkBuf = 0;

void ADC_SPI::fill_ct_buffer_completely(void)
{
	if (recording_status == COMPLETE) return;

	static uint16_t iteration_counter = 0;
	uint8_t pack5[BYTES_PER_POINT_CT];

	// Считать + упаковать 3?12 бит в 5 байт
	adc_ct_rec_pack5(pack5);

	// Адрес в FRAM: 5 байт на точку
	const uint16_t addr = (uint16_t)(iteration_counter * BYTES_PER_POINT_CT);

	fram_write_array(addr, pack5, BYTES_PER_POINT_CT);

	iteration_counter++;

	if (iteration_counter == rawPoints)
	{
		iteration_counter = 0;
		recording_status = COMPLETE;
		return;
	}
}



// 
// void ADC_SPI::fill_ct_buffer_completely()
// {
// 
// 	if (recording_status == COMPLETE) return;
// 
// 	static uint16_t iteration_counter = 0;
// 	// 	static uint8_t average_counter = 0;
// 	// 	static uint16_t average_Array[5] = {0};
// 	// 	static uint16_t average_Result[5] = {0};
// 	
// 
// 	// Phase A:
// 	adc_ct_rec(buf_ct);		// получили одно измерение россыпью бит
// 	fram_write_array(iteration_counter*12, buf_ct, 12); // Записали в FRAM
// 	
// 	iteration_counter++;
// 	// Проверяем номер вызова:
// 	if (iteration_counter == rawPoints)
// 	{										 // если он равен длине массива осциллограмм,
// 		iteration_counter = 0;				// то сбрасываем счётчик вызовов
// 		recording_status = COMPLETE;		// и меняем статус на "выполнено".
// 		return;
// 	}
// 
// 	return;
// }


inline void ADC_SPI::ct_decode_5bytes_to_3x12(const uint8_t in5[BYTES_PER_POINT_CT],
uint16_t ct[3])
{
	uint16_t ct0 = (uint16_t)((((uint16_t)in5[0]) << 4) | ((in5[1] >> 4) & 0x0Fu));
	uint16_t ct1 = (uint16_t)((((uint16_t)(in5[1] & 0x0Fu)) << 8) | (uint16_t)in5[2]);
	uint16_t ct2 = (uint16_t)((((uint16_t)in5[3]) << 4) | ((in5[4] >> 4) & 0x0Fu));

	ct[0] = (uint16_t)(ct0 & 0x0FFFu);
	ct[1] = (uint16_t)(ct1 & 0x0FFFu);
	ct[2] = (uint16_t)(ct2 & 0x0FFFu);
}


// Ф-ция заполняет заданный объём памяти нулями:
void ADC_SPI::flush_buffer(uint8_t *start_addr, uint16_t length){
	for (uint16_t i = 0; i < length; i++){
		*(start_addr + i) = 0x00;
	};
}



void clock_DOWN()
{
	set_pin_low(PORT_ADC_CLK, ADC_CLK);
	// 	asm volatile ("nop"); // 1 cycle
	// 	asm volatile ("nop"); // 1 cycle
	
}


void clock_UP()
{
	
	set_pin_high(PORT_ADC_CLK, ADC_CLK);
	//	asm volatile ("nop"); // 1 cycle
	//	asm volatile ("nop"); // 1 cycle
}

void invalidData(u8 buffer[])
{
	u8 * ptr = buffer;
	for (u8 bit = 0; bit < ADC_RES; bit++)
	{			// Цикл согласно разрядности АЦП.
		*ptr = 0b11111111;							// Сохраняем текущий бит всей группы АЦП.
		ptr++;										// Передвигаем указатель на следующий байт.
	}
}

// Ф-ция считывает одну выборку со всех трёх АЦП ТТ и сохраняет её в переданный буфер.
void ADC_SPI::adc_ct_rec(u8 buffer[])
{
	u8 * ptr = buffer;
	ADC_CS_LOW;										// АЦП начинает преобразование по спаду сигнала CS.

	ADC_CLOCK;										// Согласно документации,
	ADC_CLOCK;										// сначала идут три нулевых бита,
	ADC_CLOCK;										// их пропускаем.

	for (u8 bit = 0; bit < ADC_RES; bit++){			// Цикл согласно разрядности АЦП.
		ADC_CLOCK;									// Подаём на АЦП тактовый импульс для приёма бита.
		uint8_t pinfBits = PIN_ADC_CT & 0x07;  // Extract first 3 bits from PINF (0b00000111)
		uint8_t pineBits = (PIN_COILS & 0x07); // Extract first 3 bits from PINF and shift left by 3
		pineBits = pineBits << 3;


		*ptr = pineBits | pinfBits;  // Сохраняем текущий бит всей группы АЦП.
		ptr++;										// Передвигаем указатель на следующий байт.
	};



	ADC_CLOCK;										// Полная передача занимает 16 тактов (3 + 12 + 1).

	ADC_CS_HIGH;									// Возвращаем сигнал CS  обратно в HIGH.

	
}


void ADC_SPI::adc_ct_rec_pack5(uint8_t out5[BYTES_PER_POINT_CT])
{
	uint16_t ct0 = 0, ct1 = 0, ct2 = 0;   // A, B, C — по 12 бит

	ADC_CS_LOW;                 // старт конверсии по спаду CS

	// три "пустых" такта (у многих АЦП перед выдачей данных)
	ADC_CLOCK;
	ADC_CLOCK;
	ADC_CLOCK;

	// 12 значимых бит: MSB ? LSB
	for (uint8_t bit = 0; bit < ADC_RES; bit++)
	{
		ADC_CLOCK;                             // такт выдачи следующего бита АЦП
		uint8_t pins = (PIN_ADC_CT & 0x07u);  // взять 3 младших бита: A?bit0, B?bit1, C?bit2

		// накапливаем биты (MSB-first)
		ct0 = (uint16_t)((ct0 << 1) | ((pins >> 0) & 0x01u));
		ct1 = (uint16_t)((ct1 << 1) | ((pins >> 1) & 0x01u));
		ct2 = (uint16_t)((ct2 << 1) | ((pins >> 2) & 0x01u));
	}

	ADC_CLOCK;      // завершающий такт
	ADC_CS_HIGH;    // завершение транзакции

	// ---- УПАКОВКА 3?12 бит ? 5 байт ----
	// Схема:
	//  b0 = ct0[11:4]
	//  b1 = ct0[3:0] << 4 | ct1[11:8]
	//  b2 = ct1[7:0]
	//  b3 = ct2[11:4]
	//  b4 = ct2[3:0] << 4   (нижние 4 бита пока 0 — можно использовать под флаги/номер и т.п.)
	out5[0] = (uint8_t)((ct0 >> 4) & 0xFFu);
	out5[1] = (uint8_t)(((ct0 & 0x0Fu) << 4) | ((ct1 >> 8) & 0x0Fu));
	out5[2] = (uint8_t)(ct1 & 0xFFu);
	out5[3] = (uint8_t)((ct2 >> 4) & 0xFFu);
	out5[4] = (uint8_t)((ct2 & 0x0Fu) << 4);
}


// Ф-ция считывает одну выборку со всех девяти АЦП аналогового входа.
void ADC_SPI::adc_ai_rec(u8 buffer[])
{
	u8 * ptr = buffer;
	ADC_CS_LOW;										// АЦП начинает преобразование по спаду сигнала CS.

	ADC_CLOCK;										// Согласно документации,
	ADC_CLOCK;										// сначала идут три нулевых бита,
	ADC_CLOCK;										// их пропускаем.

	for (u8 bit = 0; bit < ADC_RES; bit++){			// Цикл согласно разрядности АЦП.
		ADC_CLOCK;									// Подаём на АЦП тактовый импульс для приёма бита.
		*ptr = PIN_ADC_AI1;							// Сохраняем текущий бит 1-й группы (АЦП с 1 по 8).
		ptr++;										// Передвигаем указатель на следующий байт.
		*ptr = PIN_ADC_AI2;							// Сохраняем текущий бит 2-й группы (9-й АЦП).
		ptr++;										// Передвигаем указатель на следующий байт.
	};

	ADC_CLOCK;										// Полная передача занимает 16 тактов (3 + 12 + 1).

	ADC_CS_HIGH;									// Возвращаем сигнал CS  обратно в HIGH.

	ADC_CLOCK;										// Минимальная пауза до следующего преобразования
	ADC_CLOCK;										// (перевода CS снова в LOW) составляет 4 такта CLK.
	ADC_CLOCK;										//
	ADC_CLOCK;										//
}


// Ф-ция вычленяет из buf_src smpl_qty значений для АЦП с номером adc_id.
void ADC_SPI::adc_ct_extract(u8 buf_src[], u16 buf_dst[], u8 adc_id, u8 smpl_qty)
{

	u8*	ptr_src		= (u8 *) buf_src;
	// 	u8*	ptr_dst		= (u8 *) buf_dst;
	u8	adc_mask	= 0;
	bit_set(adc_mask, adc_id);

	for (u8 smpl = 0; smpl < smpl_qty; smpl++){			// Перебираем по очереди все выборки.

		u16 adc_tmp = 0;								// Переменная-аккумулятор для формирования конечной выборки одного АЦП.

		for (u8 bit = 0; bit < 8; bit++){				// Цикл для получения первых 8-и бит одной выборки конкретного АЦП.
			if (*ptr_src & adc_mask){					// Если нужный нам бит равен "1"...
				adc_tmp++;								// ..то записываем "1" в младший бит "аккумулятора"...
			};
			adc_tmp <<= 1;								// ...и/или сдвигаем всё значение влево на единицу.
			ptr_src++;									// Перемещаем указатель на следующий "бит" текущей выборки.
		};

		for (u8 bit = 0; bit < (ADC_RES - 8); bit++){	// Цикл для получения оставшихся бит одной выборки конкретного АЦП.
			if (*ptr_src & adc_mask){					// Если нужный нам бит равен "1"...
				adc_tmp++;								// ..то записываем "1" в младший бит "аккумулятора"...
			};
			adc_tmp <<= 1;								// ...и/или сдвигаем всё значение влево на единицу.
			ptr_src++;									// Перемещаем указатель на следующий "бит" текущей выборки.
		};

		adc_tmp >>= 1;									// Отменяем последний сдвиг, т.к. никакого следующего бита уже не будет.
		buf_dst[smpl] = adc_tmp;						// Сохраняем получившийся результат в массив выборок конкретного АЦП.
	};
}



u16 ADC_SPI::adc_ct_extract_single(u8 buf_src[], u8 adc_id){
	// Iterate over each byte in the array
	uint16_t result = 0;

	for (uint8_t j = 0; j < ADC_RES; ++j)
	{
		// Extract the first bit of the current byte and shift it to its position in the result
		result |= (bitRead(buf_src[j], adc_id)) << (ADC_RES - j - 1);
	}
	return result;
}



void ADC_SPI::adc_buf_flush(u8 *buffer, u16 buf_sz)
{
	for (u16 i = 0; i < buf_sz; i++){
		*buffer++ = 0;
	};
};




