/**
 * @file lcp_config_bundle.cpp
 * @brief Реализация нормализованного конфигурационного комплекта LCP2116.
 */

#include "lcp_config_bundle.hpp"

#include "../x2x/x2x_catalog.hpp"
#include "../../libs/lcp_crc32/lcp_crc32.hpp"
#include "../../libs/lcp_sd_storage/lcp_sd_storage.hpp"

#include <stddef.h>
#include <string.h>

namespace
{
constexpr uint32_t MINIMUM_BAUDRATE = 300U;
constexpr uint32_t MAXIMUM_BAUDRATE = 1000000U;

static const char BAUD_FILE[] = "baud.TXT";
static const char PARITY_FILE[] = "Parity.TXT";
static const char X2X_FILE[] = "X2X.TXT";
static const char EIA_FILE[] = "EIA.TXT";

struct EthernetFiles
{
    const char* mac;
    const char* ip;
    const char* subnet;
    const char* gateway;
};

static const EthernetFiles ETHERNET_FILES[LCP_ETHERNET_COUNT] =
{
    { "MAC.txt", "IP.txt", "SUBNET.txt", "GATE.txt" },
    { "MAC2.txt", "IP2.txt", "SUBNET2.txt", "GATE2.txt" }
};

void prepare_report(LcpConfigSdReport& report)
{
    report.result = LCP_CONFIG_SD_NOT_ATTEMPTED;
    report.failed_file = 0;
    report.sd_result = SD_CONFIG_NOT_ATTEMPTED;
    report.x2x_result = X2X_CONFIG_OK;
    report.bundle_crc32 = 0U;
}

LcpConfigSdResult fail_sd(LcpConfigSdReport& report,
                          const char* file,
                          SdConfigResult result,
                          LcpConfigSdResult bundle_result)
{
    report.result = bundle_result;
    report.failed_file = file;
    report.sd_result = result;
    return report.result;
}

LcpConfigSdResult load_baud(LcpConfigBundle& bundle,
                            LcpConfigSdReport& report)
{
    uint32_t values[LCP_FIELD_PORT_COUNT + 1U];
    uint8_t loaded_count = 0U;
    const SdConfigResult result = sd_config_load_uint32(
        BAUD_FILE,
        values,
        sizeof(values) / sizeof(values[0]),
        &loaded_count);

    if (result != SD_CONFIG_OK)
    {
        return fail_sd(report,
                       BAUD_FILE,
                       result,
                       (result == SD_CONFIG_CARD_NOT_READY)
                           ? LCP_CONFIG_SD_STORAGE_NOT_READY
                           : LCP_CONFIG_SD_FILE_ERROR);
    }

    if ((loaded_count != LCP_FIELD_PORT_COUNT) ||
        (values[0] != LCP_FIELD_PORT_COUNT))
    {
        return fail_sd(report,
                       BAUD_FILE,
                       SD_CONFIG_INVALID_COUNT,
                       LCP_CONFIG_SD_INVALID_VALUE);
    }

    for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
    {
        const uint32_t baudrate = values[index + 1U];

        if ((baudrate < MINIMUM_BAUDRATE) ||
            (baudrate > MAXIMUM_BAUDRATE))
        {
            return fail_sd(report,
                           BAUD_FILE,
                           SD_CONFIG_INVALID_VALUE,
                           LCP_CONFIG_SD_INVALID_VALUE);
        }

        bundle.field_baudrate[index] = baudrate;
    }

    return LCP_CONFIG_SD_OK;
}

LcpConfigSdResult load_parity(LcpConfigBundle& bundle,
                              LcpConfigSdReport& report)
{
    int16_t values[LCP_FIELD_PORT_COUNT + 1U];
    uint8_t loaded_count = 0U;
    const SdConfigResult result = sd_config_load_int16(
        PARITY_FILE,
        values,
        sizeof(values) / sizeof(values[0]),
        &loaded_count);

    if (result != SD_CONFIG_OK)
    {
        return fail_sd(report,
                       PARITY_FILE,
                       result,
                       (result == SD_CONFIG_CARD_NOT_READY)
                           ? LCP_CONFIG_SD_STORAGE_NOT_READY
                           : LCP_CONFIG_SD_FILE_ERROR);
    }

    if ((loaded_count != LCP_FIELD_PORT_COUNT) ||
        (values[0] != LCP_FIELD_PORT_COUNT))
    {
        return fail_sd(report,
                       PARITY_FILE,
                       SD_CONFIG_INVALID_COUNT,
                       LCP_CONFIG_SD_INVALID_VALUE);
    }

    for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
    {
        const int16_t parity = values[index + 1U];

        if ((parity < 0) || (parity > 2))
        {
            return fail_sd(report,
                           PARITY_FILE,
                           SD_CONFIG_INVALID_VALUE,
                           LCP_CONFIG_SD_INVALID_VALUE);
        }

        bundle.field_parity[index] = static_cast<uint8_t>(parity);
    }

    return LCP_CONFIG_SD_OK;
}

template <size_t OctetCount>
LcpConfigSdResult load_octets(const char* file_name,
                              uint8_t (&output)[OctetCount],
                              LcpConfigSdReport& report)
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
        return fail_sd(report,
                       file_name,
                       result,
                       (result == SD_CONFIG_CARD_NOT_READY)
                           ? LCP_CONFIG_SD_STORAGE_NOT_READY
                           : LCP_CONFIG_SD_FILE_ERROR);
    }

    if ((loaded_count != OctetCount) ||
        (values[0] != static_cast<int16_t>(OctetCount)))
    {
        return fail_sd(report,
                       file_name,
                       SD_CONFIG_INVALID_COUNT,
                       LCP_CONFIG_SD_INVALID_VALUE);
    }

    for (size_t index = 0U; index < OctetCount; ++index)
    {
        const int16_t value = values[index + 1U];

        if ((value < 0) || (value > 255))
        {
            return fail_sd(report,
                           file_name,
                           SD_CONFIG_INVALID_VALUE,
                           LCP_CONFIG_SD_INVALID_VALUE);
        }

        output[index] = static_cast<uint8_t>(value);
    }

    return LCP_CONFIG_SD_OK;
}

