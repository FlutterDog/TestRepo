/**
 * @file ethernet_network_config.hpp
 * @brief Чтение канонической сетевой конфигурации двух W5500 из microSD.
 */

#pragma once

#include <stdint.h>

#include "../diagnostics/sd_config.hpp"
#include "../../hal/w5500_lite.hpp"

/** @brief Результаты независимого чтения файлов одного Ethernet-интерфейса. */
struct EthernetNetworkConfigReport
{
    SdConfigResult mac_result;     /**< Результат MAC.txt или MAC2.txt. */
    SdConfigResult ip_result;      /**< Результат IP.txt или IP2.txt. */
    SdConfigResult subnet_result;  /**< Результат SUBNET.txt или SUBNET2.txt. */
    SdConfigResult gateway_result; /**< Результат GATE.txt или GATE2.txt. */
    const char* mac_file;          /**< Каноническое имя MAC-файла. */
    const char* ip_file;           /**< Каноническое имя IP-файла. */
    const char* subnet_file;       /**< Каноническое имя subnet-файла. */
    const char* gateway_file;      /**< Каноническое имя gateway-файла. */
    uint8_t any_loaded_from_sd;    /**< Ненулевое значение, если применён хотя бы один файл. */
};

/**
 * @brief Заполняет безопасные значения по умолчанию для двух W5500.
 * @param[out] configs Массив конфигураций ETH1 и ETH2.
 */
void ethernet_network_config_set_defaults(
    W5500NetworkConfig configs[LCP_ETHERNET_COUNT]);

/**
 * @brief Инициализирует отчёты каноническими именами и единым состоянием.
 * @param[out] reports Отчёты ETH1 и ETH2.
 * @param[in] initial_result Начальное состояние каждого из восьми файлов.
 */
void ethernet_network_config_prepare_reports(
    EthernetNetworkConfigReport reports[LCP_ETHERNET_COUNT],
    SdConfigResult initial_result);

/**
 * @brief Загружает только канонические файлы установленного проекта.
 *
 * @code
 * ETH1: MAC.txt,  IP.txt,  SUBNET.txt,  GATE.txt
 * ETH2: MAC2.txt, IP2.txt, SUBNET2.txt, GATE2.txt
 * @endcode
 *
 * Каждый файл применяется независимо. Ошибка одного параметра сохраняет его
 * текущее значение, но не блокирует корректные файлы того же интерфейса.
 * Псевдонимы и альтернативные имена намеренно не поддерживаются.
 *
 * @param[in,out] configs Текущие настройки; корректные файлы заменяют поля.
 * @param[out] reports Отчёты ETH1 и ETH2.
 */
void ethernet_network_config_load(
    W5500NetworkConfig configs[LCP_ETHERNET_COUNT],
    EthernetNetworkConfigReport reports[LCP_ETHERNET_COUNT]);
