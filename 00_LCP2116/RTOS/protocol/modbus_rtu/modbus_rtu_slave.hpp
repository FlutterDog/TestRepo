/**
 * @file modbus_rtu_slave.hpp
 * @brief Неблокирующий минимальный Modbus RTU slave для примеров Lorentz.
 *
 * Реализация предназначена как ясный reusable-пример альтернативной роли
 * одного S1..S4. Один физический half-duplex UART нельзя одновременно передать
 * этому slave и ModbusRtuMaster. Baseline поддерживает только FC03.
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
    MODBUS_RTU_SLAVE_RECEIVE = 0U, /**< Приём запроса и ожидание межкадровой паузы. */
    MODBUS_RTU_SLAVE_WAIT_TX = 1U /**< Ответ принят transport и физически передаётся. */
};

/**
 * @brief Runtime минимального Modbus RTU slave.
 *
 * `transport` и `holding_registers` не принадлежат объекту и должны жить всё
 * время его использования. Карта регистров читается непосредственно при
 * формировании ответа; отдельная копия не создаётся.
 */
struct ModbusRtuSlave
{
    const ModbusRtuTransport* transport; /**< Единственный физический UART transport. */
    uint8_t slave_address;              /**< Обычный адрес 1..247. */
    uint16_t* holding_registers;        /**< Внешняя read-only для протокола карта. */
    uint16_t holding_register_count;    /**< Размер карты в 16-битных регистрах. */
    ModbusRtuSlaveState state;           /**< Текущее состояние автомата. */
    uint16_t rx_length;                 /**< Накопленная длина запроса. */
    uint16_t tx_length;                 /**< Длина сформированного ответа. */
    uint32_t last_rx_ms;                /**< Время последнего принятого байта. */
    uint32_t interframe_gap_ms;          /**< Пауза завершения RTU-кадра. */
    uint32_t request_count;              /**< Полные кадры, переданные parser. */
    uint32_t response_count;             /**< Ответы, принятые transport. */
    uint32_t error_count;                /**< CRC, формат, адрес карты или TX error. */
    uint8_t rx_buffer[MODBUS_RTU_SLAVE_RX_CAPACITY]; /**< Внутренний запрос. */
    uint8_t tx_buffer[MODBUS_RTU_SLAVE_TX_CAPACITY]; /**< Внутренний ответ. */
};

/**
 * @brief Инициализирует slave и привязывает карту holding-регистров.
 *
 * @param[out] slave Объект slave.
 * @param[in] transport Transport со сроком жизни не меньше slave.
 * @param[in] slave_address Адрес 1..247; broadcast 0 не поддерживается.
 * @param[in,out] holding_registers Карта, читаемая при FC03.
 * @param[in] holding_register_count Число доступных регистров.
 * @param[in] interframe_gap_ms Пауза, определяющая завершение неполного кадра.
 */
void modbus_rtu_slave_init(ModbusRtuSlave& slave,
                           const ModbusRtuTransport& transport,
                           uint8_t slave_address,
                           uint16_t* holding_registers,
                           uint16_t holding_register_count,
                           uint32_t interframe_gap_ms);

/**
 * @brief Сбрасывает незавершённый кадр, сохраняя конфигурацию и счётчики.
 */
void modbus_rtu_slave_reset(ModbusRtuSlave& slave);

/**
 * @brief Выполняет один неблокирующий шаг приёма или передачи.
 *
 * Цикл RX ограничен доступными байтами и статической ёмкостью. Функция не ждёт
 * новый байт и должна вызываться регулярно из прикладного service poll.
 */
void modbus_rtu_slave_poll(ModbusRtuSlave& slave);
