/**
 * @file ethernet_network_config.cpp
 * @brief Реализация чтения сетевых параметров W5500 из microSD.
 */

#include "ethernet_network_config.hpp"

#include "../../libs/lcp_sd_storage/lcp_sd_storage.hpp"

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

static const uint16_t LINE_CAPACITY = 192U;
char g_line[LINE_CAPACITY];

uint8_t is_space(char value)
{
    return ((value == ' ') || (value == '\t') ||
            (value == '\r') || (value == '\n')) ? 1U : 0U;
}

void strip_comment_and_trim(char* line)
{
    uint16_t index = 0U;

    while (line[index] != '\0')
    {
        if (((line[index] == '/') && (line[index + 1U] == '/')) ||
            (line[index] == '#'))
        {
            line[index] = '\0';
            break;
        }

        ++index;
    }

    uint16_t start = 0U;

    while ((line[start] != '\0') && (is_space(line[start]) != 0U))
    {
        ++start;
    }

    if (start != 0U)
    {
        uint16_t destination = 0U;

        while (line[start] != '\0')
        {
            line[destination++] = line[start++];
        }

        line[destination] = '\0';
    }

    uint16_t length = 0U;

    while (line[length] != '\0')
    {
        ++length;
    }

    while ((length > 0U) && (is_space(line[length - 1U]) != 0U))
    {
        line[--length] = '\0';
    }
}

uint8_t read_line(LcpSdReadFile* file,
                  char* line,
                  uint16_t capacity,
                  uint8_t* line_too_long)
{
    uint16_t length = 0U;
    int value = -1;
    *line_too_long = 0U;

    while ((value = lcp_sd_storage_read_byte(file)) >= 0)
    {
        const char character = static_cast<char>(value);

        if (character == '\n')
        {
            break;
        }

        if (length < (capacity - 1U))
        {
            line[length++] = character;
        }
        else
        {
            *line_too_long = 1U;
        }
    }

    if ((value < 0) && (length == 0U) && (*line_too_long == 0U))
    {
        return 0U;
    }

    line[length] = '\0';
    return 1U;
}

uint8_t parse_uint8(const char* text, uint8_t* output)
{
    uint16_t value = 0U;
    uint16_t index = 0U;
    uint8_t digit_found = 0U;

    if ((text == 0) || (output == 0))
    {
        return 0U;
    }

    while (text[index] != '\0')
    {
        if ((text[index] < '0') || (text[index] > '9'))
        {
            return 0U;
        }

        value = static_cast<uint16_t>(
            (value * 10U) + static_cast<uint16_t>(text[index] - '0'));
        digit_found = 1U;

        if (value > 255U)
        {
            return 0U;
        }

        ++index;
    }

    if (digit_found == 0U)
    {
        return 0U;
    }

    *output = static_cast<uint8_t>(value);
    return 1U;
}

SdConfigResult translate_open_result(LcpSdStorageResult result)
{
    if (result == LCP_SD_STORAGE_FILE_NOT_FOUND)
    {
        return SD_CONFIG_FILE_NOT_FOUND;
    }

    return (result == LCP_SD_STORAGE_OK) ?
        SD_CONFIG_OK : SD_CONFIG_FILE_OPEN_FAILED;
}

SdConfigResult load_bytes(const char* file_name,
                          uint8_t expected_count,
                          uint8_t* output)
{
    if ((file_name == 0) || (output == 0) ||
        (expected_count == 0U) || (expected_count > 6U))
    {
        return SD_CONFIG_INVALID_VALUE;
    }

    if (lcp_sd_storage_ready() == 0U)
    {
        return SD_CONFIG_CARD_NOT_READY;
    }

    LcpSdReadFile file;
    const SdConfigResult open_result = translate_open_result(
        lcp_sd_storage_open_read(file_name, &file));

    if (open_result != SD_CONFIG_OK)
    {
        return open_result;
    }

    uint8_t significant_line = 0U;
    uint8_t value_index = 0U;
    uint8_t line_too_long = 0U;

    while (read_line(&file, g_line, LINE_CAPACITY, &line_too_long) != 0U)
    {
        if (line_too_long != 0U)
        {
            lcp_sd_storage_close_read(&file);
            return SD_CONFIG_LINE_TOO_LONG;
        }

        strip_comment_and_trim(g_line);

        if (g_line[0] == '\0')
        {
            continue;
        }

        if ((significant_line != 0U) && (value_index >= expected_count))
        {
            break;
        }

        uint8_t parsed = 0U;

        if (parse_uint8(g_line, &parsed) == 0U)
        {
            lcp_sd_storage_close_read(&file);
            return SD_CONFIG_INVALID_VALUE;
        }

        if (significant_line == 0U)
        {
            if (parsed != expected_count)
            {
                lcp_sd_storage_close_read(&file);
                return SD_CONFIG_INVALID_COUNT;
            }
        }
        else
        {
            output[value_index++] = parsed;
        }

        ++significant_line;
    }

    lcp_sd_storage_close_read(&file);

    if (significant_line == 0U)
    {
        return SD_CONFIG_EMPTY_FILE;
    }

    return (value_index == expected_count) ?
        SD_CONFIG_OK : SD_CONFIG_INCOMPLETE_FILE;
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
