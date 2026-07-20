/**
 * @file diagnostic_console.cpp
 * @brief USB service console базовой прошивки Lorentz.
 */

#include "diagnostic_console.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../../platform/platform.hpp"
#include "../../board/lcp_field_ports.hpp"
#include "../../board/lcp_sc16is.hpp"
#include "../app.hpp"
#include "../version.hpp"
#include "../x2x/x2x_service.hpp"
#include "battery_status.hpp"
#include "ethernet_status.hpp"
#include "field_status.hpp"
#include "rs485_echo_test.hpp"
#include "rtc_status.hpp"
#include "sd_card_test.hpp"
#include "watchdog_status.hpp"
#include "x2x_status.hpp"

namespace
{
static const uint16_t DIAGNOSTIC_COMMAND_BUFFER_SIZE = 64U;
static const uint32_t DIAGNOSTIC_PERIODIC_REPORT_MS = 10000U;

char g_command_buffer[DIAGNOSTIC_COMMAND_BUFFER_SIZE];
uint16_t g_command_length = 0U;
uint8_t g_usb_was_open = 0U;
uint8_t g_periodic_report_enabled = 0U;
uint32_t g_last_periodic_report_ms = 0U;

char ascii_to_lower(char value)
{
    if ((value >= 'A') && (value <= 'Z'))
    {
        return static_cast<char>(value + ('a' - 'A'));
    }

    return value;
}

void command_to_lower(char* command)
{
    for (uint16_t index = 0U; command[index] != '\0'; ++index)
    {
        command[index] = ascii_to_lower(command[index]);
    }
}

void trim_command(char* command)
{
    uint16_t start = 0U;

    while ((command[start] == ' ') || (command[start] == '\t'))
    {
        ++start;
    }

    if (start != 0U)
    {
        uint16_t destination = 0U;

        while (command[start] != '\0')
        {
            command[destination++] = command[start++];
        }

        command[destination] = '\0';
    }

    uint16_t length = 0U;

    while (command[length] != '\0')
    {
        ++length;
    }

    while ((length > 0U) &&
           ((command[length - 1U] == ' ') ||
            (command[length - 1U] == '\t')))
    {
        command[--length] = '\0';
    }
}

uint8_t command_equals(const char* command, const char* expected)
{
    return (strcmp(command, expected) == 0) ? 1U : 0U;
}

const char* frame_text(HalUartFrame frame)
{
    switch (frame)
    {
        case HAL_UART_FRAME_8O1:
            return "8O1";
        case HAL_UART_FRAME_8E1:
            return "8E1";
        case HAL_UART_FRAME_8N1:
        default:
            return "8N1";
    }
}

void print_version(void)
{
    SerialUSB.print(LCP_DIAGNOSTIC_SOFTWARE_NAME);
    SerialUSB.print("\r\nversion=");
    SerialUSB.print(LCP_DIAGNOSTIC_SOFTWARE_VERSION);
    SerialUSB.print("\r\nstage=");
    SerialUSB.print(LCP_DIAGNOSTIC_SOFTWARE_STAGE);
    SerialUSB.print("\r\ntarget=");
    SerialUSB.print(LCP_DIAGNOSTIC_SOFTWARE_TARGET);
    SerialUSB.print("\r\n");
}

void print_help(void)
{
    SerialUSB.print("\r\n");
    print_version();
    SerialUSB.print("\r\nLCP diagnostic console commands:\r\n");
    SerialUSB.print("help          - print software version and command list\r\n");
    SerialUSB.print("version       - print software version only\r\n");
    SerialUSB.print("status        - print full diagnostic status\r\n");
    SerialUSB.print("rtos          - print RAM, FreeRTOS heap and LCP task stack\r\n");
    SerialUSB.print("x2x           - print X2X configuration and module status\r\n");
    SerialUSB.print("x2x reload    - validate and apply X2X.TXT\r\n");
    SerialUSB.print("x2x pause     - stop X2X polling\r\n");
    SerialUSB.print("x2x resume    - resume X2X polling\r\n");
    SerialUSB.print("x2x ldo S V   - set LDO slave S output register to V\r\n");
    SerialUSB.print("field         - print FieldSensor status for S1..S4\r\n");
    SerialUSB.print("field reload  - reload BAUD.TXT and PARITY.TXT\r\n");
    SerialUSB.print("field pause   - stop starting new FieldSensor requests\r\n");
    SerialUSB.print("field resume  - resume FieldSensor polling\r\n");
    SerialUSB.print("eth           - print dual Modbus TCP server status\r\n");
    SerialUSB.print("eth reload    - reload Ethernet TXT files and reset W5500\r\n");
    SerialUSB.print("sc16is        - print SC16IS probe report\r\n");
    SerialUSB.print("rs485         - print built-in RS-485 status\r\n");
    SerialUSB.print("sd            - print microSD status\r\n");
    SerialUSB.print("sd test       - write and read SDTEST.TXT\r\n");
    SerialUSB.print("battery       - print CR2032 backup battery status\r\n");
    SerialUSB.print("rtc           - print local RTC status\r\n");
    SerialUSB.print("rtc set YYYY-MM-DD HH:MM:SS - set local RTC date/time\r\n");
    SerialUSB.print("watchdog      - print watchdog status\r\n");
    SerialUSB.print("watchdog feed - restart watchdog counter manually\r\n");
    SerialUSB.print("watchdog test reset - stop feed and check watchdog reset\r\n");
    SerialUSB.print("uptime        - print uptime_ms\r\n");
    SerialUSB.print("periodic on   - enable full status every 10000 ms\r\n");
    SerialUSB.print("periodic off  - disable periodic status\r\n\r\n");
}

void print_rs485_field_port(LcpFieldPortId port_id,
                            const char* hardware_name,
                            uint32_t error_count)
{
    const LcpFieldPortConfig& config = lcp_field_port_config(port_id);
    SerialUSB.print(lcp_field_port_name(port_id));
    SerialUSB.print(": ");
    SerialUSB.print(hardware_name);
    SerialUSB.print(", ");
    SerialUSB.print(static_cast<unsigned long>(config.baudrate));
    SerialUSB.print(" ");
    SerialUSB.print(frame_text(config.frame));
    SerialUSB.print(", FieldSensor Modbus RTU master, error_count=");
    SerialUSB.print(static_cast<unsigned long>(error_count));
    SerialUSB.print("\r\n");
}

void print_rs485_status(void)
{
    SerialUSB.print("RS-485 built-in ports:\r\n");
    SerialUSB.print("X2X: Serial, 9600 8N1, mode=");
    SerialUSB.print((x2x_service_owns_port() != 0U) ?
                    "Modbus RTU master" : "echo test");
    SerialUSB.print(", echo=");
    SerialUSB.print((rs485_echo_test_x2x_enabled() != 0U) ?
                    "enabled" : "disabled");
    SerialUSB.print(", error_count=");
    SerialUSB.print(static_cast<int>(Serial.errorCount()));
    SerialUSB.print("\r\n");

    print_rs485_field_port(LCP_FIELD_PORT_S2,
                           "Serial1",
                           static_cast<uint32_t>(Serial1.errorCount()));
    print_rs485_field_port(LCP_FIELD_PORT_S3,
                           "Serial3",
                           static_cast<uint32_t>(Serial3.errorCount()));
    print_rs485_field_port(LCP_FIELD_PORT_S4,
                           "Serial2",
                           static_cast<uint32_t>(Serial2.errorCount()));
}

void print_uptime(void)
{
    SerialUSB.print("uptime_ms=");
    SerialUSB.print(static_cast<unsigned long>(millis()));
    SerialUSB.print("\r\n");
}

void print_rtos_summary(void)
{
    const uint32_t stack_free_words = app_rtos_lcp_stack_free_words();
    SerialUSB.print("FreeRTOS:\r\nscheduler=");
    SerialUSB.print((app_rtos_scheduler_running() != 0U) ?
                    "running" : "not_running");
    SerialUSB.print("\r\ntick_count=");
    SerialUSB.print(static_cast<unsigned long>(app_rtos_tick_count()));
    SerialUSB.print("\r\nfree_heap_bytes=");
    SerialUSB.print(static_cast<unsigned long>(app_rtos_free_heap_bytes()));
    SerialUSB.print("\r\nminimum_ever_free_heap_bytes=");
    SerialUSB.print(static_cast<unsigned long>(
        app_rtos_minimum_ever_free_heap_bytes()));
    SerialUSB.print("\r\nlcp_stack_free_words=");
    SerialUSB.print(static_cast<unsigned long>(stack_free_words));
    SerialUSB.print("\r\nlcp_stack_free_bytes=");
    SerialUSB.print(static_cast<unsigned long>(stack_free_words * 4U));
    SerialUSB.print("\r\n");
}

void print_rtos_status(void)
{
    const uint32_t heap_total_bytes = app_rtos_heap_total_bytes();
    const uint32_t heap_free_bytes = app_rtos_free_heap_bytes();
    const uint32_t heap_minimum_free_bytes =
        app_rtos_minimum_ever_free_heap_bytes();
    const uint32_t heap_used_bytes =
        (heap_total_bytes >= heap_free_bytes) ?
        (heap_total_bytes - heap_free_bytes) : 0U;

    const uint32_t stack_total_words = app_rtos_lcp_stack_total_words();
    const uint32_t stack_total_bytes = app_rtos_lcp_stack_total_bytes();
    const uint32_t stack_free_words = app_rtos_lcp_stack_free_words();
    const uint32_t stack_free_bytes = stack_free_words * 4U;
    const uint32_t stack_peak_used_words =
        (stack_total_words >= stack_free_words) ?
        (stack_total_words - stack_free_words) : 0U;
    const uint32_t stack_peak_used_bytes =
        (stack_total_bytes >= stack_free_bytes) ?
        (stack_total_bytes - stack_free_bytes) : 0U;

    SerialUSB.print("\r\n=== RTOS memory status ===\r\n");
    SerialUSB.print("scheduler=");
    SerialUSB.print((app_rtos_scheduler_running() != 0U) ?
                    "running" : "not_running");
    SerialUSB.print("\r\ntick_count=");
    SerialUSB.print(static_cast<unsigned long>(app_rtos_tick_count()));
    SerialUSB.print("\r\nallocation_model=dynamic, allocator=heap_4, task_create=xTaskCreate\r\n");

    SerialUSB.print("\r\nRAM:\r\ntotal_bytes=");
    SerialUSB.print(static_cast<unsigned long>(app_ram_total_bytes()));
    SerialUSB.print("\r\nstatic_data_bytes=");
    SerialUSB.print(static_cast<unsigned long>(app_ram_static_data_bytes()));
    SerialUSB.print("\r\nstatic_bss_bytes=");
    SerialUSB.print(static_cast<unsigned long>(app_ram_static_bss_bytes()));
    SerialUSB.print("\r\nstartup_stack_bytes=");
    SerialUSB.print(static_cast<unsigned long>(app_ram_startup_stack_bytes()));
    SerialUSB.print("\r\nc_heap_reserved_bytes=");
    SerialUSB.print(static_cast<unsigned long>(app_ram_c_heap_reserved_bytes()));
    SerialUSB.print("\r\nlinker_unassigned_bytes=");
    SerialUSB.print(static_cast<unsigned long>(
        app_ram_linker_unassigned_bytes()));

    SerialUSB.print("\r\n\r\nFreeRTOS heap:\r\ntotal_bytes=");
    SerialUSB.print(static_cast<unsigned long>(heap_total_bytes));
    SerialUSB.print("\r\nfree_bytes=");
    SerialUSB.print(static_cast<unsigned long>(heap_free_bytes));
    SerialUSB.print("\r\nminimum_ever_free_bytes=");
    SerialUSB.print(static_cast<unsigned long>(heap_minimum_free_bytes));
    SerialUSB.print("\r\nused_bytes=");
    SerialUSB.print(static_cast<unsigned long>(heap_used_bytes));

    SerialUSB.print("\r\n\r\nLCP task stack:\r\ntotal_bytes=");
    SerialUSB.print(static_cast<unsigned long>(stack_total_bytes));
    SerialUSB.print("\r\npeak_used_bytes=");
    SerialUSB.print(static_cast<unsigned long>(stack_peak_used_bytes));
    SerialUSB.print("\r\nminimum_free_bytes=");
    SerialUSB.print(static_cast<unsigned long>(stack_free_bytes));
    SerialUSB.print("\r\ntotal_words=");
    SerialUSB.print(static_cast<unsigned long>(stack_total_words));
    SerialUSB.print("\r\npeak_used_words=");
    SerialUSB.print(static_cast<unsigned long>(stack_peak_used_words));
    SerialUSB.print("\r\nminimum_free_words=");
    SerialUSB.print(static_cast<unsigned long>(stack_free_words));
    SerialUSB.print("\r\n=== end ===\r\n\r\n");
}

void print_status(void)
{
    SerialUSB.print("\r\n=== LCP diagnostic status ===\r\n");
    print_version();
    print_uptime();
    print_rtos_summary();
    SerialUSB.print("USB service CDC: open\r\n");
    print_rs485_status();
    x2x_status_print_report();
    field_status_print_report();
    lcp_sc16is_print_probe_report();
    ethernet_status_print_report();
    sd_card_test_print_report();
    battery_status_print_report();
    rtc_status_print_report();
    watchdog_status_print_report();
    SerialUSB.print("=== end ===\r\n\r\n");
}

void execute_command(char* command)
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
    else if (field_status_handle_command(command) != 0U)
    {
    }
    else if (x2x_status_handle_command(command) != 0U)
    {
    }
    else if (ethernet_status_handle_command(command) != 0U)
    {
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
        SerialUSB.print("\r\ntype help\r\n");
    }
}

void process_rx_byte(uint8_t value)
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
        g_command_buffer[g_command_length++] = static_cast<char>(value);
    }
    else
    {
        g_command_length = 0U;
        SerialUSB.print("command buffer overflow\r\n");
    }
}

void poll_usb_open_state(void)
{
    const uint8_t usb_open = SerialUSB ? 1U : 0U;

    if ((usb_open != 0U) && (g_usb_was_open == 0U))
    {
        SerialUSB.print("\r\nLCP diagnostic firmware ready. Type help.\r\n");
        g_command_length = 0U;
    }

    if (usb_open == 0U)
    {
        g_command_length = 0U;
    }

    g_usb_was_open = usb_open;
}

void poll_periodic_report(void)
{
    if (g_periodic_report_enabled == 0U)
    {
        return;
    }

    const uint32_t now_ms = millis();

    if ((uint32_t)(now_ms - g_last_periodic_report_ms) >=
        DIAGNOSTIC_PERIODIC_REPORT_MS)
    {
        g_last_periodic_report_ms = now_ms;
        print_status();
    }
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
