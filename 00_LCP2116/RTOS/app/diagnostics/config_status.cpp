/**
 * @file config_status.cpp
 * @brief Команды, status и канонический TXT-вывод конфигурации LCP2116.
 */

#include "config_status.hpp"
#include "diagnostic_output.hpp"

#include "../config/lcp_config_service.hpp"
#include "../../hal/sam3x_internal_flash.hpp"
#include "../../platform/platform.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace
{
const char HEX_DIGITS[] = "0123456789ABCDEF";

uint8_t text_equals(const char* left, const char* right)
{
    return ((left != 0) && (right != 0) &&
            (strcmp(left, right) == 0)) ? 1U : 0U;
}

void print_hex32(uint32_t value)
{
    SerialUSB.print("0x");

    for (int8_t shift = 28; shift >= 0; shift -= 4)
    {
        const uint8_t nibble = static_cast<uint8_t>((value >> shift) & 0x0FU);
        SerialUSB.print(HEX_DIGITS[nibble]);
    }
}

void print_bool(uint8_t value)
{
    SerialUSB.print((value != 0U) ? "yes" : "no");
}

void print_file_header(const char* file_name)
{
    SerialUSB.print("===== ");
    SerialUSB.print(file_name);
    SerialUSB.print(" =====\r\n");
}

void print_fin(void)
{
    SerialUSB.print("fin\r\n\r\n");
}

void print_baud(const LcpConfigBundle& bundle)
{
    print_file_header("baud.TXT");
    SerialUSB.print(static_cast<unsigned long>(LCP_FIELD_PORT_COUNT));
    SerialUSB.print("\r\n");

    for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
    {
        SerialUSB.print(static_cast<unsigned long>(bundle.field_baudrate[index]));
        SerialUSB.print("\r\n");
    }

    print_fin();
}

void print_parity(const LcpConfigBundle& bundle)
{
    print_file_header("Parity.TXT");
    SerialUSB.print(static_cast<unsigned long>(LCP_FIELD_PORT_COUNT));
    SerialUSB.print("\r\n");

    for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
    {
        SerialUSB.print(static_cast<unsigned long>(bundle.field_parity[index]));
        SerialUSB.print("\r\n");
    }

    print_fin();
}

void print_octet_file(const char* file_name,
                      const uint8_t* values,
                      uint8_t count)
{
    print_file_header(file_name);
    SerialUSB.print(static_cast<unsigned long>(count));
    SerialUSB.print("\r\n");

    for (uint8_t index = 0U; index < count; ++index)
    {
        SerialUSB.print(static_cast<unsigned long>(values[index]));
        SerialUSB.print("\r\n");
    }

    print_fin();
}

void print_x2x(const LcpConfigBundle& bundle)
{
    print_file_header("X2X.TXT");
    SerialUSB.print(static_cast<unsigned long>(bundle.x2x_module_count));
    SerialUSB.print("\r\n");

    for (uint8_t index = 0U; index < bundle.x2x_module_count; ++index)
    {
        SerialUSB.print(static_cast<unsigned long>(bundle.x2x_module_id[index]));
        SerialUSB.print("\r\n");
    }

    print_fin();
}

void print_eia(const LcpConfigBundle& bundle)
{
    print_file_header("EIA.TXT");
    SerialUSB.print(static_cast<unsigned long>(bundle.eia_count));
    SerialUSB.print("\r\n");

    for (uint8_t index = 0U; index < bundle.eia_count; ++index)
    {
        SerialUSB.print(static_cast<long>(bundle.eia_value[index]));
        SerialUSB.print("\r\n");
    }

    print_fin();
}

void print_all_files(const LcpConfigBundle& bundle)
{
    print_baud(bundle);
    print_parity(bundle);

    print_octet_file("MAC.txt", bundle.ethernet_mac[0], 6U);
    print_octet_file("IP.txt", bundle.ethernet_ip[0], 4U);
    print_octet_file("SUBNET.txt", bundle.ethernet_subnet[0], 4U);
    print_octet_file("GATE.txt", bundle.ethernet_gateway[0], 4U);

    print_octet_file("MAC2.txt", bundle.ethernet_mac[1], 6U);
    print_octet_file("IP2.txt", bundle.ethernet_ip[1], 4U);
    print_octet_file("SUBNET2.txt", bundle.ethernet_subnet[1], 4U);
    print_octet_file("GATE2.txt", bundle.ethernet_gateway[1], 4U);

    print_x2x(bundle);
    print_eia(bundle);
}

uint8_t print_named_file(const LcpConfigBundle& bundle, const char* name)
{
    if (text_equals(name, "baud.txt") != 0U)
    {
        print_baud(bundle);
    }
    else if (text_equals(name, "parity.txt") != 0U)
    {
        print_parity(bundle);
    }
    else if (text_equals(name, "mac.txt") != 0U)
    {
        print_octet_file("MAC.txt", bundle.ethernet_mac[0], 6U);
    }
    else if (text_equals(name, "ip.txt") != 0U)
    {
        print_octet_file("IP.txt", bundle.ethernet_ip[0], 4U);
    }
    else if (text_equals(name, "subnet.txt") != 0U)
    {
        print_octet_file("SUBNET.txt", bundle.ethernet_subnet[0], 4U);
    }
    else if (text_equals(name, "gate.txt") != 0U)
    {
        print_octet_file("GATE.txt", bundle.ethernet_gateway[0], 4U);
    }
    else if (text_equals(name, "mac2.txt") != 0U)
    {
        print_octet_file("MAC2.txt", bundle.ethernet_mac[1], 6U);
    }
    else if (text_equals(name, "ip2.txt") != 0U)
    {
        print_octet_file("IP2.txt", bundle.ethernet_ip[1], 4U);
    }
    else if (text_equals(name, "subnet2.txt") != 0U)
    {
        print_octet_file("SUBNET2.txt", bundle.ethernet_subnet[1], 4U);
    }
    else if (text_equals(name, "gate2.txt") != 0U)
    {
        print_octet_file("GATE2.txt", bundle.ethernet_gateway[1], 4U);
    }
    else if (text_equals(name, "x2x.txt") != 0U)
    {
        print_x2x(bundle);
    }
    else if (text_equals(name, "eia.txt") != 0U)
    {
        print_eia(bundle);
    }
    else
    {
        return 0U;
    }

    return 1U;
}

void print_slot(const char* name, LcpConfigSlotId slot_id)
{
    const LcpConfigSlotStatus& status =
        lcp_config_service_slot_status(slot_id);

    diagnostic_print_group(name);
    SerialUSB.print("valid = ");
    print_bool(status.valid);
    SerialUSB.print(", sequence = ");
    SerialUSB.print(static_cast<unsigned long>(status.sequence));
    SerialUSB.print("\r\npayload_crc32 = ");
    print_hex32(status.payload_crc32);
    SerialUSB.print(", flash = ");
    SerialUSB.print(sam3x_internal_flash_result_text(status.flash_result));
    SerialUSB.print("\r\n");
}

uint8_t section_changed(const void* left,
                        const void* right,
                        size_t length)
{
    return (memcmp(left, right, length) != 0) ? 1U : 0U;
}

void print_changed_name(const char* name, uint8_t* any)
{
    SerialUSB.print((*any != 0U) ? ", " : "");
    SerialUSB.print(name);
    *any = 1U;
}

void print_candidate_difference(void)
{
    const LcpConfigBundle* candidate = lcp_config_service_candidate();

    if (candidate == 0)
    {
        return;
    }

    const LcpConfigBundle& active = lcp_config_service_active();
    uint8_t any = 0U;

    SerialUSB.print("changed_files = ");

    if (section_changed(active.field_baudrate,
                        candidate->field_baudrate,
                        sizeof(active.field_baudrate)) != 0U)
    {
        print_changed_name("baud.TXT", &any);
    }

    if (section_changed(active.field_parity,
                        candidate->field_parity,
                        sizeof(active.field_parity)) != 0U)
    {
        print_changed_name("Parity.TXT", &any);
    }

    static const char* names[LCP_ETHERNET_COUNT][4] =
    {
        { "MAC.txt", "IP.txt", "SUBNET.txt", "GATE.txt" },
        { "MAC2.txt", "IP2.txt", "SUBNET2.txt", "GATE2.txt" }
    };

    for (uint8_t index = 0U; index < LCP_ETHERNET_COUNT; ++index)
    {
        if (section_changed(active.ethernet_mac[index],
                            candidate->ethernet_mac[index], 6U) != 0U)
        {
            print_changed_name(names[index][0], &any);
        }

        if (section_changed(active.ethernet_ip[index],
                            candidate->ethernet_ip[index], 4U) != 0U)
        {
            print_changed_name(names[index][1], &any);
        }

        if (section_changed(active.ethernet_subnet[index],
                            candidate->ethernet_subnet[index], 4U) != 0U)
        {
            print_changed_name(names[index][2], &any);
        }

        if (section_changed(active.ethernet_gateway[index],
                            candidate->ethernet_gateway[index], 4U) != 0U)
        {
            print_changed_name(names[index][3], &any);
        }
    }

    if ((active.x2x_module_count != candidate->x2x_module_count) ||
        (section_changed(active.x2x_module_id,
                         candidate->x2x_module_id,
                         sizeof(active.x2x_module_id)) != 0U))
    {
        print_changed_name("X2X.TXT", &any);
    }

    if ((active.eia_count != candidate->eia_count) ||
        (section_changed(active.eia_value,
                         candidate->eia_value,
                         sizeof(active.eia_value)) != 0U))
    {
        print_changed_name("EIA.TXT", &any);
    }

    if (any == 0U)
    {
        SerialUSB.print("none");
    }

    SerialUSB.print("\r\n");
}

void print_operation_start(LcpConfigServiceResult result)
{
    SerialUSB.print("config operation = ");
    SerialUSB.print(lcp_config_service_result_text(result));
    SerialUSB.print("\r\n");

    if (result == LCP_CONFIG_SERVICE_IN_PROGRESS)
    {
        SerialUSB.print("run 'config status' to check completion\r\n");
    }
}

void print_default_bundle(void)
{
    LcpConfigBundle defaults;
    lcp_config_bundle_set_defaults(defaults);
    diagnostic_print_banner("DEFAULT CONFIGURATION");
    print_all_files(defaults);
}
}

