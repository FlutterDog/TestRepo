
/**
 * @file app.cpp
 * @brief Диагностическая прошивка базовой проверки LCP.
 *
 * Приложение выполняет heartbeat через PLC_ok, поддерживает USB CDC
 * service console, обслуживает echo-test встроенных RS-485 портов,
 * echo-test внешних UART SC16IS7xx, TCP echo-test W5500, microSD
 * и цифровой контроль резервной батареи CR2032.
 */

#include "app.hpp"
#include "../board/lcp_board.hpp"
#include "../board/lcp_rs485.hpp"
#include "../platform/platform.hpp"
#include "diagnostics/rs485_echo_test.hpp"
#include "diagnostics/sc16is_echo_test.hpp"
#include "diagnostics/ethernet_echo_test.hpp"
#include "diagnostics/diagnostic_console.hpp"
#include "diagnostics/sd_card_test.hpp"
#include "diagnostics/battery_status.hpp"

static const byte PLC_ok = 40U;

static const uint32_t OK_LED_PERIOD_MS = 500U;

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
    ethernet_echo_test_init();
    sd_card_test_init();
    battery_status_init();
    diagnostic_console_init();
}

void loop(void)
{
    static uint32_t last_toggle_ms = 0U;
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
    ethernet_echo_test_poll();
    sd_card_test_poll();
    battery_status_poll();
    diagnostic_console_poll();
}
