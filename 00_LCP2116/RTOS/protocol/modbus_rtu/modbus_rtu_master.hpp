/**
 * @file modbus_rtu_master.hpp
 * @brief Неблокирующий Modbus RTU master для одного half-duplex порта.
 *
 * Один объект обслуживает только одну транзакцию одновременно. Для нескольких
 * физических UART создаётся отдельный ModbusRtuMaster на каждый transport.
 * Объект не владеет transport и пользовательским FC03-буфером: оба должны жить
 * до завершения транзакции.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/** Максимальный обычный адрес Modbus RTU slave. Адрес 0 зарезервирован broadcast. */
static const uint8_t MODBUS_RTU_MAX_SLAVE_ADDRESS = 247U;

/** Максимальное количество регистров FC03 в одной транзакции baseline-master. */
static const uint16_t MODBUS_RTU_MASTER_MAX_READ_REGISTERS = 100U;

/** Максимальное количество регистров FC10 в одной транзакции baseline-master. */
static const uint16_t MODBUS_RTU_MASTER_MAX_WRITE_REGISTERS = 16U;

/** Максимальный размер принятого RTU-кадра. */
static const uint16_t MODBUS_RTU_MASTER_RX_CAPACITY = 256U;

/** Максимальный размер передаваемого RTU-кадра. */
static const uint16_t MODBUS_RTU_MASTER_TX_CAPACITY = 64U;

static_assert(MODBUS_RTU_MASTER_RX_CAPACITY >=
              (5U + (2U * MODBUS_RTU_MASTER_MAX_READ_REGISTERS)),
              "Modbus RTU RX buffer is too small for configured read limit");
static_assert(MODBUS_RTU_MASTER_TX_CAPACITY >=
              (9U + (2U * MODBUS_RTU_MASTER_MAX_WRITE_REGISTERS)),
              "Modbus RTU TX buffer is too small for configured write limit");

/**
 * @brief Абстрактный байтовый transport одного Modbus RTU-порта.
 *
 * Все callback обязаны относиться к одному физическому half-duplex UART.
 * `write` должен принять весь запрос за один вызов либо вернуть меньше длины;
 * частичный запрос считается transport error. `tx_idle` означает физическое
 * окончание передачи последнего stop-bit, а не только пустой software buffer.
 */
struct ModbusRtuTransport
{
    size_t (*write)(const uint8_t* data, size_t length); /**< Передаёт полный RTU-запрос. */
    size_t (*available)(void);                           /**< Возвращает число доступных RX-байтов. */
    int (*read)(void);                                  /**< Возвращает байт 0..255 либо отрицательное значение. */
    uint8_t (*tx_idle)(void);                           /**< Возвращает 1 после физического окончания TX. */
    void (*clear_rx)(void);                             /**< Очищает накопленный RX перед новой транзакцией. */
};

/** @brief Результат последней Modbus RTU-транзакции. */
enum ModbusRtuResult : uint8_t
{
    MODBUS_RTU_RESULT_IDLE = 0U,            /**< Транзакция отсутствует. */
    MODBUS_RTU_RESULT_BUSY = 1U,            /**< Выполняется передача или ожидание ответа. */
    MODBUS_RTU_RESULT_OK = 2U,              /**< Ответ полностью проверен. */
    MODBUS_RTU_RESULT_TIMEOUT = 3U,         /**< Ответ не получен за timeout. */
    MODBUS_RTU_RESULT_CRC_ERROR = 4U,       /**< CRC ответа не совпал. */
    MODBUS_RTU_RESULT_INVALID_RESPONSE = 5U,/**< Адрес, функция, длина или данные ответа неверны. */
    MODBUS_RTU_RESULT_EXCEPTION = 6U,       /**< Slave вернул Modbus exception. */
    MODBUS_RTU_RESULT_TRANSPORT_ERROR = 7U, /**< UART не принял запрос или не завершил TX. */
    MODBUS_RTU_RESULT_INVALID_ARGUMENT = 8U /**< Параметры запуска не прошли проверку. */
};

/** @brief Внутреннее состояние автомата Modbus RTU master. */
enum ModbusRtuMasterState : uint8_t
{
    MODBUS_RTU_MASTER_STATE_IDLE = 0U,          /**< Автомат свободен. */
    MODBUS_RTU_MASTER_STATE_WAIT_TX = 1U,       /**< Ожидается физическое окончание передачи. */
    MODBUS_RTU_MASTER_STATE_WAIT_RESPONSE = 2U  /**< Ожидаются байты ответа. */
};

/** @brief Тип активной транзакции. */
enum ModbusRtuTransactionType : uint8_t
{
    MODBUS_RTU_TRANSACTION_NONE = 0U,           /**< Транзакция не выбрана. */
    MODBUS_RTU_TRANSACTION_READ_HOLDING = 1U,   /**< FC03 Read Holding Registers. */
    MODBUS_RTU_TRANSACTION_WRITE_MULTIPLE = 2U  /**< FC10 Write Multiple Registers. */
};

/**
 * @brief Полное состояние одного независимого Modbus RTU master.
 *
 * Структура содержит state machine и собственные RX/TX-кадры, но не выделяет
 * динамическую память. `read_output` устанавливается только на время FC03 и
 * должен указывать на массив не меньше `register_count` элементов.
 */
