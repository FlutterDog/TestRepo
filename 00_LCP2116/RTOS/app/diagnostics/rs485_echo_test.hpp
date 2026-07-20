/**
 * @file rs485_echo_test.hpp
 * @brief Диагностический echo-test встроенных RS-485 портов.
 */

#pragma once

#include <stdint.h>

/** @brief Инициализирует встроенные порты для RS-485 echo-test. */
void rs485_echo_test_init(void);

/** @brief Обслуживает разрешённые линии RS-485 echo-test. */
void rs485_echo_test_poll(void);

/**
 * @brief Разрешает или запрещает echo-test на физической линии X2X.
 *
 * X2X-сервис должен запрещать echo до начала Modbus master обмена, чтобы два
 * обработчика не читали один RX-буфер и не формировали конкурирующие ответы.
 */
void rs485_echo_test_set_x2x_enabled(uint8_t enabled);

/** @brief Возвращает 1, если echo-test линии X2X разрешён. */
uint8_t rs485_echo_test_x2x_enabled(void);

/**
 * @brief Разрешает или запрещает echo-test встроенных пользовательских портов.
 *
 * FieldSensor запрещает echo на S2, S3 и S4, поскольку эти линии принадлежат
 * четырём независимым Modbus RTU master-примерам.
 */
void rs485_echo_test_set_field_ports_enabled(uint8_t enabled);

/** @brief Возвращает 1, если echo-test S2, S3 и S4 разрешён. */
uint8_t rs485_echo_test_field_ports_enabled(void);
