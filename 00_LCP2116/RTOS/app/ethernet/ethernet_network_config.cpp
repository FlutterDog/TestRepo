/**
 * @file ethernet_network_config.cpp
 * @brief Реализация чтения сетевых параметров W5500 из microSD.
 */

#include "ethernet_network_config.hpp"

#include <stddef.h>

namespace
{
struct FileAliases
{
    const char* names[4];
    uint8_t count;
};

const FileAliases ETH1_IP = { { "IP.TXT", "IP1.TXT", 0, 0 }, 2U };
const FileAliases ETH2_IP = { { "IP2.TXT", 0, 0, 0 }, 1U };
const FileAliases ETH1_SUBNET =
    { { "SUBNET.TXT", "SUBNET1.TXT", 0, 0 }, 2U };
const FileAliases ETH2_SUBNET =
    { { "SUBNET2.TXT", 0, 0, 0 }, 1U };
const FileAliases ETH1_GATE =
    { { "GATE.TXT", "GATE1.TXT", "GW.TXT", "GW1.TXT" }, 4U };
const FileAliases ETH2_GATE =
    { { "GATE2.TXT", "GW2.TXT", 0, 0 }, 2U };

SdConfigResult load_ipv4(const FileAliases& aliases,
                         W5500IpAddress* address,
                         const char** used_file)
{
    int16_t values[5];
    uint8_t loaded_count = 0U;
    SdConfigResult last_result = SD_CONFIG_FILE_NOT_FOUND;

    if ((address == 0) || (used_file == 0))
    {
        return SD_CONFIG_INVALID_VALUE;
    }

    *used_file = aliases.names[0];

    for (uint8_t alias_index = 0U;
         alias_index < aliases.count;
         ++alias_index)
    {
        const char* file_name = aliases.names[alias_index];
        last_result = sd_config_load_int16(file_name,
                                           values,
                                           sizeof(values) / sizeof(values[0]),
                                           &loaded_count);

        if (last_result == SD_CONFIG_FILE_NOT_FOUND)
        {
            continue;
        }

        *used_file = file_name;

        if ((last_result != SD_CONFIG_OK) || (loaded_count != 4U))
        {
            return (last_result == SD_CONFIG_OK) ?
                SD_CONFIG_INVALID_COUNT : last_result;
        }

        for (uint8_t octet = 0U; octet < 4U; ++octet)
        {
            const int16_t value = values[octet + 1U];

            if ((value < 0) || (value > 255))
            {
                return SD_CONFIG_INVALID_VALUE;
            }
        }

        for (uint8_t octet = 0U; octet < 4U; ++octet)
        {
            address->octet[octet] =
                static_cast<uint8_t>(values[octet + 1U]);
        }

        return SD_CONFIG_OK;
    }

    return last_result;
}

void initialize_report(EthernetNetworkConfigReport& report)
{
    report.ip_result = SD_CONFIG_FILE_NOT_FOUND;
    report.subnet_result = SD_CONFIG_FILE_NOT_FOUND;
    report.gateway_result = SD_CONFIG_FILE_NOT_FOUND;
    report.ip_file = "";
    report.subnet_file = "";
    report.gateway_file = "";
    report.any_loaded_from_sd = 0U;
}

void load_interface(W5500NetworkConfig& config,
                    EthernetNetworkConfigReport& report,
                    const FileAliases& ip_aliases,
                    const FileAliases& subnet_aliases,
                    const FileAliases& gate_aliases)
{
    W5500IpAddress staged;

    report.ip_result = load_ipv4(ip_aliases, &staged, &report.ip_file);

    if (report.ip_result == SD_CONFIG_OK)
    {
        config.ip = staged;
        report.any_loaded_from_sd = 1U;
    }

    report.subnet_result =
        load_ipv4(subnet_aliases, &staged, &report.subnet_file);

    if (report.subnet_result == SD_CONFIG_OK)
    {
        config.subnet = staged;
        report.any_loaded_from_sd = 1U;
    }

    report.gateway_result =
        load_ipv4(gate_aliases, &staged, &report.gateway_file);

    if (report.gateway_result == SD_CONFIG_OK)
    {
        config.gateway = staged;
        report.any_loaded_from_sd = 1U;
    }
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

void ethernet_network_config_load(
    W5500NetworkConfig configs[LCP_ETHERNET_COUNT],
    EthernetNetworkConfigReport reports[LCP_ETHERNET_COUNT])
{
    if ((configs == 0) || (reports == 0))
    {
        return;
    }

    initialize_report(reports[LCP_ETHERNET_1]);
    initialize_report(reports[LCP_ETHERNET_2]);

    load_interface(configs[LCP_ETHERNET_1],
                   reports[LCP_ETHERNET_1],
                   ETH1_IP,
                   ETH1_SUBNET,
                   ETH1_GATE);
    load_interface(configs[LCP_ETHERNET_2],
                   reports[LCP_ETHERNET_2],
                   ETH2_IP,
                   ETH2_SUBNET,
                   ETH2_GATE);
}
