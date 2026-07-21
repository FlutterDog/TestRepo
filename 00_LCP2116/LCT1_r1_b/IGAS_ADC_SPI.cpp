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
	DDRB |= (1<<PIN_SPI_MOSI);						// Set pin MOSI as OUTPUT
	//	DDRB &= ~(1<<PIN_SPI_MISO);						// Set pin MISO as INPUT
	DDRB |= (1<<PIN_SPI_SCK);						// Set pin SCK as OUTPUT
	DDRB |= (1<<PIN_SPI_SS)	;						// Set pin SS as OUTPUT

	PORTB |= (1<<PIN_SPI_MOSI);						//Set MOSI to HIGH
	PORTB |= (1<<PIN_SPI_SCK);						//Set SCK to HIGH
	// 	PORTB |= (1<<PIN_SPI_MISO);						//Set MISO to PULL-UP

	DDRB |= (1<<PIN_ADC1_CS)|
	(1<<PIN_ADC2_CS)|
	(1<<PIN_ADC4_CS)|
	(1<<PIN_ADC5_CS)|
	(1<<PIN_ADC6_CS);
	DDRD |= (1<<PIN_ADC3_CS);

	PORTB |= (1<<PIN_ADC1_CS)|
	(1<<PIN_ADC2_CS)|
	(1<<PIN_ADC4_CS)|
	(1<<PIN_ADC5_CS)|
	(1<<PIN_ADC6_CS);
	PORTD |= (1<<PIN_ADC3_CS);
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


	//	adc_ct_rec(buf_ct);		// получили одно измерение россыпью бит

	if (amplification)
	{
		get_single_channel_dataLCT1((uint8_t*)&osc_buffer[0][PHASE_C], ADC1);	// взяли из buf_ct данные и преобразовали в измерения для канала ADC_CT_1
		//
		get_single_channel_dataLCT1((uint8_t*)&osc_buffer[0][PHASE_B], ADC3);	// взяли из buf_ct данные и преобразовали в измерения для канала
		//
		get_single_channel_dataLCT1((uint8_t*)&osc_buffer[0][PHASE_A], ADC5);	// взяли из buf_ct данные и преобразовали в измерения для канала
	}
	else
	{
		get_single_channel_dataLCT1((uint8_t*)&osc_buffer[0][PHASE_C], ADC2);	// взяли из buf_ct данные и преобразовали в измерения для канала ADC_CT_1
		//
		get_single_channel_dataLCT1((uint8_t*)&osc_buffer[0][PHASE_B], ADC4);	// взяли из buf_ct данные и преобразовали в измерения для канала
		//
		get_single_channel_dataLCT1((uint8_t*)&osc_buffer[0][PHASE_A], ADC6);	// взяли из buf_ct данные и преобразовали в измерения для канала
	}

}


// uint16_t ADC_SPI::convertSineToSemi(int16_t ADC_point, uint8_t shift_)
// {
// 	uint16_t  buffer_;
//
// 	buffer_ = abs(ADC_point - ADC_MID - 128 + shift_);  // привели среднюю точку синуса с ADC MID к нулю. взяли абсолютное значение с ADC
//
// 	return buffer_;
// }



// Ф-ция получает данные с 3-х АЦП нормального режима данные и каждый вызов последовательно
// складывает эти данные в массив осциллограмм.
// Если АЦП НР выйдет на максимум, то значения начнут считываться с парного ему АЦП КЗ,
// а в массиве switching_iteration запишется номер вызова, на котором произошло переключение.
// ВНИМАНИЕ! Обратного переключения с АЦП КЗ на АЦП НР НЕ ПРОИСХОДИТ!

uint16_t checkBuf = 0;

