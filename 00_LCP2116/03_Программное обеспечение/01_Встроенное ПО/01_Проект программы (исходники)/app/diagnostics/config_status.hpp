/**
 * @file config_status.hpp
 * @brief Диагностика внутреннего конфигурационного хранилища LCP2116.
 */

#pragma once

#include <stdint.h>

/** @brief Печатает состояние A/B-слотов, active bundle и SD-кандидата. */
void config_status_print_report(void);

/**
 * @brief Обрабатывает команды `config ...`.
 * @return 1, если команда распознана.
 */
uint8_t config_status_handle_command(const char* command);
