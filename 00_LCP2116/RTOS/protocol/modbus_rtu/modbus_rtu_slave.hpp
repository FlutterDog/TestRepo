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
    MODBUS_RTU_SLAVE_RECEIVE = 0U, /**< Приём и ожидание межкадровой паузы. */
    MODBUS_RTU_SLAVE_WAIT_TX = 1U /**< Ответ передан transport, ожидается физический TX idle. */
};

/**
 * @brief Runtime минимального Modbus RTU slave.
 *
 * Baseline-реализация поддерживает только FC03 Read Holding Registers и
 * предназначена как ясный пример альтернативной роли одного порта.
 */
struct ModbusRtuSlave
{
    const ModbusRtuTransport* transport; /**< Байтовый transport физического порта. */
    uint8_t slave_address;               /**< Адрес 1..247. */
    uint16_t* holding_registers;         /**< Внешняя read-only карта FC03. */
    uint16_t holding_register_count;     /**< Размер карты в регистрах. */
    ModbusRtuSlaveState state;            /**< Текущее состояние автомата. */
    uint16_t rx_length;                   /**< Накопленная длина входного кадра. */
    uint16_t tx_length;                   /**< Длина сформированного ответа. */
    uint32_t last_rx_ms;                  /**< millis() последнего принятого байта. */
    uint32_t interframe_gap_ms;           /**< Пауза, после которой кадр считается завершённым. */
    uint32_t request_count;               /**< Кадры, адресованные данному slave. */
    uint32_t response_count;              /**< Успешно переданные ответы. */
    uint32_t error_count;                 /**< CRC, формат, адрес или transport errors. */
    uint8_t rx_buffer[MODBUS_RTU_SLAVE_RX_CAPACITY]; /**< Внутренний RX-кадр. */
    uint8_t tx_buffer[MODBUS_RTU_SLAVE_TX_CAPACITY]; /**< Внутренний TX-кадр. */
};

/**
 * @brief Инициализирует slave и привязывает карту holding-регистров.
 * @param[out] slave Экземпляр slave.
 * @param[in] transport Байтовый transport.
 * @param[in] slave_address Адрес 1..247.
 * @param[in,out] holding_registers Карта holding-регистров.
 * @param[in] holding_register_count Размер карты.
 * @param[in] interframe_gap_ms Пауза определения конца RTU-кадра.
 */
void modbus_rtu_slave_init(ModbusRtuSlave& slave,
                           const ModbusRtuTransport& transport,
                           uint8_t slave_address,
                           uint16_t* holding_registers,
                           uint16_t holding_register_count,
                           uint32_t interframe_gap_ms);

/**
 * @brief Сбрасывает незавершённый кадр, сохраняя конфигурацию slave.
 * @param[in,out] slave Экземпляр slave.
 */
void modbus_rtu_slave_reset(ModbusRtuSlave& slave);

/**
 * @brief Выполняет один неблокирующий шаг приёма или передачи.
 * @param[in,out] slave Экземпляр slave.
 */
void modbus_rtu_slave_poll(ModbusRtuSlave& slave);
