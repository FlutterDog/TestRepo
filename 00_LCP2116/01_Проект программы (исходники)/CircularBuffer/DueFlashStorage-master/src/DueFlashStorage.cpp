/*
  DueFlashStorage.cpp - DueFlashStorage library
  Copyright (c) 2012 Cristian Maglie.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "DueFlashStorage.h"

#define FLASH_PAGE_SIZE   IFLASH1_PAGE_SIZE   // in bytes
#define FLASH_SIZE        IFLASH1_SIZE        // in bytes
#define FLASH_START       IFLASH1_ADDR        // in bytes
#define FLASH_PAGES       IFLASH1_NB_OF_PAGES

DueFlashStorage::DueFlashStorage() {
  uint32_t retCode;

  /* Initialize flash: 6 wait states for flash writing. */
  retCode = flash_init(FLASH_ACCESS_MODE_128, 6);
  if (retCode != FLASH_RC_OK) {
    SerialUSB.println("Flash init failed");
  }
}

byte DueFlashStorage::read(uint32_t address) {
  if (address >= FLASH_SIZE) return 0xFF; // Address out of range
  return ((byte *)FLASH_START)[address];
}

byte* DueFlashStorage::readAddress(uint32_t address) {
  if (address >= FLASH_SIZE) return NULL; // Address out of range
  return ((byte *)FLASH_START) + address;
}

boolean DueFlashStorage::write(uint32_t address, byte value) {
  return write(address, &value, 1);
}

boolean DueFlashStorage::write(uint32_t address, byte *data, uint32_t dataLength) {
  if (address + dataLength >= FLASH_SIZE) return false; // Address out of range

  uint32_t retCode = flash_unlock(address, address + dataLength - 1, 0, 0);
  if (retCode != FLASH_RC_OK) {
    SerialUSB.println("Failed to unlock flash for write");
    return false;
  }

  retCode = flash_write(address, data, dataLength, 0);
  if (retCode != FLASH_RC_OK) {
    SerialUSB.println("Flash write failed");
    return false;
  }

  retCode = flash_lock(address, address + dataLength - 1, 0, 0);
  if (retCode != FLASH_RC_OK) {
    SerialUSB.println("Failed to lock flash after write");
    return false;
  }

  return true;
}

boolean DueFlashStorage::write_unlocked(uint32_t address, byte value) {
  return write_unlocked(address, &value, 1);
}

boolean DueFlashStorage::write_unlocked(uint32_t address, byte *data, uint32_t dataLength) {
  if (address + dataLength >= FLASH_SIZE) return false; // Address out of range

  uint32_t retCode = flash_write(address, data, dataLength, 0);
  if (retCode != FLASH_RC_OK) {
    SerialUSB.println("Flash write failed");
    return false;
  }

  return true;
}
