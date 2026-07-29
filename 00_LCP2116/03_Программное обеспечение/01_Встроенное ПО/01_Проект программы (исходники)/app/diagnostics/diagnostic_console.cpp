/**
 * @file diagnostic_console.cpp
 * @brief USB service console базовой прошивки Lorentz.
 *
 * Команды выполняются в контексте единственной прикладной LCP task. Длительные
 * действия только запускаются обработчиком и завершаются в профильных poll().
 */

#include "diagnostic_console.hpp"
#include "diagnostic_output.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../../board/lcp_field_ports.hpp"
#include "../../board/lcp_sc16is.hpp"
#include "../../platform/platform.hpp"
#include "../app.hpp"
#include "../version.hpp"
#include "../x2x/x2x_service.hpp"
#include "battery_status.hpp"
#include "config_status.hpp"
#include "ethernet_status.hpp"
#include "field_status.hpp"
#include "rs485_echo_test.hpp"
#include "rtc_status.hpp"
#include "sd_card_test.hpp"
#include "watchdog_status.hpp"
#include "x2x_status.hpp"

namespace
{
constexpr uint16_t DIAGNOSTIC_COMMAND_BUFFER_SIZE = 64U;
constexpr uint32_t DIAGNOSTIC_PERIODIC_REPORT_MS = 10000U;

struct DiagnosticHelpEntry
{
    const char* command;
    const char* description;
};

const DiagnosticHelpEntry g_core_help[] =
{
    { "help | ?", "Show the grouped command list." },
    { "version | ver", "Show firmware version and target." },
    { "status", "Show the complete diagnostic report." },
    { "uptime", "Show raw and human-readable uptime." },
    { "rtos", "Show SRAM, heap and task-stack usage." }
};

const DiagnosticHelpEntry g_config_help[] =
{
    { "config status", "Show active source, A/B slots and last SD candidate." },
    { "config show [FILE|all]", "Export active configuration as canonical TXT." },
    { "config scan sd", "Read and validate the complete SD set twice." },
    { "config compare sd", "Scan SD and list files different from active Flash." },
    { "config import sd", "Validate and commit the complete SD set to Flash." },
    { "config rollback", "Commit the previous valid slot as a new generation." },
    { "config erase confirm", "Invalidate both internal configuration slots." },
    { "config defaults", "Show built-in defaults without changing active data." }
};

const DiagnosticHelpEntry g_x2x_help[] =
{
    { "x2x", "Show X2X configuration and module status." },
    { "x2x reload", "Reapply active internal X2X configuration." },
    { "x2x pause", "Finish the active cycle and pause polling." },
    { "x2x resume", "Resume X2X polling." },
    { "x2x ldo SLAVE VALUE", "Set the LDO1118 output register." }
};

const DiagnosticHelpEntry g_field_help[] =
{
    { "field", "Show FieldSensor status for S1..S4." },
    { "field reload", "Reapply active internal serial configuration." },
    { "field pause", "Stop starting new FieldSensor requests." },
    { "field resume", "Resume FieldSensor polling." }
};

const DiagnosticHelpEntry g_ethernet_help[] =
{
    { "eth", "Show ETH1/ETH2 Modbus TCP status and registers." },
    { "eth reload", "Reapply active network configuration and reset W5500." }
};

const DiagnosticHelpEntry g_hardware_help[] =
{
    { "rs485", "Show built-in RS-485 ownership and errors." },
    { "sc16is", "Show external UART detection and assignment." },
    { "sd", "Show microSD and filesystem status." },
    { "sd test", "Write and read SDTEST.TXT." },
    { "battery", "Show CR2032 backup-battery status." },
    { "time | rtc time", "Show only the current RTC date and time." },
    { "rtc", "Show the complete local RTC report." },
    { "rtc set YYYY-MM-DD HH:MM:SS", "Start a nonblocking RTC update." },
    { "watchdog", "Show watchdog configuration and reset cause." },
    { "watchdog feed", "Restart the watchdog counter manually." },
    { "watchdog test reset", "Stop feed and test hardware reset." }
};

const DiagnosticHelpEntry g_periodic_help[] =
{
    { "periodic on", "Print full status every 10 seconds." },
    { "periodic off", "Disable periodic full status." }
};

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
    if (command == 0)
    {
        return;
    }

