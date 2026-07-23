/**
 * @file lcp_config_bundle.hpp
 * @brief Нормализованная конфигурация LCP2116 и строгий импорт с microSD.
 */

#pragma once

#include <stdint.h>

#include "../diagnostics/sd_config.hpp"
#include "../x2x/x2x_config.hpp"
#include "../../board/lcp_field_ports.hpp"
#include "../../hal/w5500_lite.hpp"

static const uint8_t LCP_CONFIG_EIA_VALUE_COUNT = 4U;
static const uint8_t LCP_CONFIG_RESERVED_BYTES = 32U;

/**
 * @brief Версия 1 бинарного payload внутреннего хранилища.
 *
 * Поля имеют фиксированную ширину. Reserved заполняется нулями и оставляет
 * пространство для совместимых дополнений без немедленного увеличения записи.
 * Выравнивание 4 гарантирует безопасный доступ к uint32_t на Cortex-M3.
 */
struct __attribute__((packed, aligned(4))) LcpConfigBundle
{
    uint32_t field_baudrate[LCP_FIELD_PORT_COUNT];
    uint8_t field_parity[LCP_FIELD_PORT_COUNT]; /**< 0=8N1, 1=8O1, 2=8E1. */

    uint8_t ethernet_mac[LCP_ETHERNET_COUNT][6];
    uint8_t ethernet_ip[LCP_ETHERNET_COUNT][4];
    uint8_t ethernet_subnet[LCP_ETHERNET_COUNT][4];
    uint8_t ethernet_gateway[LCP_ETHERNET_COUNT][4];

    uint8_t x2x_module_count;
    uint8_t x2x_module_id[X2X_MAX_MODULES];

    uint8_t eia_count;
    int16_t eia_value[LCP_CONFIG_EIA_VALUE_COUNT];

    uint8_t reserved[LCP_CONFIG_RESERVED_BYTES];
};

static_assert(sizeof(LcpConfigBundle) == 104U,
              "LcpConfigBundle schema v1 size changed");
static_assert(alignof(LcpConfigBundle) == 4U,
              "LcpConfigBundle must remain 4-byte aligned");

enum LcpConfigSdResult : uint8_t
{
    LCP_CONFIG_SD_NOT_ATTEMPTED = 0U,
    LCP_CONFIG_SD_OK = 1U,
    LCP_CONFIG_SD_STORAGE_NOT_READY = 2U,
    LCP_CONFIG_SD_FILE_ERROR = 3U,
    LCP_CONFIG_SD_INVALID_VALUE = 4U,
    LCP_CONFIG_SD_X2X_ERROR = 5U
};

/** @brief Диагностика одного полного чтения конфигурационного комплекта. */
struct LcpConfigSdReport
{
    LcpConfigSdResult result;
    const char* failed_file;
    SdConfigResult sd_result;
    X2XConfigResult x2x_result;
    uint32_t bundle_crc32;
};

/** @brief Формирует полный безопасный набор значений по умолчанию. */
void lcp_config_bundle_set_defaults(LcpConfigBundle& bundle);

/** @brief Полностью проверяет значения уже сформированного bundle. */
uint8_t lcp_config_bundle_validate(const LcpConfigBundle& bundle);

/**
 * @brief Атомарно читает весь канонический комплект TXT с microSD.
 *
 * Output изменяется только при полном успехе всех обязательных файлов.
 */
LcpConfigSdResult lcp_config_bundle_load_sd(LcpConfigBundle& output,
                                            LcpConfigSdReport& report);

/** @return CRC32 нормализованного bundle. */
uint32_t lcp_config_bundle_crc32(const LcpConfigBundle& bundle);

/** @return 1 при полном совпадении всех нормализованных полей. */
uint8_t lcp_config_bundle_equal(const LcpConfigBundle& left,
                                const LcpConfigBundle& right);

/** @brief Преобразует bundle в аппаратные настройки S1..S4. */
void lcp_config_bundle_field_serial(
    const LcpConfigBundle& bundle,
    LcpFieldPortConfig output[LCP_FIELD_PORT_COUNT]);

/** @brief Преобразует bundle в конфигурации ETH1/ETH2. */
void lcp_config_bundle_ethernet(
    const LcpConfigBundle& bundle,
    W5500NetworkConfig output[LCP_ETHERNET_COUNT]);

/** @brief Преобразует bundle в проверенную X2XConfig. */
uint8_t lcp_config_bundle_x2x(const LcpConfigBundle& bundle,
                              X2XConfig& output);

/** @return Текст результата полного SD-импорта. */
const char* lcp_config_sd_result_text(LcpConfigSdResult result);
