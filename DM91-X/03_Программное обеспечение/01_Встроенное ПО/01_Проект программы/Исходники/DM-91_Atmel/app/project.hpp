#ifndef PROJECT_HPP
#define PROJECT_HPP

/**
 * @file project.hpp
 * @brief Общие настройки сборки прошивки DM-91x.
 *
 * @details
 * Файл задаёт базовые параметры, общие для всей прошивки датчика плотности
 * DM-91x:
 * - частоту тактирования микроконтроллера;
 * - базовые системные типы;
 * - подключение общего HAL-интерфейса.
 *
 * Файл не содержит прикладную логику, карту Modbus-регистров, EEPROM-карту,
 * глобальные переменные приложения и карту конкретных аппаратных линий.
 *
 * Аппаратные линии платы описаны в dm91_board.hpp и dm91_board.cpp.
 */

#ifndef F_CPU
/**
 * @brief Частота тактирования микроконтроллера ATmega328P, Гц.
 */
#  define F_CPU 16000000UL
#endif

#include <stdint.h>
#include <stdbool.h>

#include "../hal/compat.hpp"

#endif /* PROJECT_HPP */
