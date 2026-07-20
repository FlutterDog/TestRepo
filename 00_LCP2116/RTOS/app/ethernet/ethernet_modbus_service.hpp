/**
 * @file ethernet_modbus_service.hpp
 * @brief Modbus TCP server на двух Ethernet-портах LCP2116.
 */

#pragma once

#include <stdint.h>

#include "ethernet_network_config.hpp"
#include "../../protocol/modbus_tcp/modbus_tcp_server.hpp"

static const uint16_t LCP_MODBUS_TCP_PORT = 502U;
static const uint16_t LCP_MODBUS_TCP_HOLDING_COUNT = 12U;

/** @brief Runtime-состояние одного Ethernet-интерфейса. */
struct EthernetModbusInterfaceState
{
    W5500NetworkConfig config;
    EthernetNetworkConfigReport config_report;
    ModbusTcpServer server;
    uint8_t initialized;
    uint8_t init_ok;
    uint8_t last_socket_status;
    uint32_t transport_error_count;
};

/** @brief Инициализирует отложенный запуск двух Modbus TCP server. */
void ethernet_modbus_service_init(void);

/** @brief Обслуживает загрузку конфигурации, W5500 и Modbus TCP. */
void ethernet_modbus_service_poll(void);

/** @brief Запрашивает повторное чтение IP/subnet/gate и reset W5500. */
void ethernet_modbus_service_request_reload(void);

/** @brief Возвращает 1, если ожидается применение сетевой конфигурации. */
uint8_t ethernet_modbus_service_reload_pending(void);

/** @brief Возвращает состояние выбранного интерфейса. */
const EthernetModbusInterfaceState& ethernet_modbus_service_interface(
    LcpEthernetId ethernet_id);

/** @brief Возвращает holding register демонстрационной карты. */
uint16_t ethernet_modbus_service_holding(uint16_t address);
