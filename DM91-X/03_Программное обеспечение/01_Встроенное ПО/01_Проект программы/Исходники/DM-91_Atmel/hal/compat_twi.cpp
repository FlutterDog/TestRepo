#include "compat.hpp"

#include <avr/io.h>
#include <util/twi.h>
#include <stdint.h>

/**
 * @file compat_twi.cpp
 * @brief TWI/I2C master-драйвер для ATmega328P.
 *
 * @details
 * Реализация содержит базовые операции ведущего устройства:
 * - передачу массива байт;
 * - приём массива байт;
 * - комбинированный обмен с repeated START;
 * - обмен с шестнадцатибитными регистрами с передачей старшего байта первым.
 */

namespace
{
    /**
     * @brief Лимит ожидания установки флага TWINT.
     *
     * @details
     * Значение задано в итерациях цикла ожидания. Таймаут защищает основной
     * цикл программы от зависания при неисправности шины или отсутствии ответа
     * ведомого устройства.
     */
    static const uint16_t TWI_TIMEOUT = 60000U;

    /**
     * @brief Ожидает завершения текущей операции TWI.
     *
     * @return true — флаг TWINT установлен; false — истёк таймаут ожидания.
     */
    static bool waitTwint(void)
    {
        uint16_t timeout = TWI_TIMEOUT;

        while ((TWCR & _BV(TWINT)) == 0U)
        {
            if (timeout == 0U)
            {
                return false;
            }

            --timeout;
        }

        return true;
    }

    /**
     * @brief Формирует START или repeated START на шине TWI.
     *
     * @return true — условие START сформировано успешно; false — ошибка или таймаут.
     */
    static bool startCondition(void)
    {
        TWCR = static_cast<uint8_t>(_BV(TWINT) | _BV(TWSTA) | _BV(TWEN));

        if (!waitTwint())
        {
            return false;
        }

        const uint8_t status = TW_STATUS;
        return ((status == TW_START) || (status == TW_REP_START));
    }

    /**
     * @brief Формирует STOP на шине TWI.
     */
    static void stopCondition(void)
    {
        TWCR = static_cast<uint8_t>(_BV(TWINT) | _BV(TWEN) | _BV(TWSTO));
    }

    /**
     * @brief Передаёт один байт данных.
     *
     * @param value Передаваемый байт.
     *
     * @return true — байт подтверждён ведомым устройством; false — ошибка обмена.
     */
    static bool writeByte(uint8_t value)
    {
        TWDR = value;
        TWCR = static_cast<uint8_t>(_BV(TWINT) | _BV(TWEN));

        if (!waitTwint())
        {
            return false;
        }

        return (TW_STATUS == TW_MT_DATA_ACK);
    }

    /**
     * @brief Передаёт адрес ведомого устройства для операции записи.
     *
     * @param address Семибитный I2C-адрес устройства.
     *
     * @return true — адрес подтверждён; false — ошибка обмена.
     */
    static bool sendAddressWrite(uint8_t address)
    {
        TWDR = static_cast<uint8_t>((address << 1) | TW_WRITE);
        TWCR = static_cast<uint8_t>(_BV(TWINT) | _BV(TWEN));

        if (!waitTwint())
        {
            return false;
        }

        return (TW_STATUS == TW_MT_SLA_ACK);
    }

    /**
     * @brief Передаёт адрес ведомого устройства для операции чтения.
     *
     * @param address Семибитный I2C-адрес устройства.
     *
     * @return true — адрес подтверждён; false — ошибка обмена.
     */
    static bool sendAddressRead(uint8_t address)
    {
        TWDR = static_cast<uint8_t>((address << 1) | TW_READ);
        TWCR = static_cast<uint8_t>(_BV(TWINT) | _BV(TWEN));

        if (!waitTwint())
        {
            return false;
        }

        return (TW_STATUS == TW_MR_SLA_ACK);
    }

    /**
     * @brief Читает байт с подтверждением ACK.
     *
     * @param value Указатель для сохранения прочитанного байта.
     *
     * @return true — байт прочитан успешно; false — ошибка обмена.
     */
    static bool readByteAck(uint8_t* value)
    {
        if (value == 0)
        {
            return false;
        }

        TWCR = static_cast<uint8_t>(_BV(TWINT) | _BV(TWEN) | _BV(TWEA));

        if (!waitTwint())
        {
            return false;
        }

        if (TW_STATUS != TW_MR_DATA_ACK)
        {
            return false;
        }

        *value = TWDR;
        return true;
    }

    /**
     * @brief Читает байт без подтверждения ACK.
     *
     * @param value Указатель для сохранения прочитанного байта.
     *
     * @return true — байт прочитан успешно; false — ошибка обмена.
     */
    static bool readByteNack(uint8_t* value)
    {
        if (value == 0)
        {
            return false;
        }

        TWCR = static_cast<uint8_t>(_BV(TWINT) | _BV(TWEN));

        if (!waitTwint())
        {
            return false;
        }

        if (TW_STATUS != TW_MR_DATA_NACK)
        {
            return false;
        }

        *value = TWDR;
        return true;
    }
}