    for (uint16_t index = 0U; command[index] != '\0'; ++index)
    {
        command[index] = ascii_to_lower(command[index]);
    }
}

void trim_command(char* command)
{
    if (command == 0)
    {
        return;
    }

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
    return ((command != 0) && (expected != 0) &&
            (strcmp(command, expected) == 0)) ? 1U : 0U;
}

void print_version_lines(void)
{
    SerialUSB.print("name = ");
    SerialUSB.print(LCP_DIAGNOSTIC_SOFTWARE_NAME);
    SerialUSB.print("\r\nversion = ");
    SerialUSB.print(LCP_DIAGNOSTIC_SOFTWARE_VERSION);
    SerialUSB.print("\r\nstage = ");
    SerialUSB.print(LCP_DIAGNOSTIC_SOFTWARE_STAGE);
    SerialUSB.print("\r\ntarget = ");
    SerialUSB.print(LCP_DIAGNOSTIC_SOFTWARE_TARGET);
    SerialUSB.print("\r\n");
}

void print_version_report(void)
{
    diagnostic_print_banner("FIRMWARE VERSION");
    print_version_lines();
}

void print_help_group(const char* title,
                      const DiagnosticHelpEntry* entries,
                      size_t entry_count)
{
    diagnostic_print_group(title);

    for (size_t index = 0U; index < entry_count; ++index)
    {
        SerialUSB.print("  ");
        SerialUSB.print(entries[index].command);
        SerialUSB.print("\r\n    ");
        SerialUSB.print(entries[index].description);
        SerialUSB.print("\r\n");
    }
}

void print_help(void)
{
    diagnostic_print_banner("LCP DIAGNOSTIC CONSOLE");
    print_version_lines();
    print_help_group("Core", g_core_help,
                     sizeof(g_core_help) / sizeof(g_core_help[0]));
    print_help_group("Configuration storage", g_config_help,
                     sizeof(g_config_help) / sizeof(g_config_help[0]));
    print_help_group("X2X modules", g_x2x_help,
                     sizeof(g_x2x_help) / sizeof(g_x2x_help[0]));
    print_help_group("FieldSensor S1..S4", g_field_help,
                     sizeof(g_field_help) / sizeof(g_field_help[0]));
    print_help_group("Ethernet Modbus TCP", g_ethernet_help,
                     sizeof(g_ethernet_help) / sizeof(g_ethernet_help[0]));
    print_help_group("Hardware and service", g_hardware_help,
                     sizeof(g_hardware_help) / sizeof(g_hardware_help[0]));
    print_help_group("Periodic report", g_periodic_help,
                     sizeof(g_periodic_help) / sizeof(g_periodic_help[0]));
    SerialUSB.print("\r\nCommands are case-insensitive. Press Enter to execute.\r\n\r\n");
}

void print_uptime_lines(void)
{
    const uint32_t uptime_ms = millis();
    SerialUSB.print("uptime_ms = ");
    SerialUSB.print(static_cast<unsigned long>(uptime_ms));
    SerialUSB.print("\r\nuptime_human = ");
    diagnostic_print_duration(uptime_ms);
    SerialUSB.print("\r\n");
}

void print_uptime_report(void)
{
    diagnostic_print_banner("UPTIME");
    print_uptime_lines();
}

void print_rs485_field_port(LcpFieldPortId port_id,
                            const char* hardware_name,
                            uint32_t error_count)
{
    const LcpFieldPortConfig& config = lcp_field_port_config(port_id);

    SerialUSB.print(lcp_field_port_name(port_id));
    SerialUSB.print(": hardware = ");
    SerialUSB.print(hardware_name);
    SerialUSB.print(", serial = ");
    SerialUSB.print(static_cast<unsigned long>(config.baudrate));
    SerialUSB.print(" ");
    SerialUSB.print(diagnostic_uart_frame_text(config.frame));
    SerialUSB.print("\r\n  owner = FieldSensor master, uart_errors = ");
    SerialUSB.print(static_cast<unsigned long>(error_count));
    SerialUSB.print("\r\n");
}

