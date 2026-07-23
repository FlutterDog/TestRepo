/**
 * @file lcp_config_usb_transport.hpp
 * @brief Машинный USB CDC-протокол чтения и записи конфигурации LCP2116.
 *
 * Header-only реализация включается только из app.cpp. Это позволяет оставить
 * проект Microchip Studio без отдельной translation unit. Обычная текстовая
 * консоль продолжает работать на том же USB CDC-порту. Бинарный режим
 * активируется кадром, начинающимся с нулевого байта и сигнатуры "LCP".
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lcp_config_bundle.hpp"
#include "lcp_config_service.hpp"
#include "../../hal/sam3x_device.hpp"
#include "../../hal/sam3x_internal_flash.hpp"
#include "../../libs/lcp_crc32/lcp_crc32.hpp"
#include "../../platform/platform.hpp"

namespace lcp_config_usb
{
static const uint8_t PROTOCOL_VERSION = 1U;
static const uint16_t CONFIG_SCHEMA_VERSION = 1U;
static const uint16_t STORAGE_FORMAT_VERSION = 1U;
static const uint16_t FRAME_HEADER_BYTES = 24U;
static const uint16_t MAX_PAYLOAD_BYTES = 160U;
static const uint16_t MAX_FRAME_BYTES = FRAME_HEADER_BYTES + MAX_PAYLOAD_BYTES;
static const uint16_t RX_BYTES_PER_POLL = 64U;
static const uint16_t TX_BYTES_PER_POLL = 64U;
static const uint32_t CONFIG_RESERVED_TOTAL_BYTES = 32U * 1024U;
static const uint32_t CONFIG_SLOT_BYTES = CONFIG_RESERVED_TOTAL_BYTES / 2U;
static const uint32_t STORAGE_MAGIC = 0x4C434346UL; /* LCCF */
static const uint32_t COMMIT_MAGIC = 0x4C434343UL;  /* LCCC */
static const uint32_t RESET_DELAY_MS = 100U;

static const uint8_t FRAME_MAGIC[4] = { 0x00U, 'L', 'C', 'P' };

enum Command : uint8_t
{
    COMMAND_HELLO = 1U,
    COMMAND_GET_CONFIG = 2U,
    COMMAND_VALIDATE_CONFIG = 3U,
    COMMAND_PUT_CONFIG = 4U,
    COMMAND_GET_STATUS = 5U,
    COMMAND_REBOOT = 6U,
    COMMAND_EXIT = 7U
};

enum Status : uint8_t
{
    STATUS_OK = 0U,
    STATUS_ACCEPTED = 1U,
    STATUS_BUSY = 2U,
    STATUS_NO_CHANGE = 3U,
    STATUS_INVALID_CRC = 4U,
    STATUS_UNSUPPORTED_VERSION = 5U,
    STATUS_UNSUPPORTED_COMMAND = 6U,
    STATUS_INVALID_LENGTH = 7U,
    STATUS_INVALID_SCHEMA = 8U,
    STATUS_INVALID_CONFIG = 9U,
    STATUS_FLASH_ERROR = 10U,
    STATUS_VERIFY_ERROR = 11U,
    STATUS_REBOOT_REQUIRED = 12U,
    STATUS_INTERNAL_ERROR = 13U
};

enum WriterState : uint8_t
{
    WRITER_IDLE = 0U,
    WRITER_WRITE_RECORD = 1U,
    WRITER_WRITE_COMMIT = 2U,
    WRITER_VERIFY = 3U,
    WRITER_COMPLETE = 4U,
    WRITER_ERROR = 5U
};

struct __attribute__((packed)) StorageRecordHeader
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

struct __attribute__((packed)) StorageCommitRecord
{
    uint32_t magic;
    uint16_t storage_version;
    uint16_t schema_version;
    uint32_t sequence;
    uint32_t payload_crc32;
    uint32_t header_crc32;
    uint32_t commit_crc32;
    uint32_t reserved[2];
};

static_assert(sizeof(StorageRecordHeader) == 32U,
              "USB storage record header must remain 32 bytes");
static_assert(sizeof(StorageCommitRecord) == 32U,
              "USB storage commit record must remain 32 bytes");
