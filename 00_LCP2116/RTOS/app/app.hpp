/**
 * @file app.hpp
 * @brief Прикладной lifecycle и RTOS-метрики базовой прошивки LCP.
 *
 * Baseline использует одну FreeRTOS-задачу LCP. `setup()` и `loop()` сохранены
 * как понятная Arduino-подобная граница, но вызываются из этой задачи, а не из
 * bare-metal main. Новые сервисы добавляются парой `init()`/`poll()`; poll не
 * должен ожидать периферию или выделять память на каждом проходе.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Однократно инициализирует board, services и service console.
 *
 * Порядок важен: сначала GPIO/transport, затем microSD и потребители
 * конфигурации, после них диагностика/watchdog. Функция вызывается внутри LCP
 * task после старта планировщика.
 */
void setup(void);

/**
 * @brief Выполняет один неблокирующий проход всех baseline-services.
 *
 * Новый сервис должен получить короткий `*_poll()` в логически подходящем
 * месте. Длительная операция хранит state между вызовами, а не выполняет
 * busy-wait внутри loop().
 */
void loop(void);

/**
 * @brief Создаёт LCP task и запускает FreeRTOS scheduler.
 *
 * При штатном запуске не возвращается. Возврат scheduler означает фатальную
 * ошибку запуска; реализация переводит MCU в fail-stop с запрещёнными IRQ.
 */
void app_rtos_start(void);

/** @return 1, если scheduler находится в TASK_SCHEDULER_RUNNING. */
uint8_t app_rtos_scheduler_running(void);

/** @return Текущее число тиков FreeRTOS с 32-битным переполнением. */
uint32_t app_rtos_tick_count(void);

/**
 * @return Размер массива heap_4 в байтах (`configTOTAL_HEAP_SIZE`).
 * @note Этот массив уже входит в `.bss`; не прибавляйте его к SRAM повторно.
 */
uint32_t app_rtos_heap_total_bytes(void);

/** @return Текущий свободный heap_4 в байтах. */
uint32_t app_rtos_free_heap_bytes(void);

/**
 * @return Минимальный свободный heap_4 с момента старта в байтах.
 * Это high-water metric; занято на пике = total - minimum ever free.
 */
uint32_t app_rtos_minimum_ever_free_heap_bytes(void);

/** @return Размер стека LCP task в StackType_t words. */
uint32_t app_rtos_lcp_stack_total_words(void);

/** @return Размер стека LCP task в байтах. */
uint32_t app_rtos_lcp_stack_total_bytes(void);

/**
 * @brief Возвращает stack high-water mark LCP task.
 *
 * Это минимальный свободный остаток за всё время, а не текущий свободный стек.
 * На Cortex-M3 один StackType_t равен четырём байтам.
 *
 * @return Минимальный свободный остаток в StackType_t words либо 0 до создания task.
 */
uint32_t app_rtos_lcp_stack_free_words(void);

/** @return Полный адресный диапазон внутренней SRAM ATSAM3X8E в байтах. */
uint32_t app_ram_total_bytes(void);

/** @return Размер linker-секции `.data` в SRAM. */
uint32_t app_ram_static_data_bytes(void);

/**
 * @return Размер linker-секции `.bss` в SRAM.
 * @note Включает статический массив FreeRTOS heap_4 и прикладные zero-init buffers.
 */
uint32_t app_ram_static_bss_bytes(void);

/** @return Размер startup stack, зарезервированный linker script. */
uint32_t app_ram_startup_stack_bytes(void);

/**
 * @return Размер C-library heap, зарезервированный linker script.
 * Он не равен FreeRTOS heap_4 и в baseline не используется для task allocation.
 */
uint32_t app_ram_c_heap_reserved_bytes(void);

/**
 * @return Хвост SRAM от linker symbol `_end` до физического конца памяти.
 * Это диагностическая оценка неразмеченного linker-пространства, а не heap_4.
 */
uint32_t app_ram_linker_unassigned_bytes(void);
