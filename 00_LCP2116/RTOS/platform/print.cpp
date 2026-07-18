/**
 * @file print.cpp
 * @brief Реализация базового потокового вывода.
 */

#include "print.hpp"

size_t Print::write(const char* text)
{
    if (text == 0)
    {
        return 0U;
    }

    size_t written = 0U;

    while (*text != '\0')
    {
        written += write(static_cast<uint8_t>(*text));
        ++text;
    }

    return written;
}

size_t Print::write(const uint8_t* buffer, size_t size)
{
    if (buffer == 0)
    {
        return 0U;
    }

    size_t written = 0U;

    for (size_t index = 0U; index < size; ++index)
    {
        written += write(buffer[index]);
    }

    return written;
}

size_t Print::print(const char* text)
{
    return write(text);
}

size_t Print::print(char value)
{
    return write(static_cast<uint8_t>(value));
}

size_t Print::print(unsigned char value, int base)
{
    return print_number(static_cast<unsigned long>(value), static_cast<uint8_t>(base));
}

size_t Print::print(int value, int base)
{
    return print_signed(static_cast<long>(value), static_cast<uint8_t>(base));
}

size_t Print::print(unsigned int value, int base)
{
    return print_number(static_cast<unsigned long>(value), static_cast<uint8_t>(base));
}

size_t Print::print(long value, int base)
{
    return print_signed(value, static_cast<uint8_t>(base));
}

size_t Print::print(unsigned long value, int base)
{
    return print_number(value, static_cast<uint8_t>(base));
}

size_t Print::print(double value, int digits)
{
    if (digits < 0)
    {
        digits = 0;
    }

    if (digits > 9)
    {
        digits = 9;
    }

    size_t written = 0U;

    if (value < 0.0)
    {
        written += write(static_cast<uint8_t>('-'));
        value = -value;
    }

    double rounding = 0.5;

    for (int index = 0; index < digits; ++index)
    {
        rounding *= 0.1;
    }

    value += rounding;

    const unsigned long integer_part = static_cast<unsigned long>(value);
    double remainder = value - static_cast<double>(integer_part);

    written += print_number(integer_part, 10U);

    if (digits > 0)
    {
        written += write(static_cast<uint8_t>('.'));

        while (digits > 0)
        {
            remainder *= 10.0;
            const int digit = static_cast<int>(remainder);
            written += write(static_cast<uint8_t>('0' + digit));
            remainder -= static_cast<double>(digit);
            --digits;
        }
    }

    return written;
}

size_t Print::println(void)
{
    return write("\r\n");
}

size_t Print::println(const char* text)
{
    size_t written = print(text);
    written += println();
    return written;
}

size_t Print::println(char value)
{
    size_t written = print(value);
    written += println();
    return written;
}

size_t Print::println(unsigned char value, int base)
{
    size_t written = print(value, base);
    written += println();
    return written;
}

size_t Print::println(int value, int base)
{
    size_t written = print(value, base);
    written += println();
    return written;
}

size_t Print::println(unsigned int value, int base)
{
    size_t written = print(value, base);
    written += println();
    return written;
}

size_t Print::println(long value, int base)
{
    size_t written = print(value, base);
    written += println();
    return written;
}

size_t Print::println(unsigned long value, int base)
{
    size_t written = print(value, base);
    written += println();
    return written;
}

size_t Print::println(double value, int digits)
{
    size_t written = print(value, digits);
    written += println();
    return written;
}

size_t Print::print_number(unsigned long value, uint8_t base)
{
    char buffer[33];
    char* cursor = &buffer[32];

    *cursor = '\0';

    if ((base < 2U) || (base > 16U))
    {
        base = 10U;
    }

    do
    {
        const uint8_t digit = static_cast<uint8_t>(value % base);
        --cursor;
        *cursor = static_cast<char>((digit < 10U) ? ('0' + digit) : ('A' + digit - 10U));
        value /= base;
    }
    while (value != 0U);

    return write(cursor);
}

size_t Print::print_signed(long value, uint8_t base)
{
    if ((base == 10U) && (value < 0))
    {
        size_t written = write(static_cast<uint8_t>('-'));
        written += print_number(static_cast<unsigned long>(-value), base);
        return written;
    }

    return print_number(static_cast<unsigned long>(value), base);
}
