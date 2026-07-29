/**
 * @file rs485_echo_test.hpp
 * @brief Неблокирующий fallback echo-test физической линии X2X.
 *
 * В baseline порты S2, S3 и S4 с момента setup принадлежат FieldSensor и не
 * проходят через временный echo. Единственный динамический владелец — UART0:
 * при наличии модулей им владеет X2X master, при пустом X2X.TXT — echo-test.
 * Частичный TX сохраняется между poll-вызовами.
 */

#pragma once

#include <stdint.h>

/** @brief Инициализирует только физический UART0/X2X в 9600 8N1. */
void rs485_echo_test_init(void);

/** @brief Обслуживает X2X echo, когда он разрешён текущим владельцем порта. */
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