LcpConfigSdResult load_ethernet(LcpConfigBundle& bundle,
                                LcpConfigSdReport& report)
{
    for (uint8_t index = 0U; index < LCP_ETHERNET_COUNT; ++index)
    {
        const EthernetFiles& files = ETHERNET_FILES[index];

        if (load_octets(files.mac, bundle.ethernet_mac[index], report) !=
            LCP_CONFIG_SD_OK)
        {
            return report.result;
        }

        if (load_octets(files.ip, bundle.ethernet_ip[index], report) !=
            LCP_CONFIG_SD_OK)
        {
            return report.result;
        }

        if (load_octets(files.subnet,
                        bundle.ethernet_subnet[index],
                        report) != LCP_CONFIG_SD_OK)
        {
            return report.result;
        }

        if (load_octets(files.gateway,
                        bundle.ethernet_gateway[index],
                        report) != LCP_CONFIG_SD_OK)
        {
            return report.result;
        }
    }

    return LCP_CONFIG_SD_OK;
}

LcpConfigSdResult load_x2x(LcpConfigBundle& bundle,
                           LcpConfigSdReport& report)
{
    X2XConfig config;
    X2XConfigError error;
    const X2XConfigResult result = x2x_config_load(X2X_FILE,
                                                   config,
                                                   error);

    if (result != X2X_CONFIG_OK)
    {
        report.result = (result == X2X_CONFIG_STORAGE_NOT_READY)
            ? LCP_CONFIG_SD_STORAGE_NOT_READY
            : LCP_CONFIG_SD_X2X_ERROR;
        report.failed_file = X2X_FILE;
        report.x2x_result = result;
        report.sd_result = SD_CONFIG_NOT_ATTEMPTED;
        return report.result;
    }

    bundle.x2x_module_count = config.module_count;

    for (uint8_t index = 0U; index < X2X_MAX_MODULES; ++index)
    {
        bundle.x2x_module_id[index] =
            (index < config.module_count)
                ? static_cast<uint8_t>(config.module_types[index])
                : 0U;
    }

    return LCP_CONFIG_SD_OK;
}

