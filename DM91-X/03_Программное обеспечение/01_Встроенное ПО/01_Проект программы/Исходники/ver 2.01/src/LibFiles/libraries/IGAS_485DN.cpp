#include "IGAS_485DN.h"
/* global variables */

byte interframe_delay;  /* Modbus t3.5 = 2 ms */


/*
update_mb_slave(slave_id, holding_regs_array, number_of_regs)

checks if there is any valid request from the modbus master. If there is,
performs the action requested
*/



void IGAS_X2X_485::initMB(unsigned long COMM_BPS, int SERIAL_MODE, int allRegs)
{
	COMM_BPS = 9600;

	SERIAL_MODE = SERIAL_8N1;
	
	Serial.begin(COMM_BPS, SERIAL_MODE);                              // No parity, 1 stop bit
	UCSR0B |= ( 1 << TXCIE0 );     // UART 1 Interrupt Enabel
	regLimit = allRegs;
	
	if (COMM_BPS > 19200)
	interframe_delay = 4;
	else
	interframe_delay = 40000/COMM_BPS;
}



byte IGAS_X2X_485::update()
{
	int length = Serial.available();

	if (length == 0)
	{
		lastBytesReceived = 0;
		return 0;
	}


	unsigned char query[MAX_MESSAGE_LENGTH];
	unsigned char errpacket[EXCEPTION_SIZE + CHECKSUM_SIZE];
	unsigned int start_addr;
	int exception;
	unsigned long currentTime = millis();

	if ((PORTD & transmitMode) == transmitMode) //digitalRead(txEnPin) == HIGH)
	return 0;


	if (lastBytesReceived != length)
	{
		lastBytesReceived = length;
		Nowdt = currentTime;
		return 0;
	}

	if (currentTime - Nowdt < interframe_delay)
	return 0;

	lastBytesReceived = 0;
	//	incomingWDT = currentTime;

	length = modbus_request(slave, query);
	if (length < 1)
	return length;

	exception = validate_request(query, length);
	
	if (exception)
	{
		build_error_packet(slave, query[FUNC], exception,
		errpacket);
		send_reply(errpacket, EXCEPTION_SIZE);
		return (exception);
	}


	start_addr = ((int) query[START_H] << 8) +
	(int) query[START_L];

	int regs[MAX_MESSAGE_LENGTH+2];

	func = query[FUNC];

	switch (query[FUNC])
	{
		case FC_READ_REGS:
		return read_holding_registers(slave,
		start_addr,
		query[REGS_L],
		regs);
		break;
		case FC_WRITE_REGS:
		return preset_multiple_registers(slave,
		start_addr,
		query[REGS_L],
		query,
		regs);
		break;
		case FC_WRITE_REG:
		return write_single_register(slave,
		start_addr,
		query,
		regs);
		break;
	}
	return 0;
}



/*
CRC

INPUTS:
buf   ->  Array containing message to be sent to controller.
start ->  Start of loop in crc counter, usually 0.
cnt   ->  Amount of bytes in message being sent to controller/
OUTPUTS:
temp  ->  Returns crc byte for message.
COMMENTS:
This routine calculates the crc high and low byte of a message.
Note that this crc is only used for Modbus, not Modbus+ etc.
****************************************************************************/

unsigned int IGAS_X2X_485::crc(unsigned char * buf, unsigned char start,
unsigned char cnt)
{
	unsigned char i, j;
	unsigned temp, temp2, flag;

	temp = 0xFFFF;

	for (i = start; i < cnt; i++)
	{
		temp = temp ^ buf[i];

		for (j = 1; j <= 8; j++)
		{
			flag = temp & 0x0001;
			temp = temp >> 1;
			if (flag)
			temp = temp ^ 0xA001;
		}
	}

	/* Reverse byte order. */
	temp2 = temp >> 8;
	temp = (temp << 8) | temp2;
	temp &= 0xFFFF;

	return (temp);
}




/***********************************************************************

The following functions construct the required query into
a modbus query packet.

***********************************************************************/

/*
Start of the packet of a read_holding_register response
*/
void IGAS_X2X_485::build_read_packet(unsigned char slave, unsigned char function,
unsigned char count, unsigned char * packet)
{
	packet[SLAVE] = slave;
	packet[FUNC] = function;
	packet[2] = count * 2;
}

/*
Start of the packet of a preset_multiple_register response
*/
void IGAS_X2X_485::build_write_packet(unsigned char slave, unsigned char function,
unsigned int start_addr,
unsigned char count,
unsigned char * packet)
{
	packet[SLAVE] = slave;
	packet[FUNC] = function;
	packet[START_H] = start_addr >> 8;
	packet[START_L] = start_addr & 0x00ff;
	packet[REGS_H] = 0x00;
	packet[REGS_L] = count;
}

