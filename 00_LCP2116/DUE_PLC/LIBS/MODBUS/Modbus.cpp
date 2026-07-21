/*
Modbus.cpp - Source for Modbus Base Library
Copyright (C) 2014 André Sarmento Barbosa
*/
#include "Modbus.h"

Modbus::Modbus() {
	_regs_head = 0;
	_regs_last = 0;
}

// TRegister* Modbus::searchRegister(word address) {
// 	TRegister *reg = _regs_head;
// 	//if there is no register configured, bail
// 	if(reg == 0) return(0);
// 	//scan through the linked list until the end of the list or the register is found.
// 	//return the pointer.
// 	do {
// 		if (reg->address == address) return(reg);
// 		reg = reg->next;
// 	} while(reg);
//
// 	return(0);
// }

TRegister* Modbus::searchRegister(word address) {
	// Initialize the starting register
	TRegister* reg = _regs_head;

	// Use a for loop to traverse the linked list
	for (; reg != nullptr; reg = reg->next) {
		if (reg->address == address) {
			return reg; // Return the register if address matches
		}
	}

	// Return nullptr if register is not found
	return nullptr;
}

// void Modbus::addReg(word address, word value) {
//     TRegister *newreg;
//
// 	newreg = (TRegister *) malloc(sizeof(TRegister));
// 	newreg->address = address;
// 	newreg->value		= value;
// 	newreg->next		= 0;
//
// 	if(_regs_head == 0) {
//         _regs_head = newreg;
//         _regs_last = _regs_head;
//     } else {
//         //Assign the last register's next pointer to newreg.
//         _regs_last->next = newreg;
//         //then make temp the last register in the list.
//         _regs_last = newreg;
//     }
// }

// START Updated Code with Improvements:
void Modbus::addReg(word address, word value) {
	// Allocate memory for the new register
	TRegister *newreg = (TRegister *) malloc(sizeof(TRegister));
	if (newreg == nullptr) {
		// Handle memory allocation failure
		return;
	}

	// Initialize the new register
	newreg->address = address;
	newreg->value = value;
	newreg->next = nullptr;

	// Add the new register to the linked list
	if (_regs_head == nullptr) {
		_regs_head = newreg;
		_regs_last = _regs_head;
		} else {
		_regs_last->next = newreg;
		_regs_last = newreg;
	}
}
// FIN Updated Code with Improvements:


bool Modbus::Reg(word address, word value) {
	TRegister *reg;
	//search for the register address
	reg = this->searchRegister(address);
	//if found then assign the register value to the new value.
	if (reg) {
		reg->value = value;
		return true;
	} else
	return false;
}

word Modbus::Reg(word address) {
	TRegister *reg;
	reg = this->searchRegister(address);
	if(reg)
	return(reg->value);
	else
	return(0);
}

void Modbus::addHreg(word offset, word value) {
	this->addReg(offset + 40001, value);
}

bool Modbus::Hreg(word offset, word value) {
	return Reg(offset + 40001, value);
}

word Modbus::Hreg(word offset) {
	return Reg(offset + 40001);
}

#ifndef USE_HOLDING_REGISTERS_ONLY
void Modbus::addCoil(word offset, bool value) {
	this->addReg(offset + 1, value?0xFF00:0x0000);
}

void Modbus::addIsts(word offset, bool value) {
	this->addReg(offset + 10001, value?0xFF00:0x0000);
}

void Modbus::addIreg(word offset, word value) {
	this->addReg(offset + 30001, value);
}

bool Modbus::Coil(word offset, bool value) {
	return Reg(offset + 1, value?0xFF00:0x0000);
}

bool Modbus::Ists(word offset, bool value) {
	return Reg(offset + 10001, value?0xFF00:0x0000);
}

bool Modbus::Ireg(word offset, word value) {
	return Reg(offset + 30001, value);
}

bool Modbus::Coil(word offset) {
	if (Reg(offset + 1) == 0xFF00) {
		return true;
	} else return false;
}

bool Modbus::Ists(word offset) {
	if (Reg(offset + 10001) == 0xFF00) {
		return true;
	} else return false;
}