void print_rs485_status(void)
{
    diagnostic_print_section("RS-485 PORTS");

    diagnostic_print_group("X2X physical port");
    SerialUSB.print("hardware = Serial, serial = 9600 8N1\r\nowner = ");
    SerialUSB.print((x2x_service_owns_port() != 0U) ?
                    "X2X master" : "echo test");
    SerialUSB.print(", echo = ");
    SerialUSB.print((rs485_echo_test_x2x_enabled() != 0U) ?
                    "enabled" : "disabled");
    SerialUSB.print(", uart_errors = ");
    SerialUSB.print(static_cast<unsigned long>(Serial.errorCount()));
    SerialUSB.print("\r\n");

    diagnostic_print_group("FieldSensor physical ports");
    print_rs485_field_port(LCP_FIELD_PORT_S2, "Serial1",
                           static_cast<uint32_t>(Serial1.errorCount()));
    print_rs485_field_port(LCP_FIELD_PORT_S3, "Serial3",
                           static_cast<uint32_t>(Serial3.errorCount()));
    print_rs485_field_port(LCP_FIELD_PORT_S4, "Serial2",
                           static_cast<uint32_t>(Serial2.errorCount()));
}

void print_rtos_summary(void)
{
    const uint32_t heap_total = app_rtos_heap_total_bytes();
    const uint32_t heap_free = app_rtos_free_heap_bytes();
    const uint32_t heap_used = (heap_total >= heap_free) ?
        (heap_total - heap_free) : 0U;
    const uint32_t stack_total = app_rtos_lcp_stack_total_bytes();
    const uint32_t stack_total_words = app_rtos_lcp_stack_total_words();
    const uint32_t stack_free_words = app_rtos_lcp_stack_free_words();
    const uint32_t stack_word_bytes = (stack_total_words != 0U) ?
        (stack_total / stack_total_words) : 0U;
    const uint32_t stack_free = stack_free_words * stack_word_bytes;
    const uint32_t stack_peak_used = (stack_total >= stack_free) ?
        (stack_total - stack_free) : 0U;

    diagnostic_print_section("RTOS SUMMARY");
    SerialUSB.print("scheduler = ");
    SerialUSB.print((app_rtos_scheduler_running() != 0U) ?
                    "running" : "not running");
    SerialUSB.print(", tick_count = ");
    SerialUSB.print(static_cast<unsigned long>(app_rtos_tick_count()));
    SerialUSB.print("\r\nheap_used = ");
    SerialUSB.print(static_cast<unsigned long>(heap_used));
    SerialUSB.print("/");
    SerialUSB.print(static_cast<unsigned long>(heap_total));
    SerialUSB.print(" bytes (");
    diagnostic_print_percent(heap_used, heap_total);
    SerialUSB.print(")\r\ntask_stack_peak_used = ");
    SerialUSB.print(static_cast<unsigned long>(stack_peak_used));
    SerialUSB.print("/");
    SerialUSB.print(static_cast<unsigned long>(stack_total));
    SerialUSB.print(" bytes (");
    diagnostic_print_percent(stack_peak_used, stack_total);
    SerialUSB.print(")\r\n");
}

