/**
 * @file ethernet_modbus_service.hpp
 * @brief Read-only Modbus TCP server на двух W5500 контроллера LCP2116.
 *
 * Слой связывает три независимые части:
 *
 * 1. `ethernet_network_config.*` читает MAC/IP/subnet/gateway;
 * 2. `w5500_lite.*` передаёт и принимает цельные TCP chunks через socket 0;
 * 3. `modbus_tcp_server.*` разбирает поток и формирует protocol response.
 *
 * Прикладная holding map строится только в ethernet_modbus_service.cpp. Для
 * добавления новых регистров измените размер карты, `update_holding_map()` и
 * диагностическую расшифровку. W5500 HAL при этом менять не нужно.
 *
 * Если требуется новая Modbus-функция, сначала добавьте отдельный callback в
 * protocol/modbus_tcp, затем подключите его здесь. Не размещайте прикладные
 * FieldSensor/X2X-зависимости непосредственно в W5500 HAL.
 */

#pragma once

#include <stdint.h>

#include "ethernet_network_config.hpp"
#include "../../protocol/modbus_tcp/modbus_tcp_server.hpp"

/** Стандартный TCP-порт Modbus, одинаковый для ETH1 и ETH2. */
static const uint16_t LCP_MODBUS_TCP_PORT = 502U;

/**
 * Количество регистров на один физический FieldSensor-порт:
 * value 0, value 1 и quality.
 */
static const uint16_t LCP_MODBUS_TCP_REGISTERS_PER_FIELD_PORT = 3U;

/**
 * Полный размер read-only map: четыре порта × три регистра.
 *
 * `ethernet_modbus_service.cpp` содержит static_assert связи этого значения с
 * LCP_FIELD_PORT_COUNT. При изменении FieldSensor count/map сборка должна
 * остановиться, пока обе стороны не приведены в соответствие.
 */
static const uint16_t LCP_MODBUS_TCP_HOLDING_COUNT = 12U;

/**
 * @brief Младшие биты quality-регистра одного FieldSensor-порта.
 *
 * Старший байт содержит числовой ModbusRtuResult последней завершённой или
 * текущей транзакции. Последние значения сохраняются при ошибке, но потребитель
 * обязан проверять VALID перед их использованием.
 */
enum FieldSensorTcpQuality : uint16_t
{
    FIELD_SENSOR_TCP_QUALITY_VALID = 0x0001U, /**< Последние values допустимы. */
    FIELD_SENSOR_TCP_QUALITY_CONNECTION_LOST = 0x0002U, /**< Достигнут порог ошибок. */
    FIELD_SENSOR_TCP_QUALITY_PORT_PRESENT = 0x0004U, /**< Физический UART существует. */
    FIELD_SENSOR_TCP_QUALITY_REQUEST_ACTIVE = 0x0008U, /**< RTU master сейчас занят. */
    FIELD_SENSOR_TCP_QUALITY_SERVICE_PAUSED = 0x0010U /**< Новые RTU-запросы запрещены. */
};

/**
 * @brief Runtime одного независимого Ethernet-интерфейса.
 *
 * ETH1 и ETH2 имеют отдельные W5500, network config, socket state, потоковый
 * parser и счётчики, но публикуют одинаковую прикладную holding map.
 */
struct EthernetModbusInterfaceState
{
    W5500NetworkConfig config; /**< Фактически применённые MAC/IP/subnet/gateway. */
    EthernetNetworkConfigReport config_report; /**< Результаты четырёх файлов этого ETH. */
    ModbusTcpServer server; /**< Независимый stream/protocol engine соединения. */
    uint8_t initialized; /**< Выполнена хотя бы одна аппаратная попытка begin. */
    uint8_t init_ok; /**< VERSIONR W5500 равен ожидаемому 0x04. */
    uint8_t last_socket_status; /**< Последнее считанное аппаратное SN_SR socket 0. */
    uint32_t transport_error_count; /**< RX append overflow или неполная постановка ответа. */
};

/**
 * @brief Сбрасывает runtime и ставит отложенное применение конфигурации.
 *
 * Сервис ждёт готовности microSD ограниченное время, затем запускает оба W5500
 * с безопасными defaults. Функция сама не выполняет длительный SD wait.
 */
void ethernet_modbus_service_init(void);

/**
 * @brief Выполняет один неблокирующий шаг обоих W5500 и Modbus TCP engines.
 *
 * За один вызов каждый интерфейс принимает ограниченный chunk и формирует не
 * более заданного числа ответов. Готовый ADU хранится до полной постановки в TX.
 */
void ethernet_modbus_service_poll(void);

/**
 * @brief Запрашивает перечитывание сетевых файлов и reset обоих W5500.
 *
 * Операция выполняется следующим poll-вызовом; активные TCP-соединения будут
 * закрыты аппаратным reset. Используйте только как явную сервисную команду.
 */
void ethernet_modbus_service_request_reload(void);

/** @return 1, пока ожидается применение сетевой конфигурации, иначе 0. */
uint8_t ethernet_modbus_service_reload_pending(void);

/**
 * @brief Возвращает runtime выбранного Ethernet-интерфейса.
 * @param[in] ethernet_id ETH1 или ETH2.
 * @return Ссылка со сроком жизни программы; некорректный ID нормализуется к ETH1.
 */
const EthernetModbusInterfaceState& ethernet_modbus_service_interface(
    LcpEthernetId ethernet_id);

/**
 * @brief Возвращает один регистр текущей опубликованной карты.
 * @param[in] address Адрес 0..11.
 * @return Значение регистра либо 0 для адреса вне карты.
 */
uint16_t ethernet_modbus_service_holding(uint16_t address);
