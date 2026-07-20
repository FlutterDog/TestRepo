/**
 * @file watchdog_status.cpp
 * @brief Диагностика watchdog-таймера ATSAM3X8E.
 *
 * Нормальный режим перезапускает watchdog из watchdog_status_poll(). Команда
 * `watchdog test reset` намеренно прекращает feed; это контролируемый тест
 * аппаратного reset, а не зависание. Новые тестовые режимы должны работать
 * через флаги и poll(), без бесконечных циклов.
 */

#include "watchdog_status.hpp"
#include "diagnostic_output.hpp"

#include "../../hal/sam3x_watchdog.hpp"
#include "../../platform/platform.hpp"

#include <string.h>

namespace
{
constexpr uint16_t WATCHDOG_TIMEOUT_TICKS = 1024U;
constexpr uint32_t WATCHDOG_FEED_PERIOD_MS = 500U;

uint32_t g_last_feed_ms = 0U;
uint8_t g_test_reset_armed = 0U;
}

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
    diagnostic_print_section("WATCHDOG");

    diagnostic_print_group("Runtime");
    diagnostic_print_assignment("enabled");
    SerialUSB.print(sam3x_watchdog_enabled() ? "yes" : "no");
    SerialUSB.print(", ");
    diagnostic_print_assignment("timeout_ms");
    SerialUSB.print(static_cast<unsigned long>(
        sam3x_watchdog_timeout_ms(WATCHDOG_TIMEOUT_TICKS)));
    SerialUSB.print("\r\n");

    diagnostic_print_assignment("feed_period_ms");
    SerialUSB.print(static_cast<unsigned long>(WATCHDOG_FEED_PERIOD_MS));
    SerialUSB.print(", ");
    diagnostic_print_assignment("test_reset_armed");
    SerialUSB.print((g_test_reset_armed != 0U) ? "yes" : "no");
    SerialUSB.print("\r\n");

    diagnostic_print_assignment("mode");
    SerialUSB.print("legacy WDRPROC|WDRSTEN\r\n");
    diagnostic_print_assignment("usb_recovery_after_watchdog");
    SerialUSB.print(sam3x_watchdog_recovery_performed() ?
                    "performed" : "not performed");
    SerialUSB.print("\r\n");

    diagnostic_print_group("Last reset");
    diagnostic_print_assignment("reset_type");
    SerialUSB.print(sam3x_watchdog_reset_type_text(
        sam3x_watchdog_last_reset_type()));
    SerialUSB.print(", ");
    diagnostic_print_assignment("boot_count");
    SerialUSB.print(static_cast<unsigned long>(sam3x_watchdog_boot_count()));
    SerialUSB.print("\r\n");

    diagnostic_print_group("Raw registers");
    diagnostic_print_assignment("RSTC_SR");
    SerialUSB.print("0x");
    SerialUSB.print(static_cast<unsigned long>(
        sam3x_watchdog_reset_status_register()), HEX);
    SerialUSB.print(", ");
    diagnostic_print_assignment("WDT_MR");
    SerialUSB.print("0x");
    SerialUSB.print(static_cast<unsigned long>(
        sam3x_watchdog_mode_register()), HEX);
    SerialUSB.print("\r\n");

    diagnostic_print_assignment("WDT_SR");
    SerialUSB.print("0x");
    SerialUSB.print(static_cast<unsigned long>(
        sam3x_watchdog_status_register()), HEX);
    SerialUSB.print("\r\n");
}

uint8_t watchdog_status_handle_command(const char* command)
{
    if (command == 0)
    {
        return 0U;
    }

    if ((strcmp(command, "watchdog") == 0) ||
        (strcmp(command, "watchdog status") == 0))
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
        SerialUSB.print("watchdog reset test armed\r\n");
        SerialUSB.print("automatic feed stopped\r\n");
        SerialUSB.print("hardware reset is expected after timeout\r\n");
        SerialUSB.print("next boot may perform USB-recovery reset\r\n");
        return 1U;
    }

    return 0U;
}
