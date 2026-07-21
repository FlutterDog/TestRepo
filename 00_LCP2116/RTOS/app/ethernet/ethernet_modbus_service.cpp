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
constexpr uint32_t ETHERNET_CONFIG_WAIT_MS = 2000U;
constexpr uint8_t W5500_SOCKET_ESTABLISHED = 0x17U;
constexpr uint8_t MODBUS_EXCEPTION_ILLEGAL_ADDRESS = 0x02U;
constexpr uint16_t RX_CHUNK_CAPACITY = 128U;
constexpr uint8_t MAX_RESPONSES_PER_POLL = 2U;

static_assert(LCP_MODBUS_TCP_HOLDING_COUNT ==
              (LCP_FIELD_PORT_COUNT *
               LCP_MODBUS_TCP_REGISTERS_PER_FIELD_PORT),
              "Modbus TCP map must cover every FieldSensor port");

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

void update_holding_map(void)
{
    memset(g_holding, 0, sizeof(g_holding));
    const uint8_t service_paused = field_sensor_service_paused();

    for (uint8_t port_index = 0U;
         port_index < field_sensor_service_port_count();
         ++port_index)
    {
        const FieldSensorPortState& port = field_sensor_service_port(
            static_cast<LcpFieldPortId>(port_index));
        const uint16_t base = static_cast<uint16_t>(
            port_index * LCP_MODBUS_TCP_REGISTERS_PER_FIELD_PORT);
        uint16_t quality = 0U;

        g_holding[base] = port.registers[0];
        g_holding[base + 1U] = port.registers[1];

        if (port.valid != 0U)
        {
            quality |= FIELD_SENSOR_TCP_QUALITY_VALID;
        }

        if (port.connection_lost != 0U)
        {
            quality |= FIELD_SENSOR_TCP_QUALITY_CONNECTION_LOST;
        }

        if (port.port_present != 0U)
        {
            quality |= FIELD_SENSOR_TCP_QUALITY_PORT_PRESENT;
        }

        if (port.request_active != 0U)
        {
            quality |= FIELD_SENSOR_TCP_QUALITY_REQUEST_ACTIVE;
        }

        if (service_paused != 0U)
        {
            quality |= FIELD_SENSOR_TCP_QUALITY_SERVICE_PAUSED;
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
        (register_count > static_cast<uint16_t>(
            LCP_MODBUS_TCP_HOLDING_COUNT - start_address)))
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
    ethernet_network_config_load(configs, reports);
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

uint8_t flush_pending_response(LcpEthernetId ethernet_id,
                               EthernetModbusInterfaceState& interface_state)
{
    const uint16_t response_length =
        modbus_tcp_server_response_length(interface_state.server);

    if (response_length == 0U)
    {
        return 1U;
    }

    const uint16_t sent = w5500_lite_tcp_server_send(
        ethernet_id,
        modbus_tcp_server_response(interface_state.server),
        response_length);

    if (sent != response_length)
    {
        /* TX_FSR может быть временно мал; ответ сохраняется до следующего poll. */
        return 0U;
    }

    modbus_tcp_server_response_sent(interface_state.server);
    return 1U;
}

void reset_stream_if_disconnected(EthernetModbusInterfaceState& state)
{
    state.server.rx_length = 0U;
    state.server.tx_length = 0U;
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

    interface_state.last_socket_status =
        w5500_lite_tcp_server_status(ethernet_id);

    if (interface_state.last_socket_status == W5500_SOCKET_ESTABLISHED)
    {
        if (flush_pending_response(ethernet_id, interface_state) == 0U)
        {
            return;
        }
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
        reset_stream_if_disconnected(interface_state);
        return;
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

        if (flush_pending_response(ethernet_id, interface_state) == 0U)
        {
            break;
        }
    }
}
}

void ethernet_modbus_service_init(void)
{
    W5500NetworkConfig defaults[LCP_ETHERNET_COUNT];
    EthernetNetworkConfigReport reports[LCP_ETHERNET_COUNT];

    memset(g_interfaces, 0, sizeof(g_interfaces));
    memset(g_holding, 0, sizeof(g_holding));
    ethernet_network_config_set_defaults(defaults);
    ethernet_network_config_prepare_reports(reports, SD_CONFIG_NOT_ATTEMPTED);

    for (uint8_t index = 0U; index < LCP_ETHERNET_COUNT; ++index)
    {
        g_interfaces[index].config = defaults[index];
        g_interfaces[index].config_report = reports[index];
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