static_assert((sizeof(StorageRecordHeader) + sizeof(LcpConfigBundle)) <=
              SAM3X_INTERNAL_FLASH_PAGE_SIZE,
              "USB configuration record must fit one Flash page");

struct Runtime
{
    uint8_t active;
    uint8_t exit_after_tx;
    uint8_t reset_after_tx;
    uint8_t reset_pending;
    uint32_t reset_started_ms;

    uint8_t rx_header[FRAME_HEADER_BYTES];
    uint16_t rx_header_length;
    uint8_t rx_payload[MAX_PAYLOAD_BYTES];
    uint16_t rx_payload_length;
    uint32_t rx_expected_payload_length;

    uint8_t tx_frame[MAX_FRAME_BYTES];
    uint16_t tx_length;
    uint16_t tx_offset;

    WriterState writer_state;
    Status writer_status;
    LcpConfigSlotId target_slot;
    uint32_t target_sequence;
    uint32_t pending_crc32;
    Sam3xInternalFlashResult flash_result;
    LcpConfigBundle pending_bundle;
    uint8_t record_page[SAM3X_INTERNAL_FLASH_PAGE_SIZE];
    uint8_t commit_page[SAM3X_INTERNAL_FLASH_PAGE_SIZE];
};

static Runtime g_runtime;

inline void put_u16(uint8_t* output, uint16_t value)
{
    output[0] = static_cast<uint8_t>(value & 0xFFU);
    output[1] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
}

inline void put_u32(uint8_t* output, uint32_t value)
{
    output[0] = static_cast<uint8_t>(value & 0xFFU);
    output[1] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
    output[2] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
    output[3] = static_cast<uint8_t>((value >> 24U) & 0xFFU);
}

