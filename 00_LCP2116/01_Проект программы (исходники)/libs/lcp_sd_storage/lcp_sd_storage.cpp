
/**
 * @file lcp_sd_storage.cpp
 * @brief Минимальный SPI/FAT слой microSD для диагностической прошивки LCP.
 */

#include "lcp_sd_storage.hpp"
#include "../../board/lcp_sd.hpp"
#include "../../platform/platform.hpp"

#include <string.h>

static const uint16_t SD_BLOCK_SIZE = 512U;

static const uint8_t SD_CMD0 = 0U;
static const uint8_t SD_CMD8 = 8U;
static const uint8_t SD_CMD17 = 17U;
static const uint8_t SD_CMD24 = 24U;
static const uint8_t SD_CMD55 = 55U;
static const uint8_t SD_CMD58 = 58U;
static const uint8_t SD_ACMD41 = 41U;

static const uint8_t SD_TOKEN_SINGLE_READ = 0xFEU;
static const uint8_t SD_TOKEN_SINGLE_WRITE = 0xFEU;

static const uint8_t SD_CARD_TYPE_UNKNOWN = 0U;
static const uint8_t SD_CARD_TYPE_SDSC = 1U;
static const uint8_t SD_CARD_TYPE_SDHC = 2U;

static const uint8_t FAT_TYPE_NONE = 0U;
static const uint8_t FAT_TYPE_16 = 16U;
static const uint8_t FAT_TYPE_32 = 32U;

static const uint8_t DIR_ATTR_LONG_NAME = 0x0FU;
static const uint8_t DIR_ATTR_ARCHIVE = 0x20U;

static const uint32_t FAT16_EOC = 0xFFF8UL;
static const uint32_t FAT32_EOC = 0x0FFFFFF8UL;

struct LcpSdVolume
{
    uint8_t ready;
    uint8_t card_type;
    uint8_t fat_type;
    uint8_t sectors_per_cluster;
    uint8_t fat_count;

    uint16_t root_entry_count;
    uint32_t partition_lba;
    uint32_t fat_start_lba;
    uint32_t fat_size_sectors;
    uint32_t root_dir_lba;
    uint32_t root_dir_sectors;
    uint32_t data_start_lba;
    uint32_t root_cluster;
    uint32_t cluster_count;
};

struct LcpSdDirEntryLocation
{
    uint32_t sector_lba;
    uint16_t offset;
    uint8_t found;
};

static LcpSdVolume g_volume;
static uint8_t g_sector[SD_BLOCK_SIZE];

static uint8_t spi_transfer(uint8_t value)
{
    return SPI.transfer(value);
}

static void spi_clock_idle_bytes(uint16_t count)
{
    lcp_sd_deselect();

    for (uint16_t index = 0U; index < count; ++index)
    {
        (void)spi_transfer(0xFFU);
    }
}

static uint8_t wait_not_busy(uint32_t guard)
{
    while (guard != 0U)
    {
        if (spi_transfer(0xFFU) == 0xFFU)
        {
            return 1U;
        }

        --guard;
    }

    return 0U;
}

static uint8_t send_command(uint8_t command, uint32_t argument)
{
    uint8_t crc = 0xFFU;

    if (command == SD_CMD0)
    {
        crc = 0x95U;
    }
    else if (command == SD_CMD8)
    {
        crc = 0x87U;
    }

    lcp_sd_select();

    (void)wait_not_busy(50000UL);

    (void)spi_transfer(static_cast<uint8_t>(0x40U | command));
    (void)spi_transfer(static_cast<uint8_t>(argument >> 24U));
    (void)spi_transfer(static_cast<uint8_t>(argument >> 16U));
    (void)spi_transfer(static_cast<uint8_t>(argument >> 8U));
    (void)spi_transfer(static_cast<uint8_t>(argument));
    (void)spi_transfer(crc);

    for (uint16_t index = 0U; index < 16U; ++index)
    {
        const uint8_t response = spi_transfer(0xFFU);

        if ((response & 0x80U) == 0U)
        {
            return response;
        }
    }

    return 0xFFU;
}

