/**
 * @file ethernet_status.hpp
 * @brief Service console для двух Modbus TCP server W5500.
 *
 * Этот модуль не формирует протокол и не управляет SPI. Он отображает состояние
 * `ethernet_modbus_service`, W5500 HAL и опубликованную holding map. При
 * расширении карты сначала изменяется `ethernet_modbus_service.cpp`, затем
 * обновляются карта и расшифровка quality в ethernet_status.cpp.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Печатает ETH1/ETH2, сетевые файлы, счётчики и holding map.
 *
 * Оба интерфейса публикуют одинаковые данные, но имеют независимые W5500,
 * IP-конфигурацию, socket state и transport counters.
 */
void ethernet_status_print_report(void);

/**
 * @brief Обрабатывает `eth`, `eth status` и `eth reload`.
 *
 * `eth reload` только ставит запрос; сетевые файлы и reset обоих W5500
 * выполняются последующим ethernet_modbus_service_poll().
 *
 * @param[in] command Нормализованная строка нижнего регистра без CR/LF.
 * @return 1 для распознанной Ethernet-команды, иначе 0.
 */
uint8_t ethernet_status_handle_command(const char* command);