word Modbus::Ireg(word offset) {
	return Reg(offset + 30001);
}
#endif


void Modbus::receivePDU(byte* frame) {
	byte fcode  = frame[0];
	word field1 = (word)frame[1] << 8 | (word)frame[2];
	word field2 = (word)frame[3] << 8 | (word)frame[4];

	switch (fcode) {

		case MB_FC_WRITE_REG:
		//field1 = reg, field2 = value
		this->writeSingleRegister(field1, field2);
		break;

		case MB_FC_READ_REGS:
		//field1 = startreg, field2 = numregs
		this->readRegisters(field1, field2);
		break;

		case MB_FC_WRITE_REGS:
		//field1 = startreg, field2 = status
		this->writeMultipleRegisters(frame,field1, field2, frame[5]);
		break;

		#ifndef USE_HOLDING_REGISTERS_ONLY
		case MB_FC_READ_COILS:
		//field1 = startreg, field2 = numregs
		this->readCoils(field1, field2);
		break;

		case MB_FC_READ_INPUT_STAT:
		//field1 = startreg, field2 = numregs
		this->readInputStatus(field1, field2);
		break;

		case MB_FC_READ_INPUT_REGS:
		//field1 = startreg, field2 = numregs
		this->readInputRegisters(field1, field2);
		break;

		case MB_FC_WRITE_COIL:
		//field1 = reg, field2 = status
		this->writeSingleCoil(field1, field2);
		break;

		case MB_FC_WRITE_COILS:
		//field1 = startreg, field2 = numoutputs
		this->writeMultipleCoils(frame,field1, field2, frame[5]);
		break;

		#endif
		default:
		this->exceptionResponse(fcode, MB_EX_ILLEGAL_FUNCTION);
	}
}

void Modbus::exceptionResponse(byte fcode, byte excode) {
	//Clean frame buffer
	free(_frame);
	_len = 2;
	_frame = (byte *) malloc(_len);
	_frame[0] = fcode + 0x80;
	_frame[1] = excode;

	_reply = MB_REPLY_NORMAL;
}
//
// void Modbus::readRegisters(word startreg, word numregs) {
// 	//Check value (numregs)
// 	if (numregs < 0x0001 || numregs > 0x007D) {
// 		this->exceptionResponse(MB_FC_READ_REGS, MB_EX_ILLEGAL_VALUE);
// 		return;
// 	}
//
// 	//Check Address
// 	//*** See comments on readCoils method.
// 	if (!this->searchRegister(startreg + 40001)) {
// 		this->exceptionResponse(MB_FC_READ_REGS, MB_EX_ILLEGAL_ADDRESS);
// 		return;
// 	}
//
//
// 	//Clean frame buffer
// 	free(_frame);
// 	_len = 0;
//
// 	//calculate the query reply message length
// 	//for each register queried add 2 bytes
// 	_len = 2 + numregs * 2;
//
// 	_frame = (byte *) malloc(_len);
// 	if (!_frame) {
// 		this->exceptionResponse(MB_FC_READ_REGS, MB_EX_SLAVE_FAILURE);
// 		return;
// 	}
//
// 	_frame[0] = MB_FC_READ_REGS;
// 	_frame[1] = _len - 2;   //byte count
//
// 	word val;
// 	word i = 0;
// 	while(numregs--) {
// 		//retrieve the value from the register bank for the current register
// 		val = this->Hreg(startreg + i);
// 		//write the high byte of the register value
// 		_frame[2 + i * 2]  = val >> 8;
// 		//write the low byte of the register value
// 		_frame[3 + i * 2] = val & 0xFF;
// 		i++;
// 	}
//
// 	_reply = MB_REPLY_NORMAL;
// }

