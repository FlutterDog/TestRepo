/**
 * @file lcp_config_service.cpp
 * @brief Реализация A/B-хранилища конфигурации LCP2116.
 */

#include "lcp_config_service.hpp"

#include "../../libs/lcp_crc32/lcp_crc32.hpp"
#include "../../libs/lcp_sd_storage/lcp_sd_storage.hpp"

#include <stddef.h>
#include <string.h>

namespace
{
constexpr uint32_t CONFIG_STORAGE_MAGIC = 0x4C434346UL; /* LCCF */
constexpr uint32_t CONFIG_COMMIT_MAGIC = 0x4C434343UL;  /* LCCC */
constexpr uint16_t CONFIG_STORAGE_VERSION = 1U;
constexpr uint16_t CONFIG_SCHEMA_VERSION = 1U;
constexpr uint32_t CONFIG_RESERVED_TOTAL_BYTES = 32U * 1024U;
constexpr uint32_t CONFIG_SLOT_SIZE = CONFIG_RESERVED_TOTAL_BYTES / 2U;
constexpr uint32_t CONFIG_SLOT_COUNT = 2U;

struct __attribute__((packed)) ConfigRecordHeader
{
    uint32_t magic;
    uint16_t storage_version;
    uint16_t schema_version;
    uint32_t sequence;
    uint32_t payload_size;
    uint32_t payload_crc32;
    uint32_t header_crc32;
    uint32_t reserved[2];
};

struct __attribute__((packed)) ConfigCommitRecord
{
    uint32_t magic;
    uint16_t storage_version;
    uint16_t schema_version;
    uint32_t sequence;
    uint32_t payload_crc32;
    uint32_t header_crc32;
    uint32_t commit_crc32;
    uint32_t reserved;
};

static_assert(sizeof(ConfigRecordHeader) == 32U,
              "ConfigRecordHeader must remain 32 bytes");
static_assert(sizeof(ConfigCommitRecord) == 32U,
              "ConfigCommitRecord must remain 32 bytes");
static_assert((sizeof(ConfigRecordHeader) + sizeof(LcpConfigBundle)) <=
              SAM3X_INTERNAL_FLASH_PAGE_SIZE,
              "Configuration record must fit one Flash page");

enum ConfigServiceStep : uint8_t
{
    CONFIG_STEP_IDLE = 0U,
    CONFIG_STEP_SD_READ_FIRST = 1U,
    CONFIG_STEP_SD_READ_SECOND = 2U,
    CONFIG_STEP_WRITE_RECORD = 3U,
    CONFIG_STEP_WRITE_COMMIT = 4U,
    CONFIG_STEP_VERIFY_SLOT = 5U,
    CONFIG_STEP_ERASE_FIRST = 6U,
    CONFIG_STEP_ERASE_SECOND = 7U
};

LcpConfigBundle g_active_bundle;
LcpConfigBundle g_slot_bundle[CONFIG_SLOT_COUNT];
LcpConfigBundle g_sd_first_bundle;
LcpConfigBundle g_candidate_bundle;
LcpConfigSlotStatus g_slot_status[CONFIG_SLOT_COUNT];
LcpConfigCandidateStatus g_candidate_status;
LcpConfigSource g_source = LCP_CONFIG_SOURCE_DEFAULTS;
LcpConfigSlotId g_active_slot = LCP_CONFIG_SLOT_NONE;
LcpConfigOperation g_operation = LCP_CONFIG_OPERATION_NONE;
LcpConfigServiceResult g_result = LCP_CONFIG_SERVICE_IDLE;
ConfigServiceStep g_step = CONFIG_STEP_IDLE;
Sam3xInternalFlashResult g_last_flash_result = SAM3X_INTERNAL_FLASH_OK;
LcpConfigSlotId g_write_target = LCP_CONFIG_SLOT_NONE;
LcpConfigSlotId g_erase_first = LCP_CONFIG_SLOT_NONE;
LcpConfigSlotId g_erase_second = LCP_CONFIG_SLOT_NONE;
uint32_t g_active_sequence = 0U;
uint32_t g_active_crc32 = 0U;
uint32_t g_generation = 1U;
uint32_t g_write_sequence = 0U;
uint8_t g_auto_import_pending = 0U;
uint8_t g_record_page[SAM3X_INTERNAL_FLASH_PAGE_SIZE];
uint8_t g_commit_page[SAM3X_INTERNAL_FLASH_PAGE_SIZE];
uint8_t g_erased_page[SAM3X_INTERNAL_FLASH_PAGE_SIZE];

uint8_t slot_index(LcpConfigSlotId slot_id)
{
    return (slot_id == LCP_CONFIG_SLOT_B) ? 1U : 0U;
}

LcpConfigSlotId other_slot(LcpConfigSlotId slot_id)
{
    return (slot_id == LCP_CONFIG_SLOT_A) ?
        LCP_CONFIG_SLOT_B : LCP_CONFIG_SLOT_A;
}

uintptr_t slot_base(LcpConfigSlotId slot_id)
{
    const uintptr_t start = sam3x_internal_flash_config_start();
    return start + (static_cast<uintptr_t>(slot_index(slot_id)) *
                    CONFIG_SLOT_SIZE);
}

uintptr_t slot_commit_page(LcpConfigSlotId slot_id)
{
    return slot_base(slot_id) + CONFIG_SLOT_SIZE -
           SAM3X_INTERNAL_FLASH_PAGE_SIZE;
}

uint32_t record_header_crc32(const ConfigRecordHeader& header)
{
    ConfigRecordHeader copy = header;
    copy.header_crc32 = 0U;
    return lcp_crc32(&copy, sizeof(copy));
}

uint32_t commit_record_crc32(const ConfigCommitRecord& commit)
{
    ConfigCommitRecord copy = commit;
    copy.commit_crc32 = 0U;
    return lcp_crc32(&copy, sizeof(copy));
}

void reset_slot_status(LcpConfigSlotStatus& status)
{
    status.valid = 0U;
    status.sequence = 0U;
    status.payload_crc32 = 0U;
    status.flash_result = SAM3X_INTERNAL_FLASH_OK;
}

void reset_candidate_status(void)
{
    memset(&g_candidate_status, 0, sizeof(g_candidate_status));
    g_candidate_status.report.result = LCP_CONFIG_SD_NOT_ATTEMPTED;
    g_candidate_status.report.sd_result = SD_CONFIG_NOT_ATTEMPTED;
    g_candidate_status.report.x2x_result = X2X_CONFIG_OK;
}

uint8_t read_slot(LcpConfigSlotId slot_id)
{
    const uint8_t index = slot_index(slot_id);
    LcpConfigSlotStatus& status = g_slot_status[index];
    reset_slot_status(status);

    ConfigRecordHeader header;
    g_last_flash_result = sam3x_internal_flash_read(slot_base(slot_id),
                                                    &header,
                                                    sizeof(header));
    status.flash_result = g_last_flash_result;

    if (g_last_flash_result != SAM3X_INTERNAL_FLASH_OK)
    {
        return 0U;
    }

    if ((header.magic != CONFIG_STORAGE_MAGIC) ||
        (header.storage_version != CONFIG_STORAGE_VERSION) ||
        (header.schema_version != CONFIG_SCHEMA_VERSION) ||
        (header.sequence == 0U) ||
        (header.payload_size != sizeof(LcpConfigBundle)) ||
        (header.header_crc32 != record_header_crc32(header)))
    {
        return 0U;
    }

    LcpConfigBundle bundle;
    g_last_flash_result = sam3x_internal_flash_read(
        slot_base(slot_id) + sizeof(ConfigRecordHeader),
        &bundle,
        sizeof(bundle));
    status.flash_result = g_last_flash_result;

    if (g_last_flash_result != SAM3X_INTERNAL_FLASH_OK)
    {
        return 0U;
    }

    if ((lcp_config_bundle_validate(bundle) == 0U) ||
        (header.payload_crc32 != lcp_config_bundle_crc32(bundle)))
    {
        return 0U;
    }

    ConfigCommitRecord commit;
    g_last_flash_result = sam3x_internal_flash_read(slot_commit_page(slot_id),
                                                    &commit,
                                                    sizeof(commit));
    status.flash_result = g_last_flash_result;

    if (g_last_flash_result != SAM3X_INTERNAL_FLASH_OK)
    {
        return 0U;
    }

    if ((commit.magic != CONFIG_COMMIT_MAGIC) ||
        (commit.storage_version != CONFIG_STORAGE_VERSION) ||
        (commit.schema_version != CONFIG_SCHEMA_VERSION) ||
        (commit.sequence != header.sequence) ||
        (commit.payload_crc32 != header.payload_crc32) ||
        (commit.header_crc32 != header.header_crc32) ||
        (commit.commit_crc32 != commit_record_crc32(commit)))
    {
        return 0U;
    }

    g_slot_bundle[index] = bundle;
    status.valid = 1U;
    status.sequence = header.sequence;
    status.payload_crc32 = header.payload_crc32;
    return 1U;
}

uint8_t sequence_newer(uint32_t left, uint32_t right)
{
    return (static_cast<int32_t>(left - right) > 0) ? 1U : 0U;
}

void increment_generation(void)
{
    ++g_generation;

    if (g_generation == 0U)
    {
        g_generation = 1U;
    }
}

void activate_defaults(void)
{
    lcp_config_bundle_set_defaults(g_active_bundle);
    g_source = LCP_CONFIG_SOURCE_DEFAULTS;
    g_active_slot = LCP_CONFIG_SLOT_NONE;
    g_active_sequence = 0U;
    g_active_crc32 = lcp_config_bundle_crc32(g_active_bundle);
    increment_generation();
}

void activate_slot(LcpConfigSlotId slot_id)
{
    const uint8_t index = slot_index(slot_id);
    g_active_bundle = g_slot_bundle[index];
    g_source = LCP_CONFIG_SOURCE_INTERNAL_FLASH;
    g_active_slot = slot_id;
    g_active_sequence = g_slot_status[index].sequence;
    g_active_crc32 = g_slot_status[index].payload_crc32;
    increment_generation();
}

uint32_t next_sequence(void)
{
    uint32_t newest = 0U;

    for (uint8_t index = 0U; index < CONFIG_SLOT_COUNT; ++index)
    {
        if ((g_slot_status[index].valid != 0U) &&
            ((newest == 0U) ||
             (sequence_newer(g_slot_status[index].sequence, newest) != 0U)))
        {
            newest = g_slot_status[index].sequence;
        }
    }

    ++newest;
    return (newest == 0U) ? 1U : newest;
}

void build_write_pages(const LcpConfigBundle& bundle, uint32_t sequence)
{
    memset(g_record_page, 0xFF, sizeof(g_record_page));
    memset(g_commit_page, 0xFF, sizeof(g_commit_page));

    ConfigRecordHeader header;
    memset(&header, 0, sizeof(header));
    header.magic = CONFIG_STORAGE_MAGIC;
    header.storage_version = CONFIG_STORAGE_VERSION;
    header.schema_version = CONFIG_SCHEMA_VERSION;
    header.sequence = sequence;
    header.payload_size = sizeof(LcpConfigBundle);
    header.payload_crc32 = lcp_config_bundle_crc32(bundle);
    header.header_crc32 = record_header_crc32(header);

    memcpy(g_record_page, &header, sizeof(header));
    memcpy(g_record_page + sizeof(header), &bundle, sizeof(bundle));

    ConfigCommitRecord commit;
    memset(&commit, 0, sizeof(commit));
    commit.magic = CONFIG_COMMIT_MAGIC;
    commit.storage_version = CONFIG_STORAGE_VERSION;
    commit.schema_version = CONFIG_SCHEMA_VERSION;
    commit.sequence = sequence;
    commit.payload_crc32 = header.payload_crc32;
    commit.header_crc32 = header.header_crc32;
    commit.commit_crc32 = commit_record_crc32(commit);
    memcpy(g_commit_page, &commit, sizeof(commit));
}

void finish_operation(LcpConfigServiceResult result)
{
    g_result = result;
    g_step = CONFIG_STEP_IDLE;
}

void begin_write(const LcpConfigBundle& bundle)
{
    g_write_target = (g_active_slot == LCP_CONFIG_SLOT_NONE) ?
        LCP_CONFIG_SLOT_A : other_slot(g_active_slot);
    g_write_sequence = next_sequence();
    build_write_pages(bundle, g_write_sequence);
    g_step = CONFIG_STEP_WRITE_RECORD;
}

LcpConfigServiceResult begin_sd_operation(LcpConfigOperation operation)
{
    if (g_step != CONFIG_STEP_IDLE)
    {
        return LCP_CONFIG_SERVICE_BUSY;
    }

    g_auto_import_pending = 0U;
    g_operation = operation;
    g_result = LCP_CONFIG_SERVICE_IN_PROGRESS;
    reset_candidate_status();
    g_step = CONFIG_STEP_SD_READ_FIRST;
    return g_result;
}

void poll_sd_first(void)
{
    LcpConfigSdReport report;
    const LcpConfigSdResult result =
        lcp_config_bundle_load_sd(g_sd_first_bundle, report);

    if (result != LCP_CONFIG_SD_OK)
    {
        g_candidate_status.report = report;
        finish_operation(LCP_CONFIG_SERVICE_SD_ERROR);
        return;
    }

    g_step = CONFIG_STEP_SD_READ_SECOND;
}

void poll_sd_second(void)
{
    LcpConfigBundle second;
    LcpConfigSdReport report;
    const LcpConfigSdResult result = lcp_config_bundle_load_sd(second, report);

    if (result != LCP_CONFIG_SD_OK)
    {
        g_candidate_status.report = report;
        finish_operation(LCP_CONFIG_SERVICE_SD_ERROR);
        return;
    }

    const uint32_t first_crc = lcp_config_bundle_crc32(g_sd_first_bundle);
    const uint32_t second_crc = lcp_config_bundle_crc32(second);

    if ((first_crc != second_crc) ||
        (lcp_config_bundle_equal(g_sd_first_bundle, second) == 0U))
    {
        g_candidate_status.report = report;
        g_candidate_status.available = 1U;
        g_candidate_status.stable = 0U;
        g_candidate_status.crc32 = second_crc;
        finish_operation(LCP_CONFIG_SERVICE_SD_UNSTABLE);
        return;
    }

    g_candidate_bundle = second;
    g_candidate_status.available = 1U;
    g_candidate_status.stable = 1U;
    g_candidate_status.crc32 = second_crc;
    g_candidate_status.equal_to_active =
        lcp_config_bundle_equal(g_candidate_bundle, g_active_bundle);
    g_candidate_status.report = report;

    if ((g_operation == LCP_CONFIG_OPERATION_SCAN_SD) ||
        (g_operation == LCP_CONFIG_OPERATION_COMPARE_SD))
    {
        finish_operation(LCP_CONFIG_SERVICE_OK);
        return;
    }

    if (((g_operation == LCP_CONFIG_OPERATION_IMPORT_SD) ||
         (g_operation == LCP_CONFIG_OPERATION_AUTO_IMPORT)) &&
        (g_source == LCP_CONFIG_SOURCE_INTERNAL_FLASH) &&
        (g_candidate_status.equal_to_active != 0U))
    {
        finish_operation(LCP_CONFIG_SERVICE_NO_CHANGE);
        return;
    }

    begin_write(g_candidate_bundle);
}

void poll_write_record(void)
{
    g_last_flash_result = sam3x_internal_flash_erase_write_page(
        slot_base(g_write_target),
        g_record_page);

    if (g_last_flash_result != SAM3X_INTERNAL_FLASH_OK)
    {
        finish_operation(LCP_CONFIG_SERVICE_FLASH_ERROR);
        return;
    }

    g_step = CONFIG_STEP_WRITE_COMMIT;
}

void poll_write_commit(void)
{
    g_last_flash_result = sam3x_internal_flash_erase_write_page(
        slot_commit_page(g_write_target),
        g_commit_page);

    if (g_last_flash_result != SAM3X_INTERNAL_FLASH_OK)
    {
        finish_operation(LCP_CONFIG_SERVICE_FLASH_ERROR);
        return;
    }

    g_step = CONFIG_STEP_VERIFY_SLOT;
}

void poll_verify_slot(void)
{
    if ((read_slot(g_write_target) == 0U) ||
        (g_slot_status[slot_index(g_write_target)].sequence !=
         g_write_sequence))
    {
        finish_operation(LCP_CONFIG_SERVICE_VERIFY_ERROR);
        return;
    }

    activate_slot(g_write_target);
    finish_operation(LCP_CONFIG_SERVICE_OK);
}

void invalidate_slot(LcpConfigSlotId slot_id)
{
    reset_slot_status(g_slot_status[slot_index(slot_id)]);
    memset(&g_slot_bundle[slot_index(slot_id)], 0, sizeof(LcpConfigBundle));
}

void poll_erase_slot(LcpConfigSlotId slot_id, ConfigServiceStep next_step)
{
    g_last_flash_result = sam3x_internal_flash_erase_write_page(
        slot_commit_page(slot_id),
        g_erased_page);

    if (g_last_flash_result != SAM3X_INTERNAL_FLASH_OK)
    {
        finish_operation(LCP_CONFIG_SERVICE_FLASH_ERROR);
        return;
    }

    invalidate_slot(slot_id);
    g_step = next_step;
}

void start_auto_import_if_ready(void)
{
    if ((g_auto_import_pending != 0U) &&
        (g_step == CONFIG_STEP_IDLE) &&
        (lcp_sd_storage_ready() != 0U))
    {
        (void)begin_sd_operation(LCP_CONFIG_OPERATION_AUTO_IMPORT);
    }
}
}

