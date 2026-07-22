/**
 * @file lcp_crc32.hpp
 * @brief Небольшой CRC-32/ISO-HDLC без динамической памяти.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Вычисляет CRC-32/ISO-HDLC (polynomial 0xEDB88320).
 * @param[in] data Буфер; null допустим только при length=0.
 * @param[in] length Количество байтов.
 * @return CRC с initial/final XOR 0xFFFFFFFF.
 */
uint32_t lcp_crc32(const void* data, size_t length);
