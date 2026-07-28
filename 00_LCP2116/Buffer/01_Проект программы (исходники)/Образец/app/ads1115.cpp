#include "ads1115.hpp"

/**
 * @file ads1115.cpp
 * @brief Реализация драйвера аналого-цифрового преобразователя ADS1115.
 */

namespace hal
{
    /**
     * @brief Инициализирует аппаратный интерфейс I2C.
     *
     * @param scl_hz Частота линии SCL, Гц.
     */
    void twi_init(uint32_t scl_hz);

    /**
     * @brief Записывает шестнадцатиразрядный регистр I2C-устройства.
     *
     * @param address Семибитный I2C-адрес устройства.
     * @param reg Адрес регистра.
     * @param value Значение регистра.
     *
     * @return true — запись выполнена успешно; false — ошибка обмена.
     */
    bool twi_write_reg16(uint8_t address, uint8_t reg, uint16_t value);

    /**
     * @brief Читает шестнадцатиразрядный регистр I2C-устройства.
     *
     * @param address Семибитный I2C-адрес устройства.
     * @param reg Адрес регистра.
     * @param value Указатель для сохранения прочитанного значения.
     *
     * @return true — чтение выполнено успешно; false — ошибка обмена.
     */
    bool twi_read_reg16(uint8_t address, uint8_t reg, uint16_t* value);
}

namespace
{
    /**
     * @brief Рабочая частота шины I2C для обмена с ADS1115.
     */
    static const uint32_t ADS1115_TWI_FREQUENCY_HZ = 400000UL;

    /**
     * @brief Адрес регистра результата преобразования.
     */
    static const uint8_t ADS1115_REG_CONVERSION = 0x00U;

    /**
     * @brief Адрес конфигурационного регистра.
     */
    static const uint8_t ADS1115_REG_CONFIG = 0x01U;

    /**
     * @brief Бит запуска одиночного преобразования.
     */
    static const uint16_t ADS1115_OS_START = 0x8000U;

    /**
     * @brief Маска бита готовности результата.
     */
    static const uint16_t ADS1115_OS_READY = 0x8000U;

    /**
     * @brief Код непрерывного режима преобразования.
     */
    static const uint16_t ADS1115_MODE_CONTINUOUS = 0x0000U;

    /**
     * @brief Код одиночного режима преобразования.
     */
    static const uint16_t ADS1115_MODE_SINGLE_SHOT = 0x0100U;

    /**
     * @brief Код отключения компаратора ADS1115.
     */
    static const uint16_t ADS1115_COMP_DISABLE = 0x0003U;

    /**
     * @brief Смещение поля MUX в конфигурационном регистре.
     */
    static const uint8_t ADS1115_MUX_SHIFT = 12U;

    /**
     * @brief Смещение поля PGA в конфигурационном регистре.
     */
    static const uint8_t ADS1115_PGA_SHIFT = 9U;

    /**
     * @brief Смещение поля DR в конфигурационном регистре.
     */
    static const uint8_t ADS1115_DATA_RATE_SHIFT = 5U;

    /**
     * @brief Базовый код single-ended канала AIN0.
     *
     * @details
     * Коды single-ended каналов ADS1115:
     * AIN0 — 100b, AIN1 — 101b, AIN2 — 110b, AIN3 — 111b.
     */
    static const uint8_t ADS1115_SINGLE_ENDED_MUX_BASE = 0x04U;

    /**
     * @brief Максимальный номер single-ended канала ADS1115.
     */
    static const uint8_t ADS1115_MAX_CHANNEL = 3U;
}

ADS1115::ADS1115(uint8_t address)
    : address_(address),
      gain_(0U),
      mode_(1U),
      dataRate_(7U)
{
}

bool ADS1115::begin(void)
{
    hal::twi_init(ADS1115_TWI_FREQUENCY_HZ);
    return true;
}

void ADS1115::setGain(uint8_t gain)
{
    gain_ = static_cast<uint8_t>(gain & 0x07U);
}

void ADS1115::setMode(uint8_t mode)
{
    mode_ = (mode != 0U) ? 1U : 0U;
}

void ADS1115::setDataRate(uint8_t dataRate)
{
    dataRate_ = static_cast<uint8_t>(dataRate & 0x07U);
}

bool ADS1115::startSingleShot(uint8_t channel)
{
    if (channel > ADS1115_MAX_CHANNEL)
    {
        return false;
    }

    return writeRegister(ADS1115_REG_CONFIG, buildSingleShotConfig(channel));
}

bool ADS1115::isBusy(void)
{
    uint16_t config = 0U;

    if (!readRegister(ADS1115_REG_CONFIG, &config))
    {
        return true;
    }

    return ((config & ADS1115_OS_READY) == 0U);
}

int16_t ADS1115::getValue(void)
{
    uint16_t value = 0U;

    if (!readRegister(ADS1115_REG_CONVERSION, &value))
    {
        return 0;
    }

    return static_cast<int16_t>(value);
}

uint16_t ADS1115::buildSingleShotConfig(uint8_t channel) const
{
    const uint16_t mux = static_cast<uint16_t>(
        static_cast<uint16_t>(ADS1115_SINGLE_ENDED_MUX_BASE + channel) << ADS1115_MUX_SHIFT
    );

    const uint16_t pga = static_cast<uint16_t>(
        static_cast<uint16_t>(gain_ & 0x07U) << ADS1115_PGA_SHIFT
    );

    const uint16_t mode = (mode_ != 0U)
        ? ADS1115_MODE_SINGLE_SHOT
        : ADS1115_MODE_CONTINUOUS;

    const uint16_t dataRate = static_cast<uint16_t>(
        static_cast<uint16_t>(dataRate_ & 0x07U) << ADS1115_DATA_RATE_SHIFT
    );

    return static_cast<uint16_t>(
        ADS1115_OS_START |
        mux |
        pga |
        mode |
        dataRate |
        ADS1115_COMP_DISABLE
    );
}

bool ADS1115::writeRegister(uint8_t reg, uint16_t value)
{
    return hal::twi_write_reg16(address_, reg, value);
}

bool ADS1115::readRegister(uint8_t reg, uint16_t* value)
{
    if (value == 0)
    {
        return false;
    }

    return hal::twi_read_reg16(address_, reg, value);
}