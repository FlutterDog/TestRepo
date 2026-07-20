/**
 * @file ethernet_status.cpp
 * @brief Реализация диагностики Modbus TCP на ETH1 и ETH2.
 *
 * Аппаратное состояние W5500 и прикладная holding map выводятся раздельно.
 * При расширении карты меняйте `ethernet_modbus_service.cpp`, её Doxygen и
 * print_register_map(). W5500 HAL от состава прикладных регистров не зависит.
 */

#include "ethernet_status.hpp"
#include "diagnostic_output.hpp"

#include "../ethernet/ethernet_modbus_service.hpp"
#include "../../board/lcp_ethernet.hpp"
#include "../../board/lcp_field_ports.hpp"
#include "../../hal/w5500_lite.hpp"
#include "../../platform/platform.hpp"

#include <string.h>

namespace
{
void print_hex8(uint8_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    char text[5];
    text[0] = '0';
    text[1] = 'x';
    text[2] = hex[(value >> 4U) & 0x0FU];
    text[3] = hex[value & 0x0FU];
    text[4] = '\0';
    SerialUSB.print(text);
}

void print_hex16(uint16_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    char text[7];
    text[0] = '0';
    text[1] = 'x';
    text[2] = hex[(value >> 12U) & 0x0FU];
    text[3] = hex[(value >> 8U) & 0x0FU];
    text[4] = hex[(value >> 4U) & 0x0FU];
    text[5] = hex[value & 0x0FU];
    text[6] = '\0';
    SerialUSB.print(text);
}

void print_hex_byte(uint8_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    char text[3];
    text[0] = hex[(value >> 4U) & 0x0FU];
    text[1] = hex[value & 0x0FU];
    text[2] = '\0';
    SerialUSB.print(text);
}

void print_ip(const W5500IpAddress& ip)
{
    SerialUSB.print(static_cast<int>(ip.octet[0]));
    SerialUSB.print(".");
    SerialUSB.print(static_cast<int>(ip.octet[1]));
    SerialUSB.print(".");
    SerialUSB.print(static_cast<int>(ip.octet[2]));
    SerialUSB.print(".");
    SerialUSB.print(static_cast<int>(ip.octet[3]));
}

void print_mac(const W5500MacAddress& mac)
{
    for (uint8_t index = 0U; index < 6U; ++index)
    {
        if (index != 0U)
        {
            SerialUSB.print(":");
        }

        print_hex_byte(mac.octet[index]);
    }
}

void print_config_item(const char* label,
                       const char* file_name,
                       SdConfigResult result)
{
    SerialUSB.print("  ");
    SerialUSB.print(label);
    SerialUSB.print(": ");
    diagnostic_print_assignment("file");
    SerialUSB.print((file_name != 0) ? file_name : "");
    SerialUSB.print(", ");
    diagnostic_print_assignment("result");
    SerialUSB.print(sd_config_result_text(result));
    SerialUSB.print("\r\n");
}

void print_interface(LcpEthernetId ethernet_id)
{
    const EthernetModbusInterfaceState& state =
        ethernet_modbus_service_interface(ethernet_id);
    const uint8_t initialized = state.initialized;
    const uint8_t version = (initialized != 0U) ?
        w5500_lite_version(ethernet_id) : 0U;
    const uint8_t link_up = ((initialized != 0U) &&
                             (state.init_ok != 0U)) ?
        w5500_lite_link_up(ethernet_id) : 0U;

    diagnostic_print_group(lcp_ethernet_name(ethernet_id));

    diagnostic_print_assignment("initialized");
    SerialUSB.print((initialized != 0U) ? "yes" : "no");
    SerialUSB.print(", ");
    diagnostic_print_assignment("init");
    SerialUSB.print((state.init_ok != 0U) ? "ok" : "failed");
    SerialUSB.print(", ");
    diagnostic_print_assignment("link");
    SerialUSB.print((link_up != 0U) ? "up" : "down");
    SerialUSB.print("\r\n");

    diagnostic_print_assignment("VERSIONR");
    print_hex8(version);
    SerialUSB.print(", ");
    diagnostic_print_assignment("socket_status");
    print_hex8(state.last_socket_status);
    SerialUSB.print(", ");
    diagnostic_print_assignment("tcp_port");
    SerialUSB.print(static_cast<int>(LCP_MODBUS_TCP_PORT));
    SerialUSB.print("\r\n");

    diagnostic_print_assignment("mac");
    print_mac(state.config.mac);
    SerialUSB.print(", ");
    diagnostic_print_assignment("ip");
    print_ip(state.config.ip);
    SerialUSB.print("\r\n");

    diagnostic_print_assignment("subnet");
    print_ip(state.config.subnet);
    SerialUSB.print(", ");
    diagnostic_print_assignment("gateway");
    print_ip(state.config.gateway);
    SerialUSB.print("\r\n");

    SerialUSB.print("configuration_files:\r\n");
    print_config_item("MAC",
                      state.config_report.mac_file,
                      state.config_report.mac_result);
    print_config_item("IP",
                      state.config_report.ip_file,
                      state.config_report.ip_result);
    print_config_item("SUBNET",
                      state.config_report.subnet_file,
                      state.config_report.subnet_result);
    print_config_item("GATEWAY",
                      state.config_report.gateway_file,
                      state.config_report.gateway_result);
    SerialUSB.print("  ");
    diagnostic_print_assignment("any_loaded_from_sd");
    SerialUSB.print((state.config_report.any_loaded_from_sd != 0U) ?
                    "yes" : "no");
    SerialUSB.print("\r\n");

    diagnostic_print_assignment("requests");
    SerialUSB.print(static_cast<unsigned long>(state.server.request_count));
    SerialUSB.print(", ");
    diagnostic_print_assignment("responses");
    SerialUSB.print(static_cast<unsigned long>(state.server.response_count));
    SerialUSB.print("\r\n");

    diagnostic_print_assignment("exceptions");
    SerialUSB.print(static_cast<unsigned long>(state.server.exception_count));
    SerialUSB.print(", ");
    diagnostic_print_assignment("malformed");
    SerialUSB.print(static_cast<unsigned long>(state.server.malformed_count));
    SerialUSB.print(", ");
    diagnostic_print_assignment("transport_errors");
    SerialUSB.print(static_cast<unsigned long>(state.transport_error_count));
    SerialUSB.print("\r\n");
}

void print_quality_flags(uint16_t quality)
{
    diagnostic_print_assignment("valid");
    SerialUSB.print((quality & FIELD_SENSOR_TCP_QUALITY_VALID) ? "yes" : "no");
    SerialUSB.print(", ");
    diagnostic_print_assignment("lost");
    SerialUSB.print((quality & FIELD_SENSOR_TCP_QUALITY_CONNECTION_LOST) ?
                    "yes" : "no");
    SerialUSB.print(", ");
    diagnostic_print_assignment("present");
    SerialUSB.print((quality & FIELD_SENSOR_TCP_QUALITY_PORT_PRESENT) ?
                    "yes" : "no");
    SerialUSB.print("\r\n  ");
    diagnostic_print_assignment("request_active");
    SerialUSB.print((quality & FIELD_SENSOR_TCP_QUALITY_REQUEST_ACTIVE) ?
                    "yes" : "no");
    SerialUSB.print(", ");
    diagnostic_print_assignment("paused");
    SerialUSB.print((quality & FIELD_SENSOR_TCP_QUALITY_SERVICE_PAUSED) ?
                    "yes" : "no");
    SerialUSB.print(", ");
    diagnostic_print_assignment("last_rtu_result");
    SerialUSB.print(static_cast<unsigned long>((quality >> 8U) & 0x00FFU));
}

void print_register_map(void)
{
    diagnostic_print_group("Published holding registers");
    diagnostic_print_assignment("layout");
    SerialUSB.print("value0, value1, quality\r\n");
    diagnostic_print_assignment("registers_per_FieldSensor_port");
    SerialUSB.print("3\r\n");

    for (uint8_t port_index = 0U;
         port_index < LCP_FIELD_PORT_COUNT;
         ++port_index)
    {
        const uint16_t base = static_cast<uint16_t>(
            port_index * LCP_MODBUS_TCP_REGISTERS_PER_FIELD_PORT);
        const uint16_t value0 = ethernet_modbus_service_holding(base);
        const uint16_t value1 = ethernet_modbus_service_holding(base + 1U);
        const uint16_t quality = ethernet_modbus_service_holding(base + 2U);

        SerialUSB.print("S");
        SerialUSB.print(static_cast<int>(port_index + 1U));
        SerialUSB.print(": ");
        diagnostic_print_assignment("address");
        SerialUSB.print(static_cast<unsigned long>(base));
        SerialUSB.print("..");
        SerialUSB.print(static_cast<unsigned long>(base + 2U));
        SerialUSB.print(", ");
        diagnostic_print_assignment("quality");
        print_hex16(quality);
        SerialUSB.print("\r\n  ");
        diagnostic_print_assignment("value0");
        SerialUSB.print(static_cast<unsigned long>(value0));
        SerialUSB.print(", ");
        diagnostic_print_assignment("value1");
        SerialUSB.print(static_cast<unsigned long>(value1));
        SerialUSB.print("\r\n  ");
        print_quality_flags(quality);
        SerialUSB.print("\r\n");
    }
}
}

void ethernet_status_print_report(void)
{
    diagnostic_print_section("ETHERNET MODBUS TCP");
    diagnostic_print_assignment("service_reload");
    SerialUSB.print((ethernet_modbus_service_reload_pending() != 0U) ?
                    "pending" : "idle");
    SerialUSB.print("\r\n");
    diagnostic_print_assignment("function");
    SerialUSB.print("0x03 read-only\r\n");
    diagnostic_print_assignment("holding_address");
    SerialUSB.print("0..11\r\n");

    print_interface(LCP_ETHERNET_1);
    print_interface(LCP_ETHERNET_2);
    print_register_map();
}

uint8_t ethernet_status_handle_command(const char* command)
{
    if (command == 0)
    {
        return 0U;
    }

    if ((strcmp(command, "eth") == 0) ||
        (strcmp(command, "eth status") == 0))
    {
        ethernet_status_print_report();
        return 1U;
    }

    if (strcmp(command, "eth reload") == 0)
    {
        ethernet_modbus_service_request_reload();
        SerialUSB.print("Ethernet configuration reload: pending\r\n");
        SerialUSB.print("both W5500 interfaces will be reset\r\n");
        return 1U;
    }

    return 0U;
}
