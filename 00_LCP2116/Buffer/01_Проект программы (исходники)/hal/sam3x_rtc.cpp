
/**
 * @file sam3x_rtc.cpp
 * @brief Реализация HAL локального RTC ATSAM3X8E.
 */

#include "sam3x_rtc.hpp"
#include "sam3x_device.hpp"

#ifndef SUPC_CR_KEY_PASSWD
#define SUPC_CR_KEY_PASSWD (0xA5UL << 24)
#endif

#ifndef SUPC_CR_XTALSEL
#define SUPC_CR_XTALSEL (1UL << 3)
#endif

#ifndef SUPC_SR_OSCSEL
#define SUPC_SR_OSCSEL (1UL << 7)
#endif

#ifndef RTC_MR_HRMOD
#define RTC_MR_HRMOD (1UL << 0)
#endif

#ifndef RTC_CR_UPDTIM
#define RTC_CR_UPDTIM (1UL << 0)
#endif

#ifndef RTC_CR_UPDCAL
#define RTC_CR_UPDCAL (1UL << 1)
#endif

#ifndef RTC_SR_ACKUPD
#define RTC_SR_ACKUPD (1UL << 0)
#endif

#ifndef RTC_SCCR_ACKCLR
#define RTC_SCCR_ACKCLR (1UL << 0)
#endif

#ifndef RTC_SCCR_SECCLR
#define RTC_SCCR_SECCLR (1UL << 2)
#endif

#ifndef RTC_SCCR_TIMCLR
#define RTC_SCCR_TIMCLR (1UL << 3)
#endif

#ifndef RTC_SCCR_CALCLR
#define RTC_SCCR_CALCLR (1UL << 4)
#endif

#ifndef RTC_VER_NVTIM
#define RTC_VER_NVTIM (1UL << 0)
#endif

#ifndef RTC_VER_NVCAL
#define RTC_VER_NVCAL (1UL << 1)
#endif

#ifndef RTC_IDR_ACKDIS
#define RTC_IDR_ACKDIS (1UL << 0)
#endif

static const uint32_t RTC_BLOCKING_TIMEOUT_COUNT = 120000000UL;

static Sam3xRtcUpdateState g_update_state = SAM3X_RTC_UPDATE_IDLE;
static Sam3xRtcResult g_update_result = SAM3X_RTC_OK;
static uint32_t g_pending_time_register = 0U;
static uint32_t g_pending_calendar_register = 0U;

static uint8_t bcd_to_u8(uint32_t value)
{
    return static_cast<uint8_t>(((value >> 4U) * 10U) + (value & 0x0FU));
}

static uint32_t u8_to_bcd(uint8_t value)
{
    return static_cast<uint32_t>(((value / 10U) << 4U) | (value % 10U));
}

static uint8_t is_leap_year(uint16_t year)
{
    if ((year % 4U) != 0U)
    {
        return 0U;
    }

    if ((year % 100U) != 0U)
    {
        return 1U;
    }

    return ((year % 400U) == 0U) ? 1U : 0U;
}

static uint8_t days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t days[] =
    {
        31U, 28U, 31U, 30U, 31U, 30U,
        31U, 31U, 30U, 31U, 30U, 31U
    };

    if ((month == 2U) && (is_leap_year(year) != 0U))
    {
        return 29U;
    }

    if ((month < 1U) || (month > 12U))
    {
        return 0U;
    }

    return days[month - 1U];
}

static uint32_t read_stable_time_register(void)
{
    uint32_t first;
    uint32_t second;

    do
    {
        first = RTC->RTC_TIMR;
        second = RTC->RTC_TIMR;
    }
    while (first != second);

    return first;
}

static uint32_t read_stable_date_register(void)
{
    uint32_t first;
    uint32_t second;

    do
    {
        first = RTC->RTC_CALR;
        second = RTC->RTC_CALR;
    }
    while (first != second);

    return first;
}

static uint32_t build_time_register(const Sam3xRtcDateTime* date_time)
{
    return (u8_to_bcd(date_time->second) << 0U) |
           (u8_to_bcd(date_time->minute) << 8U) |
           (u8_to_bcd(date_time->hour) << 16U);
}

static uint32_t build_date_register(const Sam3xRtcDateTime* date_time)
{
    const uint16_t normalized_year = sam3x_rtc_normalize_year(date_time->year);
    const uint8_t century = static_cast<uint8_t>(normalized_year / 100U);
    const uint8_t year_2_digits = static_cast<uint8_t>(normalized_year % 100U);

    return (u8_to_bcd(century) << 0U) |
           (u8_to_bcd(year_2_digits) << 8U) |
           (u8_to_bcd(date_time->month) << 16U) |
           (static_cast<uint32_t>(date_time->day_of_week & 0x07U) << 21U) |
           (u8_to_bcd(date_time->day) << 24U);
}

