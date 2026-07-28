
/**
 * @file sd_card_test.hpp
 * @brief Диагностика microSD интерфейса.
 */

#pragma once

/**
 * @brief Инициализирует диагностический модуль microSD.
 */
void sd_card_test_init(void);

/**
 * @brief Обслуживает состояние microSD без блокирующего ожидания.
 */
void sd_card_test_poll(void);

/**
 * @brief Печатает отчёт microSD в USB service console.
 */
void sd_card_test_print_report(void);

/**
 * @brief Выполняет тест записи и чтения конфигурационного файла.
 */
void sd_card_test_run_file_test(void);