static void end_command(void)
{
    lcp_sd_deselect();
    (void)spi_transfer(0xFFU);
}

static uint8_t send_acmd(uint8_t command, uint32_t argument)
{
    uint8_t response = send_command(SD_CMD55, 0U);
    end_command();

    if (response > 0x01U)
    {
        return response;
    }

    response = send_command(command, argument);
    return response;
}

static uint8_t read_ocr(uint8_t* ocr)
{
    const uint8_t response = send_command(SD_CMD58, 0U);

    if (response != 0U)
    {
        end_command();
        return 0U;
    }

    for (uint8_t index = 0U; index < 4U; ++index)
    {
        ocr[index] = spi_transfer(0xFFU);
    }

    end_command();
    return 1U;
}

static uint8_t init_card(void)
{
    uint8_t response;
    uint8_t ocr[4];

    g_volume.card_type = SD_CARD_TYPE_UNKNOWN;

    spi_clock_idle_bytes(10U);

    response = send_command(SD_CMD0, 0U);
    end_command();

    if (response != 0x01U)
    {
        return 0U;
    }

    response = send_command(SD_CMD8, 0x000001AAUL);

    if (response == 0x01U)
    {
        uint8_t r7[4];

        for (uint8_t index = 0U; index < 4U; ++index)
        {
            r7[index] = spi_transfer(0xFFU);
        }

        end_command();

        if ((r7[2] != 0x01U) || (r7[3] != 0xAAU))
        {
            return 0U;
        }

        for (uint32_t guard = 0U; guard < 50000UL; ++guard)
        {
            response = send_acmd(SD_ACMD41, 0x40000000UL);
            end_command();

            if (response == 0U)
            {
                break;
            }
        }

        if (response != 0U)
        {
            return 0U;
        }

        if (read_ocr(ocr) == 0U)
        {
            return 0U;
        }

        g_volume.card_type = ((ocr[0] & 0x40U) != 0U) ? SD_CARD_TYPE_SDHC : SD_CARD_TYPE_SDSC;
        return 1U;
    }

    end_command();

    for (uint32_t guard = 0U; guard < 50000UL; ++guard)
    {
        response = send_acmd(SD_ACMD41, 0U);
        end_command();

        if (response == 0U)
        {
            g_volume.card_type = SD_CARD_TYPE_SDSC;
            return 1U;
        }
    }

    return 0U;
}

static uint32_t card_address(uint32_t sector_lba)
{
    if (g_volume.card_type == SD_CARD_TYPE_SDHC)
    {
        return sector_lba;
    }

    return sector_lba * SD_BLOCK_SIZE;
}

static uint8_t read_sector(uint32_t sector_lba, uint8_t* buffer)
{
    uint8_t response = send_command(SD_CMD17, card_address(sector_lba));

    if (response != 0U)
    {
        end_command();
        return 0U;
    }

    for (uint32_t guard = 0U; guard < 100000UL; ++guard)
    {
        const uint8_t token = spi_transfer(0xFFU);

        if (token == SD_TOKEN_SINGLE_READ)
        {
            for (uint16_t index = 0U; index < SD_BLOCK_SIZE; ++index)
            {
                buffer[index] = spi_transfer(0xFFU);
            }

            (void)spi_transfer(0xFFU);
            (void)spi_transfer(0xFFU);
            end_command();

            return 1U;
        }
    }

    end_command();
    return 0U;
}

static uint8_t write_sector(uint32_t sector_lba, const uint8_t* buffer)
{
    uint8_t response = send_command(SD_CMD24, card_address(sector_lba));

    if (response != 0U)
    {
        end_command();
        return 0U;
    }

    (void)spi_transfer(0xFFU);
    (void)spi_transfer(SD_TOKEN_SINGLE_WRITE);

    for (uint16_t index = 0U; index < SD_BLOCK_SIZE; ++index)
    {
        (void)spi_transfer(buffer[index]);
    }

    (void)spi_transfer(0xFFU);
    (void)spi_transfer(0xFFU);

    response = spi_transfer(0xFFU);

    if ((response & 0x1FU) != 0x05U)
    {
        end_command();
        return 0U;
    }

    if (wait_not_busy(500000UL) == 0U)
    {
        end_command();
        return 0U;
    }

    end_command();
    return 1U;
}

