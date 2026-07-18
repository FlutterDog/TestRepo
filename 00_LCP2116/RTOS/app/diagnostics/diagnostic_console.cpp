/**
 * @file diagnostic_console.cpp
 * @brief Реализация USB service console базовой диагностической прошивки LCP.
 */

#include "diagnostic_console.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../../platform/platform.hpp"
#include "../app.hpp"
#include "../version.hpp"
#include "../../board/lcp_sc16is.hpp"
#include "ethernet_echo_test.hpp"
#include "sd_card_test.hpp"
#include "battery_status.hpp"
#include "rtc_status.hpp"
#include "watchdog_status.hpp"

static const uint16_t DIAGNOSTIC_COMMAND_BUFFER_SIZE = 64U;
static const uint32_t DIAGNOSTIC_PERIODIC_REPORT_MS = 10000U;

static char g_command_buffer[DIAGNOSTIC_COMMAND_BUFFER_SIZE];
static uint16_t g_command_length = 0U;
static uint8_t g_usb_was_open = 0U;
static uint8_t g_periodic_report_enabled = 0U;
static uint32_t g_last_periodic_report_ms = 0U;

static char ascii_to_lower(char value)
{
    if ((value >= 'A') && (value <= 'Z'))
    {
        return static_cast<char>(value + ('a' - 'A'));
    }

    return value;
}

static void command_to_lower(char* command)
{
    for (uint16_t index = 0U; command[index] != '\0'; ++index)
    {
        command[index] = ascii_to_lower(command[index]);
    }
}

static void trim_command(char* command)
{
    uint16_t start = 0U;

    while ((command[start] == ' ') || (command[start] == '\t'))
    {
        ++start;
    }

    if (start != 0U)
    {
        uint16_t dst = 0U;

        while (command[start] != '\0')
        {
            command[dst] = command[start];
            ++dst;
            ++start;
        }

        command[dst] = '\0';
    }

    uint16_t length = 0U;

    while (command[length] != '\0')
    {
        ++length;
    }

    while ((length > 0U) && ((command[length - 1U] == ' ') || (command[length - 1U] == '\t')))
    {
        command[length - 1U] = '\0';
        --length;
    }
}

static uint8_t command_equals(const char* command, const char* expected)
{
    return (strcmp(command, expected) == 0) ? 1U : 0U;
}

static void print_version(void)
{
    SerialUSB.print(LCP_DIAGNOSTIC_SOFTWARE_NAME);
    SerialUSB.print("\r\n");
    SerialUSB.print("version=");
    SerialUSB.print(LCP_DIAGNOSTIC_SOFTWARE_VERSION);
    SerialUSB.print("\r\n");
    SerialUSB.print("stage=");
    SerialUSB.print(LCP_DIAGNOSTIC_SOFTWARE_STAGE);
    SerialUSB.print("\r\n");
    SerialUSB.print("target=");
    SerialUSB.print(LCP_DIAGNOSTIC_SOFTWARE_TARGET);
    SerialUSB.print("\r\n");
}

static void print_help(void)
{
    SerialUSB.print("\r\n");
    print_version();
    SerialUSB.print("\r\n");
    SerialUSB.print("LCP diagnostic console commands:\r\n");
    SerialUSB.print("help         - print software version and command list\r\n");
    SerialUSB.print("version      - print software version only\r\n");
    SerialUSB.print("status       - print full diagnostic status\r\n");
    SerialUSB.print("rtos         - print FreeRTOS scheduler, heap and stack status\r\n");
    SerialUSB.print("eth          - print W5500 status\r\n");
    SerialUSB.print("sc16is       - print SC16IS probe report\r\n");
    SerialUSB.print("rs485        - print built-in RS-485 status\r\n");
    SerialUSB.print("sd           - print microSD status\r\n");
    SerialUSB.print("sd test      - write and read SDTEST.TXT\r\n");
    SerialUSB.print("battery      - print CR2032 backup battery status\r\n");
    SerialUSB.print("rtc          - print local RTC status\r\n");
    SerialUSB.print("rtc set YYYY-MM-DD HH:MM:SS - set local RTC date/time\r\n");
    SerialUSB.print("watchdog     - print watchdog status\r\n");
    SerialUSB.print("watchdog feed - restart watchdog counter manually\r\n");
    SerialUSB.print("watchdog test reset - stop feed and check watchdog reset\r\n");
    SerialUSB.print("uptime       - print uptime_ms\r\n");
    SerialUSB.print("periodic on  - enable full status every 10000 ms\r\n");
    SerialUSB.print("periodic off - disable periodic status\r\n");
    SerialUSB.print("\r\n");
}

