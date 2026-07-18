
/**
 * @file sam3x_watchdog.hpp
 * @brief HAL watchdog-таймера ATSAM3X8E.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Причина последнего сброса по RSTC_SR.RSTTYP.
 */
enum Sam3xResetType
{
    SAM3X_RESET_GENERAL = 0,
    SAM3X_RESET_BACKUP = 1,
    SAM3X_RESET_WATCHDOG = 2,
    SAM3X_RESET_SOFTWARE = 3,
    SAM3X_RESET_USER = 4,
    SAM3X_RESET_UNKNOWN = 255
};

/**
 * @brief Конфигурация watchdog.
 */
struct Sam3xWatchdogConfig
{
    uint16_t timeout_ticks_256hz;
    uint8_t reset_processor;
    uint8_t reset_all;
    uint8_t halt_in_debug;
    uint8_t halt_in_idle;
};

/**
 * @brief Выполняет раннее восстановление после watchdog-reset.
 *
 * Если последний reset был вызван watchdog, функция один раз выполняет
 * software reset процессора и периферии через RSTC. Это принудительно
 * переинициализирует USB-периферию после watchdog-события.
 */
void sam3x_watchdog_boot_recovery_from_watchdog_reset(void);

/**
 * @brief Выполняет software reset процессора и периферии через RSTC.
 */
void sam3x_watchdog_software_reset_processor_and_peripherals(void);

/**
 * @brief Считывает и сохраняет причину последнего сброса.
 */
void sam3x_watchdog_capture_boot_status(void);

/**
 * @brief Возвращает сохранённую причину последнего сброса.
 */
Sam3xResetType sam3x_watchdog_last_reset_type(void);

/**
 * @brief Возвращает текстовое описание причины последнего сброса.
 */
const char* sam3x_watchdog_reset_type_text(Sam3xResetType reset_type);

/**
 * @brief Возвращает признак выполненного recovery-reset после watchdog.
 */
uint8_t sam3x_watchdog_recovery_performed(void);

/**
 * @brief Возвращает счётчик запусков, сохранённый в GPBR0.
 */
uint32_t sam3x_watchdog_boot_count(void);

/**
 * @brief Увеличивает счётчик запусков в GPBR0.
 */
void sam3x_watchdog_increment_boot_count(void);

/**
 * @brief Включает watchdog.
 *
 * Регистр WDT_MR записывается один раз после аппаратного сброса. До вызова
 * этой функции код не должен записывать WDT_MR.
 */
void sam3x_watchdog_enable(const Sam3xWatchdogConfig* config);

/**
 * @brief Отключает watchdog до первой конфигурации WDT_MR.
 *
 * Функция оставлена для сервисных сборок без watchdog. В диагностической
 * baseline-прошивке она не вызывается.
 */
void sam3x_watchdog_disable_before_enable(void);

/**
 * @brief Перезапускает watchdog-счётчик.
 */
void sam3x_watchdog_restart(void);

/**
 * @brief Возвращает признак включённого watchdog.
 */
uint8_t sam3x_watchdog_enabled(void);

/**
 * @brief Возвращает регистр WDT_MR.
 */
uint32_t sam3x_watchdog_mode_register(void);

/**
 * @brief Возвращает регистр WDT_SR.
 */
uint32_t sam3x_watchdog_status_register(void);

/**
 * @brief Возвращает регистр RSTC_SR.
 */
uint32_t sam3x_watchdog_reset_status_register(void);

/**
 * @brief Возвращает время watchdog в миллисекундах.
 */
uint32_t sam3x_watchdog_timeout_ms(uint16_t timeout_ticks_256hz);
