/**
 * @file sam3x_gpio.cpp
 * @brief Реализация HAL GPIO для ATSAM3X8E.
 */

#include "sam3x_gpio.hpp"
#include "sam3x_device.hpp"

struct HalGpioPinDescription
{
    uint32_t pin_id;
    Pio* port;
    uint32_t mask;
    uint32_t peripheral_id;
};

static const HalGpioPinDescription g_gpio_pins[] =
{
    { 2U,  PIOB, (1UL << 25), ID_PIOB },

    { 3U,  PIOC, (1UL << 28), ID_PIOC },
    { 5U,  PIOC, (1UL << 25), ID_PIOC },
    { 6U,  PIOC, (1UL << 24), ID_PIOC },

    { 7U,  PIOC, (1UL << 23), ID_PIOC },
    { 8U,  PIOC, (1UL << 22), ID_PIOC },
    { 9U,  PIOC, (1UL << 21), ID_PIOC },

    { 10U, PIOC, (1UL << 29), ID_PIOC },

    { 23U, PIOA, (1UL << 14), ID_PIOA },
    { 24U, PIOA, (1UL << 15), ID_PIOA },

    { 40U, PIOC, (1UL << 8),  ID_PIOC },
    { 41U, PIOC, (1UL << 9),  ID_PIOC },

    { 45U, PIOC, (1UL << 18), ID_PIOC },
    { 48U, PIOC, (1UL << 15), ID_PIOC },

    { 54U, PIOA, (1UL << 16), ID_PIOA },
    { 61U, PIOA, (1UL << 2),  ID_PIOA }
};

static const HalGpioPinDescription* hal_gpio_find_pin(uint32_t pin_id)
{
    const uint32_t count = sizeof(g_gpio_pins) / sizeof(g_gpio_pins[0]);

    for (uint32_t index = 0U; index < count; ++index)
    {
        if (g_gpio_pins[index].pin_id == pin_id)
        {
            return &g_gpio_pins[index];
        }
    }

    return 0;
}

static void hal_gpio_enable_clock(uint32_t peripheral_id)
{
    if (peripheral_id < 32U)
    {
        PMC->PMC_PCER0 = (1UL << peripheral_id);
    }
    else
    {
        PMC->PMC_PCER1 = (1UL << (peripheral_id - 32U));
    }
}

void hal_gpio_configure(uint32_t pin_id, HalGpioMode mode)
{
    const HalGpioPinDescription* pin = hal_gpio_find_pin(pin_id);

    if (pin == 0)
    {
        return;
    }

    hal_gpio_enable_clock(pin->peripheral_id);

    pin->port->PIO_PER = pin->mask;
    pin->port->PIO_IDR = pin->mask;

    if (mode == HAL_GPIO_OUTPUT)
    {
        pin->port->PIO_PUDR = pin->mask;
        pin->port->PIO_OER = pin->mask;
    }
    else
    {
        pin->port->PIO_ODR = pin->mask;

        if (mode == HAL_GPIO_INPUT_PULLUP)
        {
            pin->port->PIO_PUER = pin->mask;
        }
        else
        {
            pin->port->PIO_PUDR = pin->mask;
        }
    }
}

void hal_gpio_write(uint32_t pin_id, uint32_t value)
{
    const HalGpioPinDescription* pin = hal_gpio_find_pin(pin_id);

    if (pin == 0)
    {
        return;
    }

    hal_gpio_enable_clock(pin->peripheral_id);

    if (value != 0U)
    {
        pin->port->PIO_SODR = pin->mask;
    }
    else
    {
        pin->port->PIO_CODR = pin->mask;
    }
}

uint32_t hal_gpio_read(uint32_t pin_id)
{
    const HalGpioPinDescription* pin = hal_gpio_find_pin(pin_id);

    if (pin == 0)
    {
        return 0U;
    }

    hal_gpio_enable_clock(pin->peripheral_id);

    return ((pin->port->PIO_PDSR & pin->mask) != 0U) ? 1U : 0U;
}

uint8_t hal_gpio_is_valid(uint32_t pin_id)
{
    return (hal_gpio_find_pin(pin_id) != 0) ? 1U : 0U;
}