static Sam3xRtcResult prepare_datetime_registers(const Sam3xRtcDateTime* date_time)
{
    if (date_time == 0)
    {
        return SAM3X_RTC_INVALID_ARGUMENT;
    }

    Sam3xRtcDateTime normalized = *date_time;
    normalized.year = sam3x_rtc_normalize_year(normalized.year);

    if (sam3x_rtc_datetime_range_valid(&normalized) == 0U)
    {
        return SAM3X_RTC_INVALID_DATE_TIME;
    }

    normalized.day_of_week = sam3x_rtc_calculate_day_of_week(normalized.year,
                                                             normalized.month,
                                                             normalized.day);

    g_pending_time_register = build_time_register(&normalized);
    g_pending_calendar_register = build_date_register(&normalized);

    return SAM3X_RTC_OK;
}

static void complete_datetime_update(void)
{
    RTC->RTC_SCCR = RTC_SCCR_ACKCLR;
    RTC->RTC_TIMR = g_pending_time_register;
    RTC->RTC_CALR = g_pending_calendar_register;
    RTC->RTC_CR &= ~(RTC_CR_UPDTIM | RTC_CR_UPDCAL);
    RTC->RTC_SCCR = RTC_SCCR_SECCLR | RTC_SCCR_TIMCLR | RTC_SCCR_CALCLR;

    if (sam3x_rtc_registers_valid() != 0U)
    {
        g_update_result = SAM3X_RTC_OK;
        g_update_state = SAM3X_RTC_UPDATE_DONE;
    }
    else
    {
        g_update_result = SAM3X_RTC_VERIFY_FAILED;
        g_update_state = SAM3X_RTC_UPDATE_FAILED;
    }
}

void sam3x_rtc_init(void)
{
    sam3x_rtc_request_external_32k();

    RTC->RTC_MR &= ~RTC_MR_HRMOD;
    RTC->RTC_CR &= ~(RTC_CR_UPDTIM | RTC_CR_UPDCAL);
    RTC->RTC_SCCR = RTC_SCCR_ACKCLR | RTC_SCCR_SECCLR;

    g_update_state = SAM3X_RTC_UPDATE_IDLE;
    g_update_result = SAM3X_RTC_OK;
}

void sam3x_rtc_request_external_32k(void)
{
    if (sam3x_rtc_external_32k_selected() == 0U)
    {
        SUPC->SUPC_CR = SUPC_CR_KEY_PASSWD | SUPC_CR_XTALSEL;
    }
}

uint8_t sam3x_rtc_external_32k_selected(void)
{
    return ((SUPC->SUPC_SR & SUPC_SR_OSCSEL) != 0U) ? 1U : 0U;
}

Sam3xRtcResult sam3x_rtc_get_datetime(Sam3xRtcDateTime* date_time)
{
    if (date_time == 0)
    {
        return SAM3X_RTC_INVALID_ARGUMENT;
    }

    const uint32_t time_register = read_stable_time_register();
    const uint32_t date_register = read_stable_date_register();

    date_time->second = bcd_to_u8((time_register >> 0U) & 0x7FU);
    date_time->minute = bcd_to_u8((time_register >> 8U) & 0x7FU);
    date_time->hour = bcd_to_u8((time_register >> 16U) & 0x3FU);

    const uint8_t century = bcd_to_u8((date_register >> 0U) & 0x7FU);
    const uint8_t year_2_digits = bcd_to_u8((date_register >> 8U) & 0xFFU);

    if (century == 0U)
    {
        date_time->year = static_cast<uint16_t>(2000U + year_2_digits);
    }
    else
    {
        date_time->year = static_cast<uint16_t>((static_cast<uint16_t>(century) * 100U) + year_2_digits);
    }

    date_time->month = bcd_to_u8((date_register >> 16U) & 0x1FU);
    date_time->day_of_week = static_cast<uint8_t>((date_register >> 21U) & 0x07U);
    date_time->day = bcd_to_u8((date_register >> 24U) & 0x3FU);

    return SAM3X_RTC_OK;
}

Sam3xRtcResult sam3x_rtc_set_datetime(const Sam3xRtcDateTime* date_time)
{
    Sam3xRtcResult result = sam3x_rtc_start_datetime_update(date_time);

    if (result != SAM3X_RTC_OK)
    {
        return result;
    }

    for (uint32_t guard = 0U; guard < RTC_BLOCKING_TIMEOUT_COUNT; ++guard)
    {
        sam3x_rtc_poll_datetime_update();

        if (g_update_state == SAM3X_RTC_UPDATE_DONE)
        {
            return g_update_result;
        }

        if (g_update_state == SAM3X_RTC_UPDATE_FAILED)
        {
            return g_update_result;
        }
    }

    sam3x_rtc_cancel_datetime_update(SAM3X_RTC_ACK_TIMEOUT);
    return SAM3X_RTC_ACK_TIMEOUT;
}

