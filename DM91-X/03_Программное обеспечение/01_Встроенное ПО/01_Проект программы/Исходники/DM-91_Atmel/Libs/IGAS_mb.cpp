#include "IGAS_mb.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

#include "dm91_board.hpp"

/**
 * @file IGAS_mb.cpp
 * @brief Реализация ведомого устройства Modbus RTU по RS-485.
 *
 * @details
 * Модуль выполняет:
 * - приём Modbus RTU кадров через UART0;
 * - определение конца кадра по межкадровой паузе;
 * - проверку CRC16;
 * - проверку slave-адреса;
 * - обработку функций FC03, FC06 и FC16;
 * - формирование Modbus RTU ответов;
 * - переключение RS-485-трансивера через аппаратный слой платы.
 */

namespace
{
    /**
     * @brief Callback окончания физической передачи UART0.
     *
     * @details
     * Вызывается из обработчика прерывания UART TX Complete.
     * После этого события передающий сдвиговый регистр UART пуст,
     * и RS-485-трансивер можно вернуть в режим приёма.
     *
     * @warning
     * Функция выполняется в контексте прерывания и должна оставаться короткой.
     */
    void rs485TxCompleteCallback(void)
    {
        delayMicroseconds(20);
        dm91_board::rs485ReceiveMode();
    }
}

/*----------------------------------------------------------------------------
 * Конструктор
 *---------------------------------------------------------------------------*/

IGAS_mb_485::IGAS_mb_485(void)
    : slave(0),
      transmitLED(0),
      func(0),
      interframe_delay(5),
      fpAction(0),
      regLimit(0),
      Nowdt(0),
      lastBytesReceived(0)
{
}

/*----------------------------------------------------------------------------
 * Инициализация
 *---------------------------------------------------------------------------*/

void IGAS_mb_485::initMB(
    uint32_t COMM_BPS,
    uint8_t SERIAL_MODE,
    uint16_t allRegs
)
{
    Serial.begin(COMM_BPS, SERIAL_MODE);

    hal::uart0_set_tx_complete_callback(rs485TxCompleteCallback);
    hal::uart0_disable_tx_complete_interrupt();

    if (COMM_BPS == 0UL)
    {
        interframe_delay = 5U;
    }
    else
    {
        interframe_delay = static_cast<uint16_t>(40000UL / COMM_BPS);

        if (interframe_delay < 2U)
        {
            interframe_delay = 2U;
        }

        if (COMM_BPS <= 9600UL)
        {
            interframe_delay = 5U;
        }
    }

    regLimit = allRegs;
}

/*----------------------------------------------------------------------------
 * Основной обработчик
 *---------------------------------------------------------------------------*/

uint8_t IGAS_mb_485::update(void)
{
    const int16_t length_now =
        static_cast<int16_t>(Serial.available());

    if (length_now == 0)
    {
        lastBytesReceived = 0;
        return 0;
    }

    if (dm91_board::rs485IsTransmitMode())
    {
        return 0;
    }

    uint8_t query[MAX_MESSAGE_LENGTH];
    uint8_t errpacket[EXCEPTION_SIZE + CHECKSUM_SIZE];

    const uint32_t t_ms = millis();

    if (lastBytesReceived != length_now)
    {
        lastBytesReceived = length_now;
        Nowdt = t_ms;
        return 0;
    }

    if (static_cast<uint32_t>(t_ms - Nowdt) < interframe_delay)
    {
        return 0;
    }

    lastBytesReceived = 0;

    const int16_t frame_len = modbus_request(slave, query);

    if (frame_len < MIN_REQUEST_SIZE)
    {
        return 0;
    }

    const int exception =
        validate_request(query, static_cast<uint8_t>(frame_len));

    if (exception != 0)
    {
        build_error_packet(
            slave,
            query[FUNC],
            static_cast<uint8_t>(exception),
            errpacket
        );

        (void)send_reply(errpacket, EXCEPTION_SIZE);

        return static_cast<uint8_t>(exception);
    }

    const uint16_t start_addr =
        static_cast<uint16_t>(
            (static_cast<uint16_t>(query[START_H]) << 8) |
            static_cast<uint16_t>(query[START_L])
        );

    int16_t regs[MAX_MESSAGE_LENGTH];

    func = query[FUNC];

    switch (query[FUNC])
    {
        case FC_READ_REGS:
            return static_cast<uint8_t>(
                read_holding_registers(
                    slave,
                    start_addr,
                    query[REGS_L],
                    regs
                )
            );

        case FC_WRITE_REGS:
            return static_cast<uint8_t>(
                preset_multiple_registers(
                    slave,
                    start_addr,
                    query[REGS_L],
                    query,
                    regs
                )
            );

        case FC_WRITE_REG:
            (void)write_single_register(
                slave,
                start_addr,
                query,
                regs
            );
            return 0;

        default:
            return 0;
    }
}

