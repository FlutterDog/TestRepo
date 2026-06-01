/**
 * @file print.hpp
 * @brief Базовый потоковый вывод для платформенных портов.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Базовый класс потокового вывода.
 *
 * Класс задаёт единый интерфейс write(), print() и println()
 * для последовательных портов и служебных потоков проекта.
 */
class Print
{
public:
    virtual ~Print() {}

    virtual size_t write(uint8_t value) = 0;

    size_t write(const char* text);
    size_t write(const uint8_t* buffer, size_t size);

    size_t print(const char* text);
    size_t print(char value);
    size_t print(unsigned char value, int base = 10);
    size_t print(int value, int base = 10);
    size_t print(unsigned int value, int base = 10);
    size_t print(long value, int base = 10);
    size_t print(unsigned long value, int base = 10);
    size_t print(double value, int digits = 2);

    size_t println(void);
    size_t println(const char* text);
    size_t println(char value);
    size_t println(unsigned char value, int base = 10);
    size_t println(int value, int base = 10);
    size_t println(unsigned int value, int base = 10);
    size_t println(long value, int base = 10);
    size_t println(unsigned long value, int base = 10);
    size_t println(double value, int digits = 2);

private:
    size_t print_number(unsigned long value, uint8_t base);
    size_t print_signed(long value, uint8_t base);
};