Sam3xRtcResult sam3x_rtc_start_datetime_update(const Sam3xRtcDateTime* date_time)
{
    if (g_update_state == SAM3X_RTC_UPDATE_WAIT_ACK)
    {
        return SAM3X_RTC_BUSY;
    }

    const Sam3xRtcResult result = prepare_datetime_registers(date_time);

    if (result != SAM3X_RTC_OK)
    {
        g_update_result = result;
        g_update_state = SAM3X_RTC_UPDATE_FAILED;
        return result;
    }

    RTC->RTC_IDR = RTC_IDR_ACKDIS;
    RTC->RTC_CR |= (RTC_CR_UPDTIM | RTC_CR_UPDCAL);

    g_update_result = SAM3X_RTC_BUSY;
    g_update_state = SAM3X_RTC_UPDATE_WAIT_ACK;

    return SAM3X_RTC_OK;
}

void sam3x_rtc_poll_datetime_update(void)
{
    if (g_update_state != SAM3X_RTC_UPDATE_WAIT_ACK)
    {
        return;
    }

    if ((RTC->RTC_SR & RTC_SR_ACKUPD) == RTC_SR_ACKUPD)
    {
        complete_datetime_update();
    }
}

void sam3x_rtc_cancel_datetime_update(Sam3xRtcResult result)
{
    RTC->RTC_CR &= ~(RTC_CR_UPDTIM | RTC_CR_UPDCAL);
    RTC->RTC_SCCR = RTC_SCCR_ACKCLR;

    g_update_result = result;
    g_update_state = SAM3X_RTC_UPDATE_FAILED;
}

Sam3xRtcUpdateState sam3x_rtc_update_state(void)
{
    return g_update_state;
}

Sam3xRtcResult sam3x_rtc_update_result(void)
{
    return g_update_result;
}

uint8_t sam3x_rtc_update_busy(void)
{
    return (g_update_state == SAM3X_RTC_UPDATE_WAIT_ACK) ? 1U : 0U;
}

uint32_t sam3x_rtc_status_register(void)
{
    return RTC->RTC_SR;
}

uint32_t sam3x_rtc_valid_entry_register(void)
{
    return RTC->RTC_VER;
}

uint32_t sam3x_rtc_time_register(void)
{
    return RTC->RTC_TIMR;
}

uint32_t sam3x_rtc_calendar_register(void)
{
    return RTC->RTC_CALR;
}

uint8_t sam3x_rtc_registers_valid(void)
{
    return ((RTC->RTC_VER & (RTC_VER_NVTIM | RTC_VER_NVCAL)) == 0U) ? 1U : 0U;
}

uint8_t sam3x_rtc_datetime_range_valid(const Sam3xRtcDateTime* date_time)
{
    if (date_time == 0)
    {
        return 0U;
    }

    const uint16_t normalized_year = sam3x_rtc_normalize_year(date_time->year);

    if ((normalized_year < 2000U) || (normalized_year > 2099U))
    {
        return 0U;
    }

    if ((date_time->month < 1U) || (date_time->month > 12U))
    {
        return 0U;
    }

    const uint8_t max_day = days_in_month(normalized_year, date_time->month);

    if ((date_time->day < 1U) || (date_time->day > max_day))
    {
        return 0U;
    }

    if (date_time->hour > 23U)
    {
        return 0U;
    }

    if (date_time->minute > 59U)
    {
        return 0U;
    }

    if (date_time->second > 59U)
    {
        return 0U;
    }

    return 1U;
}

uint16_t sam3x_rtc_normalize_year(uint16_t year)
{
    if (year < 100U)
    {
        return static_cast<uint16_t>(2000U + year);
    }

    return year;
}

uint8_t sam3x_rtc_calculate_day_of_week(uint16_t year, uint8_t month, uint8_t day)
{
    uint16_t normalized_year = sam3x_rtc_normalize_year(year);
    uint16_t y = normalized_year;
    uint8_t m = month;

    if ((m == 1U) || (m == 2U))
    {
        m = static_cast<uint8_t>(m + 12U);
        --y;
    }

    uint8_t week = static_cast<uint8_t>((day + (2U * m) + ((3U * (m + 1U)) / 5U) + y + (y / 4U) - (y / 100U) + (y / 400U)) % 7U);
    ++week;

    return week;
}

const char* sam3x_rtc_result_text(Sam3xRtcResult result)
{
    switch (result)
    {
        case SAM3X_RTC_OK:
            return "ok";

        case SAM3X_RTC_INVALID_ARGUMENT:
            return "invalid argument";

        case SAM3X_RTC_INVALID_DATE_TIME:
            return "invalid date/time";

        case SAM3X_RTC_ACK_TIMEOUT:
            return "ack timeout";

        case SAM3X_RTC_VERIFY_FAILED:
            return "verify failed";

        case SAM3X_RTC_BUSY:
            return "busy";

        default:
            return "unknown";
    }
}

const char* sam3x_rtc_update_state_text(Sam3xRtcUpdateState state)
{
    switch (state)
    {
        case SAM3X_RTC_UPDATE_IDLE:
            return "idle";

        case SAM3X_RTC_UPDATE_WAIT_ACK:
            return "wait ack";

        case SAM3X_RTC_UPDATE_DONE:
            return "done";

        case SAM3X_RTC_UPDATE_FAILED:
            return "failed";

        default:
            return "unknown";
    }
}
