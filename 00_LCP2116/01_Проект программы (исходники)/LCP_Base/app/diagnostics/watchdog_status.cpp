
/**
 * @file watchdog_status.cpp
 * @brief Реализация диагностики watchdog-таймера ATSAM3X8E.
 */

#include "watchdog_status.hpp"
#include "../../hal/sam3x_watchdog.hpp"
#include "../../platform/platform.hpp"

#include <string.h>

static const uint16_t WATCHDOG_TIMEOUT_TICKS = 1024U;
static const uint32_t WATCHDOG_FEED_PERIOD_MS = 500U;

static uint32_t g_last_feed_ms = 0U;
static uint8_t g_test_reset_armed = 0U;

void watchdog_status_init(void)
{
    sam3x_watchdog_capture_boot_status();
    sam3x_watchdog_increment_boot_count();

    const Sam3xWatchdogConfig config =
    {
        WATCHDOG_TIMEOUT_TICKS,
        1U,
        1U,
        0U,
        0U
    };

    sam3x_watchdog_enable(&config);

    g_last_feed_ms = millis();
    g_test_reset_armed = 0U;
}

void watchdog_status_poll(void)
{
    if (g_test_reset_armed != 0U)
    {
        return;
    }

    const uint32_t now_ms = millis();

    if ((uint32_t)(now_ms - g_last_feed_ms) >= WATCHDOG_FEED_PERIOD_MS)
    {
        g_last_feed_ms = now_ms;
        sam3x_watchdog_restart();
    }
}

void watchdog_status_print_report(void)
{
    SerialUSB.print("watchdog status\r\n");

    SerialUSB.print("enabled=");
    SerialUSB.print(sam3x_watchdog_enabled() ? "yes" : "no");
    SerialUSB.print("\r\n");

    SerialUSB.print("timeout_ms=");
    SerialUSB.print(static_cast<int>(sam3x_watchdog_timeout_ms(WATCHDOG_TIMEOUT_TICKS)));
    SerialUSB.print("\r\n");

    SerialUSB.print("feed_period_ms=");
    SerialUSB.print(static_cast<int>(WATCHDOG_FEED_PERIOD_MS));
    SerialUSB.print("\r\n");

    SerialUSB.print("mode=legacy WDRPROC|WDRSTEN\r\n");

    SerialUSB.print("watchdog_usb_recovery=");
    SerialUSB.print(sam3x_watchdog_recovery_performed() ? "performed" : "not performed");
    SerialUSB.print("\r\n");

    SerialUSB.print("test_reset_armed=");
    SerialUSB.print(g_test_reset_armed ? "yes" : "no");
    SerialUSB.print("\r\n");

    SerialUSB.print("last_reset=");
    SerialUSB.print(sam3x_watchdog_reset_type_text(sam3x_watchdog_last_reset_type()));
    SerialUSB.print("\r\n");

    SerialUSB.print("boot_count=");
    SerialUSB.print(static_cast<int>(sam3x_watchdog_boot_count()));
    SerialUSB.print("\r\n");

    SerialUSB.print("RSTC_SR=0x");
    SerialUSB.print(static_cast<int>(sam3x_watchdog_reset_status_register()), HEX);
    SerialUSB.print("\r\n");

    SerialUSB.print("WDT_MR=0x");
    SerialUSB.print(static_cast<int>(sam3x_watchdog_mode_register()), HEX);
    SerialUSB.print("\r\n");

    SerialUSB.print("WDT_SR=0x");
    SerialUSB.print(static_cast<int>(sam3x_watchdog_status_register()), HEX);
    SerialUSB.print("\r\n");
}

uint8_t watchdog_status_handle_command(const char* command)
{
    if (command == 0)
    {
        return 0U;
    }

    if (strcmp(command, "watchdog") == 0)
    {
        watchdog_status_print_report();
        return 1U;
    }

    if (strcmp(command, "watchdog feed") == 0)
    {
        sam3x_watchdog_restart();
        g_last_feed_ms = millis();
        SerialUSB.print("watchdog feed: ok\r\n");
        return 1U;
    }

    if (strcmp(command, "watchdog test reset") == 0)
    {
        g_test_reset_armed = 1U;
        SerialUSB.print("watchdog test reset armed\r\n");
        SerialUSB.print("automatic feed stopped; watchdog reset is expected\r\n");
        SerialUSB.print("after watchdog reset firmware will perform one software reset for USB recovery\r\n");
        return 1U;
    }

    return 0U;
}
