
/**
 * @file lcp_battery.hpp
 * @brief Карта входа контроля резервной батареи контроллера LCP.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Цифровой вход результата компаратора резервной батареи.
 */
static const uint32_t LCP_BACKUP_BATTERY_STATUS_PIN = 50U;

/**
 * @brief Порог компаратора резервной батареи, мВ.
 */
static const uint16_t LCP_BACKUP_BATTERY_THRESHOLD_MV = 2200U;

/**
 * @brief Активный уровень статуса исправной батареи.
 *
 * При напряжении CR2032 выше порога 2.2 V компаратор формирует высокий уровень.
 */
static const uint8_t LCP_BACKUP_BATTERY_OK_ACTIVE_HIGH = 1U;

/**
 * @brief Инициализирует цифровой вход контроля резервной батареи.
 */
void lcp_battery_init_pins(void);

/**
 * @brief Возвращает сырой уровень входа контроля резервной батареи.
 *
 * @return 0 или 1.
 */
uint8_t lcp_battery_raw(void);

/**
 * @brief Возвращает признак исправной резервной батареи.
 *
 * @return 1 если напряжение CR2032 выше порога компаратора.
 */
uint8_t lcp_battery_ok(void);
