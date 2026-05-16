#include "compat.hpp"

#include <avr/interrupt.h>
#include <stdint.h>

/**
 * @file compat_serial.cpp
 * @brief Драйвер UART0 для AVR: RX/TX кольцевые буферы и обслуживание в прерываниях.
 *
 * @details
 * Реализация предоставляет низкоуровневый интерфейс hal::uart0_* и поддерживает:
 * - настройку скорости и формата кадра через SERIAL_*;
 * - выбор режима U2X по критерию минимальной ошибки baudrate;
 * - отбраковку байтов с ошибками кадра, переполнения и, при включённом паритете,
 *   ошибки чётности;
 * - RX/TX кольцевые буферы фиксированного размера;
 * - передачу через прерывание UDRE;
 * - глобальный объект Serial для совместимости со старым Arduino-подобным кодом.
 *
 * @note
 * Размеры буферов задаются макросами SERIAL_RX_BUFFER_SIZE и SERIAL_TX_BUFFER_SIZE.
 * Размеры должны быть степенями двойки.
 *
 * @warning
 * hal::uart0_write() использует активное ожидание свободного места в TX-буфере.
 * Если глобальные прерывания запрещены и TX-буфер заполнен, функция будет ждать
 * бесконечно. Поэтому не следует выполнять длинную печать в критических секциях.
 *
 * @warning
 * Этот драйвер обслуживает только UART0. Для ATmega328P это основной UART,
 * используемый в LDO для Modbus RTU / RS-485.
 */

/*----------------------------------------------------------------------------
 * Настройки RX/TX буферов
 *---------------------------------------------------------------------------*/

#ifndef SERIAL_RX_BUFFER_SIZE
#  define SERIAL_RX_BUFFER_SIZE 64u
#endif

#ifndef SERIAL_TX_BUFFER_SIZE
#  define SERIAL_TX_BUFFER_SIZE 64u
#endif

#if ((SERIAL_RX_BUFFER_SIZE & (SERIAL_RX_BUFFER_SIZE - 1u)) != 0u)
#  error "SERIAL_RX_BUFFER_SIZE must be power of two"
#endif

#if ((SERIAL_TX_BUFFER_SIZE & (SERIAL_TX_BUFFER_SIZE - 1u)) != 0u)
#  error "SERIAL_TX_BUFFER_SIZE must be power of two"
#endif

#if (SERIAL_RX_BUFFER_SIZE > 256u)
#  error "SERIAL_RX_BUFFER_SIZE must be <= 256 because buffer indexes are uint8_t"
#endif

#if (SERIAL_TX_BUFFER_SIZE > 256u)
#  error "SERIAL_TX_BUFFER_SIZE must be <= 256 because buffer indexes are uint8_t"
#endif

/**
 * @brief Маска кольцевого RX-буфера.
 *
 * @details
 * Так как размер буфера является степенью двойки, операция взятия остатка
 * заменяется битовой маской.
 */
#define RX_MASK (SERIAL_RX_BUFFER_SIZE - 1u)

/**
 * @brief Маска кольцевого TX-буфера.
 */
#define TX_MASK (SERIAL_TX_BUFFER_SIZE - 1u)

/*----------------------------------------------------------------------------
 * Буферы и индексы
 *---------------------------------------------------------------------------*/

/**
 * @brief Кольцевой буфер приёма UART0.
 */
static volatile uint8_t rx_buf[SERIAL_RX_BUFFER_SIZE];

/**
 * @brief Кольцевой буфер передачи UART0.
 */
static volatile uint8_t tx_buf[SERIAL_TX_BUFFER_SIZE];

/**
 * @brief Индекс головы RX-буфера.
 *
 * @details
 * Изменяется в ISR приёма.
 */
static volatile uint8_t rx_head = 0u;

/**
 * @brief Индекс хвоста RX-буфера.
 *
 * @details
 * Изменяется в основном коде при чтении.
 */
static volatile uint8_t rx_tail = 0u;

/**
 * @brief Индекс головы TX-буфера.
 *
 * @details
 * Изменяется в основном коде при записи.
 */
static volatile uint8_t tx_head = 0u;

/**
 * @brief Индекс хвоста TX-буфера.
 *
 * @details
 * Изменяется в ISR UDRE при выгрузке байтов в UDR0.
 */
static volatile uint8_t tx_tail = 0u;

/**
 * @brief Признак включённого контроля паритета.
 *
 * @details
 * Если паритет включён, ISR приёма отбрасывает байты с ошибкой UPE0.
 */
static volatile uint8_t s_parity_enabled = 0u;

/*----------------------------------------------------------------------------
 * Локальные утилиты кольцевых буферов
 *---------------------------------------------------------------------------*/

