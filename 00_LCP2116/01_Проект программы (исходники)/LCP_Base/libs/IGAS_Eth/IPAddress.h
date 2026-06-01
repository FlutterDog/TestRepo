
/**
 * @file IPAddress.h
 * @brief IPv4-адрес для сетевого стека проекта.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

class Print;

class IPAddress
{
public:
    IPAddress();
    IPAddress(uint8_t first, uint8_t second, uint8_t third, uint8_t fourth);
    explicit IPAddress(uint32_t address);
    explicit IPAddress(const uint8_t* address);

    bool fromString(const char* address);

    uint8_t operator[](int index) const;
    uint8_t& operator[](int index);

    IPAddress& operator=(const uint8_t* address);
    IPAddress& operator=(uint32_t address);

    bool operator==(const IPAddress& address) const;
    bool operator!=(const IPAddress& address) const;
    bool operator==(const uint8_t* address) const;

    operator uint32_t() const;

    uint8_t* raw_address();
    const uint8_t* raw_address() const;

    size_t printTo(Print& printer) const;

private:
    union
    {
        uint8_t bytes[4];
        uint32_t dword;
    } address_;
};

extern const IPAddress INADDR_NONE;

inline uint8_t* rawIPAddress(IPAddress& address)
{
    return address.raw_address();
}

inline const uint8_t* rawIPAddress(const IPAddress& address)
{
    return address.raw_address();
}
