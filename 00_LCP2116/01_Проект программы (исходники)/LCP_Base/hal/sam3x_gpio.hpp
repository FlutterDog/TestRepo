/**
 * @file sam3x_gpio.hpp
 * @brief HAL GPIO для ATSAM3X8E.
 *
 * Модуль управляет дискретными линиями через контроллеры PIO микроконтроллера.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Режим дискретной линии.
 */
enum HalGpioMode
{
    HAL_GPIO_INPUT = 0,
    HAL_GPIO_OUTPUT = 1,
    HAL_GPIO_INPUT_PULLUP = 2
};

/**
 * @brief Настраивает режим дискретной линии.
 *
 * @param pin_id Платформенный номер линии.
 * @param mode Режим линии.
 */
void hal_gpio_configure(uint32_t pin_id, HalGpioMode mode);

/**
 * @brief Устанавливает состояние дискретного выхода.
 *
 * @param pin_id Платформенный номер линии.
 * @param value Ноль задаёт низкий уровень, ненулевое значение задаёт высокий уровень.
 */
void hal_gpio_write(uint32_t pin_id, uint32_t value);

/**
 * @brief Считывает состояние дискретной линии.
 *
 * @param pin_id Платформенный номер линии.
 * @return Ноль при низком уровне, единица при высоком уровне.
 */
uint32_t hal_gpio_read(uint32_t pin_id);

/**
 * @brief Проверяет наличие линии в таблице GPIO.
 *
 * @param pin_id Платформенный номер линии.
 * @return Единица, если линия описана в таблице GPIO; ноль, если линия не описана.
 */
uint8_t hal_gpio_is_valid(uint32_t pin_id);
