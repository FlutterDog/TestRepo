
#include <Arduino.h>
#include <SPI.h>
#include <util/delay.h>


//void getReply(byte * serBuf);
void initArrays(float pressVal_, float tempVal_, float RHVal_);
int loadInt(uint16_t addr_, uint16_t default_);
void loop_50();
void rebootDM();
void loop_500();
void avarageIt(float press_, float temp_, float RH_);
int loadSignedInt(int addr_);
void PIDfilter();
void trendBuffer();
//unsigned int  readAndCheckInt(int addr_, int defValue_);
void saveInt(int addr_, int digit_);
void settings();
uint8_t loadByte(uint8_t addr_, uint8_t defValue_);
void saveSignedInt(int addr_, int digit_);
void checkError();
uint8_t checkIncoming(uint8_t ch_, unsigned int length_);
int convertFloat(float flt_, uint8_t oreder_);
void modbusProceed(int *regs,  int start_addr_,  int reg_count_, int Value_);
void initCommunication();
unsigned long  getBaud(uint8_t baudRate_);
void bridgeVIP();
void getDewPoint();
uint8_t receive_incoming_data_read(uint8_t* data_);
int read_reg_response(uint8_t* data_);
int modbus_response(uint8_t* recievedBytes);
int receive_response(uint8_t *received_string);
unsigned int crc(unsigned char *buf, int start, int cnt);
void gasFromEEPROM(byte pointer);
float sensorToConcentration(float f_);
void gasToEEPROM(byte pointer);
void initTian();
void getTian();
float loadFloat(int addr_, float default_);
void DS_proceed();