/*
Start of the packet of a write_single_register response
*/
void IGAS_X2X_485::build_write_single_packet(unsigned char slave, unsigned char function,
unsigned int write_addr, unsigned int reg_val, unsigned char * packet)
{
	packet[SLAVE] = slave;
	packet[FUNC] = function;
	packet[START_H] = write_addr >> 8;
	packet[START_L] = write_addr & 0x00ff;
	packet[REGS_H] = reg_val >> 8;
	packet[REGS_L] = reg_val & 0x00ff;
}


/*
Start of the packet of an exception response
*/
void IGAS_X2X_485::build_error_packet(unsigned char slave, unsigned char function,
unsigned char exception, unsigned char * packet)
{
	packet[SLAVE] = slave;
	packet[FUNC] = function + 0x80;
	packet[2] = exception;
}


/*************************************************************************

modbus_query( packet, length)

Function to add a checksum to the end of a packet.
Please note that the packet array must be at least 2 fields longer than
string_length.
**************************************************************************/

void IGAS_X2X_485::modbus_reply(unsigned char * packet, unsigned char string_length)
{
	int temp_crc = 0;

	temp_crc = crc(packet, 0, string_length);
	packet[string_length] = temp_crc >> 8;
	string_length++;
	packet[string_length] = temp_crc & 0x00FF;
}



/***********************************************************************

send_reply( query_string, query_length )

Function to send a reply to a modbus master.
Returns: total number of characters sent
************************************************************************/

int IGAS_X2X_485::send_reply(unsigned char * query, unsigned char string_length)
{

	modbus_reply(query, string_length);
	string_length += 2;

	return (send(query, string_length));
}


int IGAS_X2X_485::send(unsigned char * query, unsigned char string_length)
{

	unsigned char i = 0;
	UCSR0A = UCSR0A | (1 << TXC0);
	uint8_t oldSREG = SREG;
	cli();
	PORTD = PORTD | transmitMode;                  // Перевод драйвера RS485 в режим передачи сообщений
	SREG = oldSREG;

	for (i = 0; i < string_length; i++)
	{
		Serial.write(query[i]);
	}

	return i; 		/* it does not mean that the write was succesful, though */
}


/***********************************************************************

receive_request( array_for_data )

Function to monitor for a request from the modbus master.

Returns:	Total number of characters received if OK
0 if there is no request
A negative error code on failure
***********************************************************************/

int IGAS_X2X_485::receive_request(unsigned char *received_string)
{
	int bytes_received = 0;

	/* FIXME: does Serial.available wait 1.5T or 3.5T before exiting the loop? */
	while (Serial.available())
	{
		received_string[bytes_received] = Serial.read();
		bytes_received++;
		if (bytes_received >= MAX_MESSAGE_LENGTH-2)
		return NO_REPLY; 	/* port error */
	}

	return (bytes_received);
}


/*********************************************************************

modbus_request(slave_id, request_data_array)

Function to the correct request is returned and that the checksum
is correct.

Returns:	string_length if OK
0 if failed
Less than 0 for exception errors

Note: All functions used for sending or receiving data via
modbus return these return values.

**********************************************************************/

int IGAS_X2X_485::modbus_request(unsigned char slave, unsigned char * data)
{
	int response_length = 0;
	unsigned int crc_calc = 0;
	unsigned int crc_received = 0;
	// 	unsigned char recv_crc_hi = 0;
	// 	unsigned char recv_crc_lo = 0;

	response_length = receive_request(data);

	if (response_length > 0)
	{
		crc_calc = crc(data, 0, response_length - 2);
		//	recv_crc_hi = (unsigned) data[response_length - 2];
		//	recv_crc_lo = (unsigned) data[response_length - 1];
		crc_received = data[response_length - 2];
		crc_received = (unsigned) crc_received << 8;
		crc_received =
		crc_received | (unsigned) data[response_length - 1];

		/*********** check CRC of response ************/
		if (crc_calc != crc_received)
		{
			return NO_REPLY;
		}

		/* check for slave id */

		//  if (data[SLAVE] == 0) {
		//    return (response_length); //  Датчик отвечает на нулевой адрес
		//  }

		if (slave != data[SLAVE])
		{
			return NO_REPLY;
		}
	}
	return (response_length);
}

/*********************************************************************

validate_request(request_data_array, request_length, available_regs)

Function to check that the request can be processed by the slave.

Returns:	0 if OK
A negative exception code on error

**********************************************************************/

