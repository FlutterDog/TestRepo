/**
 * @file ethernet_network_config.hpp
 * @brief Чтение MAC, IP, subnet и gateway обоих W5500 из microSD.
 */

#pragma once

#include <stdint.h>

#include "../../hal/w5500_lite.hpp"
#include "../diagnostics/sd_config.hpp"

/** @brief Результаты чтения четырёх файлов одного Ethernet-интерфейса. */
struct EthernetNetworkConfigReport
{
    SdConfigResult mac_result;
    SdConfigResult ip_result;
    SdConfigResult subnet_result;
    SdConfigResult gateway_result;
    const char* mac_file;
    const char* ip_file;
    const char* subnet_file;
    const char* gateway_file;
    uint8_t any_loaded_from_sd;
};

/** @brief Заполняет безопасные значения по умолчанию для двух W5500. */
void ethernet_network_config_set_defaults(
    W5500NetworkConfig configs[LCP_ETHERNET_COUNT]);

/**
 * @brief Загружает только канонические файлы старого формата.
 *
 * ETH1: MAC.txt, IP.txt, SUBNET.txt, GATE.txt.
 * ETH2: MAC2.txt, IP2.txt, SUBNET2.txt, GATE2.txt.
 *
 * Псевдонимы и альтернативные имена намеренно не поддерживаются.
 */
void ethernet_network_config_load(
    W5500NetworkConfig configs[LCP_ETHERNET_COUNT],
    EthernetNetworkConfigReport reports[LCP_ETHERNET_COUNT]);