void Modbus::readRegisters(word startreg, word numregs) {
	// Check value (numregs)
	if (numregs < 0x0001 || numregs > 0x007D) {
		this->exceptionResponse(MB_FC_READ_REGS, MB_EX_ILLEGAL_VALUE);
		return;
	}

	// Check Address
	// Adjusted to check for the presence of registers in the range.
	// 	if (!this->searchRegister(startreg + 40001)) {
	// 		this->exceptionResponse(MB_FC_READ_REGS, MB_EX_ILLEGAL_ADDRESS);
	// 		return;
	// 	}

	for (word k = 0; k < numregs; k++) {
		if (!this->searchRegister(startreg + 40001 + k)) {
			this->exceptionResponse(MB_FC_WRITE_REGS, MB_EX_ILLEGAL_ADDRESS);
			return;
		}
	}

	// Clean frame buffer
	free(_frame);
	_len = 0;

	// Calculate the query reply message length
	// For each register queried, add 2 bytes
	_len = 2 + numregs * 2;

	// Allocate memory for the frame
	_frame = (byte *) malloc(_len);
	if (!_frame) {
		this->exceptionResponse(MB_FC_READ_REGS, MB_EX_SLAVE_FAILURE);
		return;
	}

	_frame[0] = MB_FC_READ_REGS;
	_frame[1] = _len - 2; // Byte count (excluding function code and byte count)

	// Use a for loop to process each register
	for (word i = 0; i < numregs; i++) {
		// Retrieve the value from the register bank for the current register
		word val = this->Hreg(startreg + i);
		// Write the high byte of the register value
		_frame[2 + i * 2] = val >> 8;
		// Write the low byte of the register value
		_frame[3 + i * 2] = val & 0xFF;
	}

	_reply = MB_REPLY_NORMAL;
}




void Modbus::writeSingleRegister(word reg, word value) {
	//No necessary verify illegal value (EX_ILLEGAL_VALUE) - because using word (0x0000 - 0x0FFFF)
	//Check Address and execute (reg exists?)
	if (!this->Hreg(reg, value)) {
		this->exceptionResponse(MB_FC_WRITE_REG, MB_EX_ILLEGAL_ADDRESS);
		return;
	}

	//Check for failure
	if (this->Hreg(reg) != value) {
		this->exceptionResponse(MB_FC_WRITE_REG, MB_EX_SLAVE_FAILURE);
		return;
	}

	_reply = MB_REPLY_ECHO;
}

// void Modbus::writeMultipleRegisters(byte* frame,word startreg, word numoutputs, byte bytecount) {
// 	//Check value
// 	if (numoutputs < 0x0001 || numoutputs > 0x007B || bytecount != 2 * numoutputs) {
// 		this->exceptionResponse(MB_FC_WRITE_REGS, MB_EX_ILLEGAL_VALUE);
// 		return;
// 	}
//
// 	//Check Address (startreg...startreg + numregs)
// 	for (int k = 0; k < numoutputs; k++) {
// 		if (!this->searchRegister(startreg + 40001 + k)) {
// 			this->exceptionResponse(MB_FC_WRITE_REGS, MB_EX_ILLEGAL_ADDRESS);
// 			return;
// 		}
// 	}
//
// 	//Clean frame buffer
// 	free(_frame);
// 	_len = 5;
// 	_frame = (byte *) malloc(_len);
// 	if (!_frame) {
// 		this->exceptionResponse(MB_FC_WRITE_REGS, MB_EX_SLAVE_FAILURE);
// 		return;
// 	}
//
// 	_frame[0] = MB_FC_WRITE_REGS;
// 	_frame[1] = startreg >> 8;
// 	_frame[2] = startreg & 0x00FF;
// 	_frame[3] = numoutputs >> 8;
// 	_frame[4] = numoutputs & 0x00FF;
//
// 	word val;
// 	word i = 0;
// 	while(numoutputs--) {
// 		val = (word)frame[6+i*2] << 8 | (word)frame[7+i*2];
// 		this->Hreg(startreg + i, val);
// 		i++;
// 	}
//
// 	_reply = MB_REPLY_NORMAL;
// }
void Modbus::writeMultipleRegisters(byte* frame, word startreg, word numoutputs, byte bytecount) {
	// Check value
	if (numoutputs < 0x0001 || numoutputs > 0x007B || bytecount != 2 * numoutputs) {
		this->exceptionResponse(MB_FC_WRITE_REGS, MB_EX_ILLEGAL_VALUE);
		return;
	}

	// Check Address (startreg...startreg + numoutputs)
	for (word k = 0; k < numoutputs; k++) {
		if (!this->searchRegister(startreg + 40001 + k)) {
			this->exceptionResponse(MB_FC_WRITE_REGS, MB_EX_ILLEGAL_ADDRESS);
			return;
		}
	}

	// Clean frame buffer
	free(_frame);
	_len = 5;
	_frame = (byte *) malloc(_len);
	if (!_frame) {
		this->exceptionResponse(MB_FC_WRITE_REGS, MB_EX_SLAVE_FAILURE);
		return;
	}

	// Set up the frame buffer
	_frame[0] = MB_FC_WRITE_REGS;
	_frame[1] = startreg >> 8;
	_frame[2] = startreg & 0x00FF;
	_frame[3] = numoutputs >> 8;
	_frame[4] = numoutputs & 0x00FF;

	// Use a for loop to process each register value
	for (word i = 0; i < numoutputs; i++) {
		// Combine high byte and low byte to form the register value
		word val = (word)frame[6 + i * 2] << 8 | (word)frame[7 + i * 2];
		// Write the value to the register
		this->Hreg(startreg + i, val);
	}

	_reply = MB_REPLY_NORMAL;
}