/*----------------------------------------------------------------------------
 * CRC16
 *---------------------------------------------------------------------------*/

uint16_t IGAS_mb_485::crc(uint8_t* buf, uint8_t start, uint8_t cnt)
{
    uint16_t temp = 0xFFFFU;

    for (uint8_t i = start; i < cnt; ++i)
    {
        temp ^= static_cast<uint16_t>(buf[i]);

        for (uint8_t j = 0; j < 8U; ++j)
        {
            const uint16_t lsb =
                static_cast<uint16_t>(temp & 0x0001U);

            temp >>= 1;

            if (lsb != 0U)
            {
                temp ^= 0xA001U;
            }
        }
    }

    temp = static_cast<uint16_t>((temp << 8) | (temp >> 8));

    return temp;
}

/*----------------------------------------------------------------------------
 * Формирование пакетов
 *---------------------------------------------------------------------------*/

void IGAS_mb_485::build_read_packet(
    uint8_t slave,
    uint8_t function,
    uint8_t count,
    uint8_t* packet
)
{
    packet[SLAVE] = slave;
    packet[FUNC] = function;
    packet[2] = static_cast<uint8_t>(count * 2U);
}

void IGAS_mb_485::build_write_packet(
    uint8_t slave,
    uint8_t function,
    uint16_t start_addr,
    uint8_t count,
    uint8_t* packet
)
{
    packet[SLAVE] = slave;
    packet[FUNC] = function;
    packet[START_H] = static_cast<uint8_t>(start_addr >> 8);
    packet[START_L] = static_cast<uint8_t>(start_addr & 0x00FFU);
    packet[REGS_H] = 0x00;
    packet[REGS_L] = count;
}

void IGAS_mb_485::build_write_single_packet(
    uint8_t slave,
    uint8_t function,
    uint16_t write_addr,
    uint16_t reg_val,
    uint8_t* packet
)
{
    packet[SLAVE] = slave;
    packet[FUNC] = function;
    packet[START_H] = static_cast<uint8_t>(write_addr >> 8);
    packet[START_L] = static_cast<uint8_t>(write_addr & 0x00FFU);
    packet[REGS_H] = static_cast<uint8_t>(reg_val >> 8);
    packet[REGS_L] = static_cast<uint8_t>(reg_val & 0x00FFU);
}

void IGAS_mb_485::build_error_packet(
    uint8_t slave,
    uint8_t function,
    uint8_t exception,
    uint8_t* packet
)
{
    packet[SLAVE] = slave;
    packet[FUNC] = static_cast<uint8_t>(function + 0x80U);
    packet[2] = exception;
}

void IGAS_mb_485::modbus_reply(uint8_t* packet, uint8_t string_length)
{
    const uint16_t temp_crc = crc(packet, 0, string_length);

    packet[string_length++] = static_cast<uint8_t>(temp_crc >> 8);
    packet[string_length++] = static_cast<uint8_t>(temp_crc & 0x00FFU);
}

int IGAS_mb_485::send_reply(uint8_t* query, uint8_t string_length)
{
    modbus_reply(query, string_length);

    string_length = static_cast<uint8_t>(string_length + CHECKSUM_SIZE);

    (void)send(query, string_length);

    return 0;
}

/*----------------------------------------------------------------------------
 * Передача и приём
 *---------------------------------------------------------------------------*/

