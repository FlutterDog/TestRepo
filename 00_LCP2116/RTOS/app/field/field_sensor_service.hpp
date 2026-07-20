/**
 * @file field_sensor_service.hpp
 * @brief Демонстрационный Modbus RTU master на физических портах S1..S4.
 *
 * Baseline намеренно жёстко задаёт один пример прибора и запускает его на
 * каждом порту. Три уровня конфигурации разделены:
 *
 * 1. параметры прибора (`slave_address`, FC03 start/count, period, timeout)
 *    задаются в `set_default_configs()` файла field_sensor_service.cpp;
 * 2. физические baud/parity S1..S4 загружаются в field_serial_config.cpp из
 *    `baud.TXT` и `Parity.TXT`;
 * 3. привязка S1..S4 к UART/SC16IS находится в board/lcp_field_ports.cpp.
 *
 * При добавлении другого прибора не следует изменять ModbusRtuMaster. Создайте
 * отдельный прикладной service или замените значения `set_default_configs()` и
 * обработку результата. Если прибор возвращает больше данных, увеличьте
 * FIELD_SENSOR_REGISTER_COUNT, runtime buffer и опубликованную Ethernet map.
 *
 * Один half-duplex-порт имеет ровно одного владельца. S1–S4 с момента setup
 * принадлежат этому service; диагностический echo на них не запускается.
 */

#pragma once

#include <stdint.h>

#include "field_serial_config.hpp"
#include "../../board/lcp_field_ports.hpp"
#include "../../protocol/modbus_rtu/modbus_rtu_master.hpp"

/**
 * Количество holding-регистров фиксированного baseline-прибора.
 *
 * При изменении значения одновременно проверьте:
 * - `set_default_configs()`;
 * - `FieldSensorPortState::registers`;
 * - карту `ethernet_modbus_service.cpp`;
 * - диагностику `field_status.cpp` и `ethernet_status.cpp`.
 */
static const uint8_t FIELD_SENSOR_REGISTER_COUNT = 2U;

/**
 * @brief Роль физического пользовательского порта.
 *
 * Один half-duplex-порт в каждый момент времени имеет только одну роль.
 * Baseline использует MASTER. SLAVE оставлен для отключённого reusable-примера
 * `field_sensor_slave_example.cpp` и не включается одновременно с master.
 */
enum FieldPortRole : uint8_t
{
    FIELD_PORT_DISABLED = 0U, /**< Порт не обслуживается FieldSensor. */
    FIELD_PORT_MASTER = 1U,   /**< Активный Modbus RTU master. */
    FIELD_PORT_SLAVE = 2U     /**< Альтернативный slave-режим, не baseline. */
};

/**
 * @brief Прикладные параметры одного экземпляра демонстрационного прибора.
 *
 * Структура копируется в runtime при initialize_runtime(). Изменение исходного
 * массива g_configs во время активной транзакции запрещено; reload сначала
 * ждёт, пока все четыре master станут idle.
 */
struct FieldSensorConfig
{
    FieldPortRole role;            /**< Владелец/роль физического порта. */
    uint8_t slave_address;         /**< Обычный Modbus RTU адрес 1..247. */
    uint16_t start_register;       /**< Первый holding register запроса FC03. */
    uint16_t register_count;       /**< Число регистров; не больше master limit и runtime buffer. */
    uint32_t poll_period_ms;       /**< Период между запусками запросов; 300 мс в baseline. */
    uint32_t timeout_ms;           /**< Срок физического TX и ответа; 500 мс в baseline. */
    LcpFieldPortConfig serial;     /**< Baud и frame физического S1..S4. */
};

/**
 * @brief Runtime одного физического FieldSensor-порта.
 *
 * Каждый S1..S4 имеет собственный ModbusRtuMaster, буфер, таймер и качество.
 * Последние корректные `registers` не обнуляются при ошибке. Использовать их
 * разрешено только при `valid != 0`; после пяти последовательных ошибок
 * устанавливаются `connection_lost=1` и `valid=0`.
 */
struct FieldSensorPortState
{
    FieldSensorConfig config;      /**< Применённая конфигурация прибора и UART. */
    ModbusRtuMaster master;        /**< Независимый protocol engine этого порта. */
    uint16_t registers[FIELD_SENSOR_REGISTER_COUNT]; /**< Последние корректные FC03 values. */
    uint8_t port_present;          /**< Физический transport обнаружен и доступен. */
    uint8_t request_active;        /**< Запрос запущен и ещё не завершён. */
    uint8_t valid;                 /**< 1, когда последние values имеют допустимое качество. */
    uint8_t connection_lost;       /**< 1 после порога последовательных ошибок. */
    uint8_t consecutive_failures; /**< Ошибки после последнего успешного ответа. */
    uint8_t last_exception_code;  /**< Последний Modbus exception code. */
    ModbusRtuResult last_result;  /**< Итог последней завершённой транзакции. */
    uint32_t successful_poll_count; /**< Накопительный счётчик успешных ответов. */
    uint32_t failed_poll_count;     /**< Накопительный счётчик ошибок/start failures. */
    uint32_t last_update_ms;        /**< millis() последнего корректного ответа. */
    uint32_t next_poll_ms;          /**< Абсолютный wrap-safe срок следующего запуска. */
};

/**
 * @brief Создаёт четыре baseline-конфигурации и инициализирует transport/master.
 *
 * Сначала применяются 9600 8N1 defaults. Чтение SD откладывается до готовности
 * FAT или до истечения начального ожидания.
 */
void field_sensor_service_init(void);

/**
 * @brief Выполняет по одному неблокирующему шагу каждого S1..S4.
 *
 * Функция также безопасно применяет ожидающий serial reload только после
 * завершения всех активных RTU-транзакций.
 */
void field_sensor_service_poll(void);

/** @brief Запрещает новые запросы; уже активные транзакции завершаются штатно. */
void field_sensor_service_pause(void);

/** @brief Возобновляет опрос и немедленно планирует idle-порты. */
void field_sensor_service_resume(void);

/** @return 1, если запуск новых запросов приостановлен, иначе 0. */
uint8_t field_sensor_service_paused(void);

/**
 * @brief Запрашивает безопасное повторное чтение baud.TXT и Parity.TXT.
 *
 * Каждый файл применяется независимо: ошибка baud сохраняет прежние скорости,
 * ошибка parity сохраняет прежние frame settings. Текущие values/counters
 * сбрасываются только при фактической повторной инициализации transport.
 */
void field_sensor_service_request_config_reload(void);

/** @return 1, пока ожидается завершение запросов или применение настроек. */
uint8_t field_sensor_service_config_reload_pending(void);

/** @return Ссылка на статический отчёт последней загрузки baud/parity. */
const FieldSerialConfigReport& field_sensor_service_serial_config_report(void);

/**
 * @brief Возвращает runtime выбранного физического порта.
 * @param[in] port_id Идентификатор S1..S4.
 * @return Ссылка со сроком жизни программы; некорректный ID нормализуется к S1.
 */
const FieldSensorPortState& field_sensor_service_port(LcpFieldPortId port_id);

/** @return Фиксированное количество физических портов, сейчас 4. */
uint8_t field_sensor_service_port_count(void);

/**
 * @brief Возвращает строку роли порта.
 * @param[in] role Роль из FieldPortRole.
 * @return Строковый литерал `disabled`, `master` или `slave`.
 */
const char* field_sensor_role_text(FieldPortRole role);
