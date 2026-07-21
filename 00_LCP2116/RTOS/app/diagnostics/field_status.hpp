/**
 * @file field_status.hpp
 * @brief Service console для демонстрационного FieldSensor на S1..S4.
 *
 * Этот модуль только отображает runtime и перенаправляет команды в
 * field_sensor_service. Параметры прибора — slave, FC03 start/count, period и
 * timeout — задаются в `set_default_configs()` файла
 * `app/field/field_sensor_service.cpp`, а скорость/чётность загружаются через
 * `field_serial_config.*`.
 *
 * При добавлении нового поля состояния сначала добавьте его в
 * FieldSensorPortState, затем обновите `print_port()` в field_status.cpp.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Печатает конфигурацию, значения, качество и счётчики S1..S4.
 *
 * Функция используется как самостоятельной командой `field`, так и общим
 * отчётом `status`; поэтому она сама печатает заголовок подсистемы.
 */
void field_status_print_report(void);

/**
 * @brief Обрабатывает семейство команд FieldSensor.
 *
 * Поддерживаются `field`, `field status`, `field reload`, `field pause` и
 * `field resume`. Неизвестная строка не печатает ошибку, чтобы следующий
 * обработчик общей console мог проверить свою команду.
 *
 * @param[in] command Нормализованная в нижний регистр строка без CR/LF.
 * @return 1, если команда принадлежит FieldSensor, иначе 0.
 */
uint8_t field_status_handle_command(const char* command);
