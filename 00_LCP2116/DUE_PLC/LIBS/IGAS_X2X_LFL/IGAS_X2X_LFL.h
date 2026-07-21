
#ifndef IGAS_X2X_LFL
#define IGAS_X2X_LFL

#include <Arduino.h>
#include <pins_arduino.h>
#include <SC16IS750.h>

#include <SPI.h>




/* enum of supported modbus function codes. If you implement a new one, put its function code here ! */
enum {
	FC_READ_REGS  = 0x03,   //Read contiguous block of holding register
	FC_WRITE_REG  = 0x06,   //Write single holding register
	FC_WRITE_REGS = 0x10    //Write block of contiguous registers
};

/* supported functions. If you implement a new one, put its function code into this array! */
const unsigned char fsupported[] = { FC_READ_REGS, FC_WRITE_REG, FC_WRITE_REGS };

/* constants */
enum {
	MAX_READ_REGS = 0x0F,
	MAX_WRITE_REGS = 0x0F,
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



const byte interframe_delay = 4;  /* Modbus t3.5 = 2 ms */
const byte dotDisp = 0x08;
const byte singleRegister = 1;
const byte A328P = 0;
const byte A328PB = 1;
const byte RS1_En = 13;




class IGAS_LFL_485
{

	public:
	
	void initMB();
	byte update();
	void setDelegate(void (*fp)(int*,  int ,  int , int ) );
	int send(unsigned char * query, unsigned char string_length);
	
	byte slave;
	byte transmitLED;
	byte chip;
	
	private:
	
	unsigned int crc(unsigned char * buf, unsigned char start, unsigned char cnt);
	void build_read_packet(unsigned char slave, unsigned char function,
	unsigned char count, unsigned char * packet);
	void build_write_packet(unsigned char slave, unsigned char function,
	unsigned int start_addr,  unsigned char count, unsigned char * packet);
	void build_write_single_packet(unsigned char slave, unsigned char function,
	unsigned int write_addr, unsigned int reg_val, unsigned char * packet);
	void build_error_packet(unsigned char slave, unsigned char function,
	unsigned char exception, unsigned char * packet);
	void modbus_reply(unsigned char * packet, unsigned char string_length);
	int send_reply(unsigned char * query, unsigned char string_length);
	int receive_request(unsigned char *received_string);
	int modbus_request(unsigned char slave, unsigned char * data);
	int validate_request(unsigned char * data, unsigned char length);
	int write_regs(unsigned int start_addr, unsigned char * query, int * regs);
	int preset_multiple_registers(unsigned char slave,  unsigned int start_addr,  unsigned char count,
	unsigned char * query,  int * regs);
	int write_single_register(unsigned char slave,
	unsigned int write_addr, unsigned char * query, int * regs);
	int read_holding_registers(unsigned char slave, unsigned int start_addr,
	unsigned char reg_count, int * regs);
	void (*fpAction)(int*,  int ,  int , int );
	void doAction(int *t1, int t2,int t3, int t4) ;
	int regLimit;
	unsigned long Nowdt = 0;
	unsigned int lastBytesReceived;

};

#endif

