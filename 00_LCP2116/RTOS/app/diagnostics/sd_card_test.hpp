/**
 * @file sd_card_test.hpp
 * @brief Диагностика card-detect, mount и базовых операций microSD.
 *
 * Обычный poll не пишет на карту. Запись выполняется только явной командой
 * `sd test`, которая перезаписывает отдельный файл SDTEST.TXT и не затрагивает
 * X2X.TXT, baud.TXT, Parity.TXT или Ethernet-конфигурацию.
 */

#pragma once

/** @brief Настраивает CS/detect/LED и запускает первую проверку карты. */
void sd_card_test_init(void);

/**
 * @brief Выполняет debounce detect и ограниченный по частоте повтор mount.
 *
 * Вызов не ждёт появления карты. После извлечения готовность FAT сбрасывается,
 * а после установки инициализация повторяется не чаще заданного retry period.
 */
void sd_card_test_poll(void);

/** @brief Печатает detect, mount result, FAT type и наличие SDTEST.TXT. */
void sd_card_test_print_report(void);

/**
 * @brief Синхронно записывает контрольные числа в SDTEST.TXT и читает обратно.
 *
 * Это сервисная операция для ручного запуска; не вызывайте её из cyclic poll.
 */
void sd_card_test_run_file_test(void);