static uint16_t ld16(const uint8_t* data)
{
    return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8U);
}

static uint32_t ld32(const uint8_t* data)
{
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8U) |
           (static_cast<uint32_t>(data[2]) << 16U) |
           (static_cast<uint32_t>(data[3]) << 24U);
}

static void st16(uint8_t* data, uint16_t value)
{
    data[0] = static_cast<uint8_t>(value);
    data[1] = static_cast<uint8_t>(value >> 8U);
}

static void st32(uint8_t* data, uint32_t value)
{
    data[0] = static_cast<uint8_t>(value);
    data[1] = static_cast<uint8_t>(value >> 8U);
    data[2] = static_cast<uint8_t>(value >> 16U);
    data[3] = static_cast<uint8_t>(value >> 24U);
}

static uint8_t is_valid_boot_sector(const uint8_t* sector)
{
    if ((sector[510] != 0x55U) || (sector[511] != 0xAAU))
    {
        return 0U;
    }

    if (ld16(&sector[11]) != 512U)
    {
        return 0U;
    }

    if (sector[13] == 0U)
    {
        return 0U;
    }

    if (sector[16] == 0U)
    {
        return 0U;
    }

    return 1U;
}

static uint8_t parse_volume(uint32_t partition_lba)
{
    if (read_sector(partition_lba, g_sector) == 0U)
    {
        return 0U;
    }

    if (is_valid_boot_sector(g_sector) == 0U)
    {
        return 0U;
    }

    const uint16_t reserved_sectors = ld16(&g_sector[14]);
    const uint8_t fat_count = g_sector[16];
    const uint16_t root_entry_count = ld16(&g_sector[17]);
    const uint16_t total_sectors_16 = ld16(&g_sector[19]);
    const uint32_t total_sectors_32 = ld32(&g_sector[32]);
    const uint16_t fat_size_16 = ld16(&g_sector[22]);
    const uint32_t fat_size_32 = ld32(&g_sector[36]);

    const uint32_t total_sectors = (total_sectors_16 != 0U) ? total_sectors_16 : total_sectors_32;
    const uint32_t fat_size = (fat_size_16 != 0U) ? fat_size_16 : fat_size_32;
    const uint32_t root_dir_sectors = ((static_cast<uint32_t>(root_entry_count) * 32UL) + 511UL) / 512UL;
    const uint32_t first_data_sector = reserved_sectors + (static_cast<uint32_t>(fat_count) * fat_size) + root_dir_sectors;
    const uint32_t data_sectors = total_sectors - first_data_sector;
    const uint32_t cluster_count = data_sectors / g_sector[13];

    if (cluster_count < 4085UL)
    {
        return 0U;
    }

    g_volume.partition_lba = partition_lba;
    g_volume.sectors_per_cluster = g_sector[13];
    g_volume.fat_count = fat_count;
    g_volume.root_entry_count = root_entry_count;
    g_volume.fat_start_lba = partition_lba + reserved_sectors;
    g_volume.fat_size_sectors = fat_size;
    g_volume.root_dir_lba = partition_lba + reserved_sectors + (static_cast<uint32_t>(fat_count) * fat_size);
    g_volume.root_dir_sectors = root_dir_sectors;
    g_volume.data_start_lba = partition_lba + first_data_sector;
    g_volume.cluster_count = cluster_count;

    if (cluster_count < 65525UL)
    {
        g_volume.fat_type = FAT_TYPE_16;
        g_volume.root_cluster = 0U;
    }
    else
    {
        g_volume.fat_type = FAT_TYPE_32;
        g_volume.root_cluster = ld32(&g_sector[44]);
    }

    g_volume.ready = 1U;
    return 1U;
}