LcpConfigSdResult load_eia(LcpConfigBundle& bundle,
                           LcpConfigSdReport& report)
{
    int16_t values[LCP_CONFIG_EIA_VALUE_COUNT + 1U];
    uint8_t loaded_count = 0U;
    const SdConfigResult result = sd_config_load_int16(
        EIA_FILE,
        values,
        sizeof(values) / sizeof(values[0]),
        &loaded_count);

    if (result != SD_CONFIG_OK)
    {
        return fail_sd(report,
                       EIA_FILE,
                       result,
                       (result == SD_CONFIG_CARD_NOT_READY)
                           ? LCP_CONFIG_SD_STORAGE_NOT_READY
                           : LCP_CONFIG_SD_FILE_ERROR);
    }

    if ((loaded_count != LCP_CONFIG_EIA_VALUE_COUNT) ||
        (values[0] != LCP_CONFIG_EIA_VALUE_COUNT))
    {
        return fail_sd(report,
                       EIA_FILE,
                       SD_CONFIG_INVALID_COUNT,
                       LCP_CONFIG_SD_INVALID_VALUE);
    }

    bundle.eia_count = LCP_CONFIG_EIA_VALUE_COUNT;

    for (uint8_t index = 0U; index < LCP_CONFIG_EIA_VALUE_COUNT; ++index)
    {
        const int16_t value = values[index + 1U];

        if ((value < 0) || (value > 255))
        {
            return fail_sd(report,
                           EIA_FILE,
                           SD_CONFIG_INVALID_VALUE,
                           LCP_CONFIG_SD_INVALID_VALUE);
        }

        bundle.eia_value[index] = value;
    }

    return LCP_CONFIG_SD_OK;
}

uint8_t mac_valid(const uint8_t mac[6])
{
    uint8_t any_nonzero = 0U;

    for (uint8_t index = 0U; index < 6U; ++index)
    {
        any_nonzero |= mac[index];
    }

    return ((any_nonzero != 0U) && ((mac[0] & 0x01U) == 0U)) ? 1U : 0U;
}

uint8_t ip_valid(const uint8_t ip[4])
{
    if (((ip[0] == 0U) && (ip[1] == 0U) &&
         (ip[2] == 0U) && (ip[3] == 0U)) ||
        ((ip[0] >= 224U) && (ip[0] <= 239U)) ||
        ((ip[0] == 255U) && (ip[1] == 255U) &&
         (ip[2] == 255U) && (ip[3] == 255U)))
    {
        return 0U;
    }

    return 1U;
}

uint8_t subnet_valid(const uint8_t subnet[4])
{
    const uint32_t mask =
        (static_cast<uint32_t>(subnet[0]) << 24U) |
        (static_cast<uint32_t>(subnet[1]) << 16U) |
        (static_cast<uint32_t>(subnet[2]) << 8U) |
        static_cast<uint32_t>(subnet[3]);

    if (mask == 0U)
    {
        return 0U;
    }

    const uint32_t inverted = ~mask;
    return ((inverted & (inverted + 1U)) == 0U) ? 1U : 0U;
}

HalUartFrame parity_frame(uint8_t parity)
{
    switch (parity)
    {
        case 1U:
            return HAL_UART_FRAME_8O1;
        case 2U:
            return HAL_UART_FRAME_8E1;
        case 0U:
        default:
            return HAL_UART_FRAME_8N1;
    }
}
}

