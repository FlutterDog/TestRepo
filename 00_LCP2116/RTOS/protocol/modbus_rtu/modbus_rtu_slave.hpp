/**
 * @file modbus_rtu_slave.hpp
 * @brief Неблокирующий минимальный Modbus RTU slave для примеров Lorentz.
 */

#pragma once

#include <stdint.h>

#include "modbus_rtu_master.hpp"

/** Максимальный размер входного RTU-кадра минимального slave. */
static const uint16_t MODBUS_RTU_SLAVE_RX_CAPACITY = 64U;

/** Максимальный размер выходного RTU-кадра минимального slave. */
static const uint16_t MODBUS_RTU_SLAVE_TX_CAPACITY = 64U;

/** @brief Внутреннее состояние неблокирующего slave. */
enum ModbusRtuSlaveState : uint8_t
{
    MODBUS_RTU_SLAVE_RECEIVE = 0U,
    MODBUS_RTU_SLAVE_WAIT_TX = 1U
};

/**
 * @brief Runtime минимального Modbus RTU slave.
 *
 * Baseline-реализация поддерживает функцию 0x03 Read Holding Registers.
 * Она предназначена как ясный пример альтернативной роли одного порта.
 */
struct ModbusRtuSlave
{
    const ModbusRtuTransport* transport;
    uint8_t slave_address;
    uint16_t* holding_registers;
    uint16_t holding_register_count;
    ModbusRtuSlaveState state;
    uint16_t rx_length;
    uint16_t tx_length;
    uint32_t last_rx_ms;
    uint32_t interframe_gap_ms;
    uint32_t request_count;
    uint32_t response_count;
    uint32_t error_count;
    uint8_t rx_buffer[MODBUS_RTU_SLAVE_RX_CAPACITY];
    uint8_t tx_buffer[MODBUS_RTU_SLAVE_TX_CAPACITY];
};

/** @brief Инициализирует slave и привязывает карту holding-регистров. */
void modbus_rtu_slave_init(ModbusRtuSlave& slave,
                           const ModbusRtuTransport& transport,
                           uint8_t slave_address,
                           uint16_t* holding_registers,
                           uint16_t holding_register_count,
                           uint32_t interframe_gap_ms);

/** @brief Сбрасывает незавершённый кадр, сохраняя конфигурацию slave. */
void modbus_rtu_slave_reset(ModbusRtuSlave& slave);

/** @brief Выполняет один неблокирующий шаг приёма или передачи. */
void modbus_rtu_slave_poll(ModbusRtuSlave& slave);