inline uint16_t get_u16(const uint8_t* input)
{
    return static_cast<uint16_t>(input[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(input[1]) << 8U);
}

inline uint32_t get_u32(const uint8_t* input)
{
    return static_cast<uint32_t>(input[0]) |
           (static_cast<uint32_t>(input[1]) << 8U) |
           (static_cast<uint32_t>(input[2]) << 16U) |
           (static_cast<uint32_t>(input[3]) << 24U);
}

inline uint8_t frame_magic_valid(const uint8_t* header)
{
    return (memcmp(header, FRAME_MAGIC, sizeof(FRAME_MAGIC)) == 0) ? 1U : 0U;
}

inline uint32_t storage_header_crc(const StorageRecordHeader& header)
{
    StorageRecordHeader copy = header;
    copy.header_crc32 = 0U;
    return lcp_crc32(&copy, sizeof(copy));
}

inline uint32_t storage_commit_crc(const StorageCommitRecord& commit)
{
    StorageCommitRecord copy = commit;
    copy.commit_crc32 = 0U;
    return lcp_crc32(&copy, sizeof(copy));
}

inline uintptr_t slot_base(LcpConfigSlotId slot)
{
    const uint32_t index = (slot == LCP_CONFIG_SLOT_B) ? 1U : 0U;
    return sam3x_internal_flash_config_start() +
           (static_cast<uintptr_t>(index) * CONFIG_SLOT_BYTES);
}

inline uintptr_t slot_commit_page(LcpConfigSlotId slot)
{
    return slot_base(slot) + CONFIG_SLOT_BYTES -
           SAM3X_INTERNAL_FLASH_PAGE_SIZE;
}

inline LcpConfigSlotId inactive_slot(void)
{
    const LcpConfigSlotId active = lcp_config_service_active_slot();

    if (active == LCP_CONFIG_SLOT_A)
    {
        return LCP_CONFIG_SLOT_B;
    }

    return LCP_CONFIG_SLOT_A;
}

inline uint32_t next_sequence(void)
{
    uint32_t sequence = lcp_config_service_active_sequence() + 1U;
    return (sequence == 0U) ? 1U : sequence;
}

inline void reset_rx(void)
{
    g_runtime.rx_header_length = 0U;
    g_runtime.rx_payload_length = 0U;
    g_runtime.rx_expected_payload_length = 0U;
}

inline void reset_transport_session(void)
{
    g_runtime.active = 0U;
    g_runtime.exit_after_tx = 0U;
    g_runtime.tx_length = 0U;
    g_runtime.tx_offset = 0U;
    reset_rx();
}

inline void queue_response(uint8_t command,
                           Status status,
                           uint8_t flags,
                           uint32_t request_id,
                           const uint8_t* payload,
                           uint16_t payload_length)
{
    if ((payload_length > MAX_PAYLOAD_BYTES) ||
        ((payload_length != 0U) && (payload == 0)))
    {
        return;
    }

    uint8_t* header = g_runtime.tx_frame;
    memcpy(header, FRAME_MAGIC, sizeof(FRAME_MAGIC));
    header[4] = PROTOCOL_VERSION;
    header[5] = command;
    header[6] = static_cast<uint8_t>(status);
    header[7] = flags;
    put_u32(&header[8], request_id);
    put_u32(&header[12], payload_length);
    put_u32(&header[16], lcp_crc32(payload, payload_length));
    put_u32(&header[20], lcp_crc32(header, 20U));

    if (payload_length != 0U)
    {
        memcpy(&g_runtime.tx_frame[FRAME_HEADER_BYTES], payload, payload_length);
    }

    g_runtime.tx_length = FRAME_HEADER_BYTES + payload_length;
    g_runtime.tx_offset = 0U;
}

inline void queue_simple_response(uint8_t command,
                                  Status status,
                                  uint32_t request_id)
{
    queue_response(command, status, 0U, request_id, 0, 0U);
}

inline void build_write_pages(const LcpConfigBundle& bundle,
                              uint32_t sequence)
{
    memset(g_runtime.record_page, 0xFF, sizeof(g_runtime.record_page));
    memset(g_runtime.commit_page, 0xFF, sizeof(g_runtime.commit_page));

    StorageRecordHeader header;
    memset(&header, 0, sizeof(header));
    header.magic = STORAGE_MAGIC;
    header.storage_version = STORAGE_FORMAT_VERSION;
    header.schema_version = CONFIG_SCHEMA_VERSION;
    header.sequence = sequence;
    header.payload_size = sizeof(LcpConfigBundle);
    header.payload_crc32 = lcp_config_bundle_crc32(bundle);
    header.header_crc32 = storage_header_crc(header);

    memcpy(g_runtime.record_page, &header, sizeof(header));
    memcpy(g_runtime.record_page + sizeof(header), &bundle, sizeof(bundle));

    StorageCommitRecord commit;
    memset(&commit, 0, sizeof(commit));
    commit.magic = COMMIT_MAGIC;
    commit.storage_version = STORAGE_FORMAT_VERSION;
    commit.schema_version = CONFIG_SCHEMA_VERSION;
    commit.sequence = sequence;
    commit.payload_crc32 = header.payload_crc32;
    commit.header_crc32 = header.header_crc32;
    commit.commit_crc32 = storage_commit_crc(commit);
    memcpy(g_runtime.commit_page, &commit, sizeof(commit));
}

inline uint8_t verify_target_slot(void)
{
    StorageRecordHeader header;
    StorageCommitRecord commit;
    LcpConfigBundle bundle;

    if (sam3x_internal_flash_read(slot_base(g_runtime.target_slot),
                                  &header,
                                  sizeof(header)) !=
        SAM3X_INTERNAL_FLASH_OK)
    {
        return 0U;
    }

    if ((header.magic != STORAGE_MAGIC) ||
        (header.storage_version != STORAGE_FORMAT_VERSION) ||
        (header.schema_version != CONFIG_SCHEMA_VERSION) ||
        (header.sequence != g_runtime.target_sequence) ||
        (header.payload_size != sizeof(LcpConfigBundle)) ||
        (header.payload_crc32 != g_runtime.pending_crc32) ||
        (header.header_crc32 != storage_header_crc(header)))
    {
        return 0U;
    }

    if (sam3x_internal_flash_read(slot_base(g_runtime.target_slot) +
                                  sizeof(StorageRecordHeader),
                                  &bundle,
                                  sizeof(bundle)) !=
        SAM3X_INTERNAL_FLASH_OK)
    {
        return 0U;
    }

    if ((lcp_config_bundle_validate(bundle) == 0U) ||
        (lcp_config_bundle_crc32(bundle) != g_runtime.pending_crc32) ||
        (lcp_config_bundle_equal(bundle, g_runtime.pending_bundle) == 0U))
    {
        return 0U;
    }

    if (sam3x_internal_flash_read(slot_commit_page(g_runtime.target_slot),
                                  &commit,
                                  sizeof(commit)) !=
        SAM3X_INTERNAL_FLASH_OK)
    {
        return 0U;
    }

    return ((commit.magic == COMMIT_MAGIC) &&
            (commit.storage_version == STORAGE_FORMAT_VERSION) &&
            (commit.schema_version == CONFIG_SCHEMA_VERSION) &&
            (commit.sequence == header.sequence) &&
            (commit.payload_crc32 == header.payload_crc32) &&
            (commit.header_crc32 == header.header_crc32) &&
            (commit.commit_crc32 == storage_commit_crc(commit))) ? 1U : 0U;
}

inline void poll_writer(void)
{
    switch (g_runtime.writer_state)
    {
        case WRITER_WRITE_RECORD:
            g_runtime.flash_result = sam3x_internal_flash_erase_write_page(
                slot_base(g_runtime.target_slot),
                g_runtime.record_page);

            if (g_runtime.flash_result != SAM3X_INTERNAL_FLASH_OK)
            {
                g_runtime.writer_status = STATUS_FLASH_ERROR;
                g_runtime.writer_state = WRITER_ERROR;
            }
            else
            {
                g_runtime.writer_state = WRITER_WRITE_COMMIT;
            }
            break;

        case WRITER_WRITE_COMMIT:
            g_runtime.flash_result = sam3x_internal_flash_erase_write_page(
                slot_commit_page(g_runtime.target_slot),
                g_runtime.commit_page);

            if (g_runtime.flash_result != SAM3X_INTERNAL_FLASH_OK)
            {
                g_runtime.writer_status = STATUS_FLASH_ERROR;
                g_runtime.writer_state = WRITER_ERROR;
            }
            else
            {
                g_runtime.writer_state = WRITER_VERIFY;
            }
            break;

        case WRITER_VERIFY:
            if (verify_target_slot() == 0U)
            {
                g_runtime.writer_status = STATUS_VERIFY_ERROR;
                g_runtime.writer_state = WRITER_ERROR;
            }
            else
            {
                g_runtime.writer_status = STATUS_REBOOT_REQUIRED;
                g_runtime.writer_state = WRITER_COMPLETE;
            }
            break;

        case WRITER_IDLE:
        case WRITER_COMPLETE:
        case WRITER_ERROR:
        default:
            break;
    }
}

inline Status decode_config_payload(const uint8_t* payload,
                                    uint16_t payload_length,
                                    LcpConfigBundle& bundle)
{
    static const uint16_t PREFIX_BYTES = 8U;
    const uint16_t expected_length = PREFIX_BYTES + sizeof(LcpConfigBundle);

    if ((payload == 0) || (payload_length != expected_length))
    {
        return STATUS_INVALID_LENGTH;
    }

    const uint16_t schema_version = get_u16(&payload[0]);
    const uint16_t bundle_size = get_u16(&payload[2]);
    const uint32_t expected_crc32 = get_u32(&payload[4]);

    if (schema_version != CONFIG_SCHEMA_VERSION)
    {
        return STATUS_INVALID_SCHEMA;
    }

    if (bundle_size != sizeof(LcpConfigBundle))
    {
        return STATUS_INVALID_LENGTH;
    }

    const uint8_t* bundle_bytes = &payload[PREFIX_BYTES];

    if (lcp_crc32(bundle_bytes, sizeof(LcpConfigBundle)) != expected_crc32)
    {
        return STATUS_INVALID_CRC;
    }

    memcpy(&bundle, bundle_bytes, sizeof(bundle));

    if ((lcp_config_bundle_validate(bundle) == 0U) ||
        (lcp_config_bundle_crc32(bundle) != expected_crc32))
    {
        return STATUS_INVALID_CONFIG;
    }

    return STATUS_OK;
}

inline void handle_hello(uint8_t command, uint32_t request_id)
{
    uint8_t payload[16];
    memset(payload, 0, sizeof(payload));
    put_u16(&payload[0], PROTOCOL_VERSION);
    put_u16(&payload[2], CONFIG_SCHEMA_VERSION);
    put_u16(&payload[4], sizeof(LcpConfigBundle));
    put_u16(&payload[6], MAX_PAYLOAD_BYTES);
    put_u32(&payload[8], 0x0000000FU); /* read, validate, write, reboot */
    put_u16(&payload[12], STORAGE_FORMAT_VERSION);
    put_u16(&payload[14], FRAME_HEADER_BYTES);
    queue_response(command, STATUS_OK, 0U, request_id,
                   payload, sizeof(payload));
}

inline void handle_get_config(uint8_t command, uint32_t request_id)
{
    static const uint16_t METADATA_BYTES = 20U;
    uint8_t payload[METADATA_BYTES + sizeof(LcpConfigBundle)];
    memset(payload, 0, sizeof(payload));

    const LcpConfigBundle& bundle = lcp_config_service_active();
    put_u16(&payload[0], CONFIG_SCHEMA_VERSION);
    put_u16(&payload[2], sizeof(LcpConfigBundle));
    put_u32(&payload[4], lcp_config_service_active_sequence());
    put_u32(&payload[8], lcp_config_service_generation());
    put_u32(&payload[12], lcp_config_bundle_crc32(bundle));
    payload[16] = static_cast<uint8_t>(lcp_config_service_source());
    payload[17] = static_cast<uint8_t>(lcp_config_service_active_slot());
    memcpy(&payload[METADATA_BYTES], &bundle, sizeof(bundle));

    queue_response(command, STATUS_OK, 0U, request_id,
                   payload, sizeof(payload));
}

inline void handle_validate(uint8_t command,
                            uint32_t request_id,
                            const uint8_t* payload,
                            uint16_t payload_length)
{
    LcpConfigBundle bundle;
    const Status result = decode_config_payload(payload, payload_length, bundle);
    queue_simple_response(command, result, request_id);
}

inline void handle_put(uint8_t command,
                       uint32_t request_id,
                       const uint8_t* payload,
                       uint16_t payload_length)
{
    if ((g_runtime.writer_state == WRITER_WRITE_RECORD) ||
        (g_runtime.writer_state == WRITER_WRITE_COMMIT) ||
        (g_runtime.writer_state == WRITER_VERIFY) ||
        (lcp_config_service_busy() != 0U))
    {
        queue_simple_response(command, STATUS_BUSY, request_id);
        return;
    }

    LcpConfigBundle bundle;
    const Status decode_result = decode_config_payload(payload,
                                                       payload_length,
                                                       bundle);

    if (decode_result != STATUS_OK)
    {
        queue_simple_response(command, decode_result, request_id);
        return;
    }

    if (lcp_config_bundle_equal(bundle, lcp_config_service_active()) != 0U)
    {
        g_runtime.writer_state = WRITER_COMPLETE;
        g_runtime.writer_status = STATUS_NO_CHANGE;
        g_runtime.target_slot = lcp_config_service_active_slot();
        g_runtime.target_sequence = lcp_config_service_active_sequence();
        g_runtime.pending_crc32 = lcp_config_service_active_crc32();
        queue_simple_response(command, STATUS_NO_CHANGE, request_id);
        return;
    }

    if (sam3x_internal_flash_config_size() != CONFIG_RESERVED_TOTAL_BYTES)
    {
        queue_simple_response(command, STATUS_INTERNAL_ERROR, request_id);
        return;
    }

    g_runtime.pending_bundle = bundle;
    g_runtime.pending_crc32 = lcp_config_bundle_crc32(bundle);
    g_runtime.target_slot = inactive_slot();
    g_runtime.target_sequence = next_sequence();
    g_runtime.flash_result = SAM3X_INTERNAL_FLASH_OK;
    build_write_pages(bundle, g_runtime.target_sequence);
    g_runtime.writer_status = STATUS_ACCEPTED;
    g_runtime.writer_state = WRITER_WRITE_RECORD;
    queue_simple_response(command, STATUS_ACCEPTED, request_id);
}

inline void handle_get_status(uint8_t command, uint32_t request_id)
{
    uint8_t payload[28];
    memset(payload, 0, sizeof(payload));
    payload[0] = static_cast<uint8_t>(g_runtime.writer_state);
    payload[1] = static_cast<uint8_t>(g_runtime.writer_status);
    payload[2] = static_cast<uint8_t>(lcp_config_service_source());
    payload[3] = static_cast<uint8_t>(lcp_config_service_active_slot());
    payload[4] = static_cast<uint8_t>(g_runtime.target_slot);
    payload[5] = lcp_config_service_busy();
    put_u32(&payload[8], lcp_config_service_active_sequence());
    put_u32(&payload[12], g_runtime.target_sequence);
    put_u32(&payload[16], lcp_config_service_active_crc32());
    put_u32(&payload[20], lcp_config_service_generation());
    payload[24] = static_cast<uint8_t>(g_runtime.flash_result);
    queue_response(command, STATUS_OK, 0U, request_id,
                   payload, sizeof(payload));
}

inline void handle_frame(void)
{
    const uint8_t* header = g_runtime.rx_header;
    const uint8_t command = header[5];
    const uint8_t flags = header[7];
    const uint32_t request_id = get_u32(&header[8]);
    const uint16_t payload_length =
        static_cast<uint16_t>(g_runtime.rx_expected_payload_length);

    if (header[4] != PROTOCOL_VERSION)
    {
        queue_simple_response(command, STATUS_UNSUPPORTED_VERSION, request_id);
        return;
    }

    if (header[6] != 0U)
    {
        queue_simple_response(command, STATUS_INTERNAL_ERROR, request_id);
        return;
    }

    const uint8_t* payload = (payload_length != 0U) ?
        g_runtime.rx_payload : 0;

    switch (command)
    {
        case COMMAND_HELLO:
            if (payload_length == 0U)
            {
                handle_hello(command, request_id);
            }
            else
            {
                queue_simple_response(command, STATUS_INVALID_LENGTH, request_id);
            }
            break;

        case COMMAND_GET_CONFIG:
            if (payload_length == 0U)
            {
                handle_get_config(command, request_id);
            }
            else
            {
                queue_simple_response(command, STATUS_INVALID_LENGTH, request_id);
            }
            break;

        case COMMAND_VALIDATE_CONFIG:
            handle_validate(command, request_id, payload, payload_length);
            break;

        case COMMAND_PUT_CONFIG:
            handle_put(command, request_id, payload, payload_length);
            break;

        case COMMAND_GET_STATUS:
            if (payload_length == 0U)
            {
                handle_get_status(command, request_id);
            }
            else
            {
                queue_simple_response(command, STATUS_INVALID_LENGTH, request_id);
            }
            break;

        case COMMAND_REBOOT:
            if (payload_length == 0U)
            {
                queue_simple_response(command, STATUS_OK, request_id);
                g_runtime.reset_after_tx = 1U;
            }
            else
            {
                queue_simple_response(command, STATUS_INVALID_LENGTH, request_id);
            }
            break;

        case COMMAND_EXIT:
            if (payload_length == 0U)
            {
                queue_simple_response(command, STATUS_OK, request_id);
                g_runtime.exit_after_tx = 1U;
            }
            else
            {
                queue_simple_response(command, STATUS_INVALID_LENGTH, request_id);
            }
            break;

        default:
            (void)flags;
            queue_simple_response(command, STATUS_UNSUPPORTED_COMMAND, request_id);
            break;
    }
}

inline void process_rx_byte(uint8_t value)
{
    if (g_runtime.rx_header_length < FRAME_HEADER_BYTES)
    {
        const uint16_t index = g_runtime.rx_header_length;

        if ((index < sizeof(FRAME_MAGIC)) && (value != FRAME_MAGIC[index]))
        {
            reset_rx();

            if (value == FRAME_MAGIC[0])
            {
                g_runtime.rx_header[0] = value;
                g_runtime.rx_header_length = 1U;
            }

            return;
        }

        g_runtime.rx_header[g_runtime.rx_header_length++] = value;

        if (g_runtime.rx_header_length != FRAME_HEADER_BYTES)
        {
            return;
        }

        if ((frame_magic_valid(g_runtime.rx_header) == 0U) ||
            (get_u32(&g_runtime.rx_header[20]) !=
             lcp_crc32(g_runtime.rx_header, 20U)))
        {
            reset_rx();
            return;
        }

        g_runtime.rx_expected_payload_length =
            get_u32(&g_runtime.rx_header[12]);

        if (g_runtime.rx_expected_payload_length > MAX_PAYLOAD_BYTES)
        {
            const uint8_t command = g_runtime.rx_header[5];
            const uint32_t request_id = get_u32(&g_runtime.rx_header[8]);
            queue_simple_response(command, STATUS_INVALID_LENGTH, request_id);
            reset_rx();
            return;
        }

        if (g_runtime.rx_expected_payload_length == 0U)
        {
            handle_frame();
            reset_rx();
        }

        return;
    }

    if (g_runtime.rx_payload_length < g_runtime.rx_expected_payload_length)
    {
        g_runtime.rx_payload[g_runtime.rx_payload_length++] = value;
    }

    if (g_runtime.rx_payload_length == g_runtime.rx_expected_payload_length)
    {
        const uint32_t expected_crc = get_u32(&g_runtime.rx_header[16]);
        const uint8_t command = g_runtime.rx_header[5];
        const uint32_t request_id = get_u32(&g_runtime.rx_header[8]);

        if (lcp_crc32(g_runtime.rx_payload,
                      g_runtime.rx_payload_length) != expected_crc)
        {
            queue_simple_response(command, STATUS_INVALID_CRC, request_id);
        }
        else
        {
            handle_frame();
        }

        reset_rx();
    }
}

inline void poll_tx(void)
{
    if (g_runtime.tx_offset >= g_runtime.tx_length)
    {
        g_runtime.tx_length = 0U;
        g_runtime.tx_offset = 0U;

        if (g_runtime.exit_after_tx != 0U)
        {
            g_runtime.exit_after_tx = 0U;
            reset_transport_session();
        }

        if (g_runtime.reset_after_tx != 0U)
        {
            g_runtime.reset_after_tx = 0U;
            g_runtime.reset_pending = 1U;
            g_runtime.reset_started_ms = millis();
        }

        return;
    }

    const int available = SerialUSB.availableForWrite();

    if (available <= 0)
    {
        return;
    }

    uint16_t remaining = g_runtime.tx_length - g_runtime.tx_offset;
    uint16_t chunk = (remaining < TX_BYTES_PER_POLL) ?
        remaining : TX_BYTES_PER_POLL;

    if (chunk > static_cast<uint16_t>(available))
    {
        chunk = static_cast<uint16_t>(available);
    }

    const size_t written = SerialUSB.write(
        &g_runtime.tx_frame[g_runtime.tx_offset], chunk);
    g_runtime.tx_offset += static_cast<uint16_t>(written);
}

inline void init(void)
{
    memset(&g_runtime, 0, sizeof(g_runtime));
    g_runtime.writer_state = WRITER_IDLE;
    g_runtime.writer_status = STATUS_OK;
    g_runtime.target_slot = LCP_CONFIG_SLOT_NONE;
    g_runtime.flash_result = SAM3X_INTERNAL_FLASH_OK;
}

inline uint8_t active(void)
{
    return g_runtime.active;
}

inline uint8_t flash_busy(void)
{
    return ((g_runtime.writer_state == WRITER_WRITE_RECORD) ||
            (g_runtime.writer_state == WRITER_WRITE_COMMIT) ||
            (g_runtime.writer_state == WRITER_VERIFY)) ? 1U : 0U;
}

inline void poll(void)
{
    poll_writer();

    if (g_runtime.reset_pending != 0U)
    {
        if ((uint32_t)(millis() - g_runtime.reset_started_ms) >= RESET_DELAY_MS)
        {
            NVIC_SystemReset();
        }

        return;
    }

    if (!SerialUSB)
    {
        reset_transport_session();
        return;
    }

    if (g_runtime.active == 0U)
    {
        if ((SerialUSB.available() <= 0) ||
            (SerialUSB.peek() != FRAME_MAGIC[0]))
        {
            return;
        }

        g_runtime.active = 1U;
        reset_rx();
    }

    poll_tx();

    if (g_runtime.tx_length != 0U)
    {
        return;
    }

    uint16_t processed = 0U;

    while ((SerialUSB.available() > 0) &&
           (processed < RX_BYTES_PER_POLL) &&
           (g_runtime.tx_length == 0U))
    {
        const int value = SerialUSB.read();

        if (value >= 0)
        {
            process_rx_byte(static_cast<uint8_t>(value));
            ++processed;
        }
    }

    poll_tx();
}
}
