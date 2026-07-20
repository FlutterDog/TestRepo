/**
 * @file ethernet_status.cpp
 * @brief Реализация диагностики Modbus TCP на ETH1 и ETH2.
 */

#include "ethernet_status.hpp"

#include "../ethernet/ethernet_modbus_service.hpp"
#include "../../board/lcp_ethernet.hpp"
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

void print_config_item(const char* label,
                       const char* file_name,
                       SdConfigResult result)
{
    SerialUSB.print(label);
    SerialUSB.print("=");
    SerialUSB.print((file_name != 0) ? file_name : "");
    SerialUSB.print("(");
    SerialUSB.print(sd_config_result_text(result));
    SerialUSB.print(")");
}

void print_interface(LcpEthernetId ethernet_id)
{
    const EthernetModbusInterfaceState& state =
        ethernet_modbus_service_interface(ethernet_id);

    SerialUSB.print(lcp_ethernet_name(ethernet_id));
    SerialUSB.print(": initialized=");
    SerialUSB.print((state.initialized != 0U) ? "yes" : "no");
    SerialUSB.print(", init=");
    SerialUSB.print((state.init_ok != 0U) ? "ok" : "failed");
    SerialUSB.print(", link=");
    SerialUSB.print((state.init_ok != 0U) &&
                    (w5500_lite_link_up(ethernet_id) != 0U) ?
                    "up" : "down");
    SerialUSB.print(", VERSIONR=");
    print_hex8(w5500_lite_version(ethernet_id));
    SerialUSB.print(", socket_status=");
    print_hex8(state.last_socket_status);
    SerialUSB.print(", port=");
    SerialUSB.print(static_cast<int>(LCP_MODBUS_TCP_PORT));
    SerialUSB.print("\r\n");

    SerialUSB.print("  ip=");
    print_ip(state.config.ip);
    SerialUSB.print(", subnet=");
    print_ip(state.config.subnet);
    SerialUSB.print(", gateway=");
    print_ip(state.config.gateway);
    SerialUSB.print("\r\n");

    SerialUSB.print("  config: ");
    print_config_item("ip",
                      state.config_report.ip_file,
                      state.config_report.ip_result);
    SerialUSB.print(", ");
    print_config_item("subnet",
                      state.config_report.subnet_file,
                      state.config_report.subnet_result);
    SerialUSB.print(", ");
    print_config_item("gateway",
                      state.config_report.gateway_file,
                      state.config_report.gateway_result);
    SerialUSB.print(", any_loaded_from_sd=");
    SerialUSB.print((state.config_report.any_loaded_from_sd != 0U) ?
                    "yes" : "no");
    SerialUSB.print("\r\n");

    SerialUSB.print("  requests=");
    SerialUSB.print(static_cast<unsigned long>(state.server.request_count));
    SerialUSB.print(", responses=");
    SerialUSB.print(static_cast<unsigned long>(state.server.response_count));
    SerialUSB.print(", exceptions=");
    SerialUSB.print(static_cast<unsigned long>(state.server.exception_count));
    SerialUSB.print(", malformed=");
    SerialUSB.print(static_cast<unsigned long>(state.server.malformed_count));
    SerialUSB.print(", transport_errors=");
    SerialUSB.print(static_cast<unsigned long>(state.transport_error_count));
    SerialUSB.print("\r\n");
}
}

void ethernet_status_print_report(void)
{
    SerialUSB.print("Ethernet Modbus TCP status\r\n");
    SerialUSB.print("reload=");
    SerialUSB.print((ethernet_modbus_service_reload_pending() != 0U) ?
                    "pending" : "idle");
    SerialUSB.print(", holding_registers=0..11, FC03 read-only\r\n");
    print_interface(LCP_ETHERNET_1);
    print_interface(LCP_ETHERNET_2);

    SerialUSB.print("register_map: ");

    for (uint16_t address = 0U;
         address < LCP_MODBUS_TCP_HOLDING_COUNT;
         ++address)
    {
        if (address != 0U)
        {
            SerialUSB.print(",");
        }

        SerialUSB.print(static_cast<unsigned long>(
            ethernet_modbus_service_holding(address)));
    }

    SerialUSB.print("\r\n");
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
        return 1U;
    }

    return 0U;
}
