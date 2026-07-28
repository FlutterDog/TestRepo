/*
  DueFlashStorage.h - DueFlashStorage library
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

#ifndef DueFlashStorage_h
#define DueFlashStorage_h

#include <Arduino.h>
#include <flash_efc.h>
#include <efc.h>

class DueFlashStorage {
public:
  DueFlashStorage();
  byte read(uint32_t address);
  byte* readAddress(uint32_t address);
  boolean write(uint32_t address, byte value);
  boolean write(uint32_t address, byte *data, uint32_t dataLength);
  boolean write_unlocked(uint32_t address, byte value);
  boolean write_unlocked(uint32_t address, byte *data, uint32_t dataLength);
};

#endif