void print_rtos_status(void)
{
    const uint32_t ram_total = app_ram_total_bytes();
    const uint32_t ram_unassigned = app_ram_linker_unassigned_bytes();
    const uint32_t ram_assigned = (ram_total >= ram_unassigned) ?
        (ram_total - ram_unassigned) : 0U;
    const uint32_t heap_total = app_rtos_heap_total_bytes();
    const uint32_t heap_free = app_rtos_free_heap_bytes();
    const uint32_t heap_minimum_free =
        app_rtos_minimum_ever_free_heap_bytes();
    const uint32_t heap_used = (heap_total >= heap_free) ?
        (heap_total - heap_free) : 0U;
    const uint32_t heap_peak_used = (heap_total >= heap_minimum_free) ?
        (heap_total - heap_minimum_free) : 0U;
    const uint32_t stack_total_words = app_rtos_lcp_stack_total_words();
    const uint32_t stack_total_bytes = app_rtos_lcp_stack_total_bytes();
    const uint32_t stack_free_words = app_rtos_lcp_stack_free_words();
    const uint32_t stack_word_bytes = (stack_total_words != 0U) ?
        (stack_total_bytes / stack_total_words) : 0U;
    const uint32_t stack_free_bytes = stack_free_words * stack_word_bytes;
    const uint32_t stack_peak_used_words =
        (stack_total_words >= stack_free_words) ?
        (stack_total_words - stack_free_words) : 0U;
    const uint32_t stack_peak_used_bytes =
        (stack_total_bytes >= stack_free_bytes) ?
        (stack_total_bytes - stack_free_bytes) : 0U;

    diagnostic_print_banner("RTOS MEMORY STATUS");
    diagnostic_print_group("Runtime");
    SerialUSB.print("scheduler = ");
    SerialUSB.print((app_rtos_scheduler_running() != 0U) ?
                    "running" : "not running");
    SerialUSB.print("\r\ntick_count = ");
    SerialUSB.print(static_cast<unsigned long>(app_rtos_tick_count()));
    SerialUSB.print("\r\nallocator = heap_4, task_create = xTaskCreate\r\n");
    print_uptime_lines();

    diagnostic_print_group("ATSAM3X8E SRAM");
    SerialUSB.print("total_bytes = ");
    SerialUSB.print(static_cast<unsigned long>(ram_total));
    SerialUSB.print("\r\nassigned_by_linker_bytes = ");
    SerialUSB.print(static_cast<unsigned long>(ram_assigned));
    SerialUSB.print(" (");
    diagnostic_print_percent(ram_assigned, ram_total);
    SerialUSB.print(")\r\nlinker_unassigned_bytes = ");
    SerialUSB.print(static_cast<unsigned long>(ram_unassigned));
    SerialUSB.print(" (");
    diagnostic_print_percent(ram_unassigned, ram_total);
    SerialUSB.print(")\r\nstatic_data_bytes = ");
    SerialUSB.print(static_cast<unsigned long>(app_ram_static_data_bytes()));
    SerialUSB.print("\r\nstatic_bss_bytes = ");
    SerialUSB.print(static_cast<unsigned long>(app_ram_static_bss_bytes()));
    SerialUSB.print("\r\nstartup_stack_bytes = ");
    SerialUSB.print(static_cast<unsigned long>(app_ram_startup_stack_bytes()));
    SerialUSB.print("\r\nc_heap_reserved_bytes = ");
    SerialUSB.print(static_cast<unsigned long>(app_ram_c_heap_reserved_bytes()));
    SerialUSB.print("\r\nnote = FreeRTOS heap is already included in .bss\r\n");

    diagnostic_print_group("FreeRTOS heap_4");
    SerialUSB.print("total_bytes = ");
    SerialUSB.print(static_cast<unsigned long>(heap_total));
    SerialUSB.print("\r\nused_bytes = ");
    SerialUSB.print(static_cast<unsigned long>(heap_used));
    SerialUSB.print(" (");
    diagnostic_print_percent(heap_used, heap_total);
    SerialUSB.print(")\r\nfree_bytes = ");
    SerialUSB.print(static_cast<unsigned long>(heap_free));
    SerialUSB.print(" (");
    diagnostic_print_percent(heap_free, heap_total);
    SerialUSB.print(")\r\npeak_used_bytes = ");
    SerialUSB.print(static_cast<unsigned long>(heap_peak_used));
    SerialUSB.print(" (");
    diagnostic_print_percent(heap_peak_used, heap_total);
    SerialUSB.print(")\r\nminimum_ever_free_bytes = ");
    SerialUSB.print(static_cast<unsigned long>(heap_minimum_free));
    SerialUSB.print(" (");
    diagnostic_print_percent(heap_minimum_free, heap_total);
    SerialUSB.print(")\r\n");

    diagnostic_print_group("LCP task stack");
    SerialUSB.print("total_bytes = ");
    SerialUSB.print(static_cast<unsigned long>(stack_total_bytes));
    SerialUSB.print("\r\npeak_used_bytes = ");
    SerialUSB.print(static_cast<unsigned long>(stack_peak_used_bytes));
    SerialUSB.print(" (");
    diagnostic_print_percent(stack_peak_used_bytes, stack_total_bytes);
    SerialUSB.print(")\r\nminimum_free_bytes = ");
    SerialUSB.print(static_cast<unsigned long>(stack_free_bytes));
    SerialUSB.print(" (");
    diagnostic_print_percent(stack_free_bytes, stack_total_bytes);
    SerialUSB.print(")\r\ntotal_words = ");
    SerialUSB.print(static_cast<unsigned long>(stack_total_words));
    SerialUSB.print("\r\npeak_used_words = ");
    SerialUSB.print(static_cast<unsigned long>(stack_peak_used_words));
    SerialUSB.print("\r\nminimum_free_words = ");
    SerialUSB.print(static_cast<unsigned long>(stack_free_words));
    SerialUSB.print("\r\n");
}

