/**
 * @file rs485_echo_test.cpp
 * @brief Неблокирующий echo-test встроенных RS-485 портов.
 *
 * Echo используется только пока порт не передан X2X или FieldSensor service.
 * Приём ограничен 16 байтами за poll. Если software TX-буфер принимает только
 * часть кадра, `tx_offset` сохраняет прогресс и следующий poll продолжает с
 * первого неотправленного байта.
 */

#include "rs485_echo_test.hpp"
#include "../../platform/platform.hpp"

namespace
{
constexpr uint32_t RS485_ECHO_BAUDRATE = 9600U;

/* 5 мс при 9600 8N1 превышают RTU-паузу 3.5 символа. */
constexpr uint32_t RS485_INTERFRAME_GAP_MS = 5U;

/* Даёт удалённому half-duplex устройству перейти в приём. */
constexpr uint32_t RS485_RESPONSE_DELAY_MS = 10U;
constexpr uint8_t RS485_ECHO_MAX_BYTES_PER_POLL = 16U;
constexpr uint16_t RS485_ECHO_BUFFER_SIZE = 128U;

struct Rs485EchoPort
{
    SerialPort* serial;                       /**< Физический встроенный UART. */
    const char* name;                         /**< Имя X2X/S2/S3/S4 для диагностики. */
    uint8_t buffer[RS485_ECHO_BUFFER_SIZE];   /**< Полный принятый кадр. */
    uint16_t length;                          /**< Длина кадра. */
    uint16_t tx_offset;                       /**< Первый ещё не переданный байт. */
    uint32_t last_rx_ms;                      /**< Время последнего RX-байта. */
    uint32_t response_due_ms;                 /**< Момент разрешения ответа. */
    uint8_t response_pending;                 /**< Кадр завершён и ожидает/выполняет TX. */
};

Rs485EchoPort g_echo_ports[] =
{
    { &Serial,  "X2X", {0U}, 0U, 0U, 0U, 0U, 0U },
    { &Serial1, "S2",  {0U}, 0U, 0U, 0U, 0U, 0U },
    { &Serial2, "S4",  {0U}, 0U, 0U, 0U, 0U, 0U },
    { &Serial3, "S3",  {0U}, 0U, 0U, 0U, 0U, 0U }
};

uint8_t g_x2x_echo_enabled = 1U;
uint8_t g_field_echo_enabled = 1U;

void drop_frame(Rs485EchoPort& port, uint32_t now_ms)
{
    port.length = 0U;
    port.tx_offset = 0U;
    port.last_rx_ms = now_ms;
    port.response_due_ms = 0U;
    port.response_pending = 0U;
}

void accept_rx(Rs485EchoPort& port, uint32_t now_ms)
{
    /* Half-duplex: после начала ответа новый запрос не смешивается с TX. */
    if (port.tx_offset != 0U)
    {
        return;
    }

    uint8_t processed = 0U;

    while ((port.serial->available() > 0) &&
           (processed < RS485_ECHO_MAX_BYTES_PER_POLL))
    {
        const int value = port.serial->read();

        if (value < 0)
        {
            break;
        }

        if (port.length >= RS485_ECHO_BUFFER_SIZE)
        {
            drop_frame(port, now_ms);
            return;
        }

        port.buffer[port.length++] = static_cast<uint8_t>(value);
        port.last_rx_ms = now_ms;
        port.response_pending = 0U;
        port.response_due_ms = 0U;
        ++processed;
    }
}

void arm_response_if_frame_complete(Rs485EchoPort& port,
                                    uint32_t now_ms)
{
    if ((port.length == 0U) || (port.response_pending != 0U))
    {
        return;
    }

    if ((uint32_t)(now_ms - port.last_rx_ms) < RS485_INTERFRAME_GAP_MS)
    {
        return;
    }

    port.response_due_ms = now_ms + RS485_RESPONSE_DELAY_MS;
    port.response_pending = 1U;
    port.tx_offset = 0U;
}

void send_if_response_due(Rs485EchoPort& port, uint32_t now_ms)
{
    if ((port.response_pending == 0U) ||
        (static_cast<int32_t>(now_ms - port.response_due_ms) < 0))
    {
        return;
    }

    if (port.tx_offset >= port.length)
    {
        drop_frame(port, now_ms);
        return;
    }

    const size_t remaining =
        static_cast<size_t>(port.length - port.tx_offset);
    const size_t written = port.serial->write(
        &port.buffer[port.tx_offset],
        remaining);

    port.tx_offset = static_cast<uint16_t>(port.tx_offset + written);

    if (port.tx_offset >= port.length)
    {
        drop_frame(port, now_ms);
    }
}
}

void rs485_echo_test_init(void)
{
    Serial.begin(RS485_ECHO_BAUDRATE, SERIAL_8N1);
    Serial1.begin(RS485_ECHO_BAUDRATE, SERIAL_8N1);
    Serial2.begin(RS485_ECHO_BAUDRATE, SERIAL_8N1);
    Serial3.begin(RS485_ECHO_BAUDRATE, SERIAL_8N1);

    g_x2x_echo_enabled = 1U;
    g_field_echo_enabled = 1U;

    const uint32_t now_ms = millis();

    for (size_t port_index = 0U;
         port_index < (sizeof(g_echo_ports) / sizeof(g_echo_ports[0]));
         ++port_index)
    {
        drop_frame(g_echo_ports[port_index], now_ms);
    }
}

void rs485_echo_test_poll(void)
{
    const uint32_t now_ms = millis();

    for (size_t port_index = 0U;
         port_index < (sizeof(g_echo_ports) / sizeof(g_echo_ports[0]));
         ++port_index)
    {
        if ((port_index == 0U) && (g_x2x_echo_enabled == 0U))
        {
            continue;
        }

        if ((port_index != 0U) && (g_field_echo_enabled == 0U))
        {
            continue;
        }

        accept_rx(g_echo_ports[port_index], now_ms);
        arm_response_if_frame_complete(g_echo_ports[port_index], now_ms);
        send_if_response_due(g_echo_ports[port_index], now_ms);
    }
}

void rs485_echo_test_set_x2x_enabled(uint8_t enabled)
{
    const uint8_t normalized = (enabled != 0U) ? 1U : 0U;

    if (normalized == g_x2x_echo_enabled)
    {
        return;
    }

    g_x2x_echo_enabled = normalized;
    drop_frame(g_echo_ports[0], millis());
}

uint8_t rs485_echo_test_x2x_enabled(void)
{
    return g_x2x_echo_enabled;
}

void rs485_echo_test_set_field_ports_enabled(uint8_t enabled)
{
    const uint8_t normalized = (enabled != 0U) ? 1U : 0U;

    if (normalized == g_field_echo_enabled)
    {
        return;
    }

    g_field_echo_enabled = normalized;
    const uint32_t now_ms = millis();

    for (size_t port_index = 1U;
         port_index < (sizeof(g_echo_ports) / sizeof(g_echo_ports[0]));
         ++port_index)
    {
        drop_frame(g_echo_ports[port_index], now_ms);
    }
}

uint8_t rs485_echo_test_field_ports_enabled(void)
{
    return g_field_echo_enabled;
}
