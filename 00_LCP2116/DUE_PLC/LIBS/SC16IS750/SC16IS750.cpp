#include "SC16IS750.h"


// Extra Features Register (EFCR)
#define EFCR_ENABLE_RTS 1 << 4
#define EFCR_INVERSION 1 << 5
#define EFCR_MULTIDROP 0x01

// Line Control Register (LCR)
#define LCR_ENABLE_DIVISOR_LATCH 1 << 7
#define LCR_ENABLE_PARITY 1 << 3
#define LCR_EVEN_PARITY 1 << 4
#define LCR_8BIT_LENGTH 0x03



#define XTAL_FREQUENCY 14745600UL // On-board crystal


// Configuring the
// "Programmable baud rate generator"
#define PRESCALER 1 // Default prescaler after reset
#define BAUD_RATE_DIVISOR(baud) ((XTAL_FREQUENCY/PRESCALER)/(baud*16UL))



// SC16IS750 register values
struct SPI_UART_cfg {
	char DataFormat;
	char Flow;
};


struct SPI_UART_cfg SPI_Uart_config = {
	0x03,
	// Flow control
	EFCR_ENABLE_RTS | EFCR_INVERSION | EFCR_MULTIDROP
};


void SpiUartDevice::begin(unsigned long baudrate, byte parityMode, byte chip_select) {
	/*
	* Initialize SPI and UART communications
	*/

	initUart(baudrate, parityMode, chip_select);
}

void SpiUartDevice::deselect(byte chip_select) {
	/*
	* Deslects the SPI device
	*/

	digitalWrite(chip_select, HIGH);
}


void SpiUartDevice::select(byte chip_select) {
	/*
	* Selects the SPI device
	*/

	digitalWrite(chip_select, LOW);
}


void SpiUartDevice::initUart(unsigned long baudrate, byte parityMode, byte chip_select) {
	/*
	* Initialise the UART.
	*/
	// Initialise and test SC16IS750
	configureUart(baudrate, parityMode, chip_select);

	if(!uartConnected(chip_select)){
		// while(1) {
		// Lock up if we fail to initialise SPI UART bridge.
		//  };
	}
	// The SPI - UART bridge is now successfully initialised.
}


void SpiUartDevice::setBaudRate(unsigned long baudrate, byte chip_select) {
	unsigned long divisor = BAUD_RATE_DIVISOR(baudrate);

	// Enable divisor - Enable Set Baudrate

	writeRegister(LCR, LCR_ENABLE_DIVISOR_LATCH, chip_select);

	// "Program baudrate"
	writeRegister(DLL, lowByte(divisor), chip_select);
	writeRegister(DLM, highByte(divisor), chip_select);
}


void SpiUartDevice::configureUart(unsigned long baudrate, byte parityMode, byte chip_select) {
	/*
	* Configure the settings of the UART.
	*/
	setBaudRate(baudrate,chip_select);
	/*  writeRegister(LCR, SPI_Uart_config.DataFormat, chip_select); // 8 data bit, 1 stop bit, no parity
	*/

	// Parity config

	if (parityMode == 0)  // None parity
	writeRegister(LCR, SPI_Uart_config.DataFormat, chip_select);


	if (parityMode == 1)  // Odd Parity
	{
		writeRegister(LCR, LCR_ENABLE_PARITY | LCR_8BIT_LENGTH, chip_select);
	}

	if (parityMode == 2)  // Even parity
	{
		writeRegister(LCR, LCR_ENABLE_PARITY | LCR_EVEN_PARITY | LCR_8BIT_LENGTH, chip_select);
	}


	writeRegister(EFCR, SPI_Uart_config.Flow, chip_select); // RS-485 autocontrol
	writeRegister(FCR, 0x06, chip_select); // reset TXFIFO, reset RXFIFO, non FIFO mode
	writeRegister(FCR, 0x01, chip_select); // enable FIFO mode
}


boolean SpiUartDevice::uartConnected(byte chip_select) {
	/*
	* Check that UART is connected and operational.
	*/
	// Perform read/write test to check if UART is working
	const char TEST_CHARACTER = 'H';

	writeRegister(SPR, 0x55, chip_select);

	return (readRegister(SPR, chip_select));
}


void SpiUartDevice::writeRegister(byte registerAddress, byte data, byte chip_select) {
	/*
	* Write <data> byte to the SC16IS750 register <registerAddress>
	*/
	select(chip_select);
	SPI.transfer(registerAddress);
	SPI.transfer(data);
	deselect(chip_select);
}


byte SpiUartDevice::readRegister(byte registerAddress, byte chip_select) {
	/*
	* Read byte from SC16IS750 register at <registerAddress>.
	*/
	// Used in SPI read operations to flush slave's shift register
	const byte SPI_DUMMY_BYTE = 0xFF;

	char result;

	select(chip_select);
	SPI.transfer(SPI_READ_MODE_FLAG | registerAddress);
	result = SPI.transfer(SPI_DUMMY_BYTE);
	deselect(chip_select);
	return result;
}


int SpiUartDevice::available(byte chip_select) {
	/*
	* Get the number of bytes available for reading.
	*
	* This is data that's already arrived and stored in the receive
	* buffer (up to 64 bytes).
	*/
	// readRegister(LSR) & 0x01
	//delay(2);
	return (readRegister(RXLVL, chip_select));
}


int SpiUartDevice::read(byte chip_select) {
	/*
	* Read byte from UART.
	*
	* Returns byte read or -1 if no data available.
	*/

	if (!available(chip_select)) {
		return -1;
	}

	return readRegister(RHR, chip_select);
}







void SpiUartDevice::write(byte value, byte chip_select) {

	/*
	* Write byte to UART.
	*/

	while (readRegister(TXLVL, chip_select) == 0) {
		// Wait for space in TX buffer
	};
	writeRegister(THR, value, chip_select);

}




void SpiUartDevice::flush(byte chip_select) {
	/*
	* Flush characters from SC16IS750 receive buffer.
	*/
	while(available(chip_select) > 0) {
		read(chip_select);
	}
}


void SpiUartDevice::ioSetDirection(unsigned char bits, byte chip_select) {
	writeRegister(IODIR, bits, chip_select);
}


void SpiUartDevice::ioSetState(unsigned char bits, byte chip_select) {
	writeRegister(IOSTATE, bits, chip_select);
}
