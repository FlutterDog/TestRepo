/**
 * @file rs485_echo_test.cpp
 * @brief Неблокирующий fallback echo-test физической линии X2X.
 *
 * S2–S4 сразу принадлежат FieldSensor и здесь не инициализируются. UART0 может
 * оставаться echo только при пустой X2X-конфигурации. Приём ограничен 16
 * байтами за poll, частичный TX продолжается через `tx_offset`, а зависший TX
 * сбрасывается по ограниченному timeout.
 */

#include "rs485_echo_test.hpp"
#include "../../platform/platform.hpp"

namespace
{
constexpr uint32_t RS485_ECHO_BAUDRATE = 9600U;
constexpr uint32_t RS485_INTERFRAME_GAP_MS = 5U;
constexpr uint32_t RS485_RESPONSE_DELAY_MS = 10U;
constexpr uint32_t RS485_TX_TIMEOUT_MS = 1000U;
constexpr uint8_t RS485_ECHO_MAX_BYTES_PER_POLL = 16U;
constexpr uint16_t RS485_ECHO_BUFFER_SIZE = 128U;

struct Rs485EchoState
{
    uint8_t buffer[RS485_ECHO_BUFFER_SIZE]; /**< Полный принятый кадр. */
    uint16_t length;                        /**< Длина кадра. */
    uint16_t tx_offset;                     /**< Первый ещё не переданный байт. */
    uint32_t last_rx_ms;                    /**< Время последнего RX-байта. */
    uint32_t response_due_ms;               /**< Момент разрешения ответа. */
    uint32_t tx_deadline_ms;                 /**< Крайний срок постановки всего ответа. */
    uint8_t response_pending;               /**< Кадр завершён и ожидает/выполняет TX. */
};

Rs485EchoState g_echo = { {0U}, 0U, 0U, 0U, 0U, 0U, 0U };
uint8_t g_x2x_echo_enabled = 1U;

uint8_t deadline_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (static_cast<int32_t>(now_ms - deadline_ms) >= 0) ? 1U : 0U;
}

void drop_frame(uint32_t now_ms)
{
    g_echo.length = 0U;
    g_echo.tx_offset = 0U;
    g_echo.last_rx_ms = now_ms;
    g_echo.response_due_ms = 0U;
    g_echo.tx_deadline_ms = 0U;
    g_echo.response_pending = 0U;
}

void accept_rx(uint32_t now_ms)
{
    if (g_echo.tx_offset != 0U)
    {
        return;
    }

    uint8_t processed = 0U;

    while ((Serial.available() > 0) &&
           (processed < RS485_ECHO_MAX_BYTES_PER_POLL))
    {
        const int value = Serial.read();

        if (value < 0)
        {
            break;
        }

        if (g_echo.length >= RS485_ECHO_BUFFER_SIZE)
        {
            drop_frame(now_ms);
            return;
        }

        g_echo.buffer[g_echo.length++] = static_cast<uint8_t>(value);
        g_echo.last_rx_ms = now_ms;
        g_echo.response_pending = 0U;
        g_echo.response_due_ms = 0U;
        g_echo.tx_deadline_ms = 0U;
        ++processed;
    }
}

void arm_response_if_frame_complete(uint32_t now_ms)
{
    if ((g_echo.length == 0U) || (g_echo.response_pending != 0U))
    {
        return;
    }

    if ((uint32_t)(now_ms - g_echo.last_rx_ms) < RS485_INTERFRAME_GAP_MS)
    {
        return;
    }

    g_echo.response_due_ms = now_ms + RS485_RESPONSE_DELAY_MS;
    g_echo.tx_deadline_ms = now_ms + RS485_TX_TIMEOUT_MS;
    g_echo.response_pending = 1U;
    g_echo.tx_offset = 0U;
}

void send_if_response_due(uint32_t now_ms)
{
    if (g_echo.response_pending == 0U)
    {
        return;
    }

    if (deadline_reached(now_ms, g_echo.tx_deadline_ms) != 0U)
    {
        drop_frame(now_ms);
        return;
    }

    if (static_cast<int32_t>(now_ms - g_echo.response_due_ms) < 0)
    {
        return;
    }

    const size_t remaining =
        static_cast<size_t>(g_echo.length - g_echo.tx_offset);
    const size_t written = Serial.write(
        &g_echo.buffer[g_echo.tx_offset],
        remaining);

    g_echo.tx_offset = static_cast<uint16_t>(g_echo.tx_offset + written);

    if (g_echo.tx_offset >= g_echo.length)
    {
        drop_frame(now_ms);
    }
}
}

void rs485_echo_test_init(void)
{
    Serial.begin(RS485_ECHO_BAUDRATE, SERIAL_8N1);
    g_x2x_echo_enabled = 1U;
    drop_frame(millis());
}

void rs485_echo_test_poll(void)
{
    if (g_x2x_echo_enabled == 0U)
    {
        return;
    }

    const uint32_t now_ms = millis();
    accept_rx(now_ms);
    arm_response_if_frame_complete(now_ms);
    send_if_response_due(now_ms);
}

void rs485_echo_test_set_x2x_enabled(uint8_t enabled)
{
    const uint8_t normalized = (enabled != 0U) ? 1U : 0U;

    if (normalized == g_x2x_echo_enabled)
    {
        return;
    }

    g_x2x_echo_enabled = normalized;
    drop_frame(millis());
}

uint8_t rs485_echo_test_x2x_enabled(void)
{
    return g_x2x_echo_enabled;
}
