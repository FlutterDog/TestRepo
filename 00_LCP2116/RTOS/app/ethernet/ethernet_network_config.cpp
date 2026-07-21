/**
 * @file ethernet_network_config.cpp
 * @brief Реализация чтения канонических параметров двух W5500.
 */

#include "ethernet_network_config.hpp"

#include <stddef.h>
#include <stdint.h>

namespace
{
struct EthernetFileSet
{
    const char* mac;
    const char* ip;
    const char* subnet;
    const char* gateway;
};

static const EthernetFileSet FILES[LCP_ETHERNET_COUNT] =
{
    { "MAC.txt",  "IP.txt",  "SUBNET.txt",  "GATE.txt"  },
    { "MAC2.txt", "IP2.txt", "SUBNET2.txt", "GATE2.txt" }
};

template <size_t OctetCount>
SdConfigResult load_octets(const char* file_name,
                           uint8_t (&output)[OctetCount])
{
    int16_t values[OctetCount + 1U];
    uint8_t loaded_count = 0U;
    const SdConfigResult result = sd_config_load_int16(
        file_name,
        values,
        sizeof(values) / sizeof(values[0]),
        &loaded_count);

    if (result != SD_CONFIG_OK)
    {
        return result;
    }

    if ((loaded_count != OctetCount) ||
        (values[0] != static_cast<int16_t>(OctetCount)))
    {
        return SD_CONFIG_INVALID_COUNT;
    }

    for (size_t index = 0U; index < OctetCount; ++index)
    {
        const int16_t value = values[index + 1U];

        if ((value < 0) || (value > 255))
        {
            return SD_CONFIG_INVALID_VALUE;
        }
    }

    for (size_t index = 0U; index < OctetCount; ++index)
    {
        output[index] = static_cast<uint8_t>(values[index + 1U]);
    }

    return SD_CONFIG_OK;
}

void initialize_report(EthernetNetworkConfigReport& report,
                       const EthernetFileSet& files,
                       SdConfigResult initial_result)
{
    report.mac_result = initial_result;
    report.ip_result = initial_result;
    report.subnet_result = initial_result;
    report.gateway_result = initial_result;
    report.mac_file = files.mac;
    report.ip_file = files.ip;
    report.subnet_file = files.subnet;
    report.gateway_file = files.gateway;
    report.any_loaded_from_sd = 0U;
}

void note_loaded(EthernetNetworkConfigReport& report,
                 SdConfigResult result)
{
    if (result == SD_CONFIG_OK)
    {
        report.any_loaded_from_sd = 1U;
    }
}

void load_interface(W5500NetworkConfig& config,
                    EthernetNetworkConfigReport& report,
                    const EthernetFileSet& files)
{
    W5500MacAddress mac = config.mac;
    W5500IpAddress ip = config.ip;
    W5500IpAddress subnet = config.subnet;
    W5500IpAddress gateway = config.gateway;

    report.mac_result = load_octets(files.mac, mac.octet);
    report.ip_result = load_octets(files.ip, ip.octet);
    report.subnet_result = load_octets(files.subnet, subnet.octet);
    report.gateway_result = load_octets(files.gateway, gateway.octet);

    if (report.mac_result == SD_CONFIG_OK)
    {
        config.mac = mac;
    }

    if (report.ip_result == SD_CONFIG_OK)
    {
        config.ip = ip;
    }

    if (report.subnet_result == SD_CONFIG_OK)
    {
        config.subnet = subnet;
    }

    if (report.gateway_result == SD_CONFIG_OK)
    {
        config.gateway = gateway;
    }

    note_loaded(report, report.mac_result);
    note_loaded(report, report.ip_result);
    note_loaded(report, report.subnet_result);
    note_loaded(report, report.gateway_result);
}
}

void ethernet_network_config_set_defaults(
    W5500NetworkConfig configs[LCP_ETHERNET_COUNT])
{
    if (configs == 0)
    {
        return;
    }

    configs[LCP_ETHERNET_1] =
    {
        { { 0x02U, 0x4CU, 0x43U, 0x50U, 0x00U, 0x01U } },
        { { 192U, 168U, 1U, 1U } },
        { { 192U, 168U, 1U, 254U } },
        { { 255U, 255U, 255U, 0U } }
    };

    configs[LCP_ETHERNET_2] =
    {
        { { 0x02U, 0x4CU, 0x43U, 0x50U, 0x00U, 0x02U } },
        { { 192U, 168U, 1U, 2U } },
        { { 192U, 168U, 1U, 254U } },
        { { 255U, 255U, 255U, 0U } }
    };
}

void ethernet_network_config_prepare_reports(
    EthernetNetworkConfigReport reports[LCP_ETHERNET_COUNT],
    SdConfigResult initial_result)
{
    if (reports == 0)
    {
        return;
    }

    for (uint8_t index = 0U; index < LCP_ETHERNET_COUNT; ++index)
    {
        initialize_report(reports[index], FILES[index], initial_result);
    }
}

void ethernet_network_config_load(
    W5500NetworkConfig configs[LCP_ETHERNET_COUNT],
    EthernetNetworkConfigReport reports[LCP_ETHERNET_COUNT])
{
    if ((configs == 0) || (reports == 0))
    {
        return;
    }

    ethernet_network_config_prepare_reports(reports, SD_CONFIG_NOT_ATTEMPTED);

    for (uint8_t index = 0U; index < LCP_ETHERNET_COUNT; ++index)
    {
        load_interface(configs[index], reports[index], FILES[index]);
    }
}