void print_status(void)
{
    diagnostic_print_banner("LCP DIAGNOSTIC STATUS");

    diagnostic_print_section("SYSTEM");
    print_version_lines();
    print_uptime_lines();
    SerialUSB.print("usb_service_cdc = open\r\nperiodic_report = ");
    SerialUSB.print((g_periodic_report_enabled != 0U) ?
                    "enabled" : "disabled");
    SerialUSB.print("\r\n");

    print_rtos_summary();
    config_status_print_report();
    print_rs485_status();
    x2x_status_print_report();
    field_status_print_report();
    lcp_sc16is_print_probe_report();
    ethernet_status_print_report();
    sd_card_test_print_report();
    battery_status_print_report();
    rtc_status_print_report();
    watchdog_status_print_report();

    diagnostic_print_banner("END OF STATUS");
}

uint8_t execute_local_command(const char* command)
{
    if ((command_equals(command, "help") != 0U) ||
        (command_equals(command, "?") != 0U))
    {
        print_help();
        return 1U;
    }

    if ((command_equals(command, "version") != 0U) ||
        (command_equals(command, "ver") != 0U))
    {
        print_version_report();
        return 1U;
    }

    if (command_equals(command, "status") != 0U)
    {
        print_status();
        return 1U;
    }

    if (command_equals(command, "rtos") != 0U)
    {
        print_rtos_status();
        return 1U;
    }

    if (command_equals(command, "sc16is") != 0U)
    {
        lcp_sc16is_print_probe_report();
        return 1U;
    }

    if (command_equals(command, "rs485") != 0U)
    {
        print_rs485_status();
        return 1U;
    }

    if (command_equals(command, "sd") != 0U)
    {
        sd_card_test_print_report();
        return 1U;
    }

    if (command_equals(command, "sd test") != 0U)
    {
        sd_card_test_run_file_test();
        return 1U;
    }

    if (command_equals(command, "battery") != 0U)
    {
        battery_status_print_report();
        return 1U;
    }

    if (command_equals(command, "uptime") != 0U)
    {
        print_uptime_report();
        return 1U;
    }

    if (command_equals(command, "periodic on") != 0U)
    {
        g_periodic_report_enabled = 1U;
        g_last_periodic_report_ms = millis();
        SerialUSB.print("periodic status = enabled\r\ninterval_ms = ");
        SerialUSB.print(static_cast<unsigned long>(
            DIAGNOSTIC_PERIODIC_REPORT_MS));
        SerialUSB.print("\r\n");
        return 1U;
    }

    if (command_equals(command, "periodic off") != 0U)
    {
        g_periodic_report_enabled = 0U;
        SerialUSB.print("periodic status = disabled\r\n");
        return 1U;
    }

    return 0U;
}

void execute_command(char* command)
{
    trim_command(command);
    command_to_lower(command);

    if ((command == 0) || (command[0] == '\0'))
    {
        return;
    }

    if ((execute_local_command(command) != 0U) ||
        (config_status_handle_command(command) != 0U) ||
        (field_status_handle_command(command) != 0U) ||
        (x2x_status_handle_command(command) != 0U) ||
        (ethernet_status_handle_command(command) != 0U) ||
        (rtc_status_handle_command(command) != 0U) ||
        (watchdog_status_handle_command(command) != 0U))
    {
        return;
    }

    SerialUSB.print("unknown command: ");
    SerialUSB.print(command);
    SerialUSB.print("\r\ntype 'help' or '?' for the command list\r\n");
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
        SerialUSB.print("command rejected: input exceeds 63 characters\r\n");
    }
}

void poll_usb_open_state(void)
{
    const uint8_t usb_open = SerialUSB ? 1U : 0U;

    if ((usb_open != 0U) && (g_usb_was_open == 0U))
    {
        SerialUSB.print("\r\nLCP firmware ready. Type 'help' or '?'.\r\n");
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