static uint8_t mount_volume(void)
{
    if (read_sector(0U, g_sector) == 0U)
    {
        return 0U;
    }

    if (parse_volume(0U) != 0U)
    {
        return 1U;
    }

    const uint32_t partition_lba = ld32(&g_sector[454]);

    if (partition_lba == 0U)
    {
        return 0U;
    }

    return parse_volume(partition_lba);
}

static uint32_t cluster_to_sector(uint32_t cluster)
{
    return g_volume.data_start_lba + ((cluster - 2UL) * g_volume.sectors_per_cluster);
}

static uint8_t is_eoc(uint32_t value)
{
    if (g_volume.fat_type == FAT_TYPE_16)
    {
        return (value >= FAT16_EOC) ? 1U : 0U;
    }

    return ((value & 0x0FFFFFFFUL) >= FAT32_EOC) ? 1U : 0U;
}

static uint32_t fat_entry_read(uint32_t cluster)
{
    uint32_t fat_offset;

    if (g_volume.fat_type == FAT_TYPE_16)
    {
        fat_offset = cluster * 2UL;
    }
    else
    {
        fat_offset = cluster * 4UL;
    }

    const uint32_t sector_lba = g_volume.fat_start_lba + (fat_offset / SD_BLOCK_SIZE);
    const uint16_t offset = static_cast<uint16_t>(fat_offset % SD_BLOCK_SIZE);

    if (read_sector(sector_lba, g_sector) == 0U)
    {
        return 0x0FFFFFFFUL;
    }

    if (g_volume.fat_type == FAT_TYPE_16)
    {
        return ld16(&g_sector[offset]);
    }

    return ld32(&g_sector[offset]) & 0x0FFFFFFFUL;
}

static uint8_t fat_entry_write_single(uint32_t fat_index, uint32_t cluster, uint32_t value)
{
    uint32_t fat_offset;

    if (g_volume.fat_type == FAT_TYPE_16)
    {
        fat_offset = cluster * 2UL;
    }
    else
    {
        fat_offset = cluster * 4UL;
    }

    const uint32_t sector_lba = g_volume.fat_start_lba +
                                (fat_index * g_volume.fat_size_sectors) +
                                (fat_offset / SD_BLOCK_SIZE);
    const uint16_t offset = static_cast<uint16_t>(fat_offset % SD_BLOCK_SIZE);

    if (read_sector(sector_lba, g_sector) == 0U)
    {
        return 0U;
    }

    if (g_volume.fat_type == FAT_TYPE_16)
    {
        st16(&g_sector[offset], static_cast<uint16_t>(value));
    }
    else
    {
        const uint32_t old_value = ld32(&g_sector[offset]) & 0xF0000000UL;
        st32(&g_sector[offset], old_value | (value & 0x0FFFFFFFUL));
    }

    return write_sector(sector_lba, g_sector);
}

static uint8_t fat_entry_write(uint32_t cluster, uint32_t value)
{
    for (uint8_t fat_index = 0U; fat_index < g_volume.fat_count; ++fat_index)
    {
        if (fat_entry_write_single(fat_index, cluster, value) == 0U)
        {
            return 0U;
        }
    }

    return 1U;
}

static uint8_t make_83_name(const char* file_name, uint8_t name83[11])
{
    uint8_t index = 0U;
    uint8_t ext_index = 8U;
    uint8_t in_extension = 0U;

    for (uint8_t i = 0U; i < 11U; ++i)
    {
        name83[i] = ' ';
    }

    if ((file_name == 0) || (file_name[0] == '\0'))
    {
        return 0U;
    }

    for (uint16_t src = 0U; file_name[src] != '\0'; ++src)
    {
        char c = file_name[src];

        if (c == '.')
        {
            if (in_extension != 0U)
            {
                return 0U;
            }

            in_extension = 1U;
            continue;
        }

        if ((c >= 'a') && (c <= 'z'))
        {
            c = static_cast<char>(c - ('a' - 'A'));
        }

        if (!(((c >= 'A') && (c <= 'Z')) ||
              ((c >= '0') && (c <= '9')) ||
              (c == '_') || (c == '-') || (c == '~')))
        {
            return 0U;
        }

        if (in_extension == 0U)
        {
            if (index >= 8U)
            {
                return 0U;
            }

            name83[index] = static_cast<uint8_t>(c);
            ++index;
        }
        else
        {
            if (ext_index >= 11U)
            {
                return 0U;
            }

            name83[ext_index] = static_cast<uint8_t>(c);
            ++ext_index;
        }
    }

    return (index != 0U) ? 1U : 0U;
}

