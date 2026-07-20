/**
 * @file ethernet_network_config.cpp
 * @brief Реализация чтения сетевых параметров W5500 из microSD.
 */

#include "ethernet_network_config.hpp"

#include <stddef.h>

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

SdConfigResult load_bytes(const char* file_name,
                          uint8_t expected_count,
                          uint8_t* output)
{
    int16_t values[7];
    uint8_t loaded_count = 0U;

    if ((file_name == 0) || (output == 0) ||
        (expected_count == 0U) || (expected_count > 6U))
    {
        return SD_CONFIG_INVALID_VALUE;
    }

    const SdConfigResult result = sd_config_load_int16(
        file_name,
        values,
        sizeof(values) / sizeof(values[0]),
        &loaded_count);

    if (result != SD_CONFIG_OK)
    {
        return result;
    }

    if (loaded_count != expected_count)
    {
        return SD_CONFIG_INVALID_COUNT;
    }

    for (uint8_t index = 0U; index < expected_count; ++index)
    {
        const int16_t value = values[index + 1U];

        if ((value < 0) || (value > 255))
        {
            return SD_CONFIG_INVALID_VALUE;
        }
    }

    for (uint8_t index = 0U; index < expected_count; ++index)
    {
        output[index] = static_cast<uint8_t>(values[index + 1U]);
    }

    return SD_CONFIG_OK;
}

void initialize_report(EthernetNetworkConfigReport& report,
                       const EthernetFileSet& files)
{
    report.mac_result = SD_CONFIG_FILE_NOT_FOUND;
    report.ip_result = SD_CONFIG_FILE_NOT_FOUND;
    report.subnet_result = SD_CONFIG_FILE_NOT_FOUND;
    report.gateway_result = SD_CONFIG_FILE_NOT_FOUND;
    report.mac_file = files.mac;
    report.ip_file = files.ip;
    report.subnet_file = files.subnet;
    report.gateway_file = files.gateway;
    report.any_loaded_from_sd = 0U;
}

void load_interface(W5500NetworkConfig& config,
                    EthernetNetworkConfigReport& report,
                    const EthernetFileSet& files)
{
    W5500MacAddress staged_mac;
    W5500IpAddress staged_ip;

    report.mac_result = load_bytes(files.mac, 6U, staged_mac.octet);

    if (report.mac_result == SD_CONFIG_OK)
    {
        config.mac = staged_mac;
        report.any_loaded_from_sd = 1U;
    }

    report.ip_result = load_bytes(files.ip, 4U, staged_ip.octet);

    if (report.ip_result == SD_CONFIG_OK)
    {
        config.ip = staged_ip;
        report.any_loaded_from_sd = 1U;
    }

    report.subnet_result = load_bytes(files.subnet, 4U, staged_ip.octet);

    if (report.subnet_result == SD_CONFIG_OK)
    {
        config.subnet = staged_ip;
        report.any_loaded_from_sd = 1U;
    }

    report.gateway_result = load_bytes(files.gateway, 4U, staged_ip.octet);

    if (report.gateway_result == SD_CONFIG_OK)
    {
        config.gateway = staged_ip;
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
        { { 0U, 0U, 0U, 0U } },
        { { 255U, 255U, 255U, 0U } }
    };

    configs[LCP_ETHERNET_2] =
    {
        { { 0x02U, 0x4CU, 0x43U, 0x50U, 0x00U, 0x02U } },
        { { 192U, 168U, 1U, 2U } },
        { { 0U, 0U, 0U, 0U } },
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

    for (uint8_t index = 0U; index < LCP_ETHERNET_COUNT; ++index)
    {
        initialize_report(reports[index], FILES[index]);
        load_interface(configs[index], reports[index], FILES[index]);
    }
}
