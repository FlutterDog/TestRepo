
/**
 * @file sd_card_test.cpp
 * @brief Реализация диагностики microSD интерфейса.
 */

#include "sd_card_test.hpp"
#include "sd_config.hpp"
#include "../../board/lcp_sd.hpp"
#include "../../libs/lcp_sd_storage/lcp_sd_storage.hpp"
#include "../../platform/platform.hpp"

static const uint32_t SD_DETECT_DEBOUNCE_MS = 50U;
static const uint32_t SD_INIT_RETRY_MS = 1000U;

static uint8_t g_detect_stable = 0U;
static uint8_t g_detect_last_raw = 0U;
static uint8_t g_detect_debounced = 0U;
static uint32_t g_detect_last_change_ms = 0U;
static uint32_t g_last_init_attempt_ms = 0U;
static uint8_t g_begin_attempted = 0U;
static LcpSdStorageResult g_last_storage_result = LCP_SD_STORAGE_NOT_READY;

static void sd_update_detect(uint32_t now_ms)
{
    const uint8_t raw_inserted = lcp_sd_card_inserted();

    if (raw_inserted != g_detect_last_raw)
    {
        g_detect_last_raw = raw_inserted;
        g_detect_last_change_ms = now_ms;
        g_detect_stable = 0U;
        return;
    }

    if ((g_detect_stable == 0U) && ((uint32_t)(now_ms - g_detect_last_change_ms) >= SD_DETECT_DEBOUNCE_MS))
    {
        g_detect_stable = 1U;
        g_detect_debounced = raw_inserted;

        if (g_detect_debounced == 0U)
        {
            lcp_sd_storage_reset();
            lcp_sd_set_ok_led(0U);
        }
    }
}

void sd_card_test_init(void)
{
    lcp_sd_init_pins();
    lcp_sd_deselect();
    lcp_sd_set_ok_led(0U);

    g_detect_last_raw = lcp_sd_card_inserted();
    g_detect_debounced = g_detect_last_raw;
    g_detect_last_change_ms = millis();
    g_detect_stable = 0U;
    g_last_init_attempt_ms = 0U;
    g_begin_attempted = 0U;
    g_last_storage_result = LCP_SD_STORAGE_NOT_READY;

    sd_card_test_poll();
}

void sd_card_test_poll(void)
{
    const uint32_t now_ms = millis();

    sd_update_detect(now_ms);

    if (g_detect_debounced == 0U)
    {
        return;
    }

    if (lcp_sd_storage_ready() != 0U)
    {
        lcp_sd_set_ok_led(1U);
        return;
    }

    if ((g_begin_attempted != 0U) && ((uint32_t)(now_ms - g_last_init_attempt_ms) < SD_INIT_RETRY_MS))
    {
        return;
    }

    g_begin_attempted = 1U;
    g_last_init_attempt_ms = now_ms;
    g_last_storage_result = lcp_sd_storage_begin();

    lcp_sd_set_ok_led((g_last_storage_result == LCP_SD_STORAGE_OK) ? 1U : 0U);
}

void sd_card_test_print_report(void)
{
    SerialUSB.print("microSD status\r\n");
    SerialUSB.print("CS pin=23, detect pin=25, SD_OK pin=41\r\n");

    SerialUSB.print("detect raw=");
    SerialUSB.print(static_cast<int>(lcp_sd_detect_raw()));
    SerialUSB.print("\r\n");

    SerialUSB.print("card inserted=");
    SerialUSB.print(g_detect_debounced ? "yes" : "no");
    SerialUSB.print("\r\n");

    SerialUSB.print("init attempted=");
    SerialUSB.print(g_begin_attempted ? "yes" : "no");
    SerialUSB.print("\r\n");

    SerialUSB.print("filesystem ready=");
    SerialUSB.print(lcp_sd_storage_ready() ? "yes" : "no");
    SerialUSB.print("\r\n");

    SerialUSB.print("last result=");
    SerialUSB.print(lcp_sd_storage_result_text(g_last_storage_result));
    SerialUSB.print("\r\n");

    if (lcp_sd_storage_ready() != 0U)
    {
        SerialUSB.print("fat type=FAT");
        SerialUSB.print(static_cast<int>(lcp_sd_storage_fat_type()));
        SerialUSB.print(", sectors per cluster=");
        SerialUSB.print(static_cast<int>(lcp_sd_storage_sectors_per_cluster()));
        SerialUSB.print("\r\n");

        SerialUSB.print("SDTEST.TXT exists=");
        SerialUSB.print(lcp_sd_storage_exists("SDTEST.TXT") ? "yes" : "no");
        SerialUSB.print("\r\n");
    }
}

void sd_card_test_run_file_test(void)
{
    int16_t values[4];
    int16_t loaded[4];
    uint8_t loaded_count = 0U;

    SerialUSB.print("microSD file test started\r\n");

    sd_card_test_poll();

    if (lcp_sd_storage_ready() == 0U)
    {
        SerialUSB.print("microSD file test failed: card not ready\r\n");
        return;
    }

    values[0] = 3;
    values[1] = 111;
    values[2] = -222;
    values[3] = 333;

    const SdConfigResult save_result = sd_config_save_int16("SDTEST.TXT", values, 3U);

    SerialUSB.print("save SDTEST.TXT: ");
    SerialUSB.print(sd_config_result_text(save_result));
    SerialUSB.print("\r\n");

    if (save_result != SD_CONFIG_OK)
    {
        return;
    }

    const SdConfigResult load_result = sd_config_load_int16("SDTEST.TXT", loaded, 4U, &loaded_count);

    SerialUSB.print("load SDTEST.TXT: ");
    SerialUSB.print(sd_config_result_text(load_result));
    SerialUSB.print(", count=");
    SerialUSB.print(static_cast<int>(loaded_count));
    SerialUSB.print("\r\n");

    if ((load_result == SD_CONFIG_OK) &&
        (loaded_count == 3U) &&
        (loaded[1] == 111) &&
        (loaded[2] == -222) &&
        (loaded[3] == 333))
    {
        SerialUSB.print("microSD file test OK\r\n");
    }
    else
    {
        SerialUSB.print("microSD file test failed: data mismatch\r\n");
    }
}
