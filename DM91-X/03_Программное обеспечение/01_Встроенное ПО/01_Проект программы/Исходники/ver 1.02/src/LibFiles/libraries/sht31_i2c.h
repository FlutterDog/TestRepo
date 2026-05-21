#ifndef SHT31_I2C_H
#define SHT31_I2C_H

#include <avr/io.h>

#endif

#ifdef __cplusplus
extern "C"{
	#endif

	void i2c_init(void);
	void i2c_start(void);
	void i2c_stop(void);
	void i2c_send(uint8_t);
	void i2c_get(uint8_t *, uint8_t);

	void sht31_measure_request(void);
	void sht31_get_data(void);

	volatile uint16_t	SHT31_TEMP;		// Raw Temperature Data from the Sensor
	volatile uint16_t	SHT31_HUM;		// Raw Humidity Data from the Sensor

	// 	uint16_t calc_temperature(void);
	// 	uint16_t calc_humidity(void);

	#ifdef __cplusplus
} // extern "C"
#endif