void lcp_config_service_init(void)
{
    if (sam3x_internal_flash_config_size() != CONFIG_RESERVED_TOTAL_BYTES)
    {
        memset(g_slot_status, 0, sizeof(g_slot_status));
        activate_defaults();
        g_operation = LCP_CONFIG_OPERATION_NONE;
        g_result = LCP_CONFIG_SERVICE_INVALID_STATE;
        g_step = CONFIG_STEP_IDLE;
        g_auto_import_pending = 0U;
        return;
    }

    memset(g_slot_bundle, 0, sizeof(g_slot_bundle));
    memset(g_erased_page, 0xFF, sizeof(g_erased_page));
    reset_candidate_status();
    (void)read_slot(LCP_CONFIG_SLOT_A);
    (void)read_slot(LCP_CONFIG_SLOT_B);

    const uint8_t a_valid = g_slot_status[0].valid;
    const uint8_t b_valid = g_slot_status[1].valid;

    if ((a_valid != 0U) && (b_valid != 0U))
    {
        activate_slot(
            (sequence_newer(g_slot_status[1].sequence,
                            g_slot_status[0].sequence) != 0U)
                ? LCP_CONFIG_SLOT_B
                : LCP_CONFIG_SLOT_A);
    }
    else if (a_valid != 0U)
    {
        activate_slot(LCP_CONFIG_SLOT_A);
    }
    else if (b_valid != 0U)
    {
        activate_slot(LCP_CONFIG_SLOT_B);
    }
    else
    {
        activate_defaults();
    }

    g_operation = LCP_CONFIG_OPERATION_NONE;
    g_result = LCP_CONFIG_SERVICE_IDLE;
    g_step = CONFIG_STEP_IDLE;
    g_last_flash_result = SAM3X_INTERNAL_FLASH_OK;
    g_auto_import_pending =
        (g_source == LCP_CONFIG_SOURCE_DEFAULTS) ? 1U : 0U;
}