static void print_rs485_status(void)
{
    SerialUSB.print("RS-485 built-in echo:\r\n");

    SerialUSB.print("X2X: Serial, 9600 8N1, error_count=");
    SerialUSB.print(static_cast<int>(Serial.errorCount()));
    SerialUSB.print("\r\n");

    SerialUSB.print("S2:  Serial1, 9600 8N1, error_count=");
    SerialUSB.print(static_cast<int>(Serial1.errorCount()));
    SerialUSB.print("\r\n");

    SerialUSB.print("S4:  Serial2, 9600 8N1, error_count=");
    SerialUSB.print(static_cast<int>(Serial2.errorCount()));
    SerialUSB.print("\r\n");

    SerialUSB.print("S3:  Serial3, 9600 8N1, error_count=");
    SerialUSB.print(static_cast<int>(Serial3.errorCount()));
    SerialUSB.print("\r\n");
}

static void print_uptime(void)
{
    SerialUSB.print("uptime_ms=");
    SerialUSB.print(static_cast<unsigned long>(millis()));
    SerialUSB.print("\r\n");
}

static void print_rtos_status(void)
{
    const uint32_t stack_free_words = app_rtos_lcp_stack_free_words();

    SerialUSB.print("FreeRTOS:\r\n");
    SerialUSB.print("scheduler=");
    SerialUSB.print((app_rtos_scheduler_running() != 0U) ? "running" : "not_running");
    SerialUSB.print("\r\n");
    SerialUSB.print("tick_count=");
    SerialUSB.print(static_cast<unsigned long>(app_rtos_tick_count()));
    SerialUSB.print("\r\n");
    SerialUSB.print("free_heap_bytes=");
    SerialUSB.print(static_cast<unsigned long>(app_rtos_free_heap_bytes()));
    SerialUSB.print("\r\n");
    SerialUSB.print("minimum_ever_free_heap_bytes=");
    SerialUSB.print(static_cast<unsigned long>(app_rtos_minimum_ever_free_heap_bytes()));
    SerialUSB.print("\r\n");
    SerialUSB.print("lcp_stack_free_words=");
    SerialUSB.print(static_cast<unsigned long>(stack_free_words));
    SerialUSB.print("\r\n");
    SerialUSB.print("lcp_stack_free_bytes=");
    SerialUSB.print(static_cast<unsigned long>(stack_free_words * 4U));
    SerialUSB.print("\r\n");
}

static void print_status(void)
{
    SerialUSB.print("\r\n");
    SerialUSB.print("=== LCP diagnostic status ===\r\n");
    print_version();
    print_uptime();
    print_rtos_status();
    SerialUSB.print("USB service CDC: open\r\n");
    print_rs485_status();
    lcp_sc16is_print_probe_report();
    ethernet_echo_test_print_report();
    sd_card_test_print_report();
    battery_status_print_report();
    rtc_status_print_report();
    watchdog_status_print_report();
    SerialUSB.print("=== end ===\r\n");
    SerialUSB.print("\r\n");
}

