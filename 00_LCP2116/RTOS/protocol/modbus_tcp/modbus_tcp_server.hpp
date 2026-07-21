/**
 * @file modbus_tcp_server.hpp
 * @brief Минимальный неблокирующий Modbus TCP server для holding registers.
 *
 * Protocol engine не зависит от W5500: вызывающий слой добавляет байты TCP,
 * получает готовый ответ и самостоятельно выполняет физическую передачу.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/** Максимальный размер Modbus TCP ADU по спецификации. */
static const uint16_t MODBUS_TCP_ADU_CAPACITY = 260U;

/**
 * @brief Callback чтения holding-регистров прикладной карты.
 *
 * @param[in] context Пользовательский контекст, переданный process-функции.
 * @param[in] start_address Начальный адрес FC03.
 * @param[in] register_count Количество запрошенных регистров.
 * @param[out] output Буфер не менее `register_count` элементов.
 * @return 0 при успехе либо Modbus exception code.
 */
typedef uint8_t (*ModbusTcpReadHoldingHandler)(void* context,
                                                uint16_t start_address,
                                                uint16_t register_count,
                                                uint16_t* output);

/** @brief Потоковое состояние одного независимого TCP-соединения. */
struct ModbusTcpServer
{
    uint8_t rx_buffer[MODBUS_TCP_ADU_CAPACITY]; /**< Накопленные TCP-байты. */
    uint16_t rx_length;                         /**< Число байтов в rx_buffer. */
    uint8_t tx_buffer[MODBUS_TCP_ADU_CAPACITY]; /**< Последний сформированный ответ. */
    uint16_t tx_length;                         /**< Длина ответа; 0 означает отсутствие pending TX. */
    uint32_t request_count;                     /**< Корректно выделенные из потока запросы. */
    uint32_t response_count;                    /**< Сформированные обычные и exception-ответы. */
    uint32_t exception_count;                   /**< Сформированные Modbus exception-ответы. */
    uint32_t malformed_count;                   /**< Потерянные некорректные MBAP/переполненные потоки. */
};

/**
 * @brief Обнуляет protocol engine и все диагностические счётчики.
 * @param[out] server Экземпляр соединения.
 */
void modbus_tcp_server_init(ModbusTcpServer& server);

/**
 * @brief Добавляет принятые TCP-байты в потоковый буфер.
 * @param[in,out] server Экземпляр соединения.
 * @param[in] data Принятые байты.
 * @param[in] length Количество байтов.
 * @return 1 при успехе; 0 при переполнении, после которого RX-поток сброшен.
 */
uint8_t modbus_tcp_server_append(ModbusTcpServer& server,
                                 const uint8_t* data,
                                 uint16_t length);

/**
 * @brief Обрабатывает один полный ADU, если он накоплен в RX-потоке.
 *
 * Пока `tx_length` не очищен вызовом modbus_tcp_server_response_sent(), новый
 * запрос не обрабатывается. Это сохраняет сформированный ответ при временно
 * занятом аппаратном TX-буфере.
 *
 * @param[in,out] server Экземпляр соединения.
 * @param[in] read_handler Callback прикладной карты holding-регистров.
 * @param[in] context Пользовательский контекст callback.
 * @return 1, если сформирован ответ, иначе 0.
 */
uint8_t modbus_tcp_server_process(ModbusTcpServer& server,
                                  ModbusTcpReadHoldingHandler read_handler,
                                  void* context);

/** @return Адрес неизменяемого буфера готового ответа. */
const uint8_t* modbus_tcp_server_response(const ModbusTcpServer& server);

/** @return Длина готового ответа либо 0. */
uint16_t modbus_tcp_server_response_length(const ModbusTcpServer& server);

/**
 * @brief Подтверждает полную передачу ответа и разрешает следующий запрос.
 * @param[in,out] server Экземпляр соединения.
 */
void modbus_tcp_server_response_sent(ModbusTcpServer& server);