static uint32_t entry_first_cluster(const uint8_t* entry)
{
    const uint32_t high = ld16(&entry[20]);
    const uint32_t low = ld16(&entry[26]);

    return (high << 16U) | low;
}

static void entry_set_first_cluster(uint8_t* entry, uint32_t cluster)
{
    st16(&entry[20], static_cast<uint16_t>(cluster >> 16U));
    st16(&entry[26], static_cast<uint16_t>(cluster));
}

static uint8_t entry_is_file_match(const uint8_t* entry, const uint8_t name83[11])
{
    if (entry[0] == 0x00U)
    {
        return 0U;
    }

    if (entry[0] == 0xE5U)
    {
        return 0U;
    }

    if ((entry[11] & DIR_ATTR_LONG_NAME) == DIR_ATTR_LONG_NAME)
    {
        return 0U;
    }

    return (memcmp(entry, name83, 11U) == 0) ? 1U : 0U;
}

static uint8_t scan_root_directory(const uint8_t name83[11],
                                   LcpSdDirEntryLocation* found,
                                   LcpSdDirEntryLocation* free_entry,
                                   uint8_t* entry_buffer)
{
    found->found = 0U;
    free_entry->found = 0U;

    if (g_volume.fat_type == FAT_TYPE_16)
    {
        for (uint32_t sector_index = 0U; sector_index < g_volume.root_dir_sectors; ++sector_index)
        {
            const uint32_t sector_lba = g_volume.root_dir_lba + sector_index;

            if (read_sector(sector_lba, g_sector) == 0U)
            {
                return 0U;
            }

            for (uint16_t offset = 0U; offset < SD_BLOCK_SIZE; offset += 32U)
            {
                uint8_t* entry = &g_sector[offset];

                if ((free_entry->found == 0U) && ((entry[0] == 0x00U) || (entry[0] == 0xE5U)))
                {
                    free_entry->sector_lba = sector_lba;
                    free_entry->offset = offset;
                    free_entry->found = 1U;
                }

                if (entry_is_file_match(entry, name83) != 0U)
                {
                    found->sector_lba = sector_lba;
                    found->offset = offset;
                    found->found = 1U;

                    if (entry_buffer != 0)
                    {
                        memcpy(entry_buffer, entry, 32U);
                    }

                    return 1U;
                }
            }
        }

        return 1U;
    }

    uint32_t cluster = g_volume.root_cluster;

    while (is_eoc(cluster) == 0U)
    {
        const uint32_t base_lba = cluster_to_sector(cluster);

        for (uint8_t sector_index = 0U; sector_index < g_volume.sectors_per_cluster; ++sector_index)
        {
            const uint32_t sector_lba = base_lba + sector_index;

            if (read_sector(sector_lba, g_sector) == 0U)
            {
                return 0U;
            }

            for (uint16_t offset = 0U; offset < SD_BLOCK_SIZE; offset += 32U)
            {
                uint8_t* entry = &g_sector[offset];

                if ((free_entry->found == 0U) && ((entry[0] == 0x00U) || (entry[0] == 0xE5U)))
                {
                    free_entry->sector_lba = sector_lba;
                    free_entry->offset = offset;
                    free_entry->found = 1U;
                }

                if (entry_is_file_match(entry, name83) != 0U)
                {
                    found->sector_lba = sector_lba;
                    found->offset = offset;
                    found->found = 1U;

                    if (entry_buffer != 0)
                    {
                        memcpy(entry_buffer, entry, 32U);
                    }

                    return 1U;
                }
            }
        }

        cluster = fat_entry_read(cluster);
    }

    return 1U;
}

