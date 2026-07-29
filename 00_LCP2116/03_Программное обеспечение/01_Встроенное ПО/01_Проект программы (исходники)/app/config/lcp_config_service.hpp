/**
 * @file lcp_config_service.hpp
 * @brief Надёжное A/B-хранилище конфигурации во внутренней Flash ATSAM3X8E.
 */

#pragma once

#include <stdint.h>

#include "lcp_config_bundle.hpp"
#include "../../hal/sam3x_internal_flash.hpp"

enum LcpConfigSource : uint8_t
{
    LCP_CONFIG_SOURCE_DEFAULTS = 0U,
    LCP_CONFIG_SOURCE_INTERNAL_FLASH = 1U
};

enum LcpConfigSlotId : uint8_t
{
    LCP_CONFIG_SLOT_NONE = 0U,
    LCP_CONFIG_SLOT_A = 1U,
    LCP_CONFIG_SLOT_B = 2U
};

enum LcpConfigOperation : uint8_t
{
    LCP_CONFIG_OPERATION_NONE = 0U,
    LCP_CONFIG_OPERATION_AUTO_IMPORT = 1U,
    LCP_CONFIG_OPERATION_SCAN_SD = 2U,
    LCP_CONFIG_OPERATION_COMPARE_SD = 3U,
    LCP_CONFIG_OPERATION_IMPORT_SD = 4U,
    LCP_CONFIG_OPERATION_ROLLBACK = 5U,
    LCP_CONFIG_OPERATION_ERASE = 6U
};

enum LcpConfigServiceResult : uint8_t
{
    LCP_CONFIG_SERVICE_IDLE = 0U,
    LCP_CONFIG_SERVICE_IN_PROGRESS = 1U,
    LCP_CONFIG_SERVICE_OK = 2U,
    LCP_CONFIG_SERVICE_NO_CHANGE = 3U,
    LCP_CONFIG_SERVICE_BUSY = 4U,
    LCP_CONFIG_SERVICE_SD_ERROR = 5U,
    LCP_CONFIG_SERVICE_SD_UNSTABLE = 6U,
    LCP_CONFIG_SERVICE_FLASH_ERROR = 7U,
    LCP_CONFIG_SERVICE_VERIFY_ERROR = 8U,
    LCP_CONFIG_SERVICE_NO_ROLLBACK = 9U,
    LCP_CONFIG_SERVICE_INVALID_STATE = 10U
};

/** @brief Состояние одного физического A/B-слота. */
struct LcpConfigSlotStatus
{
    uint8_t valid;
    uint32_t sequence;
    uint32_t payload_crc32;
    Sam3xInternalFlashResult flash_result;
};

/** @brief Последний дважды прочитанный кандидат с microSD. */
struct LcpConfigCandidateStatus
{
    uint8_t available;
    uint8_t stable;
    uint8_t equal_to_active;
    uint32_t crc32;
    LcpConfigSdReport report;
};

/** @brief Читает оба слота, выбирает последнее корректное поколение. */
void lcp_config_service_init(void);

/**
 * @brief Выполняет один шаг SD-проверки или Flash-записи.
 *
 * За один вызов программируется не более одной страницы Flash 256 байт.
 */
void lcp_config_service_poll(void);

/** @return Текущий рабочий нормализованный конфиг. */
const LcpConfigBundle& lcp_config_service_active(void);

/**
 * @return Поколение runtime-конфигурации. Увеличивается при смене активного
 * bundle, чтобы зависимые сервисы могли безопасно переинициализироваться.
 */
uint32_t lcp_config_service_generation(void);

/** @return Источник активного bundle: defaults или internal Flash. */
LcpConfigSource lcp_config_service_source(void);

/** @return Активный физический слот A/B либо NONE. */
LcpConfigSlotId lcp_config_service_active_slot(void);

/** @return Sequence активной записи; 0 для defaults. */
uint32_t lcp_config_service_active_sequence(void);

/** @return CRC32 активного bundle. */
uint32_t lcp_config_service_active_crc32(void);

/** @return Статус указанного физического слота. */
const LcpConfigSlotStatus& lcp_config_service_slot_status(
    LcpConfigSlotId slot_id);

/** @return Сведения о последнем SD-кандидате. */
const LcpConfigCandidateStatus& lcp_config_service_candidate_status(void);

/** @return Последняя запущенная операция. */
LcpConfigOperation lcp_config_service_operation(void);

/** @return Последний результат либо IN_PROGRESS. */
LcpConfigServiceResult lcp_config_service_result(void);

/** @return Результат последней Flash-команды. */
Sam3xInternalFlashResult lcp_config_service_last_flash_result(void);

/** @return 1, пока выполняется операция. */
uint8_t lcp_config_service_busy(void);

/** @brief Запускает двойное чтение и проверку SD без записи Flash. */
LcpConfigServiceResult lcp_config_service_request_scan_sd(void);

/** @brief Запускает двойное чтение SD для последующего сравнения. */
LcpConfigServiceResult lcp_config_service_request_compare_sd(void);

/** @brief Проверяет SD дважды и атомарно записывает новый bundle во Flash. */
LcpConfigServiceResult lcp_config_service_request_import_sd(void);

/** @brief Публикует предыдущий корректный слот как новое поколение. */
LcpConfigServiceResult lcp_config_service_request_rollback(void);

/** @brief Инвалидирует оба Flash-слота и активирует defaults. */
LcpConfigServiceResult lcp_config_service_request_erase(void);

/** @return Последний стабильный кандидат с SD либо null. */
const LcpConfigBundle* lcp_config_service_candidate(void);

/** @return Текстовые представления enum для диагностики. */
const char* lcp_config_source_text(LcpConfigSource source);
const char* lcp_config_slot_text(LcpConfigSlotId slot_id);
const char* lcp_config_operation_text(LcpConfigOperation operation);
const char* lcp_config_service_result_text(LcpConfigServiceResult result);
