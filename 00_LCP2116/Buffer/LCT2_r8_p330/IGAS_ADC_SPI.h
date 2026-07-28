#ifndef IGAS_ADC_SPI_H_
#define IGAS_ADC_SPI_H_

#include "main.h"
#include "osc_config.h"

#define	ADC_MID				2048

#define	PIN_SPI_SCK			PB1
#define	PIN_SPI_MOSI		PB2
#define	PIN_SPI_MISO		PB3
#define	PIN_SPI_SS			PB0

#define	PIN_ADC1_CS			PB7
#define	PIN_ADC2_CS			PB5
#define	PIN_ADC3_CS			PD0
#define	PIN_ADC4_CS			PB6
#define	PIN_ADC5_CS			PB4
#define	PIN_ADC6_CS			PB0

#define ADC_RES              12u            ///< разрядность АЦП (бит на канал)
#define BYTES_PER_POINT_CT    5u            ///< упаковка 3?12 бит в 5 байт
#define	ADC_SMPLS			1							// Количество оцифровок за один вызов.

// #define	ADC_PAUSE			10							// Пауза между оцифровками, мкс.
// #define	ADC_SETS			1							// Количество "наборов" АЦП. Один набор может содержать до 8-и АЦП (по количеству входов одного порта МК).
#define ADC_CT_MASK			0b00000111

#define ADC_CS_LOW			set_pin_low(PORT_ADC_CS, ADC_CS)
#define ADC_CS_HIGH			set_pin_high(PORT_ADC_CS, ADC_CS)
#define ADC_CLOCK			set_pin_low(PORT_ADC_CLK, ADC_CLK); set_pin_high(PORT_ADC_CLK, ADC_CLK)

#define ADC_CT1_GAIN_ON		set_pin_high(PORT_ADC_CT1_GAIN, ADC_CT1_GAIN)
#define ADC_CT1_GAIN_OFF	set_pin_low(PORT_ADC_CT1_GAIN, ADC_CT1_GAIN)
#define ADC_CT2_GAIN_ON		set_pin_high(PORT_ADC_CT2_GAIN, ADC_CT2_GAIN)
#define ADC_CT2_GAIN_OFF	set_pin_low(PORT_ADC_CT2_GAIN, ADC_CT2_GAIN)
#define ADC_CT3_GAIN_ON		set_pin_high(PORT_ADC_CT3_GAIN, ADC_CT3_GAIN)
#define ADC_CT3_GAIN_OFF	set_pin_low(PORT_ADC_CT3_GAIN, ADC_CT3_GAIN)
#define lowByte(w) ((uint8_t) ((w) & 0xff))
#define highByte(w) ((uint8_t) ((w) >> 8))

enum adc{
	ADC_CT_1,
	ADC_CT_2,
	ADC_CT_3,
	ADC_1 = 0,
	ADC_2,
	ADC_3,
	ADC_4,
	ADC_5,
	ADC_6,
	ADC_7,
	ADC_8,
	ADC_9,
};



#define	ADC1				0
#define	ADC2				1
#define	ADC3				2
#define	ADC4				3
#define	ADC5				4
#define	ADC6				5






#define adc_spi_max			3000 //4095				// Maximum value for ADC (12 bit)
#define adc_spi_min			1000
#define adc_min				0					// Minimum value forADC

#define rawPoints			588					// A number of oscillograms to recordю.Каждый замер 68 мс, чтоб набрать 40 мс нужно сделать 588 замеров и осреднить их на 3 измерения. получим 196 точек

#define averPoints			3				// Averaged points


#define ring_buffer_window	25					// Size of the tracking window

#define FULL_FILLING		1					//
#define TRACKING_WINDOW		0					//

#define ON					1
#define OFF					0

#define IDLE				0
#define BUSY				1
#define INPROGRESS			1
#define COMPLETE			0xFF
#define FULL				0xFF

enum
{
	PHASE_A,
	PHASE_B,
	PHASE_C,
	PHASE_A_GAIN,
	PHASE_B_GAIN,
	PHASE_C_GAIN,
	SOL_ON,
	SOL_OFF_1,
	SOL_OFF_2,
};

#define phase_quantity		3					// Total phase quantity

#define NONE				0
#define FIRST				1
#define SECOND				2
#define BOTH				3
#define bufNum				2
//extern uint8_t GLOBAL_MODE;

/// Версия формата для контроля совместимости
#define OSC_HDR_VERSION               1u


class ADC_SPI
{
	public:
	
	void spi_init(void);
	void gpio_init(void);
	void get_single_channel_data(uint8_t *data_address, uint16_t buf_ct16);
	void get_single_channel_dataLCT1(uint8_t *data_address, uint8_t adc_id);
	uint16_t get_single_channel_dataX(uint8_t *data_address, uint8_t adc_id);
	void get_ct_values_in_idle_mode(int16_t osc_buffer[][phase_quantity]);
	void fill_ct_buffer_completely();
	void flush_buffer(uint8_t *start_addr, uint16_t length);
	void adc_buf_flush(u8 *buffer, u16 buf_sz);
	void adc_ct_extract(u8 buf_src[], u16 buf_dst[], u8 adc_id, u8 smpl_qty);
	uint16_t adc_ct_extract_single(u8 buf_src[], u8 adc_id);
	void adc_ai_rec(u8 buffer[]);
	void adc_ct_rec(u8 buffer[]);
	void adc_ct_rec_pack5(uint8_t out5[BYTES_PER_POINT_CT]);
	inline void ct_decode_5bytes_to_3x12(const uint8_t in5[BYTES_PER_POINT_CT],uint16_t ct[3]);
	
	//void clock_adc_pin();
	//	void invalidData(u8 buffer[]);
	
	//void adc_ct_rec();
	//uint16_t convertSineToSemi(int16_t ADC_point, uint8_t shift_, float low_k, float high_k);



	// 		void get_all_adc_data(void);
	// 		void get_full_measurement(void);

//	int16_t	[osc_length][phase_quantity];	// An array to keep the oscillograms
//	int16_t	calcOscBuffer[PHASE_SAMPLES_MAX][phase_quantity];
	
	//	uint8_t* buffer_ptr = (uint8_t*)osc_buffer_1; // Pointer to the buffer
	//	uint8_t		adc_switch[adc_quantity];					// ON(1) / OFF (0) flags for each ADC (Набор флагов ВКЛ(1) / ВЫКЛ(0) для каждого АЦП)
	uint8_t		active_buffer_id;							// Which buffer is active now (FIRST, SECOND or NONE)
	uint8_t		recording_status;							// Status flag for oscillogram recording
	uint8_t    adcShift_A;							// Status flag for oscillogram recording
	uint8_t    adcShift_B;							// Status flag for oscillogram recording
	uint8_t    adcShift_C;							// Status flag for oscillogram recording
	uint8_t    gainAdcShift_A;							// Status flag for oscillogram recording
	uint8_t    gainAdcShift_B;							// Status flag for oscillogram recording
	uint8_t    gainAdcShift_C;							// Status flag for oscillogram recording
	
	uint8_t amplification;
	
	uint16_t solenoidCurrOn_RAW;
	uint16_t solenoidCurrOff_1_RAW;
	uint16_t solenoidCurrOff_2_RAW;
	
	
	uint16_t SOLarr[9];
	uint16_t solarr9;
	
	private:
};


#endif /* IGAS_ADC_SPI_H_ */