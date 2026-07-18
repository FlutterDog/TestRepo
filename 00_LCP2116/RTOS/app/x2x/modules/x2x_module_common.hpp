/**
 * @file x2x_module_common.hpp
 * @brief Общие операции драйверов модулей X2X.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../x2x_module.hpp"

/** Тайм-аут одной Modbus RTU транзакции X2X. */
static const uint32_t X2X_MODBUS_TRANSACTION_TIMEOUT_MS = 500U;

/** @brief Сбрасывает многошаговое состояние драйвера экземпляра. */
void x2x_module_runtime_reset(X2XModuleRuntime& runtime);

/** @brief Фиксирует успешный обмен и восстанавливает состояние связи. */
void x2x_module_mark_success(X2XDeviceHeader& device, uint32_t now_ms);

/** @brief Фиксирует ошибку обмена и контролирует порог потери связи. */
void x2x_module_mark_failure(X2XDeviceHeader& device,
                             ModbusRtuResult result,
                             uint8_t exception_code);

/**
 * @brief Преобразует два Modbus-регистра в float.
 *
 * Совместимо со старой прошивкой: первый регистр содержит младшее 16-битное
 * слово, второй — старшее.
 */
float x2x_module_registers_to_float(uint16_t low_register,
                                    uint16_t high_register);

/** @brief Декодирует последовательность float из массива регистров. */
void x2x_module_decode_float_array(float* destination,
                                   size_t float_count,
                                   const uint16_t* registers,
                                   size_t start_register,
                                   size_t register_count);