void lcp_config_service_poll(void)
{
    start_auto_import_if_ready();

    switch (g_step)
    {
        case CONFIG_STEP_SD_READ_FIRST:
            poll_sd_first();
            break;
        case CONFIG_STEP_SD_READ_SECOND:
            poll_sd_second();
            break;
        case CONFIG_STEP_WRITE_RECORD:
            poll_write_record();
            break;
        case CONFIG_STEP_WRITE_COMMIT:
            poll_write_commit();
            break;
        case CONFIG_STEP_VERIFY_SLOT:
            poll_verify_slot();
            break;
        case CONFIG_STEP_ERASE_FIRST:
            poll_erase_slot(g_erase_first, CONFIG_STEP_ERASE_SECOND);
            break;
        case CONFIG_STEP_ERASE_SECOND:
            poll_erase_slot(g_erase_second, CONFIG_STEP_IDLE);

            if (g_step == CONFIG_STEP_IDLE)
            {
                activate_defaults();
                g_auto_import_pending = 0U;
                g_result = LCP_CONFIG_SERVICE_OK;
            }
            break;
        case CONFIG_STEP_IDLE:
        default:
            break;
    }
}

const LcpConfigBundle& lcp_config_service_active(void)
{
    return g_active_bundle;
}

uint32_t lcp_config_service_generation(void)
{
    return g_generation;
}

