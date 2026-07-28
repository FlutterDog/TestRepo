#ifndef IGAS_VAR
#define IGAS_VAR


const int system_Devices_size = 200;
void* system_Devices[system_Devices_size];
uint16_t asduTail = 0;




// X2X START
int X2X_Data_Array[8][60];


unsigned int respondTimeOut[] = {100, 500, 500, 100, 500, 500, 500,500};

// X2X FIN



// LCT Vars START



#define currScopeSize 101
enum     // HMI buttons state
{
	scopeA,   //
	scopeB,
	scopeC,
	encoder
};

int scopeArray[4][currScopeSize];


// LCT Vars FIN


// MODBUS START

byte currentCycle[] = {0,0,0,0,0,0,0,0,0,0,0,0};


// MODBUS FIN



// UART START

byte S_PORT[] = {0, 1, 2, 3, 4, 5, 6};




enum                 // Not used at the moment
{
	TypeModule,
	DataQnt,
	Status,
	Data_0,
	Data_1,
	Data_2,
	Data_3,
	Data_4,
	Data_5,
	Data_6,
	Data_7,
	Data_8,
};


// UART FIN


#endif // IGAS_STRUCT