static void execute_command(char* command)
{
    trim_command(command);
    command_to_lower(command);

    if (command[0] == '\0')
    {
        return;
    }

    if (command_equals(command, "help") != 0U)
    {
        print_help();
    }
    else if (command_equals(command, "version") != 0U)
    {
        print_version();
    }
    else if (command_equals(command, "status") != 0U)
    {
        print_status();
    }
    else if (command_equals(command, "rtos") != 0U)
    {
        print_rtos_status();
    }
    else if (command_equals(command, "eth") != 0U)
    {
        ethernet_echo_test_print_report();
    }
    else if (command_equals(command, "sc16is") != 0U)
    {
        lcp_sc16is_print_probe_report();
    }
    else if (command_equals(command, "rs485") != 0U)
    {
        print_rs485_status();
    }
    else if (command_equals(command, "sd") != 0U)
    {
        sd_card_test_print_report();
    }
    else if (command_equals(command, "sd test") != 0U)
    {
        sd_card_test_run_file_test();
    }
    else if (command_equals(command, "battery") != 0U)
    {
        battery_status_print_report();
    }
    else if (rtc_status_handle_command(command) != 0U)
    {
    }
    else if (watchdog_status_handle_command(command) != 0U)
    {
    }
    else if (command_equals(command, "uptime") != 0U)
    {
        print_uptime();
    }
    else if (command_equals(command, "periodic on") != 0U)
    {
        g_periodic_report_enabled = 1U;
        g_last_periodic_report_ms = millis();
        SerialUSB.print("periodic status: on\r\n");
    }
    else if (command_equals(command, "periodic off") != 0U)
    {
        g_periodic_report_enabled = 0U;
        SerialUSB.print("periodic status: off\r\n");
    }
    else
    {
        SerialUSB.print("unknown command: ");
        SerialUSB.print(command);
        SerialUSB.print("\r\n");
        SerialUSB.print("type help\r\n");
    }
}

static void process_rx_byte(uint8_t value)
{
    if ((value == '\r') || (value == '\n'))
    {
        if (g_command_length != 0U)
        {
            g_command_buffer[g_command_length] = '\0';
            execute_command(g_command_buffer);
            g_command_length = 0U;
        }

        return;
    }

    if (g_command_length < (DIAGNOSTIC_COMMAND_BUFFER_SIZE - 1U))
    {
        g_command_buffer[g_command_length] = static_cast<char>(value);
        ++g_command_length;
    }
    else
    {
        g_command_length = 0U;
        SerialUSB.print("command buffer overflow\r\n");
    }
}

static void poll_usb_open_state(void)
{
    const uint8_t usb_open = SerialUSB ? 1U : 0U;

    if ((usb_open != 0U) && (g_usb_was_open == 0U))
    {
        SerialUSB.print("\r\n");
        SerialUSB.print("LCP diagnostic firmware ready. Type help.\r\n");
        g_command_length = 0U;
    }

    if (usb_open == 0U)
    {
        g_command_length = 0U;
    }

    g_usb_was_open = usb_open;
}

static void poll_periodic_report(void)
{
    if (g_periodic_report_enabled == 0U)
    {
        return;
    }

    const uint32_t now_ms = millis();

    if ((uint32_t)(now_ms - g_last_periodic_report_ms) >= DIAGNOSTIC_PERIODIC_REPORT_MS)
    {
        g_last_periodic_report_ms = now_ms;
        print_status();
    }
}

void diagnostic_console_init(void)
{
    g_command_length = 0U;
    g_usb_was_open = 0U;
    g_periodic_report_enabled = 0U;
    g_last_periodic_report_ms = 0U;
}

void diagnostic_console_poll(void)
{
    poll_usb_open_state();

    if (!SerialUSB)
    {
        return;
    }

    while (SerialUSB.available() > 0)
    {
        const int value = SerialUSB.read();

        if (value >= 0)
        {
            process_rx_byte(static_cast<uint8_t>(value));
        }
    }

    poll_periodic_report();
}
