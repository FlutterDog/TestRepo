#ifndef MAIN_H_
#define MAIN_H_

#define F_CPU 16000000UL


// Library headers:
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <avr/wdt.h>
#include <math.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include "snb_definitions.h"
#include "snb_macros.h"


#include "HardwareSerial.h"
#include "IGAS_ADC_SPI.h"
#include "IGAS_Interrupts.h"
#include "IGAS_485.h"
#include "IGAS_millis.h"
//#include "fram_spi.h"


// ---- Definitions:



#define CB1_ON  0b00000001  // Braker 1 ON bit (bit 0)
#define CB1_OFF 0b00000010  // Braker 1 OFF bit (bit 1)

#define CB2_ON  0b00000100  // Braker 2 ON bit (bit 2)
#define CB2_OFF 0b00001000  // Braker 2 OFF bit (bit 3)

#define CB3_ON  0b00010000  // Braker 3 ON bit (bit 4)
#define CB3_OFF 0b00100000  // Braker 3 OFF bit (bit 5)



#define lastStrike		0
#define OVF				1
#define CALC			2

enum breakerState {
	cb_NA,      // состояние когда оба концевика нулевые. 
	cb_On_Off,  // СОстояние когда выключился концевик On и значит мы ждем OFF
	cb_Off_On,  // СОстояние когда выключился концевик Off и значит мы ждем On
	cb_Off,     // Выключатель выключен
	cb_On       // Выключатель включен
};


//#define osc_int_length		30
#define adc_int_quantity	3
#define adc_int_max			1024			// Maximum value for internal ADC

#define LOOP_TIME			500				// Длина цикла - 500 мс
#define CONN_TIMEOUT		5000			// Уставка таймаута с момента последнего принятого по RS485 пакета, адресованного этому модулю.
#define OSC_REC_PERIOD		68

#define MAX_REGISTER_ID		10001

#define NO_MIN_VALUE		0

#define SQRT2				141

// Порты ввода-вывода:
#ifndef GPIO_PINS
#define MOSI				PINB2
#define MISO				PINB3
#define SCK					PINB1
#define SS					PINB0
#define FRAM_CS				PINB4
#define SDA					PIND1
#define SCL					PIND0
#define RX					PIND2
#define TX					PIND3
#define TX_EN				PIND4
#define ADC_CS				PIND5
#define ADC_CLK				PIND6
#define ADC_CT1				PINF0
#define ADC_CT2				PINF1
#define ADC_CT3				PINF2
#define ADC_CT1_GAIN		PINF5
#define ADC_CT2_GAIN		PINF6
#define ADC_CT3_GAIN		PINF7
#define ADC_A1				PINE0
#define ADC_A2				PINE1
#define ADC_A3				PINE2
#define ADC_A4				PINE3
#define ADC_A5				PINE4
#define ADC_A6				PINE5
#define ADC_A7				PINE6
#define ADC_A8				PINE7
#define ADC_A9				PINF3
#define ETH_CS				PINB0
#define ETH_IRQ				PIND7
#define LED_OK				PINB5
#define LED_X2X				PINB6
#define LED_BUF				PINB7
#define	PIN_DI				PINA
#define DI_1				PINA0
#define DI_2				PINA1
#define DI_3				PINA2
#define DI_4				PINA3
#define DI_5				PINA4
#define DI_6				PINA5
#define BIT_0				PINC0
#define BIT_1				PINC1
#define BIT_2				PINC2
#define BIT_3				PINC3
#define BIT_4				PINC4
#define BIT_5				PINC5
#define BIT_6				PINC6
#define BIT_7				PINC7
#define BIT_8				PINA6
#define BIT_9				PINA7
#define ADDR1				PING0
#define ADDR2				PING1
#define ADDR3				PING2
#define ADDR4				PING3
#define ADDR5				PING4