LcpConfigSource lcp_config_service_source(void)
{
    return g_source;
}

LcpConfigSlotId lcp_config_service_active_slot(void)
{
    return g_active_slot;
}

uint32_t lcp_config_service_active_sequence(void)
{
    return g_active_sequence;
}

uint32_t lcp_config_service_active_crc32(void)
{
    return g_active_crc32;
}

const LcpConfigSlotStatus& lcp_config_service_slot_status(
    LcpConfigSlotId slot_id)
{
    static const LcpConfigSlotStatus invalid =
    {
        0U,
        0U,
        0U,
        SAM3X_INTERNAL_FLASH_INVALID_ARGUMENT
    };

    if ((slot_id != LCP_CONFIG_SLOT_A) &&
        (slot_id != LCP_CONFIG_SLOT_B))
    {
        return invalid;
    }

    return g_slot_status[slot_index(slot_id)];
}

const LcpConfigCandidateStatus& lcp_config_service_candidate_status(void)
{
    return g_candidate_status;
}

LcpConfigOperation lcp_config_service_operation(void)
{
    return g_operation;
}

LcpConfigServiceResult lcp_config_service_result(void)
{
    return g_result;
}

Sam3xInternalFlashResult lcp_config_service_last_flash_result(void)
{
    return g_last_flash_result;
}

