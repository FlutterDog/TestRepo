/**
 * @file field_sensor_service.hpp
 * @brief Демонстрационный опрос FieldSensor через пользовательские порты S1..S4.
 */

#pragma once

#include <stdint.h>

#include "field_serial_config.hpp"
#include "../../board/lcp_field_ports.hpp"
#include "../../protocol/modbus_rtu/modbus_rtu_master.hpp"

/** Количество holding-регистров в фиксированном baseline-примере FieldSensor. */
static const uint8_t FIELD_SENSOR_REGISTER_COUNT = 2U;

/**
 * @brief Роль физического пользовательского порта.
 *
 * Один half-duplex-порт в каждый момент времени имеет только одну роль.
 */
enum FieldPortRole : uint8_t
{
    FIELD_PORT_DISABLED = 0U, /**< Порт не обслуживается FieldSensor. */
    FIELD_PORT_MASTER = 1U,   /**< Активный Modbus RTU master. */
    FIELD_PORT_SLAVE = 2U     /**< Альтернативный slave-режим для примера. */
};

/** @brief Неизменяемые параметры одного экземпляра демонстрационного прибора. */
struct FieldSensorConfig
{
    FieldPortRole role;            /**< Роль физического порта. */
    uint8_t slave_address;         /**< Modbus RTU slave address, 1..247. */
    uint16_t start_register;       /**< Начальный holding register FC03. */
    uint16_t register_count;       /**< Количество регистров; baseline использует 2. */
    uint32_t poll_period_ms;       /**< Период запуска запросов. */
    uint32_t timeout_ms;           /**< Тайм-аут одной RTU-транзакции. */
    LcpFieldPortConfig serial;     /**< Физические параметры UART. */
};

/** @brief Runtime-состояние одного экземпляра FieldSensor. */
struct FieldSensorPortState
{
    FieldSensorConfig config;      /**< Применённая конфигурация прибора и порта. */
    ModbusRtuMaster master;        /**< Независимый protocol engine данного порта. */
    uint16_t registers[FIELD_SENSOR_REGISTER_COUNT]; /**< Последние корректные значения. */
    uint8_t port_present;          /**< Физический transport обнаружен и доступен. */
    uint8_t request_active;        /**< Запрос запущен и ещё не завершён. */
    uint8_t valid;                 /**< Значения разрешено использовать. */
    uint8_t connection_lost;      /**< Зафиксирован порог последовательных ошибок. */
    uint8_t consecutive_failures; /**< Число ошибок после последнего успеха. */
    uint8_t last_exception_code;  /**< Последний Modbus exception code. */
    ModbusRtuResult last_result;  /**< Итог последней завершённой транзакции. */
    uint32_t successful_poll_count; /**< Накопительный счётчик успешных ответов. */
    uint32_t failed_poll_count;     /**< Накопительный счётчик ошибок. */
    uint32_t last_update_ms;        /**< millis() последнего корректного ответа. */
    uint32_t next_poll_ms;          /**< Абсолютный срок следующего запуска. */
};

/** @brief Инициализирует четыре активных master-примера FieldSensor. */
void field_sensor_service_init(void);

/** @brief Выполняет один неблокирующий шаг всех четырёх портов. */
void field_sensor_service_poll(void);

/** @brief Запрещает запуск новых запросов; активные запросы завершаются штатно. */
void field_sensor_service_pause(void);

/** @brief Возобновляет периодический опрос. */
void field_sensor_service_resume(void);

/** @return 1, если запуск новых запросов приостановлен, иначе 0. */
uint8_t field_sensor_service_paused(void);

/**
 * @brief Запрашивает безопасное повторное чтение baud.TXT и Parity.TXT.
 *
 * Применение откладывается до завершения всех активных RTU-транзакций.
 */
void field_sensor_service_request_config_reload(void);

/** @return 1, пока ожидается повторное применение настроек, иначе 0. */
uint8_t field_sensor_service_config_reload_pending(void);

/** @return Неизменяемая ссылка на отчёт последней загрузки serial-конфигурации. */
const FieldSerialConfigReport& field_sensor_service_serial_config_report(void);

/**
 * @brief Возвращает runtime-состояние выбранного физического порта.
 * @param[in] port_id Идентификатор S1..S4.
 * @return Неизменяемая ссылка; некорректный ID нормализуется к S1.
 */
const FieldSensorPortState& field_sensor_service_port(LcpFieldPortId port_id);

/** @return Количество физических FieldSensor-портов. */
uint8_t field_sensor_service_port_count(void);

/**
 * @brief Возвращает текст роли порта.
 * @param[in] role Роль из FieldPortRole.
 * @return Указатель на строковый литерал.
 */
const char* field_sensor_role_text(FieldPortRole role);