void config_status_print_report(void)
{
    diagnostic_print_section("CONFIGURATION STORAGE");

    diagnostic_print_group("Active configuration");
    SerialUSB.print("source = ");
    SerialUSB.print(lcp_config_source_text(lcp_config_service_source()));
    SerialUSB.print(", slot = ");
    SerialUSB.print(lcp_config_slot_text(lcp_config_service_active_slot()));
    SerialUSB.print("\r\nsequence = ");
    SerialUSB.print(static_cast<unsigned long>(
        lcp_config_service_active_sequence()));
    SerialUSB.print(", generation = ");
    SerialUSB.print(static_cast<unsigned long>(
        lcp_config_service_generation()));
    SerialUSB.print("\r\npayload_crc32 = ");
    print_hex32(lcp_config_service_active_crc32());
    SerialUSB.print("\r\nflash_start = ");
    print_hex32(static_cast<uint32_t>(
        sam3x_internal_flash_config_start()));
    SerialUSB.print(", flash_bytes = ");
    SerialUSB.print(static_cast<unsigned long>(
        sam3x_internal_flash_config_size()));
    SerialUSB.print("\r\n");

    print_slot("Slot A", LCP_CONFIG_SLOT_A);
    print_slot("Slot B", LCP_CONFIG_SLOT_B);

    diagnostic_print_group("Operation");
    SerialUSB.print("busy = ");
    print_bool(lcp_config_service_busy());
    SerialUSB.print(", operation = ");
    SerialUSB.print(lcp_config_operation_text(
        lcp_config_service_operation()));
    SerialUSB.print("\r\nresult = ");
    SerialUSB.print(lcp_config_service_result_text(
        lcp_config_service_result()));
    SerialUSB.print(", flash_result = ");
    SerialUSB.print(sam3x_internal_flash_result_text(
        lcp_config_service_last_flash_result()));
    SerialUSB.print("\r\n");

    const LcpConfigCandidateStatus& candidate =
        lcp_config_service_candidate_status();
    diagnostic_print_group("Last SD candidate");
    SerialUSB.print("available = ");
    print_bool(candidate.available);
    SerialUSB.print(", stable = ");
    print_bool(candidate.stable);
    SerialUSB.print(", equal_to_active = ");
    print_bool(candidate.equal_to_active);
    SerialUSB.print("\r\ncrc32 = ");
    print_hex32(candidate.crc32);
    SerialUSB.print(", result = ");
    SerialUSB.print(lcp_config_sd_result_text(candidate.report.result));
    SerialUSB.print("\r\n");

    if (candidate.report.failed_file != 0)
    {
        SerialUSB.print("failed_file = ");
        SerialUSB.print(candidate.report.failed_file);
        SerialUSB.print(", file_result = ");
        SerialUSB.print(sd_config_result_text(candidate.report.sd_result));
        SerialUSB.print(", x2x_result = ");
        SerialUSB.print(x2x_config_result_text(candidate.report.x2x_result));
        SerialUSB.print("\r\n");
    }

    if ((candidate.available != 0U) && (candidate.stable != 0U))
    {
        print_candidate_difference();
    }
}

