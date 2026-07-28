
/**
 * @file sd_config.cpp
 * @brief Чтение и запись числовых конфигурационных файлов с microSD.
 */

#include "sd_config.hpp"
#include "../../libs/lcp_sd_storage/lcp_sd_storage.hpp"

#include <stddef.h>
#include <stdint.h>

static const uint16_t SD_CONFIG_LINE_BUFFER_SIZE = 64U;
static const uint16_t SD_CONFIG_WRITE_BUFFER_SIZE = 1024U;

static char g_line_buffer[SD_CONFIG_LINE_BUFFER_SIZE];
static uint8_t g_write_buffer[SD_CONFIG_WRITE_BUFFER_SIZE];

static uint8_t is_space(char value)
{
    return ((value == ' ') || (value == '\t') || (value == '\r') || (value == '\n')) ? 1U : 0U;
}

static void strip_comment_and_trim(char* line)
{
    uint16_t index = 0U;

    while (line[index] != '\0')
    {
        if ((line[index] == '/') && (line[index + 1U] == '/'))
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
        uint16_t dst = 0U;

        while (line[start] != '\0')
        {
            line[dst] = line[start];
            ++dst;
            ++start;
        }

        line[dst] = '\0';
    }

    uint16_t length = 0U;

    while (line[length] != '\0')
    {
        ++length;
    }

    while ((length > 0U) && (is_space(line[length - 1U]) != 0U))
    {
        line[length - 1U] = '\0';
        --length;
    }
}

static uint8_t parse_int16(const char* text, int16_t* value)
{
    int32_t sign = 1L;
    int32_t result = 0L;
    uint16_t index = 0U;
    uint8_t digit_found = 0U;

    if ((text == 0) || (value == 0))
    {
        return 0U;
    }

    if (text[index] == '-')
    {
        sign = -1L;
        ++index;
    }
    else if (text[index] == '+')
    {
        ++index;
    }

    while (text[index] != '\0')
    {
        if ((text[index] < '0') || (text[index] > '9'))
        {
            return 0U;
        }

        digit_found = 1U;
        result = (result * 10L) + static_cast<int32_t>(text[index] - '0');

        if ((sign * result) > 32767L)
        {
            return 0U;
        }

        if ((sign * result) < -32768L)
        {
            return 0U;
        }

        ++index;
    }

    if (digit_found == 0U)
    {
        return 0U;
    }

    *value = static_cast<int16_t>(sign * result);
    return 1U;
}

