/**
 * @file x2x_service.hpp
 * @brief Управление конфигурацией и циклическим опросом модулей X2X.
 */

#pragma once

#include <stdint.h>

#include "x2x_config.hpp"
#include "x2x_registry.hpp"
#include "x2x_types.hpp"
#include "../../protocol/modbus_rtu/modbus_rtu_master.hpp"

/** @brief Инициализирует X2X-порт, master и состояние сервиса. */
void x2x_service_init(void);

/** @brief Выполняет один неблокирующий шаг загрузки и опроса X2X. */
void x2x_service_poll(void);

/**
 * @brief Повторно читает X2X.TXT и применяет его при успешной проверке.
 *
 * При активной транзакции операция откладывается до окончания текущего цикла
 * модуля и возвращает X2X_CONFIG_APPLY_PENDING. При ошибке файла ранее
 * активная конфигурация продолжает работать.
 */
X2XConfigResult x2x_service_reload_config(void);

/**
 * @brief Запрашивает остановку опроса после завершения текущего цикла.
 *
 * Функция не обрывает физическую передачу и не оставляет DE в активном
 * состоянии.
 */
void x2x_service_pause(void);

/** @brief Возобновляет циклический опрос. */
void x2x_service_resume(void);

/** @brief Возвращает 1, если опрос уже приостановлен. */
uint8_t x2x_service_paused(void);

/** @brief Возвращает 1, если ожидается безопасная точка для паузы. */
uint8_t x2x_service_pause_pending(void);

/** @brief Возвращает 1, если применение X2X.TXT отложено. */
uint8_t x2x_service_reload_pending(void);

/** @brief Возвращает 1, если применена корректная конфигурация. */
uint8_t x2x_service_config_loaded(void);

/**
 * @brief Возвращает 1, если X2X-сервис использует UART0.
 *
 * При нулевом количестве модулей порт не захватывается и может применяться
 * диагностическим echo-test.
 */
uint8_t x2x_service_owns_port(void);

/** @brief Возвращает результат последней завершённой загрузки X2X.TXT. */
X2XConfigResult x2x_service_last_config_result(void);

/** @brief Возвращает подробности последней ошибки X2X.TXT. */
const X2XConfigError& x2x_service_last_config_error(void);

/** @brief Возвращает результат последней операции построения реестра. */
X2XRegistryResult x2x_service_last_registry_result(void);

/** @brief Возвращает адрес текущего опрашиваемого модуля. */
uint8_t x2x_service_current_slave(void);

/** @brief Возвращает результат текущей или последней Modbus-транзакции. */
ModbusRtuResult x2x_service_modbus_result(void);

/** @brief Возвращает буфер последней осциллограммы LCT. */
const X2XWaveformBuffer& x2x_service_waveform(void);

/**
 * @brief Устанавливает значение выходного регистра экземпляра LDO1118.
 *
 * Значение будет передано при следующем цикле опроса данного slave.
 *
 * @return 1 при успешной установке, иначе 0.
 */
uint8_t x2x_service_set_ldo_output(uint8_t slave_address, int16_t value);
