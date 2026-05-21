
#include "sht31_i2c.h"



#define DEV_ADDR_R			0b10001001		// SHT31 Address 0x44 + Read Mode
#define DEV_ADDR_W			0b10001000		// SHT31 Address 0x44 + Write Mode
#define ACK					1				// Send ACK (ACK)
#define NACK				0				// Don't Send ACK (NACK)
#define i2c_wait			while(!(TWCR & (1 << TWINT)))	// Waiting I2C Transmission Complete





/*
TWBR	SCL			Frame time		ACK
====	===			==========		===
72		100kHz		209us			Yes
12		400kHz		63,5us			Yes
8		500kHz		53us			Yes
6		571kHz		48,75us			Yes
4		667kHz		43,75us			Yes
3		727kHz		41,375us		Yes
2		800kHz		NAK				NO
1		889kHz		NAK				NO
*/


void i2c_init(void)
{
	DDRC |= (1<<PORTC4)|(1<<PORTC5);		// SDA (C4) and SCL (C5) Pin Direction is OUTPUT
	PORTC |= (1<<PORTC4)|(1<<PORTC5);		// SDA (C4) and SCL (C5) Pin Mode is HIGH (LOG1)
	TWSR &= ~((1<<TWPS0)|(1<<TWPS1));		// I2C Prescaller is 1
	TWBR = 10;								// I2C bit rate is ~347 kHz (see table above)
}


void i2c_start(void)
{						// Perform START Condition
	TWCR = (1<<TWEN)|(1<<TWSTA)|(1<<TWINT);
	i2c_wait;
}


void i2c_stop(void)
{						// Perform STOP Condition
	TWCR = (1<<TWEN)|(1<<TWSTO)|(1<<TWINT);
}


void i2c_send(uint8_t data)
{				// Master Transmitter Mode
	TWDR = data;
	TWCR = (1<<TWEN)|(1<<TWINT);
	i2c_wait;
}


void i2c_get(uint8_t *data, uint8_t ack)
{	// Master Receiver Mode
	TWCR = (1<<TWINT)|(1<<TWEN);
	if (ack) TWCR |= (1<<TWEA);				// Generate ACK if Needed
	i2c_wait;
	*data = TWDR;
}

void sht31_measure_request(void)
{
	i2c_start();
	i2c_send(DEV_ADDR_W);
	i2c_send(0x24);
	i2c_send(0x00);
	i2c_stop();
}

void sht31_get_data(void)
{
	uint8_t TEMP_MSB;
	uint8_t TEMP_LSB;
	uint8_t HUM_MSB;
	uint8_t HUM_LSB;
	uint8_t CRC;

	i2c_start();

	i2c_send(DEV_ADDR_R);

	i2c_get(&TEMP_MSB, ACK);
	i2c_get(&TEMP_LSB, ACK);
	i2c_get(&CRC, ACK);
	i2c_get(&HUM_MSB, ACK);
	i2c_get(&HUM_LSB, NACK);

	i2c_stop();

	SHT31_TEMP = (TEMP_MSB << 8) + TEMP_LSB;			// Make 16-bit Unsigned Integer from Two 8-bit Ones
	SHT31_HUM = (HUM_MSB << 8) + HUM_LSB;
}

// 
// float calc_temperature(void)
// {
// 	return (-45.0 + 175.0 * (SHT31_TEMP / 0xFFFF));	// The Formula is T[C] = -45 + 175 * (RawData / (2^16 - 1))
// }
// 
// float calc_humidity(void)
// {
// 	return (100.0 * (SHT31_HUM / 0xFFFF));				// The Formula is RH(%) = 100 * (RawData / (2^16 - 1))
// }




/* Example:
int main(void)
{
i2c_init();
sht31_measure_request();
_delay_ms(13);					// Minimal Working Delay for Our Sensor Example
sht31_get_data();
}
*/
