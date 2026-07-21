/**
 * @file w5500_lite.hpp
 * @brief Минимальный polling HAL W5500 для одного TCP server socket.
 *
 * Реализация не выделяет динамическую память и обслуживает socket 0 каждого
 * W5500. Протокольная обработка Modbus TCP находится уровнем выше.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../board/lcp_ethernet.hpp"

/** @brief IPv4-адрес в сетевом порядке октетов. */
struct W5500IpAddress
{
    uint8_t octet[4]; /**< Октеты от старшего к младшему. */
};

/** @brief 48-битный MAC-адрес. */
struct W5500MacAddress
{
    uint8_t octet[6]; /**< Шесть байтов MAC в порядке отображения. */
};

/** @brief Полный набор статических сетевых параметров одного W5500. */
struct W5500NetworkConfig
{
    W5500MacAddress mac;     /**< Source Hardware Address Register. */
    W5500IpAddress ip;       /**< Source IP Address Register. */
    W5500IpAddress gateway;  /**< Gateway Address Register. */
    W5500IpAddress subnet;   /**< Subnet Mask Register. */
};

/**
 * @brief Выполняет reset W5500 и применяет статические сетевые параметры.
 * @param[in] ethernet_id ETH1 или ETH2.
 * @param[in] config MAC, IP, gateway и subnet.
 * @return 1, если VERSIONR равен 0x04, иначе 0.
 */
uint8_t w5500_lite_begin(LcpEthernetId ethernet_id,
                         const W5500NetworkConfig& config);

/** @brief Возвращает VERSIONR выбранного W5500. */
uint8_t w5500_lite_version(LcpEthernetId ethernet_id);

/** @brief Возвращает регистр PHYCFGR выбранного W5500. */
uint8_t w5500_lite_phy_status(LcpEthernetId ethernet_id);

/** @return 1 при активном физическом Ethernet link, иначе 0. */
uint8_t w5500_lite_link_up(LcpEthernetId ethernet_id);

/**
 * @brief Открывает socket 0 в режиме TCP LISTEN.
 * @param[in] ethernet_id ETH1 или ETH2.
 * @param[in] port Локальный TCP-порт в host byte order.
 */
void w5500_lite_tcp_server_begin(LcpEthernetId ethernet_id, uint16_t port);

/**
 * @brief Поддерживает состояние socket 0 и принимает доступные TCP-байты.
 *
 * CLOSED и CLOSE_WAIT автоматически переводятся через OPEN в LISTEN.
 *
 * @param[in] ethernet_id ETH1 или ETH2.
 * @param[in] port Локальный TCP-порт.
 * @param[out] buffer Буфер приёма.
 * @param[in] capacity Размер буфера в байтах.
 * @return Количество принятых байтов.
 */
uint16_t w5500_lite_tcp_server_receive(LcpEthernetId ethernet_id,
                                       uint16_t port,
                                       uint8_t* buffer,
                                       uint16_t capacity);

/**
 * @brief Ставит в TX-буфер один целый TCP-фрагмент.
 *
 * Функция использует all-or-nothing контракт: при недостаточном свободном
 * месте не передаёт частичный Modbus TCP ADU и возвращает 0.
 *
 * @param[in] ethernet_id ETH1 или ETH2.
 * @param[in] buffer Передаваемые байты.
 * @param[in] length Требуемая длина.
 * @return `length` при успешной постановке в очередь, иначе 0.
 */
uint16_t w5500_lite_tcp_server_send(LcpEthernetId ethernet_id,
                                    const uint8_t* buffer,
                                    uint16_t length);

/** @brief Возвращает аппаратный SN_SR socket 0. */
uint8_t w5500_lite_tcp_server_status(LcpEthernetId ethernet_id);