#define DDR_MOSI			DDRB
#define DDR_MISO			DDRB
#define DDR_SCK				DDRB
#define DDR_SS				DDRB
#define DDR_FRAM_CS			DDRB
#define DDR_SDA				DDRD
#define DDR_SCL				DDRD
#define DDR_RX				DDRD
#define DDR_TX				DDRD
#define DDR_TX_EN			DDRD
#define DDR_ADC_CS			DDRD
#define DDR_ADC_CLK			DDRD
#define DDR_ADC_CT			DDRF
#define DDR_ADC_CT1			DDRF
#define DDR_ADC_CT2			DDRF
#define DDR_ADC_CT3			DDRF
#define DDR_ADC_CT1_GAIN	DDRF
#define DDR_ADC_CT2_GAIN	DDRF
#define DDR_ADC_CT3_GAIN	DDRF
#define DDR_ADC_A1			DDRE
#define DDR_ADC_A2			DDRE
#define DDR_ADC_A3			DDRE
#define DDR_ADC_A4			DDRE
#define DDR_ADC_A5			DDRE
#define DDR_ADC_A6			DDRE
#define DDR_ADC_A7			DDRE
#define DDR_ADC_A8			DDRE
#define DDR_ADC_A9			DDRF
#define DDR_ETH_CS			DDRB
#define DDR_ETH_IRQ			DDRD
#define DDR_LED_OK			DDRB
#define DDR_LED_X2X			DDRB
#define DDR_LED_BUF			DDRB
#define DDR_DI_1			DDRA
#define DDR_DI_2			DDRA
#define DDR_DI_3			DDRA
#define DDR_DI_4			DDRA
#define DDR_DI_5			DDRA
#define DDR_DI_6			DDRA
#define DDR_ENC				DDRC
#define DDR_ENC_BIT_0		DDRC
#define DDR_ENC_BIT_1		DDRC
#define DDR_ENC_BIT_2		DDRC
#define DDR_ENC_BIT_3		DDRC
#define DDR_ENC_BIT_4		DDRC
#define DDR_ENC_BIT_5		DDRC
#define DDR_ENC_BIT_6		DDRC
#define DDR_ENC_BIT_7		DDRC
#define DDR_ENC_BIT_8		DDRA
#define DDR_ENC_BIT_9		DDRA
#define DDR_ADDR1			DDRG
#define DDR_ADDR2			DDRG
#define DDR_ADDR3			DDRG
#define DDR_ADDR4			DDRG
#define DDR_ADDR5			DDRG

#define PORT_MOSI			PORTB
#define PORT_MISO			PORTB
#define PORT_SCK			PORTB
#define PORT_SS				PORTB

#define PORT_SDA			PORTD
#define PORT_SCL			PORTD
#define PORT_RX				PORTD
#define PORT_TX				PORTD
#define PORT_TX_EN			PORTD
#define PORT_ADC_CS			PORTD
#define PORT_ADC_CLK		PORTD
#define PORT_ADC_CT			PORTF
#define PORT_ADC_CT1		PORTF
#define PORT_ADC_CT2		PORTF
#define PORT_ADC_CT3		PORTF
#define PORT_ADC_CT1_GAIN	PORTF
#define PORT_ADC_CT2_GAIN	PORTF
#define PORT_ADC_CT3_GAIN	PORTF
#define PORT_ADC_AI1		PORTE
#define PORT_ADC_AI2		PORTF
#define PORT_ADC_A1			PORTE
#define PORT_ADC_A2			PORTE
#define PORT_ADC_A3			PORTE
#define PORT_ADC_A4			PORTE
#define PORT_ADC_A5			PORTE
#define PORT_ADC_A6			PORTE
#define PORT_ADC_A7			PORTE
#define PORT_ADC_A8			PORTE
#define PORT_ADC_A9			PORTF
#define PORT_ETH_CS			PORTB
#define PORT_ETH_IRQ		PORTD
#define PORT_LED_OK			PORTB
#define PORT_LED_X2X		PORTB
#define PORT_LED_BUF		PORTB
#define PORT_DI_1			PORTA
#define PORT_DI_2			PORTA
#define PORT_DI_3			PORTA
#define PORT_DI_4			PORTA
#define PORT_DI_5			PORTA
#define PORT_DI_6			PORTA
#define PORT_ENC			PORTC
#define PORT_ENC_BIT_0		PORTC
#define PORT_ENC_BIT_1		PORTC
#define PORT_ENC_BIT_2		PORTC
#define PORT_ENC_BIT_3		PORTC
#define PORT_ENC_BIT_4		PORTC
#define PORT_ENC_BIT_5		PORTC
#define PORT_ENC_BIT_6		PORTC
#define PORT_ENC_BIT_7		PORTC
#define PORT_ENC_BIT_8		PORTA
#define PORT_ENC_BIT_9		PORTA
#define PORT_ADDR1			PORTG
#define PORT_ADDR2			PORTG
#define PORT_ADDR3			PORTG
#define PORT_ADDR4			PORTG
#define PORT_ADDR5			PORTG

#define PIN_MOSI			PINB
#define PIN_MISO			PINB
#define PIN_SCK				PINB
#define PIN_SS				PINB

