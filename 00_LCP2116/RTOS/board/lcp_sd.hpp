
/**
 * @file lcp_sd.hpp
 * @brief Карта GPIO интерфейса microSD контроллера LCP.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Линия CS microSD.
 */
static const uint32_t LCP_SD_CS_PIN = 23U;

/**
 * @brief Линия аппаратного детектора карты.
 */
static const uint32_t LCP_SD_DETECT_PIN = 25U;

/**
 * @brief Линия индикации готовности microSD.
 */
static const uint32_t LCP_SD_OK_PIN = 41U;

/**
 * @brief Активный уровень детектора установленной карты.
 *
 * На плате LCP установленная карта формирует высокий уровень на SD_Detect.
 */
static const uint8_t LCP_SD_DETECT_ACTIVE_LOW = 0U;

/**
 * @brief Инициализирует GPIO microSD.
 */
void lcp_sd_init_pins(void);

/**
 * @brief Возвращает сырой уровень SD_Detect.
 */
uint8_t lcp_sd_detect_raw(void);

/**
 * @brief Возвращает признак установленной карты по аппаратному детектору.
 */
uint8_t lcp_sd_card_inserted(void);

/**
 * @brief Управляет индикатором готовности microSD.
 */
void lcp_sd_set_ok_led(uint8_t enabled);

/**
 * @brief Активирует CS microSD.
 */
void lcp_sd_select(void);

/**
 * @brief Деактивирует CS microSD.
 */
void lcp_sd_deselect(void);