uint8_t lcp_config_service_busy(void)
{
    return (g_step != CONFIG_STEP_IDLE) ? 1U : 0U;
}

LcpConfigServiceResult lcp_config_service_request_scan_sd(void)
{
    return begin_sd_operation(LCP_CONFIG_OPERATION_SCAN_SD);
}

LcpConfigServiceResult lcp_config_service_request_compare_sd(void)
{
    return begin_sd_operation(LCP_CONFIG_OPERATION_COMPARE_SD);
}

LcpConfigServiceResult lcp_config_service_request_import_sd(void)
{
    return begin_sd_operation(LCP_CONFIG_OPERATION_IMPORT_SD);
}

LcpConfigServiceResult lcp_config_service_request_rollback(void)
{
    if (g_step != CONFIG_STEP_IDLE)
    {
        return LCP_CONFIG_SERVICE_BUSY;
    }

    if (g_active_slot == LCP_CONFIG_SLOT_NONE)
    {
        g_operation = LCP_CONFIG_OPERATION_ROLLBACK;
        g_result = LCP_CONFIG_SERVICE_NO_ROLLBACK;
        return g_result;
    }

    const LcpConfigSlotId previous = other_slot(g_active_slot);

    if (g_slot_status[slot_index(previous)].valid == 0U)
    {
        g_operation = LCP_CONFIG_OPERATION_ROLLBACK;
        g_result = LCP_CONFIG_SERVICE_NO_ROLLBACK;
        return g_result;
    }

    g_operation = LCP_CONFIG_OPERATION_ROLLBACK;
    g_result = LCP_CONFIG_SERVICE_IN_PROGRESS;
    begin_write(g_slot_bundle[slot_index(previous)]);
    return g_result;
}