int IGAS_mb_485::send(uint8_t* query, uint8_t string_length)
{
    if ((query == 0) || (string_length == 0U))
    {
        return 0;
    }

    {
        const uint8_t old_sreg = SREG;
        cli();

        dm91_board::rs485TransmitMode();

        SREG = old_sreg;
    }

    transmitLED = dotDisp;

    delayMicroseconds(10);

    hal::uart0_enable_tx_complete_interrupt();

    uint8_t i = 0U;

    for (i = 0U; i < string_length; ++i)
    {
        Serial.write(query[i]);
    }

    return static_cast<int>(i);
}

int IGAS_mb_485::receive_request(uint8_t* received_string)
{
    int bytes_received = 0;

    while (Serial.available() != 0)
    {
        if (bytes_received >= MAX_MESSAGE_LENGTH)
        {
            return NO_REPLY;
        }

        const int byte_value = Serial.read();

        if (byte_value < 0)
        {
            break;
        }

        received_string[bytes_received] =
            static_cast<uint8_t>(byte_value);

        ++bytes_received;
    }

    return bytes_received;
}

int16_t IGAS_mb_485::modbus_request(uint8_t slave, uint8_t* data)
{
    const int response_length = receive_request(data);

    if (response_length <= 0)
    {
        return static_cast<int16_t>(response_length);
    }

    if (response_length < CHECKSUM_SIZE)
    {
        return NO_REPLY;
    }

    const uint16_t crc_calc =
        crc(data, 0, static_cast<uint8_t>(response_length - CHECKSUM_SIZE));

    uint16_t crc_received =
        static_cast<uint16_t>(data[response_length - 2]);

    crc_received =
        static_cast<uint16_t>(
            (crc_received << 8) |
            static_cast<uint16_t>(data[response_length - 1])
        );

    if (crc_calc != crc_received)
    {
        return NO_REPLY;
    }

    if (slave != data[SLAVE])
    {
        return NO_REPLY;
    }

    return static_cast<int16_t>(response_length);
}

/*----------------------------------------------------------------------------
 * Валидация запроса
 *---------------------------------------------------------------------------*/

