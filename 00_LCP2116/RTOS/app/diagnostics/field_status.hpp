/**
 * @file field_status.hpp
 * @brief Диагностика демонстрационного FieldSensor на S1..S4.
 */

#pragma once

#include <stdint.h>

/** @brief Печатает состояние всех четырёх FieldSensor master. */
void field_status_print_report(void);

/**
 * @brief Обрабатывает команды field, field status, field pause и field resume.
 *
 * @return 1, если команда распознана.
 */
uint8_t field_status_handle_command(const char* command);
