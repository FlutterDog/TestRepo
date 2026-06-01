#include "app.hpp"
#include "../board/lcp_board.hpp"
#include "../platform/platform.hpp"
#include "../libs/SC16IS7xx/SC16IS7xx.h"

static SpiUartDevice UARTX;

void setup(void)
{
	lcp_board_init_gpio();

	Serial.begin(9600U, SERIAL_8N1);
	Serial1.begin(9600U, SERIAL_8N1);
	Serial2.begin(9600U, SERIAL_8N1);
	Serial3.begin(9600U, SERIAL_8N1);

	SerialUSB.begin(115200U, SERIAL_8N1);

	SPI.begin();

	UARTX.begin(9600UL, 0U, LCP_PIN_UART1_CS, CH_A);

	const boolean sc16_connected = UARTX.uartConnected(LCP_PIN_UART1_CS, CH_A);
	(void)sc16_connected;
}

void loop(void)
{
	delay(1U);
}