int IGAS_mb_485::validate_request(uint8_t* data, uint8_t length)
{
    if (length < MIN_REQUEST_SIZE)
    {
        return EXC_REGS_QUANT;
    }

    bool function_supported = false;

    for (uint8_t i = 0U; i < static_cast<uint8_t>(sizeof(fsupported)); ++i)
    {
        if (fsupported[i] == data[FUNC])
        {
            function_supported = true;
            break;
        }
    }

    if (!function_supported)
    {
        return EXC_FUNC_CODE;
    }

    const uint16_t start_addr =
        static_cast<uint16_t>(
            (static_cast<uint16_t>(data[START_H]) << 8) |
            static_cast<uint16_t>(data[START_L])
        );

    if (start_addr >= regLimit)
    {
        return EXC_ADDR_RANGE;
    }

    if (data[FUNC] == FC_WRITE_REG)
    {
        return 0;
    }

    const uint16_t regs_num =
        static_cast<uint16_t>(
            (static_cast<uint16_t>(data[REGS_H]) << 8) |
            static_cast<uint16_t>(data[REGS_L])
        );

    uint8_t max_regs_num = 0U;

    if (data[FUNC] == FC_READ_REGS)
    {
        max_regs_num = MAX_READ_REGS;
    }
    else if (data[FUNC] == FC_WRITE_REGS)
    {
        max_regs_num = MAX_WRITE_REGS;
    }

    if ((regs_num < 1U) || (regs_num > static_cast<uint16_t>(max_regs_num)))
    {
        return EXC_REGS_QUANT;
    }

    if (regs_num > static_cast<uint16_t>(regLimit - start_addr))
    {
        return EXC_ADDR_RANGE;
    }

    if (data[FUNC] == FC_WRITE_REGS)
    {
        if (length < MIN_WRITE_MULTIPLE_REQUEST_SIZE)
        {
            return EXC_REGS_QUANT;
        }

        const uint8_t byte_count = data[BYTE_CNT];

        if (byte_count != static_cast<uint8_t>(regs_num * 2U))
        {
            return EXC_REGS_QUANT;
        }

        const uint16_t expected_length =
            static_cast<uint16_t>(
                static_cast<uint16_t>(BYTE_CNT + 1U) +
                static_cast<uint16_t>(byte_count) +
                CHECKSUM_SIZE
            );

        if (static_cast<uint16_t>(length) != expected_length)
        {
            return EXC_REGS_QUANT;
        }
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * Операции с регистрами
 *---------------------------------------------------------------------------*/

int IGAS_mb_485::write_regs(
    uint16_t start_addr,
    uint8_t* query,
    int16_t* regs
)
{
    uint16_t i = 0U;

    for (i = 0U; i < static_cast<uint16_t>(query[REGS_L]); ++i)
    {
        int16_t temp =
            static_cast<int16_t>(
                static_cast<uint16_t>(query[(BYTE_CNT + 1) + i * 2U]) << 8
            );

        temp = static_cast<int16_t>(
            temp |
            static_cast<int16_t>(query[(BYTE_CNT + 2) + i * 2U])
        );

        doAction(
            regs,
            static_cast<int16_t>(start_addr + i),
            singleRegister,
            temp
        );
    }

    return static_cast<int>(i);
}

int IGAS_mb_485::preset_multiple_registers(
    uint8_t slave,
    uint16_t start_addr,
    uint8_t count,
    uint8_t* query,
    int16_t* regs
)
{
    uint8_t packet[RESPONSE_SIZE + CHECKSUM_SIZE];

    build_write_packet(
        slave,
        FC_WRITE_REGS,
        start_addr,
        count,
        packet
    );

    if (write_regs(start_addr, query, regs) != 0)
    {
        return send_reply(packet, RESPONSE_SIZE);
    }

    return 0;
}

int IGAS_mb_485::write_single_register(
    uint8_t slave,
    uint16_t write_addr,
    uint8_t* query,
    int16_t* regs
)
{
    const uint16_t reg_val =
        static_cast<uint16_t>(
            (static_cast<uint16_t>(query[REGS_H]) << 8) |
            static_cast<uint16_t>(query[REGS_L])
        );

    uint8_t packet[RESPONSE_SIZE + CHECKSUM_SIZE];

    build_write_single_packet(
        slave,
        FC_WRITE_REG,
        write_addr,
        reg_val,
        packet
    );

    doAction(
        regs,
        static_cast<int16_t>(write_addr),
        singleRegister,
        static_cast<int16_t>(reg_val)
    );

    return send_reply(packet, RESPONSE_SIZE);
}

int IGAS_mb_485::read_holding_registers(
    uint8_t slave,
    uint16_t start_addr,
    uint8_t reg_count,
    int16_t* regs
)
{
    uint8_t packet[MAX_MESSAGE_LENGTH];
    int packet_size = 3;

    build_read_packet(
        slave,
        FC_READ_REGS,
        reg_count,
        packet
    );

    doAction(
        regs,
        static_cast<int16_t>(start_addr),
        static_cast<int16_t>(reg_count),
        0
    );

    for (uint8_t i = 0U; i < reg_count; ++i)
    {
        const uint16_t reg_value = static_cast<uint16_t>(regs[i]);

        packet[packet_size++] =
            static_cast<uint8_t>(reg_value >> 8);

        packet[packet_size++] =
            static_cast<uint8_t>(reg_value & 0x00FFU);
    }

    return send_reply(packet, static_cast<uint8_t>(packet_size));
}

/*----------------------------------------------------------------------------
 * Делегат карты регистров
 *---------------------------------------------------------------------------*/

void IGAS_mb_485::setDelegate(
    void (*fp)(int16_t*, int16_t, int16_t, int16_t)
)
{
    fpAction = fp;
}

void IGAS_mb_485::doAction(
    int16_t* t1,
    int16_t t2,
    int16_t t3,
    int16_t t4
)
{
    if (fpAction != 0)
    {
        (*fpAction)(t1, t2, t3, t4);
    }
}