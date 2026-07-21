/**
 * @file battery_status.cpp
 * @brief Диагностика резервной батареи CR2032.
 *
 * Вход контроллера получает дискретный результат внешнего компаратора, поэтому
 * прошивка не измеряет напряжение батареи. Значение 2.2 V — аппаратный порог.
 * Если в новой плате появится ADC, добавляйте его отдельным полем и не заменяйте
 * существующие raw/debounced состояния.
 */

#include "battery_status.hpp"
#include "diagnostic_output.hpp"

#include "../../board/lcp_battery.hpp"
#include "../../platform/platform.hpp"

namespace
{
constexpr uint32_t BATTERY_DEBOUNCE_MS = 100U;

uint8_t g_last_raw = 0U;
uint8_t g_debounced_raw = 0U;
uint8_t g_stable = 0U;
uint32_t g_last_change_ms = 0U;

uint8_t battery_raw_to_ok(uint8_t raw)
{
    if (LCP_BACKUP_BATTERY_OK_ACTIVE_HIGH != 0U)
    {
        return (raw != 0U) ? 1U : 0U;
    }

    return (raw == 0U) ? 1U : 0U;
}
}

void battery_status_init(void)
{
    lcp_battery_init_pins();

    g_last_raw = lcp_battery_raw();
    g_debounced_raw = g_last_raw;
    g_stable = 0U;
    g_last_change_ms = millis();
}

void battery_status_poll(void)
{
    const uint32_t now_ms = millis();
    const uint8_t raw = lcp_battery_raw();

    if (raw != g_last_raw)
    {
        g_last_raw = raw;
        g_last_change_ms = now_ms;
        g_stable = 0U;
        return;
    }

    if ((g_stable == 0U) &&
        ((uint32_t)(now_ms - g_last_change_ms) >= BATTERY_DEBOUNCE_MS))
    {
        g_stable = 1U;
        g_debounced_raw = raw;
    }
}

void battery_status_print_report(void)
{
    const uint8_t raw = lcp_battery_raw();
    const uint8_t debounced_ok = battery_raw_to_ok(g_debounced_raw);
    const uint8_t instant_ok = lcp_battery_ok();

    diagnostic_print_section("RTC BACKUP BATTERY");

    diagnostic_print_assignment("battery");
    SerialUSB.print("CR2032, ");
    diagnostic_print_assignment("input_pin");
    SerialUSB.print("50\r\n");

    diagnostic_print_assignment("comparator_threshold");
    SerialUSB.print("2.2 V\r\n");
    diagnostic_print_assignment("voltage_measurement");
    SerialUSB.print("not available\r\n");

    diagnostic_print_assignment("debounce_ms");
    SerialUSB.print(static_cast<unsigned long>(BATTERY_DEBOUNCE_MS));
    SerialUSB.print(", ");
    diagnostic_print_assignment("input_active_high");
    SerialUSB.print((LCP_BACKUP_BATTERY_OK_ACTIVE_HIGH != 0U) ? "yes" : "no");
    SerialUSB.print("\r\n");

    diagnostic_print_assignment("raw");
    SerialUSB.print(static_cast<int>(raw));
    SerialUSB.print(", ");
    diagnostic_print_assignment("debounced_raw");
    SerialUSB.print(static_cast<int>(g_debounced_raw));
    SerialUSB.print(", ");
    diagnostic_print_assignment("stable");
    SerialUSB.print((g_stable != 0U) ? "yes" : "no");
    SerialUSB.print("\r\n");

    diagnostic_print_assignment("instant_state");
    SerialUSB.print((instant_ok != 0U) ? "ok" : "low");
    SerialUSB.print(", ");
    diagnostic_print_assignment("debounced_state");
    SerialUSB.print((debounced_ok != 0U) ? "ok" : "low");
    SerialUSB.print("\r\n");

    diagnostic_print_assignment("action");
    SerialUSB.print((debounced_ok != 0U) ?
                    "none" :
                    "replace CR2032 battery");
    SerialUSB.print("\r\n");
}