#ifndef USE_HOLDING_REGISTERS_ONLY
void Modbus::readCoils(word startreg, word numregs) {
	//Check value (numregs)
	if (numregs < 0x0001 || numregs > 0x07D0) {
		this->exceptionResponse(MB_FC_READ_COILS, MB_EX_ILLEGAL_VALUE);
		return;
	}

	//Check Address
	//Check only startreg. Is this correct?
	//When I check all registers in range I got errors in ScadaBR
	//I think that ScadaBR request more than one in the single request
	//when you have more then one datapoint configured from same type.
	
	// START Modified by Zakr -> validate all registers in the requested range
	for (word i = 0; i < numregs; i++) {
		if (!this->searchRegister(startreg + i+1)) {
			this->exceptionResponse(MB_FC_READ_COILS, MB_EX_ILLEGAL_ADDRESS);
			return;
		}
	}
	
	
	// FIN Modified by Zakr

	//     if (!this->searchRegister(startreg + 1)) {
	//         this->exceptionResponse(MB_FC_READ_COILS, MB_EX_ILLEGAL_ADDRESS);
	//         return;
	//     }

	//Clean frame buffer
	free(_frame);
	_len = 0;

	//Determine the message length = function type, byte count and
	//for each group of 8 registers the message length increases by 1
	_len = 2 + numregs/8;
	if (numregs%8) _len++; //Add 1 to the message length for the partial byte.


	_frame = (byte *) malloc(_len);
	if (!_frame) {
		this->exceptionResponse(MB_FC_READ_COILS, MB_EX_SLAVE_FAILURE);
		return;
	}

	_frame[0] = MB_FC_READ_COILS;
	_frame[1] = _len - 2; //byte count (_len - function code and byte count)


	byte bitn = 0;
	word totregs = numregs;
	word i = 0;
	word byteIndex = 0; // Byte index in the frame
	
	for (word j = 2; j < _len; j++) {
		_frame[j] = 0;
	}
	
	for (word j = 0; j < numregs; j++) {
		if (this->Coil(startreg)) {
			bitSet(_frame[2 + byteIndex], bitn);
			} else {
			bitClear(_frame[2 + byteIndex], bitn);
		}

		// Increment bit index
		bitn++;
		if (bitn == 8) {
			bitn = 0;
			byteIndex++;  // Move to the next byte after 8 bits
		}

		// Increment the register
		startreg++;
	}
	
	
	// 	while (numregs--) {
	// 		//	i = (totregs - numregs) / 8;
	//
	// 		SerialUSB.print("i -> ");
	// 		SerialUSB.print(i);
	// 		SerialUSB.print(";  Coil = ");
	// 		SerialUSB.print(this->Coil(startreg));
	//
	// 		if (this->Coil(startreg))
	// 		bitSet(_frame[2+i], bitn);
	//
	// 		SerialUSB.print(";  _frame[2+i] = ");
	// 		SerialUSB.println(_frame[2+i]);
	//
	// 		// 		else
	// 		// 		bitClear(_frame[2+i], bitn);
	// 		//increment the bit index
	// 		bitn++;
	// 		if (bitn == 8) {
	// 			bitn = 0;
	// 			i++;  // Move to the next byte after 8 bits
	// 		}
	// 		startreg++;
	// 	}

	_reply = MB_REPLY_NORMAL;
}