/**
 * @brief Возвращает количество байт в RX-буфере.
 *
 * @return Количество доступных байт.
 */
static inline uint8_t rx_count(void)
{
    return static_cast<uint8_t>((rx_head - rx_tail) & RX_MASK);
}

/**
 * @brief Возвращает количество байт в TX-буфере.
 *
 * @return Количество байт, ожидающих отправки.
 */
static inline uint8_t tx_count(void)
{
    return static_cast<uint8_t>((tx_head - tx_tail) & TX_MASK);
}

/**
 * @brief Возвращает количество свободных мест в TX-буфере.
 *
 * @return Количество свободных байтов.
 *
 * @details
 * Одно место в кольцевом буфере намеренно резервируется, чтобы отличать
 * пустой буфер от полного.
 */
static inline uint8_t tx_space(void)
{
    return static_cast<uint8_t>(TX_MASK - tx_count());
}

/**
 * @brief Абсолютная разница двух uint32_t значений.
 *
 * @param a Первое значение.
 * @param b Второе значение.
 *
 * @return |a - b|.
 */
static inline uint32_t u32_abs_diff(uint32_t a, uint32_t b)
{
    return (a > b) ? (a - b) : (b - a);
}

/*----------------------------------------------------------------------------
 * Настройка UART0: baudrate и формат кадра
 *---------------------------------------------------------------------------*/

/**
 * @brief Рассчитывает UBRR и фактическую скорость для заданного режима UART.
 *
 * @param baud Запрошенная скорость, бод.
 * @param use_u2x true — использовать режим U2X, false — обычный режим.
 * @param[out] ubrr Рассчитанное значение UBRR.
 * @param[out] actual_baud Фактическая скорость при рассчитанном UBRR.
 *
 * @return true, если расчёт валиден.
 * @return false, если скорость не может быть представлена корректным UBRR.
 */
static bool uart0_calc_ubrr(
    uint32_t baud,
    uint8_t use_u2x,
    uint16_t* ubrr,
    uint32_t* actual_baud
)
{
    if (baud == 0u)
    {
        return false;
    }

    const uint32_t denom = use_u2x ? 8u : 16u;
    const uint32_t div = denom * baud;

    if (div == 0u)
    {
        return false;
    }

    const uint32_t q = static_cast<uint32_t>(F_CPU / div);
    const uint32_t r = static_cast<uint32_t>(F_CPU % div);

    if (q == 0u)
    {
        return false;
    }

    uint32_t ubrr_u32 = q - 1u;

    /*
     * Округление к ближайшему значению:
     * UBRR = round(F_CPU / (denom * baud)) - one.
     */
    if ((2u * r) >= div)
    {
        ubrr_u32 += 1u;
    }

    if (ubrr_u32 > 4095u)
    {
        return false;
    }

    *ubrr = static_cast<uint16_t>(ubrr_u32);
    *actual_baud = static_cast<uint32_t>(
        F_CPU / (denom * (static_cast<uint32_t>(*ubrr) + 1u))
    );

    return true;
}

/**
 * @brief Настраивает скорость UART0 и выбирает режим U2X.
 *
 * @param baud Скорость обмена, бод.
 *
 * @details
 * Выбирается обычный режим или режим U2X в зависимости от того, какой вариант
 * даёт меньшую абсолютную ошибку скорости.
 *
 * Для F_CPU = sixteen MHz сохранено проектное исключение:
 * - five hundred seventy-six hundred baud используется без U2X, если расчёт валиден.
 *
 * Это поведение оставлено из старой кодовой базы.
 */
static void uart0_set_baud(uint32_t baud)
{
    uint16_t ubrr_norm = 0u;
    uint16_t ubrr_u2x = 0u;

    uint32_t act_norm = 0u;
    uint32_t act_u2x = 0u;

    const bool ok_norm = uart0_calc_ubrr(baud, 0u, &ubrr_norm, &act_norm);
    const bool ok_u2x  = uart0_calc_ubrr(baud, 1u, &ubrr_u2x,  &act_u2x);

    uint8_t use_u2x = 0u;
    uint16_t ubrr = 0u;

#if (F_CPU == 16000000UL)
    /*
     * Проектное исключение: для 57600 бод на 16 MHz исторически используется
     * обычный режим без U2X.
     */
    if ((baud == 57600u) && ok_norm)
    {
        use_u2x = 0u;
        ubrr = ubrr_norm;
    }
    else
#endif
    {
        if (ok_norm && ok_u2x)
        {
            const uint32_t err_norm = u32_abs_diff(act_norm, baud);
            const uint32_t err_u2x  = u32_abs_diff(act_u2x,  baud);

            use_u2x = (err_u2x < err_norm) ? 1u : 0u;
            ubrr = use_u2x ? ubrr_u2x : ubrr_norm;
        }
        else if (ok_u2x)
        {
            use_u2x = 1u;
            ubrr = ubrr_u2x;
        }
        else if (ok_norm)
        {
            use_u2x = 0u;
            ubrr = ubrr_norm;
        }
        else
        {
            /*
             * Fallback на минимальный делитель.
             * Ситуация нештатная, но это лучше, чем оставить регистры в
             * неопределённом состоянии.
             */
            use_u2x = 0u;
            ubrr = 0u;
        }
    }

    if (use_u2x)
    {
        UCSR0A = _BV(U2X0);
    }
    else
    {
        UCSR0A = 0u;
    }

    UBRR0H = static_cast<uint8_t>(ubrr >> 8);
    UBRR0L = static_cast<uint8_t>(ubrr & 0xFFu);
}