struct ModbusRtuMaster
{
    const ModbusRtuTransport* transport; /**< Transport, живущий не меньше master. */
    ModbusRtuMasterState state;          /**< Текущее состояние автомата. */
    ModbusRtuTransactionType transaction_type; /**< Тип текущей транзакции. */
    ModbusRtuResult result;              /**< Текущий или итоговый результат. */
    uint8_t slave_address;               /**< Адрес текущего slave, 1..247. */
    uint8_t function_code;               /**< FC03 или FC10. */
    uint8_t exception_code;              /**< Последний exception code. */
    uint16_t start_address;              /**< Начальный адрес регистров. */
    uint16_t register_count;             /**< Количество регистров. */
    uint16_t* read_output;               /**< Внешний буфер результата FC03. */
    uint32_t deadline_ms;                /**< Абсолютный deadline текущей стадии. */
    uint32_t timeout_ms;                 /**< Timeout ответа после окончания TX. */
    uint32_t next_request_ms;            /**< Конец обязательной межкадровой паузы. */
    uint32_t interframe_gap_ms;           /**< Минимальная пауза между транзакциями. */
    uint16_t expected_response_length;   /**< Ожидаемая длина обычного ответа. */
    uint16_t rx_length;                  /**< Число принятых байтов. */
    uint16_t tx_length;                  /**< Длина сформированного запроса. */
    uint8_t rx_buffer[MODBUS_RTU_MASTER_RX_CAPACITY]; /**< Внутренний RX-кадр. */
    uint8_t tx_buffer[MODBUS_RTU_MASTER_TX_CAPACITY]; /**< Внутренний TX-кадр. */
};

/**
 * @brief Инициализирует объект Modbus RTU master.
 *
 * После инициализации новый запрос разрешён сразу. Значение
 * `interframe_gap_ms` применяется после каждой завершённой транзакции.
 *
 * @param[out] master Объект master.
 * @param[in] transport Байтовый transport со сроком жизни не меньше master.
 * @param[in] interframe_gap_ms Минимальная пауза между RTU-кадрами.
 */
void modbus_rtu_master_init(ModbusRtuMaster& master,
                            const ModbusRtuTransport& transport,
                            uint32_t interframe_gap_ms);

/**
 * @brief Сбрасывает текущую транзакцию, сохраняя transport и межкадровый срок.
 *
 * Последний пользовательский FC03-буфер после вызова больше не используется.
 *
 * @param[in,out] master Объект master.
 */
void modbus_rtu_master_reset(ModbusRtuMaster& master);

/**
 * @brief Проверяет состояние автомата и межкадровую паузу.
 * @param[in] master Объект master.
 * @return 1, когда разрешено начать новый запрос, иначе 0.
 */
uint8_t modbus_rtu_master_ready(const ModbusRtuMaster& master);

/**
 * @brief Запускает FC03 Read Holding Registers.
 *
 * Функция только формирует и передаёт запрос. Ответ принимается последующими
 * вызовами modbus_rtu_master_poll().
 *
 * @param[in,out] master Объект master.
 * @param[in] slave_address Обычный адрес 1..247; broadcast 0 не поддерживается.
 * @param[in] start_address Начальный holding register.
 * @param[in] register_count Количество 1..MODBUS_RTU_MASTER_MAX_READ_REGISTERS.
 * @param[out] output Буфер минимум `register_count` элементов, живущий до конца транзакции.
 * @param[in] timeout_ms Timeout физического TX и последующего ответа.
 * @return 1, если запрос принят и полностью передан transport, иначе 0.
 */
uint8_t modbus_rtu_master_start_read_holding(ModbusRtuMaster& master,
                                             uint8_t slave_address,
                                             uint16_t start_address,
                                             uint16_t register_count,
                                             uint16_t* output,
                                             uint32_t timeout_ms);

/**
 * @brief Запускает FC10 Write Multiple Registers.
 *
 * Значения копируются во внутренний TX-кадр до возврата, поэтому массив
 * `values` после вызова больше не используется.
 *
 * @param[in,out] master Объект master.
 * @param[in] slave_address Обычный адрес 1..247; broadcast 0 не поддерживается.
 * @param[in] start_address Начальный holding register.
 * @param[in] values Массив записываемых значений.
 * @param[in] register_count Количество 1..MODBUS_RTU_MASTER_MAX_WRITE_REGISTERS.
 * @param[in] timeout_ms Timeout физического TX и последующего ответа.
 * @return 1, если запрос принят и полностью передан transport, иначе 0.
 */
uint8_t modbus_rtu_master_start_write_multiple(ModbusRtuMaster& master,
                                               uint8_t slave_address,
                                               uint16_t start_address,
                                               const uint16_t* values,
                                               uint16_t register_count,
                                               uint32_t timeout_ms);

/**
 * @brief Выполняет один неблокирующий шаг автомата master.
 *
 * Функцию следует вызывать часто из service poll. Все внутренние циклы
 * ограничены доступным RX и размером статического буфера.
 */
void modbus_rtu_master_poll(ModbusRtuMaster& master);

/** @return 1, пока транзакция не завершена, иначе 0. */
uint8_t modbus_rtu_master_busy(const ModbusRtuMaster& master);

/** @return Текущий или последний результат транзакции. */
ModbusRtuResult modbus_rtu_master_result(const ModbusRtuMaster& master);

/** @return Exception code последнего ответа либо 0. */
uint8_t modbus_rtu_master_exception_code(const ModbusRtuMaster& master);

/** @return Указатель на строковый литерал с описанием результата. */
const char* modbus_rtu_result_text(ModbusRtuResult result);
