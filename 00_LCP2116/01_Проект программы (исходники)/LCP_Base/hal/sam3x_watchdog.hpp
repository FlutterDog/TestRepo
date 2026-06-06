
/**
 * @file sam3x_watchdog.hpp
 * @brief Управление watchdog ATSAM3X8E.
 */

#pragma once

/**
 * @brief Отключает watchdog микроконтроллера.
 *
 * Функция выполняется в начале main() до инициализации прикладных
 * подсистем. Watchdog ATSAM3X8E после reset остаётся активным до
 * явного отключения через WDT_MR.WDDIS.
 */
void hal_watchdog_disable(void);