void lcp_config_bundle_set_defaults(LcpConfigBundle& bundle)
{
    memset(&bundle, 0, sizeof(bundle));

    for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
    {
        bundle.field_baudrate[index] = 9600U;
        bundle.field_parity[index] = 0U;
    }

    const uint8_t mac1[6] = { 0x02U, 0x4CU, 0x43U, 0x50U, 0x00U, 0x01U };
    const uint8_t mac2[6] = { 0x02U, 0x4CU, 0x43U, 0x50U, 0x00U, 0x02U };
    const uint8_t ip1[4] = { 192U, 168U, 1U, 1U };
    const uint8_t ip2[4] = { 192U, 168U, 1U, 2U };
    const uint8_t gateway[4] = { 192U, 168U, 1U, 254U };
    const uint8_t subnet[4] = { 255U, 255U, 255U, 0U };

    memcpy(bundle.ethernet_mac[0], mac1, sizeof(mac1));
    memcpy(bundle.ethernet_mac[1], mac2, sizeof(mac2));
    memcpy(bundle.ethernet_ip[0], ip1, sizeof(ip1));
    memcpy(bundle.ethernet_ip[1], ip2, sizeof(ip2));
    memcpy(bundle.ethernet_gateway[0], gateway, sizeof(gateway));
    memcpy(bundle.ethernet_gateway[1], gateway, sizeof(gateway));
    memcpy(bundle.ethernet_subnet[0], subnet, sizeof(subnet));
    memcpy(bundle.ethernet_subnet[1], subnet, sizeof(subnet));

    bundle.x2x_module_count = 0U;
    bundle.eia_count = LCP_CONFIG_EIA_VALUE_COUNT;
    bundle.eia_value[0] = 3;
    bundle.eia_value[1] = 4;
    bundle.eia_value[2] = 5;
    bundle.eia_value[3] = 6;
}

uint8_t lcp_config_bundle_validate(const LcpConfigBundle& bundle)
{
    for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
    {
        if ((bundle.field_baudrate[index] < MINIMUM_BAUDRATE) ||
            (bundle.field_baudrate[index] > MAXIMUM_BAUDRATE) ||
            (bundle.field_parity[index] > 2U))
        {
            return 0U;
        }
    }

    for (uint8_t index = 0U; index < LCP_ETHERNET_COUNT; ++index)
    {
        if ((mac_valid(bundle.ethernet_mac[index]) == 0U) ||
            (ip_valid(bundle.ethernet_ip[index]) == 0U) ||
            (subnet_valid(bundle.ethernet_subnet[index]) == 0U))
        {
            return 0U;
        }
    }

    if (bundle.x2x_module_count > X2X_MAX_MODULES)
    {
        return 0U;
    }

    for (uint8_t index = 0U; index < X2X_MAX_MODULES; ++index)
    {
        if (index < bundle.x2x_module_count)
        {
            const X2XModuleDescriptor* descriptor =
                x2x_catalog_find_by_id(bundle.x2x_module_id[index]);

            if ((descriptor == 0) ||
                (descriptor->allowed_in_x2x_config == 0U))
            {
                return 0U;
            }
        }
        else if (bundle.x2x_module_id[index] != 0U)
        {
            return 0U;
        }
    }

    if (bundle.eia_count != LCP_CONFIG_EIA_VALUE_COUNT)
    {
        return 0U;
    }

    for (uint8_t index = 0U; index < LCP_CONFIG_EIA_VALUE_COUNT; ++index)
    {
        if ((bundle.eia_value[index] < 0) ||
            (bundle.eia_value[index] > 255))
        {
            return 0U;
        }
    }

    for (uint8_t index = 0U; index < LCP_CONFIG_RESERVED_BYTES; ++index)
    {
        if (bundle.reserved[index] != 0U)
        {
            return 0U;
        }
    }

    return 1U;
}

