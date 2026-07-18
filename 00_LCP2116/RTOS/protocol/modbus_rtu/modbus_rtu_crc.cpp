/**
 * @file modbus_rtu_crc.cpp
 * @brief Реализация CRC-16 Modbus RTU.
 */

#include "modbus_rtu_crc.hpp"

uint16_t modbus_rtu_crc16(const uint8_t* data, size_t length)
{
    uint16_t crc = 0xFFFFU;

    if (data == 0)
    {
        return crc;
    }

    for (size_t byte_index = 0U; byte_index < length; ++byte_index)
    {
        crc ^= static_cast<uint16_t>(data[byte_index]);

        for (uint8_t bit_index = 0U; bit_index < 8U; ++bit_index)
        {
            if ((crc & 0x0001U) != 0U)
            {
                crc = static_cast<uint16_t>((crc >> 1U) ^ 0xA001U);
            }
            else
            {
                crc = static_cast<uint16_t>(crc >> 1U);
            }
        }
    }

    return crc;
}
