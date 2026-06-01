
/**
 * @file IPAddress.cpp
 * @brief Реализация IPv4-адреса.
 */

#include "IPAddress.h"
#include "../../platform/platform.hpp"

#include <string.h>

const IPAddress INADDR_NONE(0U, 0U, 0U, 0U);

IPAddress::IPAddress()
{
    address_.dword = 0U;
}

IPAddress::IPAddress(uint8_t first, uint8_t second, uint8_t third, uint8_t fourth)
{
    address_.bytes[0] = first;
    address_.bytes[1] = second;
    address_.bytes[2] = third;
    address_.bytes[3] = fourth;
}

IPAddress::IPAddress(uint32_t address)
{
    address_.dword = address;
}

IPAddress::IPAddress(const uint8_t* address)
{
    if (address != 0)
    {
        memcpy(address_.bytes, address, sizeof(address_.bytes));
    }
    else
    {
        address_.dword = 0U;
    }
}

bool IPAddress::fromString(const char* address)
{
    uint16_t accumulator = 0U;
    uint8_t dots = 0U;

    if (address == 0)
    {
        return false;
    }

    while (*address != '\0')
    {
        const char character = *address++;

        if ((character >= '0') && (character <= '9'))
        {
            accumulator = static_cast<uint16_t>((accumulator * 10U) + static_cast<uint16_t>(character - '0'));

            if (accumulator > 255U)
            {
                return false;
            }
        }
        else if (character == '.')
        {
            if (dots >= 3U)
            {
                return false;
            }

            address_.bytes[dots] = static_cast<uint8_t>(accumulator);
            ++dots;
            accumulator = 0U;
        }
        else
        {
            return false;
        }
    }

    if (dots != 3U)
    {
        return false;
    }

    address_.bytes[3] = static_cast<uint8_t>(accumulator);
    return true;
}

uint8_t IPAddress::operator[](int index) const
{
    return address_.bytes[index];
}

uint8_t& IPAddress::operator[](int index)
{
    return address_.bytes[index];
}

IPAddress& IPAddress::operator=(const uint8_t* address)
{
    if (address != 0)
    {
        memcpy(address_.bytes, address, sizeof(address_.bytes));
    }
    else
    {
        address_.dword = 0U;
    }

    return *this;
}

IPAddress& IPAddress::operator=(uint32_t address)
{
    address_.dword = address;
    return *this;
}

bool IPAddress::operator==(const IPAddress& address) const
{
    return address_.dword == address.address_.dword;
}

bool IPAddress::operator!=(const IPAddress& address) const
{
    return !(*this == address);
}

bool IPAddress::operator==(const uint8_t* address) const
{
    if (address == 0)
    {
        return false;
    }

    return memcmp(address_.bytes, address, sizeof(address_.bytes)) == 0;
}

IPAddress::operator uint32_t() const
{
    return address_.dword;
}

uint8_t* IPAddress::raw_address()
{
    return address_.bytes;
}

const uint8_t* IPAddress::raw_address() const
{
    return address_.bytes;
}

size_t IPAddress::printTo(Print& printer) const
{
    size_t written = 0U;
    written += printer.print(address_.bytes[0], DEC);
    written += printer.print('.');
    written += printer.print(address_.bytes[1], DEC);
    written += printer.print('.');
    written += printer.print(address_.bytes[2], DEC);
    written += printer.print('.');
    written += printer.print(address_.bytes[3], DEC);
    return written;
}
