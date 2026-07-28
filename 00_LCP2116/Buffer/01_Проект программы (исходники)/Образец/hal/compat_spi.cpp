#include "compat.hpp"

#include <avr/io.h>
#include <stdint.h>

/**
 * @file compat_spi.cpp
 * @brief SPI master-драйвер для ATmega328P.
 *
 * @details
 * Реализация используется для обмена с внешними SPI-регистрами платы LAI.
 *
 * Настройки интерфейса:
 * - режим ведущего;
 * - SPI mode 0;
 * - передача старшим битом вперёд;
 * - частота SCK = F_CPU / 16.
 */

namespace hal
{
    void spi_init(void)
    {
        /*
         * Для ATmega328P:
         * PB2 — SS;
         * PB3 — MOSI;
         * PB4 — MISO;
         * PB5 — SCK.
         *
         * PB2 должен быть настроен как выход и удерживаться в высоком уровне,
         * чтобы аппаратный SPI оставался в режиме ведущего.
         */
        DDRB |= static_cast<uint8_t>(_BV(PB2) | _BV(PB3) | _BV(PB5));
        DDRB &= static_cast<uint8_t>(~_BV(PB4));

        PORTB |= static_cast<uint8_t>(_BV(PB2));

        SPCR = static_cast<uint8_t>(_BV(SPE) | _BV(MSTR) | _BV(SPR0));
        SPSR = 0U;
    }

    uint8_t spi_txrx(uint8_t value)
    {
        SPDR = value;

        while ((SPSR & _BV(SPIF)) == 0U)
        {
            /*
             * Ожидание завершения передачи байта.
             */
        }

        return SPDR;
    }
}