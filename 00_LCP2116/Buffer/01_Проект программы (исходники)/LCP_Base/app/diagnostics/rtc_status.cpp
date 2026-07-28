
/**
 * @file rtc_status.cpp
 * @brief Реализация диагностики локального RTC ATSAM3X8E.
 */

#include "rtc_status.hpp"
#include "../../hal/sam3x_rtc.hpp"
#include "../../platform/platform.hpp"

#include <string.h>

static const uint32_t RTC_STATUS_POLL_MS = 1000U;
static const uint32_t RTC_UPDATE_TIMEOUT_MS = 2500U;
static const uint32_t RTC_COMMAND_WAIT_MS = 1500U;

static Sam3xRtcDateTime g_last_datetime;
static Sam3xRtcResult g_last_read_result = SAM3X_RTC_INVALID_ARGUMENT;
static uint32_t g_last_poll_ms = 0U;
static uint32_t g_update_start_ms = 0U;

static uint8_t is_space(char value)
{
    return ((value == ' ') || (value == '\t')) ? 1U : 0U;
}

static const char* skip_spaces(const char* text)
{
    while ((text != 0) && (is_space(*text) != 0U))
    {
        ++text;
    }

    return text;
}

static uint8_t parse_2_digits(const char* text, uint8_t* value)
{
    if ((text[0] < '0') || (text[0] > '9') || (text[1] < '0') || (text[1] > '9'))
    {
        return 0U;
    }

    *value = static_cast<uint8_t>(((text[0] - '0') * 10U) + (text[1] - '0'));
    return 1U;
}

static uint8_t parse_4_digits(const char* text, uint16_t* value)
{
    uint16_t result = 0U;

    for (uint8_t index = 0U; index < 4U; ++index)
    {
        if ((text[index] < '0') || (text[index] > '9'))
        {
            return 0U;
        }

        result = static_cast<uint16_t>((result * 10U) + (text[index] - '0'));
    }

    *value = result;
    return 1U;
}

static uint8_t parse_year(const char* text, uint16_t* value, uint8_t* consumed)
{
    if ((text[0] >= '0') && (text[0] <= '9') &&
        (text[1] >= '0') && (text[1] <= '9') &&
        ((text[2] == '-') || (text[2] == '.')))
    {
        uint8_t year2 = 0U;

        if (parse_2_digits(text, &year2) == 0U)
        {
            return 0U;
        }

        *value = static_cast<uint16_t>(2000U + year2);
        *consumed = 2U;
        return 1U;
    }

    if (parse_4_digits(text, value) == 0U)
    {
        return 0U;
    }

    *value = sam3x_rtc_normalize_year(*value);
    *consumed = 4U;
    return 1U;
}

static uint8_t parse_datetime_value(const char* value, Sam3xRtcDateTime* date_time)
{
    uint8_t year_length = 0U;

    if ((value == 0) || (date_time == 0))
    {
        return 0U;
    }

    value = skip_spaces(value);

    if (parse_year(value, &date_time->year, &year_length) == 0U)
    {
        return 0U;
    }

    value += year_length;

    if ((*value != '-') && (*value != '.'))
    {
        return 0U;
    }

    ++value;

    if (parse_2_digits(value, &date_time->month) == 0U)
    {
        return 0U;
    }

    value += 2;

    if ((*value != '-') && (*value != '.'))
    {
        return 0U;
    }

    ++value;

    if (parse_2_digits(value, &date_time->day) == 0U)
    {
        return 0U;
    }

    value += 2;

    if ((*value != ' ') && (*value != 't'))
    {
        return 0U;
    }

    ++value;

    value = skip_spaces(value);

    if (parse_2_digits(value, &date_time->hour) == 0U)
    {
        return 0U;
    }

    value += 2;

    if (*value != ':')
    {
        return 0U;
    }

    ++value;

    if (parse_2_digits(value, &date_time->minute) == 0U)
    {
        return 0U;
    }

    value += 2;

    if (*value != ':')
    {
        return 0U;
    }

    ++value;

    if (parse_2_digits(value, &date_time->second) == 0U)
    {
        return 0U;
    }

    value += 2;
    value = skip_spaces(value);

    if (*value != '\0')
    {
        return 0U;
    }

    date_time->day_of_week = sam3x_rtc_calculate_day_of_week(date_time->year,
                                                             date_time->month,
                                                             date_time->day);
    return 1U;
}

