/**
 * @file rs485_echo_test.hpp
 * @brief Неблокирующий echo-test встроенных RS-485 портов.
 *
 * Echo является временным владельцем UART только до запуска штатного service.
 * X2X забирает Serial, FieldSensor — Serial1/2/3. Реализация ограничивает RX за
 * один poll и хранит частичный TX offset, поэтому длинный ответ не теряется при
 * заполнении software TX buffer.
 */

#pragma once

#include <stdint.h>

/** @brief Запускает Serial/Serial1/Serial2/Serial3 в диагностическом 9600 8N1. */
void rs485_echo_test_init(void);

/** @brief Обслуживает только те линии, которые ещё принадлежат echo-test. */
void rs485_echo_test_poll(void);

/**
 * @brief Передаёт или освобождает владение физической линией X2X.
 *
 * При изменении владельца незавершённый echo-кадр сбрасывается, чтобы X2X
 * master не получил остаток диагностического ответа.
 *
 * @param[in] enabled Ненулевое значение разрешает echo.
 */
void rs485_echo_test_set_x2x_enabled(uint8_t enabled);

/** @return 1, если линия X2X принадлежит echo-test, иначе 0. */
uint8_t rs485_echo_test_x2x_enabled(void);

/**
 * @brief Передаёт или освобождает S2, S3 и S4 как единую группу.
 *
 * FieldSensor вызывает эту функцию с 0 до начала Modbus master polling.
 *
 * @param[in] enabled Ненулевое значение разрешает echo на S2/S3/S4.
 */
void rs485_echo_test_set_field_ports_enabled(uint8_t enabled);

/** @return 1, если S2/S3/S4 принадлежат echo-test, иначе 0. */
uint8_t rs485_echo_test_field_ports_enabled(void);