static void free_cluster_chain(uint32_t first_cluster)
{
    uint32_t cluster = first_cluster;

    while ((cluster >= 2UL) && (cluster <= (g_volume.cluster_count + 1UL)) && (is_eoc(cluster) == 0U))
    {
        const uint32_t next = fat_entry_read(cluster);
        (void)fat_entry_write(cluster, 0U);
        cluster = next;
    }

    if ((cluster >= 2UL) && (cluster <= (g_volume.cluster_count + 1UL)))
    {
        (void)fat_entry_write(cluster, 0U);
    }
}

static uint8_t mark_directory_entry_deleted(const LcpSdDirEntryLocation& location)
{
    if (read_sector(location.sector_lba, g_sector) == 0U)
    {
        return 0U;
    }

    g_sector[location.offset] = 0xE5U;

    return write_sector(location.sector_lba, g_sector);
}

static uint32_t allocate_cluster(void)
{
    for (uint32_t cluster = 2UL; cluster <= (g_volume.cluster_count + 1UL); ++cluster)
    {
        if (fat_entry_read(cluster) == 0U)
        {
            const uint32_t eoc = (g_volume.fat_type == FAT_TYPE_16) ? 0xFFFFUL : 0x0FFFFFFFUL;

            if (fat_entry_write(cluster, eoc) == 0U)
            {
                return 0U;
            }

            return cluster;
        }
    }

    return 0U;
}

static uint8_t allocate_chain(uint32_t count, uint32_t* first_cluster)
{
    uint32_t first = 0U;
    uint32_t previous = 0U;

    for (uint32_t index = 0U; index < count; ++index)
    {
        const uint32_t cluster = allocate_cluster();

        if (cluster == 0U)
        {
            if (first != 0U)
            {
                free_cluster_chain(first);
            }

            return 0U;
        }

        if (first == 0U)
        {
            first = cluster;
        }

        if (previous != 0U)
        {
            if (fat_entry_write(previous, cluster) == 0U)
            {
                free_cluster_chain(first);
                return 0U;
            }
        }

        previous = cluster;
    }

    *first_cluster = first;
    return 1U;
}

static uint8_t write_file_data(uint32_t first_cluster, const uint8_t* data, size_t length)
{
    uint32_t cluster = first_cluster;
    size_t written = 0U;

    while ((written < length) && (is_eoc(cluster) == 0U))
    {
        const uint32_t base_lba = cluster_to_sector(cluster);

        for (uint8_t sector_index = 0U; sector_index < g_volume.sectors_per_cluster; ++sector_index)
        {
            memset(g_sector, 0, SD_BLOCK_SIZE);

            for (uint16_t offset = 0U; (offset < SD_BLOCK_SIZE) && (written < length); ++offset)
            {
                g_sector[offset] = data[written];
                ++written;
            }

            if (write_sector(base_lba + sector_index, g_sector) == 0U)
            {
                return 0U;
            }

            if (written >= length)
            {
                break;
            }
        }

        cluster = fat_entry_read(cluster);
    }

    return (written == length) ? 1U : 0U;
}

static uint8_t create_directory_entry(const LcpSdDirEntryLocation& location,
                                      const uint8_t name83[11],
                                      uint32_t first_cluster,
                                      uint32_t file_size)
{
    if (read_sector(location.sector_lba, g_sector) == 0U)
    {
        return 0U;
    }

    uint8_t* entry = &g_sector[location.offset];

    memset(entry, 0, 32U);
    memcpy(entry, name83, 11U);
    entry[11] = DIR_ATTR_ARCHIVE;
    entry_set_first_cluster(entry, first_cluster);
    st32(&entry[28], file_size);

    return write_sector(location.sector_lba, g_sector);
}

LcpSdStorageResult lcp_sd_storage_begin(void)
{
    memset(&g_volume, 0, sizeof(g_volume));

    if (lcp_sd_card_inserted() == 0U)
    {
        return LCP_SD_STORAGE_NO_CARD;
    }

    SPI.begin();

    if (init_card() == 0U)
    {
        return LCP_SD_STORAGE_CARD_INIT_FAILED;
    }

    if (mount_volume() == 0U)
    {
        return LCP_SD_STORAGE_VOLUME_UNSUPPORTED;
    }

    return LCP_SD_STORAGE_OK;
}