void ADC_SPI::fill_ct_buffer_completely(int16_t osc_buffer[][phase_quantity], uint8_t BLOCK_GAIN_)
{

	if (recording_status == COMPLETE) return;

	static uint16_t iteration_counter = 0;
	// 	static uint8_t average_counter = 0;
	// 	static uint16_t average_Array[5] = {0};
	// 	static uint16_t average_Result[5] = {0};
	

	// Phase A:
	//	adc_ct_rec(buf_ct);		// получили одно измерение россыпью бит
	if (BLOCK_GAIN_)
	{
		get_single_channel_dataLCT1((uint8_t*)&osc_buffer[iteration_counter][PHASE_C], ADC1);	// взяли из buf_ct данные и преобразовали в измерения для канала ADC_CT_1
		//
		get_single_channel_dataLCT1((uint8_t*)&osc_buffer[iteration_counter][PHASE_B], ADC3);	// взяли из buf_ct данные и преобразовали в измерения для канала
		//
		get_single_channel_dataLCT1((uint8_t*)&osc_buffer[iteration_counter][PHASE_A], ADC5);	// взяли из buf_ct данные и преобразовали в измерения для канала
	}
	else
	{
		get_single_channel_dataLCT1((uint8_t*)&osc_buffer[iteration_counter][PHASE_C], ADC2);	// взяли из buf_ct данные и преобразовали в измерения для канала ADC_CT_1
		//
		get_single_channel_dataLCT1((uint8_t*)&osc_buffer[iteration_counter][PHASE_B], ADC4);	// взяли из buf_ct данные и преобразовали в измерения для канала
		//
		get_single_channel_dataLCT1((uint8_t*)&osc_buffer[iteration_counter][PHASE_A], ADC6);	// взяли из buf_ct данные и преобразовали в измерения для канала
	}
	
	//	fram_write_array(iteration_counter*12, buf_ct, 12); // Записали в FRAM
	
	iteration_counter++;
	// Проверяем номер вызова:
	if (iteration_counter == osc_length)
	{										 // если он равен длине массива осциллограмм,
		iteration_counter = 0;				// то сбрасываем счётчик вызовов
		recording_status = COMPLETE;		// и меняем статус на "выполнено".
		return;
	}

	return;
	
	
	// 		set_pin_high(PORT_ADC_CT1_GAIN, ADC_CT1_GAIN);
	// 		set_pin_high(PORT_ADC_CT2_GAIN, ADC_CT2_GAIN);
	// 		set_pin_high(PORT_ADC_CT3_GAIN, ADC_CT3_GAIN);
	// 		amplification = 1;
	
	//
	//
	// 	osc_buffer[iteration_counter][PHASE_A] += adc_ct_extract_single(buf_ct, ADC1);  // преобразовали биты параллельного порта в байты измерений
	// 	osc_buffer[iteration_counter][PHASE_B] += adc_ct_extract_single(buf_ct, ADC2);
	// 	osc_buffer[iteration_counter][PHASE_C] += adc_ct_extract_single(buf_ct, ADC3);
	// 	average_counter++;
	//
	//
	// 	if (average_counter >= 3)
	// 	{										 // если он равен длине массива осциллограмм,
	// 		average_counter = 0;				// то сбрасываем счётчик вызовов
	// 		iteration_counter++;
	// 		// Проверяем номер вызова:
	// 		if (iteration_counter == osc_length)
	// 		{										 // если он равен длине массива осциллограмм,
	// 			iteration_counter = 0;				// то сбрасываем счётчик вызовов
	// 			recording_status = COMPLETE;		// и меняем статус на "выполнено".
	// 		}
	//
	//
	// 	}


	
}


/*
// Function gets data from six ADC channels and put it into the array:
// void ADC_SPI::get_all_adc_data(void){
// 	for (uint8_t current_adc = 0; current_adc < adc_spi_quantity; current_adc++){
// 		spi_get_single_channel_data((uint8_t *)&adc_spi_data[current_adc], current_adc);
// 	};
// }


// Function completely fills the oscillogram array:
// void ADC_SPI::get_full_measurement(void){
// 	for (uint16_t osc_number = 0; osc_number < osc_spi_length; osc_number++){
// 		for (uint8_t current_adc = 0; current_adc < adc_spi_quantity; current_adc++){
// 			spi_get_single_channel_data((uint8_t *)&adc_spi_oscillogram[osc_number][current_adc], current_adc);
// 		};
// 	};
// }
*/


// Ф-ция заполняет заданный объём памяти нулями:
void ADC_SPI::flush_buffer(uint8_t *start_addr, uint16_t length){
	for (uint16_t i = 0; i < length; i++){
		*(start_addr + i) = 0x00;
	};
}


//
// void ADC_SPI::adc_ct_rec()
// {
// 	ADC_CS_LOW;										// АЦП начинает преобразование по спаду сигнала CS.
//
// 	ADC_CLOCK;										// Согласно документации,
// 	ADC_CLOCK;										// сначала идут три нулевых бита,
// 	ADC_CLOCK;										// их пропускаем.
//
// 	for (u8 bit = 0; bit < ADC_RES; bit++){			// Цикл согласно разрядности АЦП.
// 		ADC_CLOCK;									// Подаём на АЦП тактовый импульс для приёма бита.
// 		 *buffer_ptr = PIN_ADC_CT; 							// Сохраняем текущий бит всей группы АЦП.
// 		 buffer_ptr++;										// Передвигаем указатель на следующий байт.
// 	};
//
// 	ADC_CLOCK;										// Полная передача занимает 16 тактов (3 + 12 + 1).
//
// 	ADC_CS_HIGH;									// Возвращаем сигнал CS  обратно в HIGH.
//
// 	// 	ADC_CLOCK;										// Минимальная пауза до следующего преобразования
// 	// 	ADC_CLOCK;										// (перевода CS снова в LOW) составляет 4 такта CLK.
// 	// 	ADC_CLOCK;										//
// 	// 	ADC_CLOCK;										//
// }

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

