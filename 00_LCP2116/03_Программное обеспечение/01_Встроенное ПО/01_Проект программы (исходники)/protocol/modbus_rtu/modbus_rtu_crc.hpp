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

/**
 * @brief Проверяет реализацию CRC по известному Modbus RTU кадру.
 *
 * Контрольные данные: `01 03 00 00 00 01`, ожидаемое значение CRC 0x0A84,
 * передаваемые байты `84 0A`.
 *
 * @return 1 при успешной проверке, иначе 0.
 */
uint8_t modbus_rtu_crc_self_test(void);