#define PIN_SDA				PIND
#define PIN_SCL				PIND
#define PIN_RX				PIND
#define PIN_TX				PIND
#define PIN_TX_EN			PIND
#define PIN_ADC_CS			PIND
#define PIN_ADC_CT			PINF
#define PIN_ADC_CLK			PIND
#define PIN_ADC_CT1			PINF
#define PIN_ADC_CT2			PINF
#define PIN_ADC_CT3			PINF
#define PIN_ADC_CT1_GAIN	PINF
#define PIN_ADC_CT2_GAIN	PINF
#define PIN_ADC_CT3_GAIN	PINF
#define PIN_ADC_AI1			PINE
#define PIN_ADC_AI2			PINF
#define PIN_COILS			PINE
#define PIN_ADC_A1			PINE
#define PIN_ADC_A2			PINE
#define PIN_ADC_A3			PINE
#define PIN_ADC_A4			PINE
#define PIN_ADC_A5			PINE
#define PIN_ADC_A6			PINE
#define PIN_ADC_A7			PINE
#define PIN_ADC_A8			PINE
#define PIN_ADC_A9			PINF
#define PIN_ETH_CS			PINB
#define PIN_ETH_IRQ			PIND
#define PIN_LED_OK			PINB
#define PIN_LED_X2X			PINB
#define PIN_LED_BUF			PINB
//#define PIN_DI_1			PINA
//#define PIN_DI_2			PINA
//#define PIN_DI_3			PINA
//#define PIN_DI_4			PINA
//#define PIN_DI_5			PINA
//#define PIN_DI_6			PINA
#define PIN_BIT_0			PINC
#define PIN_BIT_1			PINC
#define PIN_BIT_2			PINC
#define PIN_BIT_3			PINC
#define PIN_BIT_4			PINC
#define PIN_BIT_5			PINC
#define PIN_BIT_6			PINC
#define PIN_BIT_7			PINC
#define PIN_BIT_8			PINA
#define PIN_BIT_9			PINA
#define PIN_ADDR1			PING
#define PIN_ADDR2			PING
#define PIN_ADDR3			PING
#define PIN_ADDR4			PING
#define PIN_ADDR5			PING
#endif

#define FRAM_CMD_WREN	0b00000110	// Write Enable
#define FRAM_CMD_WRDI	0b00000100	// Reset Write Enable
#define FRAM_CMD_RDSR	0b00000101	// Read Status Register
#define FRAM_CMD_WRSR	0b00000001	// Write Status Register
#define FRAM_CMD_READ	0b00000011	// Read Memory Code
#define FRAM_CMD_WRITE	0b00000010	// Write Memory Code
#define FRAM_CMD_RDID	0b10011111	// Read Device ID

#define FRAM_CS_ON	set_pin_low(PORT_FRAM_CS, FRAM_CS)
#define FRAM_CS_OFF	set_pin_high(PORT_FRAM_CS, FRAM_CS)

#define PORT_FRAM_CS		PORTB


// Почему это значение не подхватывается из "IGAS_ADC_SPI.h"?
#define phase_quantity		3				// Total phase quantity
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))

//#include "adc_soft-spi.h"



// ---- Function Prototypes:
void		gpio_init(void);
void		comm_init(void);
void		loop();
void		modbus_timeout_check();
void modbusProceed(int16_t *regs, uint16_t addr_base, uint16_t reg_qty_, int16_t Value_);
//void		timer_routines(int16_t buffer[][phase_quantity]);

uint8_t		check_DI_status(void);
void		finding_Current_Start_Point();
void		calcArcLifetime(void);
void		calcFullTime();
void		findShortCircuit();
void		calcLast_kA2s();

void		calculations();
//void		moveFramToBuf();
void		loadSettings();

void		calcSwitchCurrent();
void sendByte(uint8_t x_);
void		getRMS(int16_t osc_buffer[][phase_quantity], uint8_t command_);
int64_t		sqrForRMS(int16_t osc_buffer[][phase_quantity], uint8_t phase_, uint8_t shift_);
void		calcTimeStates();
//void		adc_buf_flush(u8 *buffer, u16 buf_sz);
//void		adc_ai_rec(u8 buffer[]);
int convertFloat(float flt_, uint8_t oreder_);
float loadFloat(int addr_, float default_);
uint16_t myAbs(int16_t x);
void saveFloat(int addr_, float Value_);
uint16_t calcOwnTime(unsigned long cbTimer_X);
uint8_t checkSwitchFin();
void calcSwitchTimes();

void fram_init(void);
// u8 fram_cmd(u8 command);
u8 fram_read_byte(u16 addr);
// void fram_write(u16 addr, u8 data);
//u8 fram_read_byte(u16 addr);
void fram_write_byte(u16 addr, u8 data);
//u8 fram_cmd(u8 command);
void fram_write_array(u16 addr, u8* data, u16 length);
void fram_read_array(u16 addr, u8* data, u16 length);
void process_adc_data(int16_t osc_buffer_[][3], uint16_t num_measurements, uint8_t averPoints_);
uint8_t checkConsistensy(uint8_t phase_);
breakerState getBreakerState(uint8_t curr_state, uint8_t open_mask, uint8_t closed_mask, breakerState CB_State_);
void loadDefaults();
void averageIt();
void trendBuffer();
void PIDfilter();
void RMS_calculations();
void updateSwitchCnt();
int16_t getSingleFramValue(uint16_t address_, uint8_t channel_);
//void putDataToBuffers();
//void getDataFromBuffers();
void calibrFromEEPROM(byte pointer);
void calibrToEEPROM(byte channel_);
void captureFinContactTime();
void captureContactTimes();
void update_isr_microtimer(void);
void uart1_quiet_enter(void);
void uart1_quiet_leave(void);
void osc_begin_strike_capture(uint8_t strike_index,uint8_t  aux_type,uint8_t  last_op);
void osc_end_strike_capture(void);
void record_scope(void);



// Дополнительные заголовочные файлы:
//#include "snb_serial-hw_m128.h"
//#include "fram_spi.h"

#endif /* MAIN_H_ */