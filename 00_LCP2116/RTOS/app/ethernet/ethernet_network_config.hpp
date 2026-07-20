/**
 * @file ethernet_network_config.hpp
 * @brief Чтение IP, subnet и gateway обоих W5500 из microSD.
 */

#pragma once

#include <stdint.h>

#include "../../hal/w5500_lite.hpp"
#include "../diagnostics/sd_config.hpp"

/** @brief Результаты чтения трёх файлов одного Ethernet-интерфейса. */
struct EthernetNetworkConfigReport
{
    SdConfigResult ip_result;
    SdConfigResult subnet_result;
    SdConfigResult gateway_result;
    const char* ip_file;
    const char* subnet_file;
    const char* gateway_file;
    uint8_t any_loaded_from_sd;
};

/** @brief Заполняет безопасные значения по умолчанию для двух W5500. */
void ethernet_network_config_set_defaults(
    W5500NetworkConfig configs[LCP_ETHERNET_COUNT]);

/**
 * @brief Загружает настройки из файлов старого формата.
 *
 * ETH1 aliases: IP.TXT/IP1.TXT, SUBNET.TXT/SUBNET1.TXT,
 * GATE.TXT/GATE1.TXT/GW.TXT/GW1.TXT.
 * ETH2: IP2.TXT, SUBNET2.TXT, GATE2.TXT/GW2.TXT.
 */
void ethernet_network_config_load(
    W5500NetworkConfig configs[LCP_ETHERNET_COUNT],
    EthernetNetworkConfigReport reports[LCP_ETHERNET_COUNT]);
