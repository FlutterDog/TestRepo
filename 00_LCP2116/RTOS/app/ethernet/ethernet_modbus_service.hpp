/**
 * @file ethernet_modbus_service.hpp
 * @brief Read-only Modbus TCP server на двух Ethernet-портах LCP2116.
 */

#pragma once

#include <stdint.h>

#include "ethernet_network_config.hpp"
#include "../../protocol/modbus_tcp/modbus_tcp_server.hpp"

/** Стандартный TCP-порт Modbus. */
static const uint16_t LCP_MODBUS_TCP_PORT = 502U;

/** Количество публикуемых регистров на один физический FieldSensor-порт. */
static const uint16_t LCP_MODBUS_TCP_REGISTERS_PER_FIELD_PORT = 3U;

/** Полный размер read-only holding map: четыре порта по три регистра. */
static const uint16_t LCP_MODBUS_TCP_HOLDING_COUNT = 12U;

/**
 * @brief Биты регистра качества каждого FieldSensor-порта.
 *
 * Старший байт дополнительно содержит ModbusRtuResult последней транзакции.
 */
enum FieldSensorTcpQuality : uint16_t
{
    FIELD_SENSOR_TCP_QUALITY_VALID = 0x0001U,       /**< Последние значения валидны. */
    FIELD_SENSOR_TCP_QUALITY_CONNECTION_LOST = 0x0002U, /**< Зафиксирована потеря связи. */
    FIELD_SENSOR_TCP_QUALITY_PORT_PRESENT = 0x0004U,    /**< Физический UART доступен. */
    FIELD_SENSOR_TCP_QUALITY_REQUEST_ACTIVE = 0x0008U,  /**< RTU-транзакция выполняется. */
    FIELD_SENSOR_TCP_QUALITY_SERVICE_PAUSED = 0x0010U   /**< Новые RTU-запросы остановлены. */
};

/** @brief Runtime-состояние одного Ethernet-интерфейса. */
struct EthernetModbusInterfaceState
{
    W5500NetworkConfig config;                 /**< Фактически применённые сетевые параметры. */
    EthernetNetworkConfigReport config_report; /**< Результаты чтения четырёх канонических файлов. */
    ModbusTcpServer server;                    /**< Независимый потоковый protocol engine. */
    uint8_t initialized;                       /**< Сервис выполнил попытку аппаратного запуска. */
    uint8_t init_ok;                           /**< VERSIONR W5500 подтверждён. */
    uint8_t last_socket_status;                /**< Последнее значение аппаратного SN_SR. */
    uint32_t transport_error_count;            /**< Ошибки приёма или переполнения потокового буфера. */
};

/** @brief Инициализирует отложенный запуск двух Modbus TCP server. */
void ethernet_modbus_service_init(void);

/** @brief Выполняет один неблокирующий шаг конфигурации, W5500 и Modbus TCP. */
void ethernet_modbus_service_poll(void);

/**
 * @brief Запрашивает повторное чтение MAC/IP/subnet/gateway и reset обоих W5500.
 */
void ethernet_modbus_service_request_reload(void);

/** @return 1, если ожидается применение сетевой конфигурации, иначе 0. */
uint8_t ethernet_modbus_service_reload_pending(void);

/**
 * @brief Возвращает состояние выбранного Ethernet-интерфейса.
 * @param[in] ethernet_id ETH1 или ETH2.
 * @return Неизменяемая ссылка; некорректный ID нормализуется к ETH1.
 */
const EthernetModbusInterfaceState& ethernet_modbus_service_interface(
    LcpEthernetId ethernet_id);

/**
 * @brief Возвращает holding register демонстрационной карты.
 * @param[in] address Адрес 0..11.
 * @return Значение регистра либо 0 для адреса вне карты.
 */
uint16_t ethernet_modbus_service_holding(uint16_t address);
