/**
 * @file ethernet_modbus_service.cpp
 * @brief Реализация Modbus TCP server на ETH1 и ETH2.
 */

#include "ethernet_modbus_service.hpp"

#include "../field/field_sensor_service.hpp"
#include "../../board/lcp_ethernet.hpp"
#include "../../hal/w5500_lite.hpp"
#include "../../libs/lcp_sd_storage/lcp_sd_storage.hpp"
#include "../../platform/platform.hpp"

#include <string.h>

namespace
{
static const uint32_t ETHERNET_CONFIG_WAIT_MS = 2000U;
static const uint8_t W5500_SOCKET_ESTABLISHED = 0x17U;
static const uint8_t MODBUS_EXCEPTION_ILLEGAL_ADDRESS = 0x02U;
static const uint16_t RX_CHUNK_CAPACITY = 128U;
static const uint8_t MAX_RESPONSES_PER_POLL = 2U;

EthernetModbusInterfaceState g_interfaces[LCP_ETHERNET_COUNT];
uint16_t g_holding[LCP_MODBUS_TCP_HOLDING_COUNT];
uint8_t g_rx_chunk[LCP_ETHERNET_COUNT][RX_CHUNK_CAPACITY];
uint8_t g_reload_pending = 1U;
uint32_t g_start_ms = 0U;

LcpEthernetId normalize_ethernet_id(LcpEthernetId ethernet_id)
{
    return (ethernet_id < LCP_ETHERNET_COUNT) ?
        ethernet_id : LCP_ETHERNET_1;
}

void reset_config_report(EthernetNetworkConfigReport& report,
                         LcpEthernetId ethernet_id)
{
    report.mac_result = SD_CONFIG_CARD_NOT_READY;
    report.ip_result = SD_CONFIG_CARD_NOT_READY;
    report.subnet_result = SD_CONFIG_CARD_NOT_READY;
    report.gateway_result = SD_CONFIG_CARD_NOT_READY;

    if (ethernet_id == LCP_ETHERNET_2)
    {
        report.mac_file = "MAC2.txt";
        report.ip_file = "IP2.txt";
        report.subnet_file = "SUBNET2.txt";
        report.gateway_file = "GATE2.txt";
    }
    else
    {
        report.mac_file = "MAC.txt";
        report.ip_file = "IP.txt";
        report.subnet_file = "SUBNET.txt";
        report.gateway_file = "GATE.txt";
    }

    report.any_loaded_from_sd = 0U;
}

void update_holding_map(void)
{
    memset(g_holding, 0, sizeof(g_holding));

    for (uint8_t port_index = 0U;
         port_index < field_sensor_service_port_count();
         ++port_index)
    {
        const FieldSensorPortState& port = field_sensor_service_port(
            static_cast<LcpFieldPortId>(port_index));
        const uint16_t base = static_cast<uint16_t>(port_index * 3U);
        uint16_t quality = 0U;

        g_holding[base] = port.registers[0];
        g_holding[base + 1U] = port.registers[1];

        if (port.valid != 0U)
        {
            quality |= 0x0001U;
        }

        if (port.connection_lost != 0U)
        {
            quality |= 0x0002U;
        }

        if (port.port_present != 0U)
        {
            quality |= 0x0004U;
        }

        if (port.request_active != 0U)
        {
            quality |= 0x0008U;
        }

        if (field_sensor_service_paused() != 0U)
        {
            quality |= 0x0010U;
        }

        quality |= static_cast<uint16_t>(
            static_cast<uint16_t>(port.last_result) << 8U);
        g_holding[base + 2U] = quality;
    }
}

uint8_t read_holding(void*,
                     uint16_t start_address,
                     uint16_t register_count,
                     uint16_t* output)
{
    if ((output == 0) ||
        (start_address >= LCP_MODBUS_TCP_HOLDING_COUNT) ||
        (register_count >
         static_cast<uint16_t>(LCP_MODBUS_TCP_HOLDING_COUNT - start_address)))
    {
        return MODBUS_EXCEPTION_ILLEGAL_ADDRESS;
    }

    for (uint16_t index = 0U; index < register_count; ++index)
    {
        output[index] = g_holding[start_address + index];
    }

    return 0U;
}

void apply_configuration(void)
{
    W5500NetworkConfig configs[LCP_ETHERNET_COUNT];
    EthernetNetworkConfigReport reports[LCP_ETHERNET_COUNT];

    ethernet_network_config_set_defaults(configs);

    for (uint8_t index = 0U; index < LCP_ETHERNET_COUNT; ++index)
    {
        reset_config_report(reports[index],
                            static_cast<LcpEthernetId>(index));
    }

    if (lcp_sd_storage_ready() != 0U)
    {
        ethernet_network_config_load(configs, reports);
    }

    lcp_ethernet_init_pins();

    for (uint8_t index = 0U; index < LCP_ETHERNET_COUNT; ++index)
    {
        EthernetModbusInterfaceState& interface_state = g_interfaces[index];
        interface_state.config = configs[index];
        interface_state.config_report = reports[index];
        modbus_tcp_server_init(interface_state.server);
        interface_state.transport_error_count = 0U;
        interface_state.initialized = 1U;
        interface_state.init_ok = w5500_lite_begin(
            static_cast<LcpEthernetId>(index),
            interface_state.config);
        interface_state.last_socket_status = 0U;

        if (interface_state.init_ok != 0U)
        {
            w5500_lite_tcp_server_begin(static_cast<LcpEthernetId>(index),
                                        LCP_MODBUS_TCP_PORT);
        }
    }

    g_reload_pending = 0U;
}

void poll_interface(LcpEthernetId ethernet_id)
{
    EthernetModbusInterfaceState& interface_state =
        g_interfaces[ethernet_id];

    if ((interface_state.initialized == 0U) ||
        (interface_state.init_ok == 0U))
    {
        return;
    }

    const uint16_t received = w5500_lite_tcp_server_receive(
        ethernet_id,
        LCP_MODBUS_TCP_PORT,
        g_rx_chunk[ethernet_id],
        RX_CHUNK_CAPACITY);

    interface_state.last_socket_status =
        w5500_lite_tcp_server_status(ethernet_id);

    if (interface_state.last_socket_status != W5500_SOCKET_ESTABLISHED)
    {
        interface_state.server.rx_length = 0U;
        interface_state.server.tx_length = 0U;
    }

    if (received != 0U)
    {
        if (modbus_tcp_server_append(interface_state.server,
                                     g_rx_chunk[ethernet_id],
                                     received) == 0U)
        {
            ++interface_state.transport_error_count;
        }
    }

    for (uint8_t response_index = 0U;
         response_index < MAX_RESPONSES_PER_POLL;
         ++response_index)
    {
        if (modbus_tcp_server_process(interface_state.server,
                                      read_holding,
                                      0) == 0U)
        {
            break;
        }

        const uint16_t response_length =
            modbus_tcp_server_response_length(interface_state.server);
        const uint16_t sent = w5500_lite_tcp_server_send(
            ethernet_id,
            modbus_tcp_server_response(interface_state.server),
            response_length);

        if (sent != response_length)
        {
            ++interface_state.transport_error_count;
        }

        modbus_tcp_server_response_sent(interface_state.server);
    }
}
}