int IGAS_X2X_485::validate_request(unsigned char * data, unsigned char length)
{
	unsigned int i, fcnt = 0;
	unsigned int regs_num = 0;
	unsigned int start_addr = 0;
	unsigned char max_regs_num = 0;

	/* check function code */
	for (i = 0; i < sizeof(fsupported); i++)
	{
		if (fsupported[i] == data[FUNC])
		{
			fcnt = 1;
			break;
		}
	}
	if (0 == fcnt)
	return EXC_FUNC_CODE;

	if (FC_WRITE_REG == data[FUNC])
	{
		/* For function write single reg, this is the target reg.*/
		regs_num = ((unsigned int) data[START_H] << 8) + (unsigned int) data[START_L];
		if (regs_num >= regLimit)
		return EXC_ADDR_RANGE;
		return 0;
	}

	/* For functions read/write regs, this is the range. */
	regs_num = ((unsigned int) data[REGS_H] << 8) + (unsigned int) data[REGS_L];

	/* check quantity of registers */
	if (FC_READ_REGS == data[FUNC])
	max_regs_num = MAX_READ_REGS;
	else if (FC_WRITE_REGS == data[FUNC])
	max_regs_num = MAX_WRITE_REGS;

	if ((regs_num < 1) || (regs_num > max_regs_num))
	return EXC_REGS_QUANT;

	/* check registers range, start address is 0 */
	start_addr = ((int) data[START_H] << 8) + (int) data[START_L];
	if (start_addr > regLimit)
	return EXC_ADDR_RANGE;

	return 0; 		/* OK, no exception */
}



/************************************************************************

write_regs(first_register, data_array, registers_array)

writes into the slave's holding registers the data in query,
starting at start_addr.

Returns:   the number of registers written
************************************************************************/

int IGAS_X2X_485::write_regs(unsigned int start_addr, unsigned char * query, int * regs)
{
	int temp = 0;
	unsigned int i = 0;

	for (i = 0; i < query[REGS_L]; i++)
	{
		/* shift reg hi_byte to temp */
		temp = (int) query[(BYTE_CNT + 1) + i * 2] << 8;
		/* OR with lo_byte           */
		temp = temp | (int) query[(BYTE_CNT + 2) + i * 2];

		doAction(regs, start_addr + i, singleRegister, temp);
	}
	return i;
}

/************************************************************************

preset_multiple_registers(slave_id, first_register, number_of_registers,
data_array, registers_array)

Write the data from an array into the holding registers of the slave.

*************************************************************************/

int IGAS_X2X_485::preset_multiple_registers(unsigned char slave,
unsigned int start_addr,
unsigned char count,
unsigned char * query,
int * regs)
{
	unsigned char function = FC_WRITE_REGS;	/* Preset Multiple Registers */
	int status = 0;
	unsigned char packet[RESPONSE_SIZE + CHECKSUM_SIZE];

	build_write_packet(slave, function, start_addr, count, packet);

	if (write_regs(start_addr, query, regs))
	{
		status = send_reply(packet, RESPONSE_SIZE);
	}

	return (status);
}




/************************************************************************

write_single_register(slave_id, write_addr, data_array, registers_array)

Write a single int val into a single holding register of the slave.

*************************************************************************/

int IGAS_X2X_485::write_single_register(unsigned char slave,
unsigned int write_addr, unsigned char * query, int * regs)
{
	unsigned char function = FC_WRITE_REG; /* Function: Write Single Register */
	int status = 0;
	unsigned int reg_val;
	unsigned char packet[RESPONSE_SIZE + CHECKSUM_SIZE];

	reg_val = query[REGS_H] << 8 | query[REGS_L];
	build_write_single_packet(slave, function, write_addr, reg_val, packet);

	doAction(regs, write_addr, singleRegister, reg_val);

	status = send_reply(packet, RESPONSE_SIZE);

	return (status);
}


/************************************************************************

read_holding_registers(slave_id, first_register, number_of_registers,
registers_array)

reads the slave's holdings registers and sends them to the Modbus master

*************************************************************************/

int IGAS_X2X_485::read_holding_registers(unsigned char slave, unsigned int start_addr,

unsigned char reg_count, int * regs)
{
	unsigned char function = 0x03; 	/* Function 03: Read Holding Registers */
	int packet_size = 3;
	int status;
	unsigned int i;
	unsigned char packet[MAX_MESSAGE_LENGTH];

	build_read_packet(slave, function, reg_count, packet);

	doAction(regs, start_addr, reg_count, 0);

	for (i = 0; i < reg_count; i++)
	{
		packet[packet_size] = regs[i] >> 8;
		packet_size++;
		packet[packet_size] = regs[i] & 0x00FF;
		packet_size++;
	}

	status = send_reply(packet, packet_size);

	return (status);
}

void IGAS_X2X_485::setDelegate(void (*fp)(int*,  int,  int, int ) )
{
	fpAction = fp;
}


void IGAS_X2X_485::doAction(int *t1, int t2,int t3, int t4)
{

	(*fpAction)(t1,t2, t3,t4);
}



// Add to main loop with correct RS enable PORT pin!

ISR(USART_TX_vect)  {
	PORTD = PORTD & receiveMode;
}


