
/**
 * @file lcp_ethernet.hpp
 * @brief Карта двух Ethernet-контроллеров W5500 контроллера LCP.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Идентификатор физического Ethernet-интерфейса.
 */
enum LcpEthernetId
{
    LCP_ETHERNET_1 = 0,
    LCP_ETHERNET_2 = 1,
    LCP_ETHERNET_COUNT = 2
};

/**
 * @brief Линия CS первого W5500.
 */
static const uint32_t LCP_ETH1_CS_PIN = 10U;

/**
 * @brief Линия CS второго W5500.
 */
static const uint32_t LCP_ETH2_CS_PIN = 48U;

/**
 * @brief Инициализирует GPIO двух W5500.
 */
void lcp_ethernet_init_pins(void);

/**
 * @brief Выполняет аппаратный reset выбранного W5500.
 *
 * @param ethernet_id Идентификатор Ethernet-интерфейса.
 */
void lcp_ethernet_reset(LcpEthernetId ethernet_id);

/**
 * @brief Активирует SPI CS выбранного W5500.
 *
 * @param ethernet_id Идентификатор Ethernet-интерфейса.
 */
void lcp_ethernet_select(LcpEthernetId ethernet_id);

/**
 * @brief Деактивирует SPI CS выбранного W5500.
 *
 * @param ethernet_id Идентификатор Ethernet-интерфейса.
 */
void lcp_ethernet_deselect(LcpEthernetId ethernet_id);

/**
 * @brief Деактивирует SPI CS обоих W5500.
 */
void lcp_ethernet_deselect_all(void);

/**
 * @brief Возвращает имя Ethernet-интерфейса.
 *
 * @param ethernet_id Идентификатор Ethernet-интерфейса.
 *
 * @return Строка имени.
 */
const char* lcp_ethernet_name(LcpEthernetId ethernet_id);
