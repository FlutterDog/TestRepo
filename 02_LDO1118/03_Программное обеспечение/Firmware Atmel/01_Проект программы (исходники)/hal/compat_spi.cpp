#include "compat.hpp"

/**
 * @file compat_spi.cpp
 * @brief Низкоуровневый SPI-драйвер для AVR (ведущий, Mode 0, Fosc/4).
 *
 * @details
 * Настройка линий SPI для типового AVR (например, ATmega328P):
 * - PB3 — MOSI (выход)
 * - PB4 — MISO (вход)
 * - PB5 — SCK  (выход)
 * - PB2 — SS   (выход; должен оставаться выходом для сохранения режима Master)
 *
 * Передача реализована в блокирующем виде: `spi_txrx()` ожидает установки флага SPIF.
 */

namespace hal
{
    void spi_init(void)
    {
        /*
         * Важно: в режиме Master аппаратный вход SS должен быть сконфигурирован как OUTPUT,
         * иначе при появлении LOW на SS контроллер может переключиться в Slave.
         */
        DDRB |= (uint8_t)(_BV(DDB3) | _BV(DDB5) | _BV(DDB2)); /* MOSI, SCK, SS -> output */
        DDRB &= (uint8_t)~_BV(DDB4);                          /* MISO -> input */

        /*
         * SPI: enable, master, mode 0 (CPOL=0, CPHA=0), clock = Fosc/4
         * SPR1:0 = 00, SPI2X = 0.
         */
        SPCR = (uint8_t)(_BV(SPE) | _BV(MSTR));
        SPSR = 0u;
    }

    uint8_t spi_txrx(uint8_t v)
    {
        SPDR = v;

        /* Ожидание завершения передачи (SPIF=1). */
        while ((SPSR & _BV(SPIF)) == 0u) {
            /* busy-wait */
        }

        return SPDR;
    }
} // namespace hal