void lcp_sd_storage_reset(void)
{
    memset(&g_volume, 0, sizeof(g_volume));
}

uint8_t lcp_sd_storage_ready(void)
{
    return g_volume.ready;
}

const char* lcp_sd_storage_result_text(LcpSdStorageResult result)
{
    switch (result)
    {
        case LCP_SD_STORAGE_OK:
            return "ok";

        case LCP_SD_STORAGE_NO_CARD:
            return "no card";

        case LCP_SD_STORAGE_CARD_INIT_FAILED:
            return "card init failed";

        case LCP_SD_STORAGE_VOLUME_UNSUPPORTED:
            return "volume unsupported";

        case LCP_SD_STORAGE_FILE_NOT_FOUND:
            return "file not found";

        case LCP_SD_STORAGE_FILE_OPEN_FAILED:
            return "file open failed";

        case LCP_SD_STORAGE_FILE_EXISTS_ERROR:
            return "file exists error";

        case LCP_SD_STORAGE_DIRECTORY_FULL:
            return "directory full";

        case LCP_SD_STORAGE_NO_FREE_CLUSTER:
            return "no free cluster";

        case LCP_SD_STORAGE_READ_FAILED:
            return "read failed";

        case LCP_SD_STORAGE_WRITE_FAILED:
            return "write failed";

        case LCP_SD_STORAGE_INVALID_NAME:
            return "invalid name";

        case LCP_SD_STORAGE_NOT_READY:
            return "not ready";

        default:
            return "unknown";
    }
}

uint8_t lcp_sd_storage_exists(const char* file_name)
{
    uint8_t name83[11];
    LcpSdDirEntryLocation found;
    LcpSdDirEntryLocation free_entry;

    if (g_volume.ready == 0U)
    {
        return 0U;
    }

    if (make_83_name(file_name, name83) == 0U)
    {
        return 0U;
    }

    if (scan_root_directory(name83, &found, &free_entry, 0) == 0U)
    {
        return 0U;
    }

    return found.found;
}

LcpSdStorageResult lcp_sd_storage_open_read(const char* file_name, LcpSdReadFile* file)
{
    uint8_t name83[11];
    uint8_t entry[32];
    LcpSdDirEntryLocation found;
    LcpSdDirEntryLocation free_entry;

    if ((g_volume.ready == 0U) || (file == 0))
    {
        return LCP_SD_STORAGE_NOT_READY;
    }

    if (make_83_name(file_name, name83) == 0U)
    {
        return LCP_SD_STORAGE_INVALID_NAME;
    }

    if (scan_root_directory(name83, &found, &free_entry, entry) == 0U)
    {
        return LCP_SD_STORAGE_READ_FAILED;
    }

    if (found.found == 0U)
    {
        return LCP_SD_STORAGE_FILE_NOT_FOUND;
    }

    memset(file, 0, sizeof(*file));

    file->first_cluster = entry_first_cluster(entry);
    file->current_cluster = file->first_cluster;
    file->current_cluster_index = 0U;
    file->file_size = ld32(&entry[28]);
    file->position = 0U;
    file->loaded_sector_lba = 0xFFFFFFFFUL;
    file->sector_loaded = 0U;

    return LCP_SD_STORAGE_OK;
}

