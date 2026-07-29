/**
 * @file battery_status.hpp
 * @brief Диагностика резервной батареи CR2032.
 *
 * Плата предоставляет дискретный выход компаратора, а не аналоговое напряжение.
 * Поэтому отчёт показывает instant/debounced state относительно аппаратного
 * порога, но не должен интерпретироваться как измерение в вольтах.
 */

#pragma once

/** @brief Настраивает вход компаратора и сбрасывает debounce state. */
void battery_status_init(void);

/**
 * @brief Выполняет неблокирующий debounce входа батареи.
 *
 * Вызов должен оставаться в app loop. При добавлении ADC-измерения его следует
 * реализовать отдельным HAL/API, не заменяя текущий дискретный признак.
 */
void battery_status_poll(void);

/** @brief Печатает hardware threshold, raw, stable state и требуемое действие. */
void battery_status_print_report(void);
