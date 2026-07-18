/**
 * @file modbus_rtu_crc.hpp
 * @brief Расчёт контрольной суммы Modbus RTU CRC-16.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Вычисляет стандартную CRC-16 Modbus с полиномом 0xA001.
 *
 * В Modbus RTU младший байт результата передаётся первым.
 *
 * @param data Указатель на данные.
 * @param length Количество байтов.
 *
 * @return Значение CRC-16.
 */
uint16_t modbus_rtu_crc16(const uint8_t* data, size_t length);
