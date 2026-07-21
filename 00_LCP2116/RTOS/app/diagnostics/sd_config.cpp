/**
 * @file sd_config.cpp
 * @brief Реализация общего парсера числовых файлов microSD.
 */

#include "sd_config.hpp"
#include "../../libs/lcp_sd_storage/lcp_sd_storage.hpp"

#include <stddef.h>
#include <stdint.h>

namespace
{
static const uint16_t SD_CONFIG_LINE_BUFFER_SIZE = 192U;
static const uint16_t SD_CONFIG_WRITE_BUFFER_SIZE = 1024U;

char g_line_buffer[SD_CONFIG_LINE_BUFFER_SIZE];
uint8_t g_write_buffer[SD_CONFIG_WRITE_BUFFER_SIZE];

uint8_t is_space(char value)
{
    return ((value == ' ') || (value == '\t') ||
            (value == '\r') || (value == '\n')) ? 1U : 0U;
}

char ascii_lower(char value)
{
    if ((value >= 'A') && (value <= 'Z'))
    {
        return static_cast<char>(value + ('a' - 'A'));
    }

    return value;
}

void remove_utf8_bom(char* line)
{
    if ((static_cast<uint8_t>(line[0]) != 0xEFU) ||
        (static_cast<uint8_t>(line[1]) != 0xBBU) ||
        (static_cast<uint8_t>(line[2]) != 0xBFU))
    {
        return;
    }

    uint16_t source = 3U;
    uint16_t destination = 0U;

    while (line[source] != '\0')
    {
        line[destination++] = line[source++];
    }

    line[destination] = '\0';
}

void strip_comment_and_trim(char* line)
{
    remove_utf8_bom(line);

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

uint8_t is_fin_marker(const char* text)
{
    return ((ascii_lower(text[0]) == 'f') &&
            (ascii_lower(text[1]) == 'i') &&
            (ascii_lower(text[2]) == 'n') &&
            (text[3] == '\0')) ? 1U : 0U;
}

uint8_t parse_uint32(const char* text, uint32_t* output)
{
    uint32_t value = 0U;
    uint16_t index = 0U;
    uint8_t digit_found = 0U;

    if ((text == 0) || (output == 0))
    {
        return 0U;
    }

    if (text[index] == '+')
    {
        ++index;
    }

    while (text[index] != '\0')
    {
        if ((text[index] < '0') || (text[index] > '9'))
        {
            return 0U;
        }

        const uint32_t digit = static_cast<uint32_t>(text[index] - '0');

        if (value > ((0xFFFFFFFFUL - digit) / 10UL))
        {
            return 0U;
        }

        value = (value * 10UL) + digit;
        digit_found = 1U;
        ++index;
    }

    if (digit_found == 0U)
    {
        return 0U;
    }

    *output = value;
    return 1U;
}

uint8_t parse_int16(const char* text, int16_t* output)
{
    uint16_t index = 0U;
    uint8_t negative = 0U;
    uint8_t digit_found = 0U;
    uint32_t magnitude = 0U;
    uint32_t limit = 32767U;

    if ((text == 0) || (output == 0))
    {
        return 0U;
    }

    if (text[index] == '-')
    {
        negative = 1U;
        limit = 32768U;
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

        const uint32_t digit = static_cast<uint32_t>(text[index] - '0');

        if (magnitude > ((limit - digit) / 10U))
        {
            return 0U;
        }

        magnitude = (magnitude * 10U) + digit;
        digit_found = 1U;
        ++index;
    }

    if (digit_found == 0U)
    {
        return 0U;
    }

    const int32_t signed_value = (negative != 0U) ?
        -static_cast<int32_t>(magnitude) : static_cast<int32_t>(magnitude);
    *output = static_cast<int16_t>(signed_value);
    return 1U;
}

uint8_t read_physical_line(LcpSdReadFile* file,
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

SdConfigResult translate_open_result(LcpSdStorageResult result)
{
    if (result == LCP_SD_STORAGE_FILE_NOT_FOUND)
    {
        return SD_CONFIG_FILE_NOT_FOUND;
    }

    return (result == LCP_SD_STORAGE_OK) ?
        SD_CONFIG_OK : SD_CONFIG_FILE_OPEN_FAILED;
}

template <typename ValueType>
SdConfigResult load_numeric_file(const char* file_name,
                                 ValueType* output,
                                 size_t capacity,
                                 uint8_t* loaded_count,
                                 uint8_t (*parse_value)(const char*, ValueType*))
{
    if ((file_name == 0) || (output == 0) || (loaded_count == 0) ||
        (parse_value == 0) || (capacity < 2U))
    {
        return SD_CONFIG_INVALID_COUNT;
    }

    *loaded_count = 0U;

    for (size_t index = 0U; index < capacity; ++index)
    {
        output[index] = static_cast<ValueType>(0);
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

    uint8_t expected_count = 0U;
    uint8_t value_index = 0U;
    uint8_t count_read = 0U;
    uint8_t line_too_long = 0U;

    while (read_physical_line(&file,
                              g_line_buffer,
                              SD_CONFIG_LINE_BUFFER_SIZE,
                              &line_too_long) != 0U)
    {
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

        if (is_fin_marker(g_line_buffer) != 0U)
        {
            break;
        }

        if (count_read == 0U)
        {
            uint32_t parsed_count = 0U;

            if ((parse_uint32(g_line_buffer, &parsed_count) == 0U) ||
                (parsed_count == 0U) ||
                (parsed_count > 255U) ||
                (parsed_count >= capacity))
            {
                lcp_sd_storage_close_read(&file);
                return SD_CONFIG_INVALID_COUNT;
            }

            expected_count = static_cast<uint8_t>(parsed_count);
            output[0] = static_cast<ValueType>(parsed_count);
            count_read = 1U;
            continue;
        }

        if (value_index >= expected_count)
        {
            lcp_sd_storage_close_read(&file);
            return SD_CONFIG_INVALID_COUNT;
        }

        ValueType parsed_value = static_cast<ValueType>(0);

        if (parse_value(g_line_buffer, &parsed_value) == 0U)
        {
            lcp_sd_storage_close_read(&file);
            return SD_CONFIG_INVALID_VALUE;
        }

        ++value_index;
        output[value_index] = parsed_value;
    }

    lcp_sd_storage_close_read(&file);

    if (count_read == 0U)
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

uint8_t append_char(uint8_t* buffer,
                    uint16_t* position,
                    uint16_t capacity,
                    char value)
{
    if ((buffer == 0) || (position == 0) || (*position >= capacity))
    {
        return 0U;
    }

    buffer[*position] = static_cast<uint8_t>(value);
    ++(*position);
    return 1U;
}

uint8_t append_uint(uint8_t* buffer,
                    uint16_t* position,
                    uint16_t capacity,
                    uint16_t value)
{
    char temporary[6];
    uint8_t length = 0U;

    if (value == 0U)
    {
        return append_char(buffer, position, capacity, '0');
    }

    while (value != 0U)
    {
        temporary[length++] = static_cast<char>('0' + (value % 10U));
        value = static_cast<uint16_t>(value / 10U);
    }

    while (length != 0U)
    {
        if (append_char(buffer,
                        position,
                        capacity,
                        temporary[--length]) == 0U)
        {
            return 0U;
        }
    }

    return 1U;
}

uint8_t append_int16(uint8_t* buffer,
                     uint16_t* position,
                     uint16_t capacity,
                     int16_t value)
{
    uint16_t magnitude = 0U;

    if (value < 0)
    {
        if (append_char(buffer, position, capacity, '-') == 0U)
        {
            return 0U;
        }

        magnitude = static_cast<uint16_t>(-
            static_cast<int32_t>(value));
    }
    else
    {
        magnitude = static_cast<uint16_t>(value);
    }

    return append_uint(buffer, position, capacity, magnitude);
}
}

SdConfigResult sd_config_load_int16(const char* file_name,
                                    int16_t* output,
                                    size_t capacity,
                                    uint8_t* loaded_count)
{
    return load_numeric_file(file_name,
                             output,
                             capacity,
                             loaded_count,
                             parse_int16);
}

SdConfigResult sd_config_load_uint32(const char* file_name,
                                     uint32_t* output,
                                     size_t capacity,
                                     uint8_t* loaded_count)
{
    return load_numeric_file(file_name,
                             output,
                             capacity,
                             loaded_count,
                             parse_uint32);
}

SdConfigResult sd_config_save_int16(const char* file_name,
                                    const int16_t* input,
                                    uint8_t size)
{
    uint16_t position = 0U;

    if ((file_name == 0) || (input == 0) || (size == 0U))
    {
        return SD_CONFIG_INVALID_COUNT;
    }

    if (lcp_sd_storage_ready() == 0U)
    {
        return SD_CONFIG_CARD_NOT_READY;
    }

    if (append_uint(g_write_buffer,
                    &position,
                    SD_CONFIG_WRITE_BUFFER_SIZE,
                    size) == 0U)
    {
        return SD_CONFIG_WRITE_FAILED;
    }

    if ((append_char(g_write_buffer,
                     &position,
                     SD_CONFIG_WRITE_BUFFER_SIZE,
                     '\r') == 0U) ||
        (append_char(g_write_buffer,
                     &position,
                     SD_CONFIG_WRITE_BUFFER_SIZE,
                     '\n') == 0U))
    {
        return SD_CONFIG_WRITE_FAILED;
    }

    for (uint8_t index = 1U; index <= size; ++index)
    {
        if (append_int16(g_write_buffer,
                         &position,
                         SD_CONFIG_WRITE_BUFFER_SIZE,
                         input[index]) == 0U)
        {
            return SD_CONFIG_WRITE_FAILED;
        }

        if ((append_char(g_write_buffer,
                         &position,
                         SD_CONFIG_WRITE_BUFFER_SIZE,
                         '\r') == 0U) ||
            (append_char(g_write_buffer,
                         &position,
                         SD_CONFIG_WRITE_BUFFER_SIZE,
                         '\n') == 0U))
        {
            return SD_CONFIG_WRITE_FAILED;
        }
    }

    const LcpSdStorageResult result = lcp_sd_storage_write_file(
        file_name,
        g_write_buffer,
        position);

    return (result == LCP_SD_STORAGE_OK) ?
        SD_CONFIG_OK : SD_CONFIG_STORAGE_ERROR;
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
        case SD_CONFIG_NOT_ATTEMPTED:
            return "not attempted";
        default:
            return "unknown";
    }
}