static void print_2_digits(uint8_t value)
{
    SerialUSB.print(static_cast<int>(value / 10U));
    SerialUSB.print(static_cast<int>(value % 10U));
}

static void print_4_digits(uint16_t value)
{
    SerialUSB.print(static_cast<int>((value / 1000U) % 10U));
    SerialUSB.print(static_cast<int>((value / 100U) % 10U));
    SerialUSB.print(static_cast<int>((value / 10U) % 10U));
    SerialUSB.print(static_cast<int>(value % 10U));
}

static void print_datetime(const Sam3xRtcDateTime& date_time)
{
    print_4_digits(sam3x_rtc_normalize_year(date_time.year));
    SerialUSB.print("-");
    print_2_digits(date_time.month);
    SerialUSB.print("-");
    print_2_digits(date_time.day);
    SerialUSB.print(" ");
    print_2_digits(date_time.hour);
    SerialUSB.print(":");
    print_2_digits(date_time.minute);
    SerialUSB.print(":");
    print_2_digits(date_time.second);
}

static void poll_rtc_now(void)
{
    g_last_read_result = sam3x_rtc_get_datetime(&g_last_datetime);
}

static void service_rtc_update_timeout(void)
{
    if (sam3x_rtc_update_busy() == 0U)
    {
        return;
    }

    const uint32_t now_ms = millis();

    if ((uint32_t)(now_ms - g_update_start_ms) >= RTC_UPDATE_TIMEOUT_MS)
    {
        sam3x_rtc_cancel_datetime_update(SAM3X_RTC_ACK_TIMEOUT);
    }
}

void rtc_status_init(void)
{
    sam3x_rtc_init();

    g_last_poll_ms = millis();
    g_update_start_ms = 0U;
    poll_rtc_now();
}

void rtc_status_poll(void)
{
    const uint32_t now_ms = millis();

    sam3x_rtc_poll_datetime_update();
    service_rtc_update_timeout();

    if ((uint32_t)(now_ms - g_last_poll_ms) >= RTC_STATUS_POLL_MS)
    {
        g_last_poll_ms = now_ms;
        poll_rtc_now();
    }
}

