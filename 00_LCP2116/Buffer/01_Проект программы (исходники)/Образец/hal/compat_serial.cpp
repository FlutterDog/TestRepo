#include "compat.hpp"

#include <avr/interrupt.h>
#include <stdint.h>

/**
 * @file compat_serial.cpp
 * @brief Драйвер UART0 для AVR с RX/TX кольцевыми буферами.
 *
 * @details
 * Реализация предоставляет низкоуровневый интерфейс hal::uart0_* и поддерживает:
 * - настройку скорости и формата кадра через SERIAL_*;
 * - выбор режима U2X по минимальной ошибке baudrate;
 * - отбраковку байтов с ошибками кадра, переполнения и паритета;
 * - RX/TX кольцевые буферы фиксированного размера;
 * - передачу через прерывание UDRE;
 * - callback окончания физической передачи UART TX Complete;
 * - глобальный объект SerialPort для потоковой работы с UART0.
 *
 * @note
 * Размеры буферов задаются макросами SERIAL_RX_BUFFER_SIZE и SERIAL_TX_BUFFER_SIZE.
 * Размеры должны быть степенями двойки.
 *
 * @warning
 * hal::uart0_write() использует активное ожидание свободного места в TX-буфере.
 * Если глобальные прерывания запрещены и TX-буфер заполнен, функция будет ждать
 * бесконечно.
 *
 * @warning
 * Этот драйвер обслуживает только UART0.
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
 */
static volatile uint8_t rx_head = 0u;

/**
 * @brief Индекс хвоста RX-буфера.
 */
static volatile uint8_t rx_tail = 0u;

/**
 * @brief Индекс головы TX-буфера.
 */
static volatile uint8_t tx_head = 0u;

/**
 * @brief Индекс хвоста TX-буфера.
 */
static volatile uint8_t tx_tail = 0u;

/**
 * @brief Признак включённого контроля паритета.
 */
static volatile uint8_t s_parity_enabled = 0u;

/**
 * @brief Callback окончания физической передачи UART0.
 *
 * @details
 * Callback вызывается из ISR UART TX Complete. Внешний слой может использовать
 * это событие для действий после полного окончания передачи последнего байта.
 */
static hal::Uart0TxCompleteCallback s_tx_complete_callback = 0;

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
 * Одно место в кольцевом буфере резервируется для различения пустого
 * и полного состояния.
 */
static inline uint8_t tx_space(void)
{
    return static_cast<uint8_t>(TX_MASK - tx_count());
}

/**
 * @brief Возвращает абсолютную разницу двух uint32_t значений.
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
 * @param baud Требуемая скорость обмена, бод.
 * @param use_u2x 1 — использовать режим U2X; 0 — обычный режим.
 * @param ubrr Указатель для возврата рассчитанного значения UBRR.
 * @param actual_baud Указатель для возврата фактической скорости.
 *
 * @return true, если расчёт выполнен успешно.
 * @return false, если расчёт невозможен для заданных параметров.
 */