int lcp_sd_storage_read_byte(LcpSdReadFile* file)
{
    if ((file == 0) || (g_volume.ready == 0U))
    {
        return -1;
    }

    if (file->position >= file->file_size)
    {
        return -1;
    }

    const uint32_t cluster_size_bytes = static_cast<uint32_t>(g_volume.sectors_per_cluster) * SD_BLOCK_SIZE;
    const uint32_t target_cluster_index = file->position / cluster_size_bytes;

    if (target_cluster_index < file->current_cluster_index)
    {
        file->current_cluster = file->first_cluster;
        file->current_cluster_index = 0U;
        file->sector_loaded = 0U;
    }

    while (file->current_cluster_index < target_cluster_index)
    {
        file->current_cluster = fat_entry_read(file->current_cluster);
        ++file->current_cluster_index;
        file->sector_loaded = 0U;

        if (is_eoc(file->current_cluster) != 0U)
        {
            return -1;
        }
    }

    const uint32_t offset_in_cluster = file->position % cluster_size_bytes;
    const uint32_t sector_lba = cluster_to_sector(file->current_cluster) + (offset_in_cluster / SD_BLOCK_SIZE);
    const uint16_t offset_in_sector = static_cast<uint16_t>(offset_in_cluster % SD_BLOCK_SIZE);

    if ((file->sector_loaded == 0U) || (file->loaded_sector_lba != sector_lba))
    {
        if (read_sector(sector_lba, file->sector) == 0U)
        {
            return -1;
        }

        file->loaded_sector_lba = sector_lba;
        file->sector_loaded = 1U;
    }

    const uint8_t value = file->sector[offset_in_sector];
    ++file->position;

    return value;
}

void lcp_sd_storage_close_read(LcpSdReadFile* file)
{
    if (file != 0)
    {
        memset(file, 0, sizeof(*file));
    }
}

LcpSdStorageResult lcp_sd_storage_write_file(const char* file_name, const uint8_t* data, size_t length)
{
    uint8_t name83[11];
    uint8_t entry[32];
    LcpSdDirEntryLocation found;
    LcpSdDirEntryLocation free_entry;
    uint32_t first_cluster = 0U;

    if (g_volume.ready == 0U)
    {
        return LCP_SD_STORAGE_NOT_READY;
    }

    if ((data == 0) && (length != 0U))
    {
        return LCP_SD_STORAGE_WRITE_FAILED;
    }

    if (make_83_name(file_name, name83) == 0U)
    {
        return LCP_SD_STORAGE_INVALID_NAME;
    }

    if (scan_root_directory(name83, &found, &free_entry, entry) == 0U)
    {
        return LCP_SD_STORAGE_READ_FAILED;
    }

    if (found.found != 0U)
    {
        const uint32_t old_first_cluster = entry_first_cluster(entry);

        if (old_first_cluster >= 2UL)
        {
            free_cluster_chain(old_first_cluster);
        }

        if (mark_directory_entry_deleted(found) == 0U)
        {
            return LCP_SD_STORAGE_WRITE_FAILED;
        }

        if (scan_root_directory(name83, &found, &free_entry, 0) == 0U)
        {
            return LCP_SD_STORAGE_READ_FAILED;
        }
    }

    if (free_entry.found == 0U)
    {
        return LCP_SD_STORAGE_DIRECTORY_FULL;
    }

    const uint32_t cluster_size_bytes = static_cast<uint32_t>(g_volume.sectors_per_cluster) * SD_BLOCK_SIZE;
    uint32_t clusters_needed = static_cast<uint32_t>((length + cluster_size_bytes - 1U) / cluster_size_bytes);

    if (clusters_needed == 0U)
    {
        clusters_needed = 1U;
    }

    if (allocate_chain(clusters_needed, &first_cluster) == 0U)
    {
        return LCP_SD_STORAGE_NO_FREE_CLUSTER;
    }

    if (write_file_data(first_cluster, data, length) == 0U)
    {
        free_cluster_chain(first_cluster);
        return LCP_SD_STORAGE_WRITE_FAILED;
    }

    if (create_directory_entry(free_entry, name83, first_cluster, static_cast<uint32_t>(length)) == 0U)
    {
        free_cluster_chain(first_cluster);
        return LCP_SD_STORAGE_WRITE_FAILED;
    }

    return LCP_SD_STORAGE_OK;
}

uint8_t lcp_sd_storage_fat_type(void)
{
    return g_volume.fat_type;
}

uint8_t lcp_sd_storage_sectors_per_cluster(void)
{
    return g_volume.sectors_per_cluster;
}
