/**
 * @file app.cpp
 * @brief Диагностическая прошивка базовой проверки LCP.
 *
 * Приложение выполняет heartbeat через PLC_ok, поддерживает USB CDC
 * service port, обслуживает echo-test встроенных RS-485 портов
 * и echo-test внешних UART SC16IS7xx.
 */

#include "app.hpp"
#include "../board/lcp_board.hpp"
#include "../board/lcp_rs485.hpp"
#include "../platform/platform.hpp"
#include "diagnostics/rs485_echo_test.hpp"
#include "diagnostics/sc16is_echo_test.hpp"

static const byte PLC_ok = 40U;

static const uint32_t OK_LED_PERIOD_MS = 500U;
static const uint32_t USB_STATUS_PERIOD_MS = 2000U;

void setup(void)
{
    lcp_board_init_gpio();
    lcp_rs485_init_builtin_ports();

    pinMode(PLC_ok, OUTPUT);
    digitalWrite(PLC_ok, LOW);

    SerialUSB.begin(115200U, SERIAL_8N1);

    SPI.begin();

    rs485_echo_test_init();
    sc16is_echo_test_init();
}

void loop(void)
{
    static uint32_t last_toggle_ms = 0U;
    static uint32_t last_usb_status_ms = 0U;
    static uint8_t led_state = LOW;

    const uint32_t now_ms = millis();

    if ((uint32_t)(now_ms - last_toggle_ms) >= OK_LED_PERIOD_MS)
    {
        last_toggle_ms = now_ms;
        led_state = (led_state == LOW) ? HIGH : LOW;
        digitalWrite(PLC_ok, led_state);
    }

    rs485_echo_test_poll();
    sc16is_echo_test_poll();
    sc16is_echo_test_print_report_once();

    if ((uint32_t)(now_ms - last_usb_status_ms) >= USB_STATUS_PERIOD_MS)
    {
        last_usb_status_ms = now_ms;

        if (SerialUSB)
        {
            SerialUSB.write((const uint8_t*)"LCP RS485/SC16IS echo test OK\r\n", 31U);
        }
    }
}