void rtc_status_print_report(void)
{
    Sam3xRtcDateTime now;

    sam3x_rtc_poll_datetime_update();
    service_rtc_update_timeout();

    const Sam3xRtcResult read_result = sam3x_rtc_get_datetime(&now);
    const uint8_t register_valid = sam3x_rtc_registers_valid();
    const uint8_t range_valid = sam3x_rtc_datetime_range_valid(&now);

    SerialUSB.print("local RTC status\r\n");

    SerialUSB.print("slow clock source=");
    SerialUSB.print(sam3x_rtc_external_32k_selected() ? "external 32.768 kHz crystal" : "internal slow clock");
    SerialUSB.print("\r\n");

    SerialUSB.print("update state=");
    SerialUSB.print(sam3x_rtc_update_state_text(sam3x_rtc_update_state()));
    SerialUSB.print("\r\n");

    SerialUSB.print("update result=");
    SerialUSB.print(sam3x_rtc_result_text(sam3x_rtc_update_result()));
    SerialUSB.print("\r\n");

    SerialUSB.print("read result=");
    SerialUSB.print(sam3x_rtc_result_text(read_result));
    SerialUSB.print("\r\n");

    SerialUSB.print("datetime=");
    if (read_result == SAM3X_RTC_OK)
    {
        print_datetime(now);
    }
    else
    {
        SerialUSB.print("unavailable");
    }
    SerialUSB.print("\r\n");

    SerialUSB.print("day_of_week=");
    SerialUSB.print(static_cast<int>(now.day_of_week));
    SerialUSB.print("\r\n");

    SerialUSB.print("RTC_SR=0x");
    SerialUSB.print(static_cast<int>(sam3x_rtc_status_register()), HEX);
    SerialUSB.print("\r\n");

    SerialUSB.print("RTC_VER=0x");
    SerialUSB.print(static_cast<int>(sam3x_rtc_valid_entry_register()), HEX);
    SerialUSB.print("\r\n");

    SerialUSB.print("RTC_TIMR=0x");
    SerialUSB.print(static_cast<int>(sam3x_rtc_time_register()), HEX);
    SerialUSB.print("\r\n");

    SerialUSB.print("RTC_CALR=0x");
    SerialUSB.print(static_cast<int>(sam3x_rtc_calendar_register()), HEX);
    SerialUSB.print("\r\n");

    SerialUSB.print("registers valid=");
    SerialUSB.print(register_valid ? "yes" : "no");
    SerialUSB.print("\r\n");

    SerialUSB.print("range valid=");
    SerialUSB.print(range_valid ? "yes" : "no");
    SerialUSB.print("\r\n");

    if ((register_valid != 0U) && (range_valid != 0U))
    {
        SerialUSB.print("message=local RTC date/time valid\r\n");
    }
    else
    {
        SerialUSB.print("message=set local RTC date/time\r\n");
    }
}

uint8_t rtc_status_handle_command(const char* command)
{
    if (command == 0)
    {
        return 0U;
    }

    if (strcmp(command, "rtc") == 0)
    {
        rtc_status_print_report();
        return 1U;
    }

    if (strncmp(command, "rtc set", 7U) == 0)
    {
        Sam3xRtcDateTime date_time;
        const char* value = &command[7];

        if (parse_datetime_value(value, &date_time) == 0U)
        {
            SerialUSB.print("rtc set failed: expected format rtc set YYYY-MM-DD HH:MM:SS\r\n");
            SerialUSB.print("accepted examples:\r\n");
            SerialUSB.print("rtc set 2026-06-08 21:06:30\r\n");
            SerialUSB.print("rtc set 26-06-08 21:06:30\r\n");
            SerialUSB.print("rtc set 2026.06.08 21:06:30\r\n");
            SerialUSB.print("rtc set 2026-06-08T21:06:30\r\n");
            return 1U;
        }

        date_time.year = sam3x_rtc_normalize_year(date_time.year);

        SerialUSB.print("rtc parsed datetime=");
        print_datetime(date_time);
        SerialUSB.print("\r\n");

        const Sam3xRtcResult start_result = sam3x_rtc_start_datetime_update(&date_time);

        if (start_result != SAM3X_RTC_OK)
        {
            SerialUSB.print("rtc set start result=");
            SerialUSB.print(sam3x_rtc_result_text(start_result));
            SerialUSB.print("\r\n");
            rtc_status_print_report();
            return 1U;
        }

        g_update_start_ms = millis();

        while ((sam3x_rtc_update_busy() != 0U) &&
               ((uint32_t)(millis() - g_update_start_ms) < RTC_COMMAND_WAIT_MS))
        {
            sam3x_rtc_poll_datetime_update();
        }

        if (sam3x_rtc_update_busy() != 0U)
        {
            SerialUSB.print("rtc set started: waiting for RTC ACKUPD\r\n");
        }
        else
        {
            SerialUSB.print("rtc set result=");
            SerialUSB.print(sam3x_rtc_result_text(sam3x_rtc_update_result()));
            SerialUSB.print("\r\n");
        }

        rtc_status_print_report();
        return 1U;
    }

    return 0U;
}