static bool uart0_calc_ubrr(
    uint32_t baud,
    uint8_t use_u2x,
    uint16_t* ubrr,
    uint32_t* actual_baud
)
{
    if ((baud == 0u) || (ubrr == 0) || (actual_baud == 0))
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
 * @brief Настраивает скорость UART0.
 *
 * @param baud Требуемая скорость обмена, бод.
 *
 * @details
 * Функция рассчитывает варианты для обычного режима и режима U2X,
 * выбирает вариант с меньшей ошибкой и записывает UBRR0H/UBRR0L.
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
 * @brief Применяет формат кадра UART.
 *
 * @param cfg Конфигурационный байт SERIAL_*.
 *
 * @details
 * Настраиваются:
 * - количество бит данных;
 * - количество стоп-битов;
 * - режим паритета.
 */
static void uart0_set_frame_from_cfg(uint8_t cfg)
{
    uint8_t ucsrc = 0u;

    switch (cfg & 0x06u)
    {
        case 0x00u:
            break;

        case 0x02u:
            ucsrc |= _BV(UCSZ00);
            break;

        case 0x04u:
            ucsrc |= _BV(UCSZ01);
            break;

        case 0x06u:
            ucsrc |= static_cast<uint8_t>(_BV(UCSZ01) | _BV(UCSZ00));
            break;

        default:
            break;
    }

    if ((cfg & 0x08u) != 0u)
    {
        ucsrc |= _BV(USBS0);
    }

    const uint8_t parity = static_cast<uint8_t>(cfg & 0x30u);

    if (parity == 0x20u)
    {
        ucsrc |= _BV(UPM01);
        s_parity_enabled = 1u;
    }
    else if (parity == 0x30u)
    {
        ucsrc |= static_cast<uint8_t>(_BV(UPM01) | _BV(UPM00));
        s_parity_enabled = 1u;
    }
    else
    {
        s_parity_enabled = 0u;
    }

    UCSR0C = ucsrc;
}

/**
 * @brief Включает UART0, приём, передачу и прерывание приёма.
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
     * На время перенастройки UART0 прерывания кратковременно запрещаются.
     * После инициализации очищаются программные RX/TX буферы.
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

        UCSR0A |= _BV(TXC0);

        uart0_enable_rx_tx_irq();

        SREG = old_sreg;
    }

    /**
     * @brief Возвращает количество принятых байт.
     *
     * @return Количество байт, доступных для чтения.
     */
    uint8_t uart0_available(void)
    {
        return rx_count();
    }

    /**
     * @brief Читает один байт из RX-буфера.
     *
     * @return Значение от 0 до 255, если байт прочитан.
     * @return -1, если RX-буфер пуст.
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
     * Фактическая передача выполняется в ISR UDRE.
     */
    void uart0_write(uint8_t b)
    {
        while (tx_space() == 0u)
        {
            /*
             * Ожидание освобождения места в TX-буфере.
             */
        }

        const uint8_t was_empty = (tx_head == tx_tail);

        const uint8_t h = tx_head;
        tx_buf[h] = b;
        tx_head = static_cast<uint8_t>((h + 1u) & TX_MASK);

        if (was_empty)
        {
            /*
             * TXC0 очищается перед запуском передачи нового блока данных.
             * Флаг очищается записью логической единицы.
             */
            UCSR0A |= _BV(TXC0);
        }

        UCSR0B |= _BV(UDRIE0);
    }

    /**
     * @brief Назначает callback окончания физической передачи UART0.
     *
     * @param callback Указатель на callback-функцию или 0.
     */
    void uart0_set_tx_complete_callback(Uart0TxCompleteCallback callback)
    {
        const uint8_t old_sreg = SREG;
        cli();

        s_tx_complete_callback = callback;

        SREG = old_sreg;
    }

    /**
     * @brief Очищает TXC0 и разрешает прерывание UART0 TX Complete.
     *
     * @details
     * Функцию следует вызывать до постановки первого байта кадра в TX-буфер.
     * После завершения физической передачи последнего байта UART установит TXC0,
     * будет вызван ISR TX Complete, затем прерывание TX Complete отключится.
     */
    void uart0_enable_tx_complete_interrupt(void)
    {
        const uint8_t old_sreg = SREG;
        cli();

        UCSR0A |= static_cast<uint8_t>(_BV(TXC0));
        UCSR0B |= static_cast<uint8_t>(_BV(TXCIE0));

        SREG = old_sreg;
    }

    /**
     * @brief Запрещает прерывание UART0 TX Complete.
     */
    void uart0_disable_tx_complete_interrupt(void)
    {
        const uint8_t old_sreg = SREG;
        cli();

        UCSR0B &= static_cast<uint8_t>(~_BV(TXCIE0));

        SREG = old_sreg;
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

#if defined(USART_TX_vect)
#  define UART0_TX_VECTOR USART_TX_vect
#elif defined(USART0_TX_vect)
#  define UART0_TX_VECTOR USART0_TX_vect
#else
#  error "UART0 TX vector name is not defined for this MCU"
#endif

/*----------------------------------------------------------------------------
 * Обработчики прерываний UART0
 *---------------------------------------------------------------------------*/

/**
 * @brief ISR приёма байта UART0.
 *
 * @details
 * Считывает статус UART до чтения UDR0, проверяет ошибки кадра,
 * переполнения и паритета, затем помещает байт в RX-буфер.
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
         * RX-буфер заполнен. Принятый байт отбрасывается.
         */
    }
}

/**
 * @brief ISR готовности регистра данных UART0.
 *
 * @details
 * Выгружает следующий байт из TX-буфера в UDR0.
 * Если буфер пуст, отключает прерывание UDRE.
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

/**
 * @brief ISR окончания физической передачи UART0.
 *
 * @details
 * Срабатывает после передачи последнего стоп-бита, когда регистр данных
 * и сдвиговый регистр передатчика пусты. После срабатывания прерывание
 * TX Complete отключается и вызывается назначенный callback.
 */
ISR(UART0_TX_VECTOR)
{
    UCSR0B &= static_cast<uint8_t>(~_BV(TXCIE0));

    if (s_tx_complete_callback != 0)
    {
        s_tx_complete_callback();
    }
}

/*----------------------------------------------------------------------------
 * Глобальный объект UART0
 *---------------------------------------------------------------------------*/

/**
 * @brief Глобальный потоковый объект UART0.
 *
 * @details
 * Объявление находится в compat.hpp.
 * Определение должно быть ровно одно во всём проекте.
 */
SerialPort Serial;