void Modbus::readInputStatus(word startreg, word numregs) {
	//Check value (numregs)
	if (numregs < 0x0001 || numregs > 0x07D0) {
		this->exceptionResponse(MB_FC_READ_INPUT_STAT, MB_EX_ILLEGAL_VALUE);
		return;
	}

	//Check Address
	//*** See comments on readCoils method.
	if (!this->searchRegister(startreg + 10001)) {
		this->exceptionResponse(MB_FC_READ_COILS, MB_EX_ILLEGAL_ADDRESS);
		return;
	}

	//Clean frame buffer
	free(_frame);
	_len = 0;

	//Determine the message length = function type, byte count and
	//for each group of 8 registers the message length increases by 1
	_len = 2 + numregs/8;
	if (numregs%8) _len++; //Add 1 to the message length for the partial byte.

	_frame = (byte *) malloc(_len);
	if (!_frame) {
		this->exceptionResponse(MB_FC_READ_INPUT_STAT, MB_EX_SLAVE_FAILURE);
		return;
	}

	_frame[0] = MB_FC_READ_INPUT_STAT;
	_frame[1] = _len - 2;

	byte bitn = 0;
	word totregs = numregs;
	word i;
	while (numregs--) {
		i = (totregs - numregs) / 8;
		if (this->Ists(startreg))
		bitSet(_frame[2+i], bitn);
		else
		bitClear(_frame[2+i], bitn);
		//increment the bit index
		bitn++;
		if (bitn == 8) bitn = 0;
		//increment the register
		startreg++;
	}

	_reply = MB_REPLY_NORMAL;
}
//
// void Modbus::readInputRegisters(word startreg, word numregs) {
// 	//Check value (numregs)
// 	if (numregs < 0x0001 || numregs > 0x007D) {
// 		this->exceptionResponse(MB_FC_READ_INPUT_REGS, MB_EX_ILLEGAL_VALUE);
// 		return;
// 	}
//
// 	//Check Address
// 	//*** See comments on readCoils method.
// 	if (!this->searchRegister(startreg + 30001)) {
// 		this->exceptionResponse(MB_FC_READ_COILS, MB_EX_ILLEGAL_ADDRESS);
// 		return;
// 	}
//
// 	//Clean frame buffer
// 	free(_frame);
// 	_len = 0;
//
// 	//calculate the query reply message length
// 	//for each register queried add 2 bytes
// 	_len = 2 + numregs * 2;
//
// 	_frame = (byte *) malloc(_len);
// 	if (!_frame) {
// 		this->exceptionResponse(MB_FC_READ_INPUT_REGS, MB_EX_SLAVE_FAILURE);
// 		return;
// 	}
//
// 	_frame[0] = MB_FC_READ_INPUT_REGS;
// 	_frame[1] = _len - 2;
//
// 	word val;
// 	word i = 0;
// 	while(numregs--) {
// 		//retrieve the value from the register bank for the current register
// 		val = this->Ireg(startreg + i);
// 		//write the high byte of the register value
// 		_frame[2 + i * 2]  = val >> 8;
// 		//write the low byte of the register value
// 		_frame[3 + i * 2] = val & 0xFF;
// 		i++;
// 	}
//
// 	_reply = MB_REPLY_NORMAL;
// }