LcpConfigServiceResult lcp_config_service_request_erase(void)
{
    if (g_step != CONFIG_STEP_IDLE)
    {
        return LCP_CONFIG_SERVICE_BUSY;
    }

    g_operation = LCP_CONFIG_OPERATION_ERASE;
    g_result = LCP_CONFIG_SERVICE_IN_PROGRESS;
    g_auto_import_pending = 0U;

    if (g_active_slot == LCP_CONFIG_SLOT_A)
    {
        g_erase_first = LCP_CONFIG_SLOT_B;
        g_erase_second = LCP_CONFIG_SLOT_A;
    }
    else if (g_active_slot == LCP_CONFIG_SLOT_B)
    {
        g_erase_first = LCP_CONFIG_SLOT_A;
        g_erase_second = LCP_CONFIG_SLOT_B;
    }
    else
    {
        g_erase_first = LCP_CONFIG_SLOT_A;
        g_erase_second = LCP_CONFIG_SLOT_B;
    }

    g_step = CONFIG_STEP_ERASE_FIRST;
    return g_result;
}

const LcpConfigBundle* lcp_config_service_candidate(void)
{
    return ((g_candidate_status.available != 0U) &&
            (g_candidate_status.stable != 0U))
        ? &g_candidate_bundle
        : 0;
}

