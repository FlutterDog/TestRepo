/**
 * @file field_sensor_slave_example.cpp
 * @brief Отключённый пример альтернативного Modbus RTU slave-режима.
 *
 * Рабочая baseline-конфигурация использует S1..S4 как четыре независимых
 * Modbus RTU master. Один физический half-duplex порт нельзя одновременно
 * обслуживать как master и slave.
 *
 * Для включения примера разработчик должен:
 * 1. изменить роль выбранного порта в field_sensor_service.cpp на
 *    FIELD_PORT_SLAVE;
 * 2. исключить этот порт из master-опроса;
 * 3. перенести вызовы init/poll ниже в прикладной lifecycle;
 * 4. выбрать S1, S2, S3 или S4 в lcp_field_port_transport().
 */

#if 0

#include "../../board/lcp_field_ports.hpp"
#include "../../protocol/modbus_rtu/modbus_rtu_slave.hpp"

namespace
{
uint16_t g_example_holding_registers[2] =
{
    123U,
    456U
};

ModbusRtuSlave g_example_slave;
}

void field_sensor_slave_example_init(void)
{
    /*
     * Пример: S1, slave address 1, holding registers 0 и 1.
     * Для другого порта заменить LCP_FIELD_PORT_S1 на S2, S3 или S4.
     */
    modbus_rtu_slave_init(g_example_slave,
                          lcp_field_port_transport(LCP_FIELD_PORT_S1),
                          1U,
                          g_example_holding_registers,
                          2U,
                          5U);
}

void field_sensor_slave_example_poll(void)
{
    modbus_rtu_slave_poll(g_example_slave);
}

#endif
