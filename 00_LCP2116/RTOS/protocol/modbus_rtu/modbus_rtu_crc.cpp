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

uint8_t modbus_rtu_crc_self_test(void)
{
    static const uint8_t TEST_FRAME[] =
    {
        0x01U, 0x03U, 0x00U, 0x00U, 0x00U, 0x01U
    };
    static const uint16_t EXPECTED_CRC = 0x0A84U;

    return (modbus_rtu_crc16(TEST_FRAME, sizeof(TEST_FRAME)) == EXPECTED_CRC)
               ? 1U
               : 0U;
}
