
/**
 * @file battery_status.cpp
 * @brief Реализация диагностики резервной батареи CR2032.
 */

#include "battery_status.hpp"
#include "../../board/lcp_battery.hpp"
#include "../../platform/platform.hpp"

static const uint32_t BATTERY_DEBOUNCE_MS = 100U;

static uint8_t g_last_raw = 0U;
static uint8_t g_debounced_raw = 0U;
static uint8_t g_stable = 0U;
static uint32_t g_last_change_ms = 0U;

static uint8_t battery_raw_to_ok(uint8_t raw)
{
    if (LCP_BACKUP_BATTERY_OK_ACTIVE_HIGH != 0U)
    {
        return (raw != 0U) ? 1U : 0U;
    }

    return (raw == 0U) ? 1U : 0U;
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

    if ((g_stable == 0U) && ((uint32_t)(now_ms - g_last_change_ms) >= BATTERY_DEBOUNCE_MS))
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

    SerialUSB.print("backup battery status\r\n");
    SerialUSB.print("pin=50, comparator threshold=2.2 V, battery=CR2032, purpose=RTC backup power\r\n");

    SerialUSB.print("raw=");
    SerialUSB.print(static_cast<int>(raw));
    SerialUSB.print("\r\n");

    SerialUSB.print("debounced_raw=");
    SerialUSB.print(static_cast<int>(g_debounced_raw));
    SerialUSB.print("\r\n");

    SerialUSB.print("instant_state=");
    SerialUSB.print(instant_ok ? "ok" : "low");
    SerialUSB.print("\r\n");

    SerialUSB.print("state=");
    SerialUSB.print(debounced_ok ? "ok" : "low");
    SerialUSB.print("\r\n");

    if (debounced_ok != 0U)
    {
        SerialUSB.print("message=backup battery voltage is above 2.2 V\r\n");
    }
    else
    {
        SerialUSB.print("message=replace CR2032 battery\r\n");
    }
}
