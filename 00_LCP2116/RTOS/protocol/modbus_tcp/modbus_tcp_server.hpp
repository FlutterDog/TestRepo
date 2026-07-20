/**
 * @file modbus_tcp_server.hpp
 * @brief Минимальный неблокирующий Modbus TCP server для holding registers.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

static const uint16_t MODBUS_TCP_ADU_CAPACITY = 260U;

typedef uint8_t (*ModbusTcpReadHoldingHandler)(void* context,
                                                uint16_t start_address,
                                                uint16_t register_count,
                                                uint16_t* output);

/** @brief Состояние одного Modbus TCP соединения. */
struct ModbusTcpServer
{
    uint8_t rx_buffer[MODBUS_TCP_ADU_CAPACITY];
    uint16_t rx_length;
    uint8_t tx_buffer[MODBUS_TCP_ADU_CAPACITY];
    uint16_t tx_length;
    uint32_t request_count;
    uint32_t response_count;
    uint32_t exception_count;
    uint32_t malformed_count;
};

/** @brief Инициализирует чистое состояние protocol engine. */
void modbus_tcp_server_init(ModbusTcpServer& server);

/** @brief Добавляет принятые TCP-байты в потоковый буфер. */
uint8_t modbus_tcp_server_append(ModbusTcpServer& server,
                                 const uint8_t* data,
                                 uint16_t length);

/**
 * @brief Обрабатывает один полный ADU, если он уже накоплен.
 *
 * @return 1 если сформирован ответ, иначе 0.
 */
uint8_t modbus_tcp_server_process(ModbusTcpServer& server,
                                  ModbusTcpReadHoldingHandler read_handler,
                                  void* context);

/** @brief Возвращает адрес готового ответа. */
const uint8_t* modbus_tcp_server_response(const ModbusTcpServer& server);

/** @brief Возвращает длину готового ответа. */
uint16_t modbus_tcp_server_response_length(const ModbusTcpServer& server);

/** @brief Подтверждает передачу ответа и очищает tx_length. */
void modbus_tcp_server_response_sent(ModbusTcpServer& server);