/**
 * @brief Применяет формат кадра UART из конфигурационного байта SERIAL_*.
 *
 * @param cfg Константа формата кадра.
 *
 * @details
 * Поддерживаются:
 * - five, six, seven, eight бит данных;
 * - one или two стоп-бита;
 * - без паритета, even parity, odd parity.
 *
 * Биты cfg:
 * - 0x06 — длина слова;
 * - 0x08 — два стоп-бита;
 * - 0x30 — паритет.
 */
static void uart0_set_frame_from_cfg(uint8_t cfg)
{
    uint8_t ucsrc = 0u;

    /*
     * Длина слова: UCSZ01:0.
     * Режим nine-bit data не используется.
     */
    switch (cfg & 0x06u)
    {
        case 0x00u:
            /* five data bits */
            break;

        case 0x02u:
            /* six data bits */
            ucsrc |= _BV(UCSZ00);
            break;

        case 0x04u:
            /* seven data bits */
            ucsrc |= _BV(UCSZ01);
            break;

        case 0x06u:
            /* eight data bits */
            ucsrc |= static_cast<uint8_t>(_BV(UCSZ01) | _BV(UCSZ00));
            break;

        default:
            break;
    }

    /*
     * Stop bits: USBS0 = one означает two stop bits.
     */
    if ((cfg & 0x08u) != 0u)
    {
        ucsrc |= _BV(USBS0);
    }

    /*
     * Parity: UPM01:0.
     */
    const uint8_t parity = static_cast<uint8_t>(cfg & 0x30u);

    if (parity == 0x20u)
    {
        /* even parity */
        ucsrc |= _BV(UPM01);
        s_parity_enabled = 1u;
    }
    else if (parity == 0x30u)
    {
        /* odd parity */
        ucsrc |= static_cast<uint8_t>(_BV(UPM01) | _BV(UPM00));
        s_parity_enabled = 1u;
    }
    else
    {
        /* no parity */
        s_parity_enabled = 0u;
    }

    UCSR0C = ucsrc;
}

/**
 * @brief Включает UART0, приём, передачу и RX complete interrupt.
 */
static inline void uart0_enable_rx_tx_irq(void)
{
    UCSR0B = static_cast<uint8_t>(_BV(RXEN0) | _BV(TXEN0) | _BV(RXCIE0));
}

/**
 * @brief Полностью отключает UART0 и связанные прерывания.
 */
static inline void uart0_disable_rx_tx_irq(void)
{
    UCSR0B = 0u;
}

/*----------------------------------------------------------------------------
 * Реализация HAL UART0
 *---------------------------------------------------------------------------*/

namespace hal
{
    /**
     * @brief Инициализирует UART0.
     *
     * @param baud Скорость обмена, бод.
     * @param cfg Формат кадра SERIAL_*.
     *
     * @details
     * На время перенастройки UART глобальные прерывания запрещаются, а затем
     * восстанавливаются в прежнее состояние через сохранённый SREG.
     *
     * Это важно, потому что в текущей архитектуре main() может разрешить
     * прерывания до вызова setup(), а Serial.begin() вызывается уже из setup().
     */
    void uart0_init(uint32_t baud, uint8_t cfg)
    {
        const uint8_t old_sreg = SREG;
        cli();

        uart0_disable_rx_tx_irq();

        uart0_set_baud(baud);
        uart0_set_frame_from_cfg(cfg);

        rx_head = 0u;
        rx_tail = 0u;
        tx_head = 0u;
        tx_tail = 0u;

        /*
         * Сброс флага завершения передачи.
         * TXC0 сбрасывается записью логической единицы.
         */
        UCSR0A |= _BV(TXC0);

        uart0_enable_rx_tx_irq();

        SREG = old_sreg;
    }

    /**
     * @brief Возвращает количество принятых байт, доступных для чтения.
     *
     * @return Количество байт в RX-буфере.
     */
    uint8_t uart0_available(void)
    {
        return rx_count();
    }