// void ADC_SPI::adc_ct_rec(u8 buffer[])
// {
// 	u8 * ptr = buffer;
// 	ADC_CS_LOW;										// АЦП начинает преобразование по спаду сигнала CS.
//  	asm volatile ("nop"); // 1 cycle
//  	asm volatile ("nop"); // 1 cycle
//  	clock_DOWN();
//
// 	if (PIN_ADC_CT)
// 	{
// 		invalidData(buffer);
// 		return;
// 	}
// 	clock_UP();
//
// 	clock_DOWN();
// 	if (PIN_ADC_CT)
// 	{
// 		invalidData(buffer);
// 		return;
// 	}
// 	clock_UP();
// 	clock_DOWN();
// 	if (PIN_ADC_CT)
// 	{
// 		invalidData(buffer);
// 		return;
// 	}
// 	clock_UP();
// 	// их пропускаем.
//
// 	for (u8 bit = 0; bit < ADC_RES; bit++){			// Цикл согласно разрядности АЦП.
// 		clock_DOWN();									// Подаём на АЦП тактовый импульс для приёма бита.
// 		*ptr = PIN_ADC_CT;							// Сохраняем текущий бит всей группы АЦП.
// 		clock_UP();
// 		ptr++;										// Передвигаем указатель на следующий байт.
// 	};
//
// 	clock_DOWN();
//
// 	clock_UP();										// Полная передача занимает 16 тактов (3 + 12 + 1).
//
// 	ADC_CS_HIGH;									// Возвращаем сигнал CS  обратно в HIGH.
//
// 	// 	ADC_CLOCK;										// Минимальная пауза до следующего преобразования
// 	// 	ADC_CLOCK;										// (перевода CS снова в LOW) составляет 4 такта CLK.
// 	// 	ADC_CLOCK;										//
// 	// 	ADC_CLOCK;										//
// }

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
		*ptr = PIN_ADC_CT;							// Сохраняем текущий бит всей группы АЦП.
		ptr++;										// Передвигаем указатель на следующий байт.
	};

	ADC_CLOCK;										// Полная передача занимает 16 тактов (3 + 12 + 1).

	ADC_CS_HIGH;									// Возвращаем сигнал CS  обратно в HIGH.

	// 	ADC_CLOCK;										// Минимальная пауза до следующего преобразования
	// 	ADC_CLOCK;										// (перевода CS снова в LOW) составляет 4 такта CLK.
	// 	ADC_CLOCK;										//
	// 	ADC_CLOCK;										//
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
//
// u16 ADC_SPI::adc_ct_extract_single(u8 buf_src[], u8 adc_id){
//
// 	u8*	ptr_src	= buf_src;
// 	u8	adc_mask = 0;
// 	bit_set(adc_mask, adc_id);
//
// 	u16 adc_tmp = 0;								// Переменная-аккумулятор для формирования конечной выборки одного АЦП.
//
// 	for (u8 bit = 0; bit < 8; bit++){				// Цикл для получения первых 8-и бит одной выборки конкретного АЦП.
// 		if (*ptr_src & adc_mask){					// Если нужный нам бит равен "1"...
// 			adc_tmp++;								// ..то записываем "1" в младший бит "аккумулятора"...
// 		};
// 		adc_tmp <<= 1;								// ...и/или сдвигаем всё значение влево на единицу.
// 		ptr_src++;									// Перемещаем указатель на следующий "бит" текущей выборки.
// 	};
//
// 	for (u8 bit = 0; bit < (ADC_RES - 8); bit++){	// Цикл для получения оставшихся бит одной выборки конкретного АЦП.
// 		if (*ptr_src & adc_mask){					// Если нужный нам бит равен "1"...
// 			adc_tmp++;								// ..то записываем "1" в младший бит "аккумулятора"...
// 		};
// 		adc_tmp <<= 1;								// ...и/или сдвигаем всё значение влево на единицу.
// 		ptr_src++;									// Перемещаем указатель на следующий "бит" текущей выборки.
// 	};
//
// 	adc_tmp >>= 1;									// Отменяем последний сдвиг, т.к. никакого следующего бита уже не будет.
//
// 	// Преобразование из Litte-Endian в Big Endian: 		// !!!!!
// 	u8 tmpL = adc_tmp;	// L
// 	adc_tmp >>= 8;
// 	u8 tmpM = adc_tmp;	// M
// 	adc_tmp = tmpL;
// 	adc_tmp <<= 8;
// 	adc_tmp |= tmpM;
//
// 	return adc_tmp;									// Сохраняем получившийся результат в массив выборок конкретного АЦП.
// }



void ADC_SPI::adc_buf_flush(u8 *buffer, u16 buf_sz)
{
	for (u16 i = 0; i < buf_sz; i++){
		*buffer++ = 0;
	};
};