namespace hal
{
    void twi_init(uint32_t scl_hz)
    {
        if (scl_hz == 0UL)
        {
            scl_hz = 100000UL;
        }

        /*
         * Prescaler = 1.
         * SCL = F_CPU / (16 + 2 * TWBR * prescaler).
         */
        TWSR = 0U;

        uint32_t twbrValue = 0UL;

        if (F_CPU > (16UL * scl_hz))
        {
            twbrValue = ((F_CPU / scl_hz) - 16UL) / 2UL;
        }

        if (twbrValue > 255UL)
        {
            twbrValue = 255UL;
        }

        TWBR = static_cast<uint8_t>(twbrValue);

        /*
         * Включение аппаратного TWI.
         * Подтягивающие резисторы SDA/SCL должны быть обеспечены схемой платы.
         */
        TWCR = static_cast<uint8_t>(_BV(TWEN));
    }

    bool twi_write(uint8_t address, const uint8_t* data, uint8_t length)
    {
        if ((length > 0U) && (data == 0))
        {
            return false;
        }

        bool ok = startCondition();

        if (ok)
        {
            ok = sendAddressWrite(address);
        }

        for (uint8_t index = 0U; ok && (index < length); ++index)
        {
            ok = writeByte(data[index]);
        }

        stopCondition();
        return ok;
    }

    bool twi_read(uint8_t address, uint8_t* data, uint8_t length)
    {
        if (length == 0U)
        {
            return true;
        }

        if (data == 0)
        {
            return false;
        }

        bool ok = startCondition();

        if (ok)
        {
            ok = sendAddressRead(address);
        }

        for (uint8_t index = 0U; ok && (index < length); ++index)
        {
            if (index < static_cast<uint8_t>(length - 1U))
            {
                ok = readByteAck(&data[index]);
            }
            else
            {
                ok = readByteNack(&data[index]);
            }
        }

        stopCondition();
        return ok;
    }

    bool twi_write_read(
        uint8_t address,
        const uint8_t* tx_data,
        uint8_t tx_length,
        uint8_t* rx_data,
        uint8_t rx_length
    )
    {
        if (rx_length == 0U)
        {
            return twi_write(address, tx_data, tx_length);
        }

        if (tx_length == 0U)
        {
            return twi_read(address, rx_data, rx_length);
        }

        if ((tx_data == 0) || (rx_data == 0))
        {
            return false;
        }

        bool ok = startCondition();

        if (ok)
        {
            ok = sendAddressWrite(address);
        }

        for (uint8_t index = 0U; ok && (index < tx_length); ++index)
        {
            ok = writeByte(tx_data[index]);
        }

        if (ok)
        {
            ok = startCondition();
        }

        if (ok)
        {
            ok = sendAddressRead(address);
        }

        for (uint8_t index = 0U; ok && (index < rx_length); ++index)
        {
            if (index < static_cast<uint8_t>(rx_length - 1U))
            {
                ok = readByteAck(&rx_data[index]);
            }
            else
            {
                ok = readByteNack(&rx_data[index]);
            }
        }

        stopCondition();
        return ok;
    }

    bool twi_write_reg16(uint8_t address, uint8_t reg, uint16_t value)
    {
        bool ok = startCondition();

        if (ok)
        {
            ok = sendAddressWrite(address);
        }

        if (ok)
        {
            ok = writeByte(reg);
        }

        if (ok)
        {
            ok = writeByte(static_cast<uint8_t>(value >> 8));
        }

        if (ok)
        {
            ok = writeByte(static_cast<uint8_t>(value & 0x00FFU));
        }

        stopCondition();
        return ok;
    }

    bool twi_read_reg16(uint8_t address, uint8_t reg, uint16_t* value)
    {
        if (value == 0)
        {
            return false;
        }

        bool ok = startCondition();

        if (ok)
        {
            ok = sendAddressWrite(address);
        }

        if (ok)
        {
            ok = writeByte(reg);
        }

        if (ok)
        {
            ok = startCondition();
        }

        if (ok)
        {
            ok = sendAddressRead(address);
        }

        uint8_t msb = 0U;
        uint8_t lsb = 0U;

        if (ok)
        {
            ok = readByteAck(&msb);
        }

        if (ok)
        {
            ok = readByteNack(&lsb);
        }

        stopCondition();

        if (ok)
        {
            *value = static_cast<uint16_t>(
                (static_cast<uint16_t>(msb) << 8) |
                static_cast<uint16_t>(lsb)
            );
        }

        return ok;
    }
}