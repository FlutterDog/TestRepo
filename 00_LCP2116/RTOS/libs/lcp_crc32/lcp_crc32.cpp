/**
 * @file lcp_crc32.cpp
 * @brief Реализация CRC-32/ISO-HDLC.
 */

#include "lcp_crc32.hpp"

uint32_t lcp_crc32(const void* data, size_t length)
{
    if ((data == 0) && (length != 0U))
    {
        return 0U;
    }

    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint32_t crc = 0xFFFFFFFFUL;

    for (size_t index = 0U; index < length; ++index)
    {
        crc ^= bytes[index];

        for (uint8_t bit = 0U; bit < 8U; ++bit)
        {
            const uint32_t mask = 0UL - (crc & 1UL);
            crc = (crc >> 1U) ^ (0xEDB88320UL & mask);
        }
    }

    return crc ^ 0xFFFFFFFFUL;
}
