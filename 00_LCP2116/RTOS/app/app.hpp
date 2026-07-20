/**
 * @file app.hpp
 * @brief Прикладной и RTOS-интерфейс базовой прошивки LCP.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Выполняет однократную инициализацию baseline-прошивки LCP.
 *
 * Функция вызывается из основной задачи после запуска планировщика FreeRTOS.
 */
void setup(void);

/**
 * @brief Выполняет один цикл обслуживания baseline-прошивки LCP.
 */
void loop(void);

/**
 * @brief Создаёт базовую задачу LCP и запускает планировщик FreeRTOS.
 *
 * При штатном запуске функция не возвращается.
 */
void app_rtos_start(void);

/** @brief Возвращает 1, если планировщик FreeRTOS работает. */
uint8_t app_rtos_scheduler_running(void);

/** @brief Возвращает текущее количество тиков FreeRTOS. */
uint32_t app_rtos_tick_count(void);

/** @brief Возвращает полный размер heap FreeRTOS в байтах. */
uint32_t app_rtos_heap_total_bytes(void);

/** @brief Возвращает текущий свободный объём heap FreeRTOS в байтах. */
uint32_t app_rtos_free_heap_bytes(void);

/** @brief Возвращает минимальный за всё время свободный heap в байтах. */
uint32_t app_rtos_minimum_ever_free_heap_bytes(void);

/** @brief Возвращает полный размер стека базовой задачи LCP в словах. */
uint32_t app_rtos_lcp_stack_total_words(void);

/** @brief Возвращает полный размер стека базовой задачи LCP в байтах. */
uint32_t app_rtos_lcp_stack_total_bytes(void);

/**
 * @brief Возвращает минимальный остаток стека базовой задачи LCP.
 *
 * Значение указано в словах StackType_t, для Cortex-M3 одно слово равно
 * четырём байтам.
 */
uint32_t app_rtos_lcp_stack_free_words(void);

/** @brief Возвращает общий размер внутренней SRAM ATSAM3X8E в байтах. */
uint32_t app_ram_total_bytes(void);

/** @brief Возвращает размер инициализированной секции .data в SRAM. */
uint32_t app_ram_static_data_bytes(void);

/** @brief Возвращает размер нулевой секции .bss в SRAM. */
uint32_t app_ram_static_bss_bytes(void);

/** @brief Возвращает размер стартового стека из linker script. */
uint32_t app_ram_startup_stack_bytes(void);

/** @brief Возвращает размер C-library heap из linker script. */
uint32_t app_ram_c_heap_reserved_bytes(void);

/** @brief Возвращает незанятый linker-секциями хвост SRAM. */
uint32_t app_ram_linker_unassigned_bytes(void);
