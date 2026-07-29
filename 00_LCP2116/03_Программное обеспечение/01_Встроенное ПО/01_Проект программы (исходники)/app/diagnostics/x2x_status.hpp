/**
 * @file x2x_status.hpp
 * @brief USB-диагностика конфигурации и опроса модулей X2X.
 *
 * Общие поля устройств печатаются в x2x_status.cpp. Значения конкретного типа
 * выводит `X2XModuleDescriptor::print`, зарегистрированный в x2x_catalog.cpp.
 * Поэтому новый модуль не должен добавлять switch в console: его Doxygen,
 * construct/poll/print callback и descriptor являются единой точкой расширения.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Печатает X2X.TXT, состояние service, каждый module slot и waveform.
 *
 * Функция безопасно сообщает отсутствующий registry slot, неизвестный catalog
 * descriptor и отсутствующий module-specific print callback вместо тихого
 * пропуска данных.
 */
void x2x_status_print_report(void);

/**
 * @brief Обрабатывает сервисные команды X2X.
 *
 * Поддерживаются:
 * - `x2x`, `x2x status`;
 * - `x2x reload`;
 * - `x2x pause`, `x2x resume`;
 * - `x2x ldo SLAVE VALUE`, где SLAVE=1..6, VALUE=int16.
 *
 * @param[in] command Нормализованная строка нижнего регистра без CR/LF.
 * @return 1, если команда относится к X2X, иначе 0.
 */
uint8_t x2x_status_handle_command(const char* command);