void ethernet_modbus_service_init(void)
{
    W5500NetworkConfig defaults[LCP_ETHERNET_COUNT];

    memset(g_interfaces, 0, sizeof(g_interfaces));
    memset(g_holding, 0, sizeof(g_holding));
    ethernet_network_config_set_defaults(defaults);

    for (uint8_t index = 0U; index < LCP_ETHERNET_COUNT; ++index)
    {
        g_interfaces[index].config = defaults[index];
        reset_config_report(g_interfaces[index].config_report,
                            static_cast<LcpEthernetId>(index));
        modbus_tcp_server_init(g_interfaces[index].server);
    }

    g_reload_pending = 1U;
    g_start_ms = millis();
}

void ethernet_modbus_service_poll(void)
{
    if (g_reload_pending != 0U)
    {
        if ((lcp_sd_storage_ready() != 0U) ||
            ((uint32_t)(millis() - g_start_ms) >= ETHERNET_CONFIG_WAIT_MS))
        {
            apply_configuration();
        }
        else
        {
            return;
        }
    }

    update_holding_map();
    poll_interface(LCP_ETHERNET_1);
    poll_interface(LCP_ETHERNET_2);
}

void ethernet_modbus_service_request_reload(void)
{
    g_reload_pending = 1U;
    g_start_ms = millis();
}

uint8_t ethernet_modbus_service_reload_pending(void)
{
    return g_reload_pending;
}

const EthernetModbusInterfaceState& ethernet_modbus_service_interface(
    LcpEthernetId ethernet_id)
{
    return g_interfaces[normalize_ethernet_id(ethernet_id)];
}

uint16_t ethernet_modbus_service_holding(uint16_t address)
{
    return (address < LCP_MODBUS_TCP_HOLDING_COUNT) ?
        g_holding[address] : 0U;
}