    /**
     * @brief Читает один байт из RX-буфера.
     *
     * @return Значение от zero до two hundred fifty-five.
     * @return -1, если данных нет.
     */
    int16_t uart0_read(void)
    {
        if (rx_head == rx_tail)
        {
            return -1;
        }

        const uint8_t t = rx_tail;
        const uint8_t v = rx_buf[t];

        rx_tail = static_cast<uint8_t>((t + 1u) & RX_MASK);

        return static_cast<int16_t>(v);
    }

    /**
     * @brief Записывает один байт в TX-буфер.
     *
     * @param b Передаваемый байт.
     *
     * @details
     * Если TX-буфер заполнен, функция ожидает освобождения места.
     * Фактическая передача выполняется из ISR UDRE.
     */
    void uart0_write(uint8_t b)
    {
        while (tx_space() == 0u)
        {
            /*
             * Ожидание освобождения места в TX-буфере.
             * ISR UDRE выгружает данные из буфера в UDR0.
             */
        }

        const uint8_t was_empty = (tx_head == tx_tail);

        const uint8_t h = tx_head;
        tx_buf[h] = b;
        tx_head = static_cast<uint8_t>((h + 1u) & TX_MASK);

        if (was_empty)
        {
            /*
             * Сброс TXC перед началом новой передачи.
             * Это важно для RS-485: Modbus-слой ждёт TX complete interrupt,
             * чтобы вернуть драйвер в режим приёма после физического окончания кадра.
             */
            UCSR0A |= _BV(TXC0);
        }

        /*
         * Разрешаем UDRE interrupt. Он заберёт байты из TX-буфера и загрузит их в UDR0.
         */
        UCSR0B |= _BV(UDRIE0);
    }
}

/*----------------------------------------------------------------------------
 * Имена векторов прерываний UART0
 *---------------------------------------------------------------------------*/

#if defined(USART_RX_vect)
#  define UART0_RX_VECTOR USART_RX_vect
#elif defined(USART0_RX_vect)
#  define UART0_RX_VECTOR USART0_RX_vect
#else
#  error "UART0 RX vector name is not defined for this MCU"
#endif

#if defined(USART_UDRE_vect)
#  define UART0_UDRE_VECTOR USART_UDRE_vect
#elif defined(USART0_UDRE_vect)
#  define UART0_UDRE_VECTOR USART0_UDRE_vect
#else
#  error "UART0 UDRE vector name is not defined for this MCU"
#endif

/*----------------------------------------------------------------------------
 * Обработчики прерываний UART0
 *---------------------------------------------------------------------------*/

/**
 * @brief ISR приёма байта UART0.
 *
 * @details
 * Алгоритм:
 * - сначала считывается UCSR0A;
 * - затем считывается UDR0;
 * - байты с FE0 или DOR0 отбрасываются;
 * - если включён паритет, байты с UPE0 отбрасываются;
 * - корректный байт помещается в RX-буфер, если там есть место;
 * - при переполнении RX-буфера байт теряется.
 */
ISR(UART0_RX_VECTOR)
{
    const uint8_t status = UCSR0A;
    const uint8_t d = UDR0;

    if ((status & static_cast<uint8_t>(_BV(FE0) | _BV(DOR0))) != 0u)
    {
        return;
    }

    if ((s_parity_enabled != 0u) && ((status & _BV(UPE0)) != 0u))
    {
        return;
    }

    const uint8_t next = static_cast<uint8_t>((rx_head + 1u) & RX_MASK);

    if (next != rx_tail)
    {
        rx_buf[rx_head] = d;
        rx_head = next;
    }
    else
    {
        /*
         * RX overflow.
         * Байт потерян. Отдельный счётчик ошибок пока не ведётся,
         * чтобы не менять старый внешний интерфейс HAL.
         */
    }
}

/**
 * @brief ISR UDRE UART0.
 *
 * @details
 * Срабатывает, когда регистр данных UART пуст и готов принять следующий байт.
 * Если TX-буфер пуст, прерывание UDRE отключается.
 */
ISR(UART0_UDRE_VECTOR)
{
    if (tx_head == tx_tail)
    {
        UCSR0B &= static_cast<uint8_t>(~_BV(UDRIE0));
        return;
    }

    const uint8_t t = tx_tail;

    UDR0 = tx_buf[t];
    tx_tail = static_cast<uint8_t>((t + 1u) & TX_MASK);
}

/*----------------------------------------------------------------------------
 * Глобальный объект Serial
 *---------------------------------------------------------------------------*/

/**
 * @brief Глобальный потоковый интерфейс к UART0.
 *
 * @details
 * Объявление находится в compat.hpp.
 * Определение должно быть ровно одно во всём проекте.
 */
SerialPort Serial;