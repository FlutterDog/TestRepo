/**
 * @file w5500_lite.hpp
 * @brief Минимальный polling HAL W5500 для TCP server.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../board/lcp_ethernet.hpp"

/** @brief IPv4-адрес. */
struct W5500IpAddress
{
    uint8_t octet[4];
};

/** @brief MAC-адрес. */
struct W5500MacAddress
{
    uint8_t octet[6];
};

/** @brief Сетевые параметры W5500. */
struct W5500NetworkConfig
{
    W5500MacAddress mac;
    W5500IpAddress ip;
    W5500IpAddress gateway;
    W5500IpAddress subnet;
};

/** @brief Инициализирует W5500 и применяет сетевые параметры. */
uint8_t w5500_lite_begin(LcpEthernetId ethernet_id,
                         const W5500NetworkConfig& config);

/** @brief Возвращает VERSIONR выбранного W5500. */
uint8_t w5500_lite_version(LcpEthernetId ethernet_id);

/** @brief Возвращает регистр PHYCFGR выбранного W5500. */
uint8_t w5500_lite_phy_status(LcpEthernetId ethernet_id);

/** @brief Возвращает признак активного линка. */
uint8_t w5500_lite_link_up(LcpEthernetId ethernet_id);

/** @brief Запускает TCP-сервер на socket 0. */
void w5500_lite_tcp_server_begin(LcpEthernetId ethernet_id, uint16_t port);

/**
 * @brief Обслуживает состояние socket 0 и принимает доступные TCP-байты.
 *
 * Функция автоматически восстанавливает CLOSED/INIT/LISTEN состояния.
 */
uint16_t w5500_lite_tcp_server_receive(LcpEthernetId ethernet_id,
                                       uint16_t port,
                                       uint8_t* buffer,
                                       uint16_t capacity);

/** @brief Передаёт данные через установленное соединение socket 0. */
uint16_t w5500_lite_tcp_server_send(LcpEthernetId ethernet_id,
                                    const uint8_t* buffer,
                                    uint16_t length);

/** @brief Возвращает аппаратный статус socket 0. */
uint8_t w5500_lite_tcp_server_status(LcpEthernetId ethernet_id);

/** @brief Совместимый диагностический echo wrapper. */
void w5500_lite_tcp_echo_poll(LcpEthernetId ethernet_id, uint16_t port);
