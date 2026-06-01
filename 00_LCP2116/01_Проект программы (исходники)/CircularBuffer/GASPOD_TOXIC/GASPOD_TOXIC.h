#ifndef GASPOD_TOXIC
#define GASPOD_TOXIC


enum
{
	notAck,			// Not acknowledged
	N2_low_P,		// Filled with N2 low pressure
	N2_high_P,		// Filled with N2 high pressure
	vacuum,			// vacuumed
	gas_low_P,		// Filled with Gas low pressure
	gas_high_P,		// Filled with Gas high pressure
	disabled,		// Filled with Gas high pressure
};

enum
{
	pipeLine1,			// Not acknowledged
	pipeLine2,		// Filled with N2 low pressure
	pipeLine3,		// Filled with N2 high pressure
	pipeLine4,			// vacuumed
	pipeLine5,		// Filled with Gas low pressure
	pipeLine6,		// Filled with Gas high pressure
	pipeLine7,		// Filled with Gas high pressure
	pipeLine8,
	pipeLine9,
	pipeLine10,
};
// Define the structures
//
// struct pipeStates {
// 	uint8_t notAck;			// Not acknowledged
// 	uint8_t N2_low_P;		// Filled with N2 low pressure
// 	uint8_t N2_high_P;		// Filled with N2 high pressure
// 	uint8_t vacuum;			// vacuumed
// 	uint8_t gas_low_P;		// Filled with Gas low pressure
// 	uint8_t gas_high_P;		// Filled with Gas high pressure
// 	uint8_t disabled;		// Filled with Gas high pressure
// };

//
// struct LCP1116 : public DeviceCommon
// {
//
// };


#endif // IGAS_STRUCT