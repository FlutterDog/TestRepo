/**
 * @file modbus_rtu_master.hpp
 * @brief Неблокирующий Modbus RTU master для одного half-duplex порта.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/** Максимальное количество регистров в одном запросе чтения. */
static const uint16_t MODBUS_RTU_MASTER_MAX_READ_REGISTERS = 100U;

/** Максимальное количество регистров в одном запросе записи. */
static const uint16_t MODBUS_RTU_MASTER_MAX_WRITE_REGISTERS = 16U;

/** Максимальный размер принятого RTU-кадра. */
static const uint16_t MODBUS_RTU_MASTER_RX_CAPACITY = 256U;

/** Максимальный размер передаваемого RTU-кадра. */
static const uint16_t MODBUS_RTU_MASTER_TX_CAPACITY = 64U;

/**
 * @brief Абстрактный байтовый транспорт Modbus RTU.
 *
 * Реализация транспорта отвечает за управление DE/RE, UART RX/TX и очистку
 * устаревших принятых данных перед новой транзакцией.
 */
struct ModbusRtuTransport
{
    size_t (*write)(const uint8_t* data, size_t length);
    size_t (*available)(void);
    int (*read)(void);
    uint8_t (*tx_idle)(void);
    void (*clear_rx)(void);
};

/** @brief Результат последней Modbus RTU транзакции. */
enum ModbusRtuResult : uint8_t
{
    MODBUS_RTU_RESULT_IDLE = 0U,
    MODBUS_RTU_RESULT_BUSY = 1U,
    MODBUS_RTU_RESULT_OK = 2U,
    MODBUS_RTU_RESULT_TIMEOUT = 3U,
    MODBUS_RTU_RESULT_CRC_ERROR = 4U,
    MODBUS_RTU_RESULT_INVALID_RESPONSE = 5U,
    MODBUS_RTU_RESULT_EXCEPTION = 6U,
    MODBUS_RTU_RESULT_TRANSPORT_ERROR = 7U,
    MODBUS_RTU_RESULT_INVALID_ARGUMENT = 8U
};

/** @brief Внутреннее состояние автомата Modbus RTU master. */
enum ModbusRtuMasterState : uint8_t
{
    MODBUS_RTU_MASTER_STATE_IDLE = 0U,
    MODBUS_RTU_MASTER_STATE_WAIT_TX = 1U,
    MODBUS_RTU_MASTER_STATE_WAIT_RESPONSE = 2U
};

/** @brief Тип активной транзакции. */
enum ModbusRtuTransactionType : uint8_t
{
    MODBUS_RTU_TRANSACTION_NONE = 0U,
    MODBUS_RTU_TRANSACTION_READ_HOLDING = 1U,
    MODBUS_RTU_TRANSACTION_WRITE_MULTIPLE = 2U
};

/**
 * @brief Runtime-состояние одного Modbus RTU master.
 *
 * Структура открыта для статического размещения, но её поля должны изменяться
 * только функциями этого модуля.
 */
struct ModbusRtuMaster
{
    const ModbusRtuTransport* transport;
    ModbusRtuMasterState state;
    ModbusRtuTransactionType transaction_type;
    ModbusRtuResult result;
    uint8_t slave_address;
    uint8_t function_code;
    uint8_t exception_code;
    uint16_t start_address;
    uint16_t register_count;
    uint16_t* read_output;
    uint32_t deadline_ms;
    uint32_t timeout_ms;
    uint16_t expected_response_length;
    uint16_t rx_length;
    uint16_t tx_length;
    uint8_t rx_buffer[MODBUS_RTU_MASTER_RX_CAPACITY];
    uint8_t tx_buffer[MODBUS_RTU_MASTER_TX_CAPACITY];
};

/** @brief Инициализирует объект Modbus RTU master. */
void modbus_rtu_master_init(ModbusRtuMaster& master,
                            const ModbusRtuTransport& transport);

/** @brief Полностью сбрасывает текущую и последнюю транзакцию. */
void modbus_rtu_master_reset(ModbusRtuMaster& master);

/**
 * @brief Запускает чтение holding-регистров функцией 0x03.
 *
 * @return 1, если запрос принят, иначе 0.
 */
uint8_t modbus_rtu_master_start_read_holding(ModbusRtuMaster& master,
                                             uint8_t slave_address,
                                             uint16_t start_address,
                                             uint16_t register_count,
                                             uint16_t* output,
                                             uint32_t timeout_ms);

/**
 * @brief Запускает запись нескольких holding-регистров функцией 0x10.
 *
 * @return 1, если запрос принят, иначе 0.
 */
uint8_t modbus_rtu_master_start_write_multiple(ModbusRtuMaster& master,
                                               uint8_t slave_address,
                                               uint16_t start_address,
                                               const uint16_t* values,
                                               uint16_t register_count,
                                               uint32_t timeout_ms);

/** @brief Выполняет один неблокирующий шаг автомата master. */
void modbus_rtu_master_poll(ModbusRtuMaster& master);

/** @brief Возвращает 1, пока транзакция не завершена. */
uint8_t modbus_rtu_master_busy(const ModbusRtuMaster& master);

/** @brief Возвращает результат последней транзакции. */
ModbusRtuResult modbus_rtu_master_result(const ModbusRtuMaster& master);

/** @brief Возвращает exception code последнего ответа Modbus. */
uint8_t modbus_rtu_master_exception_code(const ModbusRtuMaster& master);

/** @brief Возвращает текстовое описание результата. */
const char* modbus_rtu_result_text(ModbusRtuResult result);
