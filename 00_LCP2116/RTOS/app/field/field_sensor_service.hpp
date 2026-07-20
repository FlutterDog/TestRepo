/**
 * @file field_sensor_service.hpp
 * @brief Демонстрационный опрос FieldSensor через пользовательские порты S1..S4.
 */

#pragma once

#include <stdint.h>

#include "../../board/lcp_field_ports.hpp"
#include "../../protocol/modbus_rtu/modbus_rtu_master.hpp"

/** @brief Роль физического пользовательского порта. */
enum FieldPortRole : uint8_t
{
    FIELD_PORT_DISABLED = 0U,
    FIELD_PORT_MASTER = 1U,
    FIELD_PORT_SLAVE = 2U
};

/** @brief Жёстко заданные параметры демонстрационного прибора. */
struct FieldSensorConfig
{
    FieldPortRole role;
    uint8_t slave_address;
    uint16_t start_register;
    uint16_t register_count;
    uint32_t poll_period_ms;
    uint32_t timeout_ms;
    LcpFieldPortConfig serial;
};

/** @brief Runtime-состояние одного экземпляра FieldSensor. */
struct FieldSensorPortState
{
    FieldSensorConfig config;
    ModbusRtuMaster master;
    uint16_t registers[2];
    uint8_t port_present;
    uint8_t request_active;
    uint8_t valid;
    uint8_t connection_lost;
    uint8_t consecutive_failures;
    uint8_t last_exception_code;
    ModbusRtuResult last_result;
    uint32_t successful_poll_count;
    uint32_t failed_poll_count;
    uint32_t last_update_ms;
    uint32_t next_poll_ms;
};

/** @brief Инициализирует четыре активных master-примера FieldSensor. */
void field_sensor_service_init(void);

/** @brief Выполняет один неблокирующий шаг всех четырёх портов. */
void field_sensor_service_poll(void);

/** @brief Запрещает запуск новых запросов; активные запросы завершаются штатно. */
void field_sensor_service_pause(void);

/** @brief Возобновляет периодический опрос. */
void field_sensor_service_resume(void);

/** @brief Возвращает 1, если запуск новых запросов приостановлен. */
uint8_t field_sensor_service_paused(void);

/** @brief Возвращает runtime-состояние выбранного порта. */
const FieldSensorPortState& field_sensor_service_port(LcpFieldPortId port_id);

/** @brief Возвращает количество сконфигурированных портов. */
uint8_t field_sensor_service_port_count(void);

/** @brief Возвращает текст роли порта. */
const char* field_sensor_role_text(FieldPortRole role);
