/**
 * @file ethernet_status.hpp
 * @brief Service console для двух Modbus TCP server W5500.
 */

#pragma once

#include <stdint.h>

/** @brief Печатает состояние обоих Ethernet-интерфейсов. */
void ethernet_status_print_report(void);

/** @brief Обрабатывает команды eth и eth reload. */
uint8_t ethernet_status_handle_command(const char* command);