static uint8_t read_logical_line(LcpSdReadFile* file, char* line, uint16_t capacity, uint8_t* line_too_long)
{
    uint16_t length = 0U;
    int value;

    *line_too_long = 0U;

    while ((value = lcp_sd_storage_read_byte(file)) >= 0)
    {
        const char c = static_cast<char>(value);

        if (c == '\n')
        {
            break;
        }

        if (length < (capacity - 1U))
        {
            line[length] = c;
            ++length;
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

static uint8_t append_char(uint8_t* buffer, uint16_t* position, uint16_t capacity, char value)
{
    if (*position >= capacity)
    {
        return 0U;
    }

    buffer[*position] = static_cast<uint8_t>(value);
    ++(*position);

    return 1U;
}

static uint8_t append_uint(uint8_t* buffer, uint16_t* position, uint16_t capacity, uint16_t value)
{
    char temp[6];
    uint8_t length = 0U;

    if (value == 0U)
    {
        return append_char(buffer, position, capacity, '0');
    }

    while (value != 0U)
    {
        temp[length] = static_cast<char>('0' + (value % 10U));
        value = static_cast<uint16_t>(value / 10U);
        ++length;
    }

    while (length != 0U)
    {
        --length;

        if (append_char(buffer, position, capacity, temp[length]) == 0U)
        {
            return 0U;
        }
    }

    return 1U;
}

static uint8_t append_int16(uint8_t* buffer, uint16_t* position, uint16_t capacity, int16_t value)
{
    uint16_t magnitude;

    if (value < 0)
    {
        if (append_char(buffer, position, capacity, '-') == 0U)
        {
            return 0U;
        }

        magnitude = static_cast<uint16_t>(-(static_cast<int32_t>(value)));
    }
    else
    {
        magnitude = static_cast<uint16_t>(value);
    }

    return append_uint(buffer, position, capacity, magnitude);
}

SdConfigResult sd_config_load_int16(const char* file_name,
                                    int16_t* output,
                                    size_t capacity,
                                    uint8_t* loaded_count)
{
    LcpSdReadFile file;
    LcpSdStorageResult storage_result;
    uint8_t line_too_long = 0U;
    uint8_t logical_line_index = 0U;
    uint8_t expected_count = 0U;
    uint8_t value_index = 0U;

    if ((output == 0) || (loaded_count == 0) || (capacity == 0U))
    {
        return SD_CONFIG_INVALID_COUNT;
    }

    *loaded_count = 0U;

    for (size_t index = 0U; index < capacity; ++index)
    {
        output[index] = 0;
    }

    if (lcp_sd_storage_ready() == 0U)
    {
        return SD_CONFIG_CARD_NOT_READY;
    }

    storage_result = lcp_sd_storage_open_read(file_name, &file);

    if (storage_result == LCP_SD_STORAGE_FILE_NOT_FOUND)
    {
        return SD_CONFIG_FILE_NOT_FOUND;
    }

    if (storage_result != LCP_SD_STORAGE_OK)
    {
        return SD_CONFIG_FILE_OPEN_FAILED;
    }

    while (read_logical_line(&file, g_line_buffer, SD_CONFIG_LINE_BUFFER_SIZE, &line_too_long) != 0U)
    {
        int16_t parsed_value = 0;

        if (line_too_long != 0U)
        {
            lcp_sd_storage_close_read(&file);
            return SD_CONFIG_LINE_TOO_LONG;
        }

        strip_comment_and_trim(g_line_buffer);

        if (g_line_buffer[0] == '\0')
        {
            continue;
        }

        if (parse_int16(g_line_buffer, &parsed_value) == 0U)
        {
            lcp_sd_storage_close_read(&file);
            return SD_CONFIG_INVALID_VALUE;
        }

        if (logical_line_index == 0U)
        {
            if ((parsed_value <= 0) || (static_cast<size_t>(parsed_value) >= capacity) || (parsed_value > 255))
            {
                lcp_sd_storage_close_read(&file);
                return SD_CONFIG_INVALID_COUNT;
            }

            expected_count = static_cast<uint8_t>(parsed_value);
            output[0] = parsed_value;
        }
        else
        {
            if (value_index >= expected_count)
            {
                break;
            }

            ++value_index;
            output[value_index] = parsed_value;
        }

        ++logical_line_index;
    }

    lcp_sd_storage_close_read(&file);

    if (logical_line_index == 0U)
    {
        return SD_CONFIG_EMPTY_FILE;
    }

    if (value_index != expected_count)
    {
        return SD_CONFIG_INCOMPLETE_FILE;
    }

    *loaded_count = value_index;
    return SD_CONFIG_OK;
}

SdConfigResult sd_config_save_int16(const char* file_name,
                                    const int16_t* input,
                                    uint8_t size)
{
    uint16_t position = 0U;

    if ((input == 0) || (size == 0U))
    {
        return SD_CONFIG_INVALID_COUNT;
    }

    if (lcp_sd_storage_ready() == 0U)
    {
        return SD_CONFIG_CARD_NOT_READY;
    }

    if (append_uint(g_write_buffer, &position, SD_CONFIG_WRITE_BUFFER_SIZE, size) == 0U)
    {
        return SD_CONFIG_WRITE_FAILED;
    }

    if ((append_char(g_write_buffer, &position, SD_CONFIG_WRITE_BUFFER_SIZE, '\r') == 0U) ||
        (append_char(g_write_buffer, &position, SD_CONFIG_WRITE_BUFFER_SIZE, '\n') == 0U))
    {
        return SD_CONFIG_WRITE_FAILED;
    }

    for (uint8_t index = 1U; index <= size; ++index)
    {
        if (append_int16(g_write_buffer, &position, SD_CONFIG_WRITE_BUFFER_SIZE, input[index]) == 0U)
        {
            return SD_CONFIG_WRITE_FAILED;
        }

        if ((append_char(g_write_buffer, &position, SD_CONFIG_WRITE_BUFFER_SIZE, '\r') == 0U) ||
            (append_char(g_write_buffer, &position, SD_CONFIG_WRITE_BUFFER_SIZE, '\n') == 0U))
        {
            return SD_CONFIG_WRITE_FAILED;
        }
    }

    const LcpSdStorageResult result = lcp_sd_storage_write_file(file_name, g_write_buffer, position);

    return (result == LCP_SD_STORAGE_OK) ? SD_CONFIG_OK : SD_CONFIG_STORAGE_ERROR;
}

const char* sd_config_result_text(SdConfigResult result)
{
    switch (result)
    {
        case SD_CONFIG_OK:
            return "ok";

        case SD_CONFIG_CARD_NOT_READY:
            return "card not ready";

        case SD_CONFIG_FILE_NOT_FOUND:
            return "file not found";

        case SD_CONFIG_FILE_OPEN_FAILED:
            return "file open failed";

        case SD_CONFIG_EMPTY_FILE:
            return "empty file";

        case SD_CONFIG_INVALID_COUNT:
            return "invalid count";

        case SD_CONFIG_INVALID_VALUE:
            return "invalid value";

        case SD_CONFIG_INCOMPLETE_FILE:
            return "incomplete file";

        case SD_CONFIG_WRITE_FAILED:
            return "write failed";

        case SD_CONFIG_LINE_TOO_LONG:
            return "line too long";

        case SD_CONFIG_STORAGE_ERROR:
            return "storage error";

        default:
            return "unknown";
    }
}