const char* lcp_config_source_text(LcpConfigSource source)
{
    return (source == LCP_CONFIG_SOURCE_INTERNAL_FLASH)
        ? "internal Flash"
        : "defaults";
}

const char* lcp_config_slot_text(LcpConfigSlotId slot_id)
{
    switch (slot_id)
    {
        case LCP_CONFIG_SLOT_A:
            return "A";
        case LCP_CONFIG_SLOT_B:
            return "B";
        case LCP_CONFIG_SLOT_NONE:
        default:
            return "none";
    }
}

const char* lcp_config_operation_text(LcpConfigOperation operation)
{
    switch (operation)
    {
        case LCP_CONFIG_OPERATION_AUTO_IMPORT:
            return "automatic SD import";
        case LCP_CONFIG_OPERATION_SCAN_SD:
            return "SD scan";
        case LCP_CONFIG_OPERATION_COMPARE_SD:
            return "SD compare";
        case LCP_CONFIG_OPERATION_IMPORT_SD:
            return "SD import";
        case LCP_CONFIG_OPERATION_ROLLBACK:
            return "rollback";
        case LCP_CONFIG_OPERATION_ERASE:
            return "erase";
        case LCP_CONFIG_OPERATION_NONE:
        default:
            return "none";
    }
}

const char* lcp_config_service_result_text(LcpConfigServiceResult result)
{
    switch (result)
    {
        case LCP_CONFIG_SERVICE_IDLE:
            return "idle";
        case LCP_CONFIG_SERVICE_IN_PROGRESS:
            return "in progress";
        case LCP_CONFIG_SERVICE_OK:
            return "ok";
        case LCP_CONFIG_SERVICE_NO_CHANGE:
            return "no change";
        case LCP_CONFIG_SERVICE_BUSY:
            return "busy";
        case LCP_CONFIG_SERVICE_SD_ERROR:
            return "SD error";
        case LCP_CONFIG_SERVICE_SD_UNSTABLE:
            return "SD changed between reads";
        case LCP_CONFIG_SERVICE_FLASH_ERROR:
            return "Flash error";
        case LCP_CONFIG_SERVICE_VERIFY_ERROR:
            return "verify error";
        case LCP_CONFIG_SERVICE_NO_ROLLBACK:
            return "no rollback slot";
        case LCP_CONFIG_SERVICE_INVALID_STATE:
            return "invalid storage layout";
        default:
            return "unknown";
    }
}
