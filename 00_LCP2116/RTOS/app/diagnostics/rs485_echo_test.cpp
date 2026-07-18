/**
 * @file rs485_echo_test.cpp
 * @brief Реализация echo-test встроенных RS-485 портов.
 */

#include "rs485_echo_test.hpp"
#include "../../platform/platform.hpp"

static const uint32_t RS485_ECHO_BAUDRATE = 9600U;

/*
 * Для 9600 8N1 длительность одного символа составляет около 1.04 мс.
 * Пауза 5 мс превышает 3.5 символа и фиксирует окончание принятой посылки.
 */
static const uint32_t RS485_INTERFRAME_GAP_MS = 5U;

/*
 * Пауза перед ответом даёт внешнему half-duplex RS-485 устройству
 * завершить передачу и вернуть свой драйвер в режим приёма.
 */
static const uint32_t RS485_RESPONSE_DELAY_MS = 10U;
static const uint8_t RS485_ECHO_MAX_BYTES_PER_POLL = 16U;
static const uint16_t RS485_ECHO_BUFFER_SIZE = 128U;

struct Rs485EchoPort
{
    SerialPort* serial;
    const char* name;
    uint8_t buffer[RS485_ECHO_BUFFER_SIZE];
    uint16_t length;
    uint32_t last_rx_ms;
    uint32_t response_due_ms;
    uint8_t response_pending;
};

static Rs485EchoPort g_echo_ports[] =
{
    { &Serial,  "X2X", {0U}, 0U, 0U, 0U, 0U },
    { &Serial1, "S2",  {0U}, 0U, 0U, 0U, 0U },
    { &Serial2, "S4",  {0U}, 0U, 0U, 0U, 0U },
    { &Serial3, "S3",  {0U}, 0U, 0U, 0U, 0U }
};

static uint8_t g_x2x_echo_enabled = 1U;

static void rs485_echo_drop_frame(Rs485EchoPort& port, uint32_t now_ms)
{
    port.length = 0U;
    port.last_rx_ms = now_ms;
    port.response_due_ms = 0U;
    port.response_pending = 0U;
}

static void rs485_echo_accept_rx(Rs485EchoPort& port, uint32_t now_ms)
{
    uint8_t processed = 0U;

    while ((port.serial->available() > 0) &&
           (processed < RS485_ECHO_MAX_BYTES_PER_POLL))
    {
        const int value = port.serial->read();

        if (value >= 0)
        {
            if (port.length < RS485_ECHO_BUFFER_SIZE)
            {
                port.buffer[port.length] = static_cast<uint8_t>(value);
                ++port.length;
                port.last_rx_ms = now_ms;
                port.response_pending = 0U;
            }
            else
            {
                rs485_echo_drop_frame(port, now_ms);
            }
        }

        ++processed;
    }
}

static void rs485_echo_arm_response_if_frame_complete(Rs485EchoPort& port,
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
}

static void rs485_echo_send_if_response_due(Rs485EchoPort& port,
                                              uint32_t now_ms)
{
    if (port.response_pending == 0U)
    {
        return;
    }

    if ((int32_t)(now_ms - port.response_due_ms) < 0)
    {
        return;
    }

    (void)port.serial->write(port.buffer, port.length);
    rs485_echo_drop_frame(port, now_ms);
}

void rs485_echo_test_init(void)
{
    Serial.begin(RS485_ECHO_BAUDRATE, SERIAL_8N1);
    Serial1.begin(RS485_ECHO_BAUDRATE, SERIAL_8N1);
    Serial2.begin(RS485_ECHO_BAUDRATE, SERIAL_8N1);
    Serial3.begin(RS485_ECHO_BAUDRATE, SERIAL_8N1);

    g_x2x_echo_enabled = 1U;

    for (size_t port_index = 0U;
         port_index < (sizeof(g_echo_ports) / sizeof(g_echo_ports[0]));
         ++port_index)
    {
        rs485_echo_drop_frame(g_echo_ports[port_index], millis());
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

        rs485_echo_accept_rx(g_echo_ports[port_index], now_ms);
        rs485_echo_arm_response_if_frame_complete(g_echo_ports[port_index],
                                                   now_ms);
        rs485_echo_send_if_response_due(g_echo_ports[port_index], now_ms);
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
    rs485_echo_drop_frame(g_echo_ports[0], millis());
}

uint8_t rs485_echo_test_x2x_enabled(void)
{
    return g_x2x_echo_enabled;
}