uint8_t config_status_handle_command(const char* command)
{
    if ((command == 0) ||
        (strncmp(command, "config", 6U) != 0))
    {
        return 0U;
    }

    if ((text_equals(command, "config") != 0U) ||
        (text_equals(command, "config status") != 0U))
    {
        diagnostic_print_banner("CONFIGURATION STATUS");
        config_status_print_report();
        return 1U;
    }

    if ((text_equals(command, "config show") != 0U) ||
        (text_equals(command, "config show all") != 0U))
    {
        diagnostic_print_banner("ACTIVE CONFIGURATION");
        print_all_files(lcp_config_service_active());
        return 1U;
    }

    static const char SHOW_PREFIX[] = "config show ";

    if (strncmp(command, SHOW_PREFIX, sizeof(SHOW_PREFIX) - 1U) == 0)
    {
        diagnostic_print_banner("ACTIVE CONFIGURATION FILE");

        if (print_named_file(lcp_config_service_active(),
                             command + sizeof(SHOW_PREFIX) - 1U) == 0U)
        {
            SerialUSB.print("unknown configuration file\r\n");
        }

        return 1U;
    }

    if (text_equals(command, "config defaults") != 0U)
    {
        print_default_bundle();
        return 1U;
    }

    if (text_equals(command, "config scan sd") != 0U)
    {
        print_operation_start(lcp_config_service_request_scan_sd());
        return 1U;
    }

    if (text_equals(command, "config compare sd") != 0U)
    {
        print_operation_start(lcp_config_service_request_compare_sd());
        return 1U;
    }

    if (text_equals(command, "config import sd") != 0U)
    {
        print_operation_start(lcp_config_service_request_import_sd());
        return 1U;
    }

    if (text_equals(command, "config rollback") != 0U)
    {
        print_operation_start(lcp_config_service_request_rollback());
        return 1U;
    }

    if (text_equals(command, "config erase confirm") != 0U)
    {
        print_operation_start(lcp_config_service_request_erase());
        return 1U;
    }

    if (text_equals(command, "config erase") != 0U)
    {
        SerialUSB.print("configuration erase not started\r\n");
        SerialUSB.print("use 'config erase confirm' to invalidate both slots\r\n");
        return 1U;
    }

    SerialUSB.print("unknown config command; type 'help'\r\n");
    return 1U;
}
