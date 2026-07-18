/**
 * @file platform_gpio.cpp
 * @brief Платформенные функции дискретного ввода-вывода.
 *
 * Модуль связывает прикладной интерфейс GPIO с HAL ATSAM3X8E.
 */

#include "platform.hpp"
#include "../hal/sam3x_gpio.hpp"

void pinMode(uint32_t pin, uint32_t mode)
{
    HalGpioMode gpio_mode = HAL_GPIO_INPUT;

    if (mode == OUTPUT)
    {
        gpio_mode = HAL_GPIO_OUTPUT;
    }
    else if (mode == INPUT_PULLUP)
    {
        gpio_mode = HAL_GPIO_INPUT_PULLUP;
    }
    else
    {
        gpio_mode = HAL_GPIO_INPUT;
    }

    hal_gpio_configure(pin, gpio_mode);
}

void digitalWrite(uint32_t pin, uint32_t value)
{
    hal_gpio_write(pin, value);
}

uint32_t digitalRead(uint32_t pin)
{
    return hal_gpio_read(pin);
}
