/*
* Project: Lightweight millisecond tracking library
* Author: Zak Kemble, contact@zakkemble.net
* Copyright: (C) 2018 by Zak Kemble
* License: GNU GPL v3 (see License_GPL-3.0.txt) or MIT (see License_MIT.txt)
* Web: http://blog.zakkemble.net/millisecond-tracking-library-for-avr/
*/

#ifndef IGAS_MILLIS_H_
#define IGAS_MILLIS_H_


#include "main.h"


/**
* Milliseconds data type \n
* Data type				- Max time span			- Memory used \n
* unsigned char			- 255 milliseconds		- 1 byte \n
* unsigned int			- 65.54 seconds			- 2 bytes \n
* uint32_t			- 49.71 days			- 4 bytes \n
* uint32_t long	- 584.9 million years	- 8 bytes
*/

//#define millis() millis_get()

#ifdef __cplusplus
	extern "C" {
#endif

	void		millis_init(void);	// Init timer1 = 1ms
	uint32_t	millis (void);		// returm ms from start

#ifdef __cplusplus
	}
#endif


#endif /* MILLIS_H_ */