void Modbus::readInputRegisters(word startreg, word numregs) {
	// Check value (numregs)
	if (numregs < 0x0001 || numregs > 0x007D) {
		this->exceptionResponse(MB_FC_READ_INPUT_REGS, MB_EX_ILLEGAL_VALUE);
		return;
	}

	// Check Address
	// Adjusted to check for the presence of registers in the range.
	// 	if (!this->searchRegister(startreg + 1)) {
	// 		this->exceptionResponse(MB_FC_READ_INPUT_REGS, MB_EX_ILLEGAL_ADDRESS);
	// 		return;
	// 	}
	
	for (word i = 0; i < numregs; i++) {
		if (!this->searchRegister(startreg + i+30001)) {
			this->exceptionResponse(MB_FC_READ_COILS, MB_EX_ILLEGAL_ADDRESS);
			return;
		}
	}

	// Clean frame buffer
	free(_frame);
	_len = 0;

	// Calculate the query reply message length
	// For each register queried add 2 bytes
	_len = 2 + numregs * 2;

	// Allocate memory for the frame
	_frame = (byte *) malloc(_len);
	if (!_frame) {
		this->exceptionResponse(MB_FC_READ_INPUT_REGS, MB_EX_SLAVE_FAILURE);
		return;
	}

	_frame[0] = MB_FC_READ_INPUT_REGS;
	_frame[1] = _len - 2; // Byte count (excluding function code and byte count)

	// Use a for loop to process each register
	for (word i = 0; i < numregs; i++) {
		// Retrieve the value from the register bank for the current register
		word val = this->Ireg(startreg + i);
		// Write the high byte of the register value
		_frame[2 + i * 2] = val >> 8;
		// Write the low byte of the register value
		_frame[3 + i * 2] = val & 0xFF;
	}

	_reply = MB_REPLY_NORMAL;
}


void Modbus::writeSingleCoil(word reg, word status) {
	//Check value (status)
	if (status != 0xFF00 && status != 0x0000) {
		this->exceptionResponse(MB_FC_WRITE_COIL, MB_EX_ILLEGAL_VALUE);
		return;
	}

	//Check Address and execute (reg exists?)
	if (!this->Coil(reg, (bool)status)) {
		this->exceptionResponse(MB_FC_WRITE_COIL, MB_EX_ILLEGAL_ADDRESS);
		return;
	}

	//Check for failure
	if (this->Coil(reg) != (bool)status) {
		this->exceptionResponse(MB_FC_WRITE_COIL, MB_EX_SLAVE_FAILURE);
		return;
	}

	_reply = MB_REPLY_ECHO;
}

void Modbus::writeMultipleCoils(byte* frame,word startreg, word numoutputs, byte bytecount) {
	//Check value
	word bytecount_calc = numoutputs / 8;
	if (numoutputs%8) bytecount_calc++;
	if (numoutputs < 0x0001 || numoutputs > 0x07B0 || bytecount != bytecount_calc) {
		this->exceptionResponse(MB_FC_WRITE_COILS, MB_EX_ILLEGAL_VALUE);
		return;
	}

	//Check Address (startreg...startreg + numregs)
	for (int k = 0; k < numoutputs; k++) {
		if (!this->searchRegister(startreg + 1 + k)) {
			this->exceptionResponse(MB_FC_WRITE_COILS, MB_EX_ILLEGAL_ADDRESS);
			return;
		}
	}

	//Clean frame buffer
	free(_frame);
	_len = 5;
	_frame = (byte *) malloc(_len);
	if (!_frame) {
		this->exceptionResponse(MB_FC_WRITE_COILS, MB_EX_SLAVE_FAILURE);
		return;
	}

	_frame[0] = MB_FC_WRITE_COILS;
	_frame[1] = startreg >> 8;
	_frame[2] = startreg & 0x00FF;
	_frame[3] = numoutputs >> 8;
	_frame[4] = numoutputs & 0x00FF;

	byte bitn = 0;
	word totoutputs = numoutputs;
	word i;
	while (numoutputs--) {
		i = (totoutputs - numoutputs) / 8;
		this->Coil(startreg, bitRead(frame[6+i], bitn));
		//increment the bit index
		bitn++;
		if (bitn == 8) bitn = 0;
		//increment the register
		startreg++;
	}

	_reply = MB_REPLY_NORMAL;
}
#endif



