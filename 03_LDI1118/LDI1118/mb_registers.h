/*
case :			//
	regs[i] = ;
	break;
*/

// Состояние дискретных входов:
case 0:
	regs[i] = mb_reg0;
	break;

// Серийный номер устройства:
case 39:
	regs[i] = eeprom_read_word((u16 *)EEP_DEV_SN);
	break;

// Версия прошивки:
case 40:
	regs[i] = DEV_SW_VER;
	break;

// Modbus-адрес устройства:
case 100:
	//if ((writeReg) && (Value_ < 248)){
		if (writeReg){
		modbus.slave_id = Value_;
	//	eeprom_write_byte((u8 *)EEP_MB_ADDR, modbus.slave_id);
	};
	regs[i] = modbus.slave_id;
	break;

// Запись серийного номера устройства:
case 591:
	if (writeReg){
		eeprom_write_word((u16 *)EEP_DEV_SN, (u16) Value_);
	};
	regs[i] = eeprom_read_word((u16 *)EEP_DEV_SN);
	break;

// Перезагрузка:
case 594:
	if (writeReg){
		if (Value_ == 1){
			reboot();
		};
	};
	regs[i] = 0;
	break;

// Переход в загрузчик:
case 900:
	if (writeReg){
		cli();
		GotoBootloader();
	};
	break;

// Версия платы:
case 1000:
	if (writeReg){
		eeprom_write_word((u16 *)EEP_DEV_HW_VER, (u16) Value_);
	};
	regs[i] = eeprom_read_word((u16 *)EEP_DEV_HW_VER);
	break;

// Версия модуля:
case 1001:
	regs[i] = DEV_ID;
	break;

default:
	regs[i] = -1;     // Возвращанем -1, если запрашиваемый регистр не используется.
	break;
