#ifndef IGAS_485_H
#define IGAS_485_H

#include "main.h"


/* enum of supported modbus function codes. If you implement a new one, put its function code here ! */
enum {
	FC_READ_REGS  = 0x03,   //Read contiguous block of holding register
	FC_WRITE_REG  = 0x06,   //Write single holding register
	FC_WRITE_REGS = 0x10    //Write block of contiguous registers
};

/* supported functions. If you implement a new one, put its function code into this array! */
const uint8_t fsupported[] = { FC_READ_REGS, FC_WRITE_REG, FC_WRITE_REGS };

/* constants */
enum {
	MAX_READ_REGS = 16,
	MAX_WRITE_REGS = 16,
	MAX_MESSAGE_LENGTH = 40
};


enum {
	RESPONSE_SIZE = 6,
	EXCEPTION_SIZE = 3,
	CHECKSUM_SIZE = 2
};

/* exceptions code */
enum {
	NO_REPLY = -1,
	EXC_FUNC_CODE = 1,
	EXC_ADDR_RANGE = 2,
	EXC_REGS_QUANT = 3,
	EXC_EXECUTE = 4
};

/* positions inside the query/response array */
enum {
	SLAVE = 0,
	FUNC,
	START_H,
	START_L,
	REGS_H,
	REGS_L,
	BYTE_CNT
};


const uint8_t receiveMode = 0b11101111;
const uint8_t transmitMode = 0b00010000;
const uint8_t dotDisp = 0x08;
const uint8_t singleRegister = 1;



class IGAS_X2X_485
{
	public:
	void		init(uint32_t COMM_BPS/*, int SERIAL_MODE = SERIAL_8N1, int allRegs = 1000*/);
	uint8_t		update(uint32_t currentTime);
	void		setDelegate(void (*fp)(int16_t*,  uint16_t ,  uint16_t , int16_t ) );
	int			send(uint8_t * query, uint8_t string_length);
	uint8_t		slave_id;
	uint8_t		transmitLED;
	uint8_t		func;
	private:
	uint16_t	crc(uint8_t * buf, uint8_t start, uint8_t cnt);
	void		build_read_packet(uint8_t slave, uint8_t function, 	uint8_t count, uint8_t * packet);
	void		build_write_packet(uint8_t slave, uint8_t function, uint16_t start_addr, uint8_t count, uint8_t * packet);
	void		build_write_single_packet(uint8_t slave, uint8_t function, uint16_t write_addr, uint16_t reg_val, uint8_t * packet);
	void		build_error_packet(uint8_t slave, uint8_t function, uint8_t exception, uint8_t * packet);
	void		modbus_reply(uint8_t * packet, uint8_t string_length);
	int			send_reply(uint8_t * query, uint8_t string_length);
	int			receive_request(uint8_t *received_string);
	int			modbus_request(uint8_t slave, uint8_t * data);
	int			validate_request(uint8_t * data, uint8_t length);
	int			write_regs(uint16_t start_addr, uint8_t * query, int16_t * regs);
	int			preset_multiple_registers(uint8_t slave, uint16_t start_addr, uint8_t count, uint8_t * query, int16_t * regs);
	int			write_single_register(uint8_t slave, uint16_t write_addr, uint8_t * query, int16_t * regs);
	int			read_holding_registers(uint8_t slave, uint16_t start_addr, uint8_t reg_count, int16_t * regs);
	void		(*fpAction)(int16_t*,  uint16_t ,  uint16_t , int16_t );
	void doAction(int16_t *t1, uint16_t t2,uint16_t t3, int16_t t4);

	uint16_t	regLimit;
	uint32_t	Nowdt;
	int			lastBytesReceived;
};


#endif /* IGAS_485_H */