LcpConfigSdResult lcp_config_bundle_load_sd(LcpConfigBundle& output,
                                            LcpConfigSdReport& report)
{
    prepare_report(report);

    if (lcp_sd_storage_ready() == 0U)
    {
        report.result = LCP_CONFIG_SD_STORAGE_NOT_READY;
        report.sd_result = SD_CONFIG_CARD_NOT_READY;
        return report.result;
    }

    LcpConfigBundle candidate;
    lcp_config_bundle_set_defaults(candidate);

    if (load_baud(candidate, report) != LCP_CONFIG_SD_OK)
    {
        return report.result;
    }

    if (load_parity(candidate, report) != LCP_CONFIG_SD_OK)
    {
        return report.result;
    }

    if (load_ethernet(candidate, report) != LCP_CONFIG_SD_OK)
    {
        return report.result;
    }

    if (load_x2x(candidate, report) != LCP_CONFIG_SD_OK)
    {
        return report.result;
    }

    if (load_eia(candidate, report) != LCP_CONFIG_SD_OK)
    {
        return report.result;
    }

    if (lcp_config_bundle_validate(candidate) == 0U)
    {
        report.result = LCP_CONFIG_SD_INVALID_VALUE;
        report.failed_file = "bundle";
        report.sd_result = SD_CONFIG_INVALID_VALUE;
        return report.result;
    }

    output = candidate;
    report.result = LCP_CONFIG_SD_OK;
    report.bundle_crc32 = lcp_config_bundle_crc32(candidate);
    return report.result;
}

uint32_t lcp_config_bundle_crc32(const LcpConfigBundle& bundle)
{
    return lcp_crc32(&bundle, sizeof(bundle));
}

uint8_t lcp_config_bundle_equal(const LcpConfigBundle& left,
                                const LcpConfigBundle& right)
{
    return (memcmp(&left, &right, sizeof(left)) == 0) ? 1U : 0U;
}

void lcp_config_bundle_field_serial(
    const LcpConfigBundle& bundle,
    LcpFieldPortConfig output[LCP_FIELD_PORT_COUNT])
{
    if (output == 0)
    {
        return;
    }

    for (uint8_t index = 0U; index < LCP_FIELD_PORT_COUNT; ++index)
    {
        output[index].baudrate = bundle.field_baudrate[index];
        output[index].frame = parity_frame(bundle.field_parity[index]);
    }
}

void lcp_config_bundle_ethernet(
    const LcpConfigBundle& bundle,
    W5500NetworkConfig output[LCP_ETHERNET_COUNT])
{
    if (output == 0)
    {
        return;
    }

    for (uint8_t index = 0U; index < LCP_ETHERNET_COUNT; ++index)
    {
        memcpy(output[index].mac.octet,
               bundle.ethernet_mac[index],
               sizeof(output[index].mac.octet));
        memcpy(output[index].ip.octet,
               bundle.ethernet_ip[index],
               sizeof(output[index].ip.octet));
        memcpy(output[index].subnet.octet,
               bundle.ethernet_subnet[index],
               sizeof(output[index].subnet.octet));
        memcpy(output[index].gateway.octet,
               bundle.ethernet_gateway[index],
               sizeof(output[index].gateway.octet));
    }
}

uint8_t lcp_config_bundle_x2x(const LcpConfigBundle& bundle,
                              X2XConfig& output)
{
    X2XConfigError error;
    x2x_config_reset(output, error);

    if (lcp_config_bundle_validate(bundle) == 0U)
    {
        return 0U;
    }

    output.module_count = bundle.x2x_module_count;

    for (uint8_t index = 0U; index < X2X_MAX_MODULES; ++index)
    {
        output.module_types[index] =
            (index < bundle.x2x_module_count)
                ? static_cast<X2XDeviceType>(bundle.x2x_module_id[index])
                : X2X_DEVICE_LCP;
    }

    return 1U;
}

const char* lcp_config_sd_result_text(LcpConfigSdResult result)
{
    switch (result)
    {
        case LCP_CONFIG_SD_NOT_ATTEMPTED:
            return "not attempted";
        case LCP_CONFIG_SD_OK:
            return "ok";
        case LCP_CONFIG_SD_STORAGE_NOT_READY:
            return "storage not ready";
        case LCP_CONFIG_SD_FILE_ERROR:
            return "file error";
        case LCP_CONFIG_SD_INVALID_VALUE:
            return "invalid value";
        case LCP_CONFIG_SD_X2X_ERROR:
            return "X2X error";
        default:
            return "unknown";
    }
}
