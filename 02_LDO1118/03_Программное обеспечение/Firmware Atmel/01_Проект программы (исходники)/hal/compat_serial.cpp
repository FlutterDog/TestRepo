#include "compat.hpp"

#include <avr/interrupt.h>
#include <stdint.h>

/**
 * @file compat_serial.cpp
 * @brief Драйвер UART0 для AVR: RX/TX кольцевые буферы, обслуживание в прерываниях.
 *
 * @details
 * Реализация предоставляет низкоуровневый интерфейс `hal::uart0_*` и поддерживает:
 * - настройку скорости и формата кадра (константы `SERIAL_*` из compat.hpp);
 * - выбор режима U2X (двойная скорость UART) по критерию минимальной ошибки baudrate;
 * - отбраковку байтов с ошибками кадра/переполнения и (при включённом паритете) ошибки чётности;
 * - RX/TX кольцевые буферы фиксированного размера (степень двойки);
 * - передачу через прерывание UDRE (пустой регистр данных).
 *
 * @note
 * Размеры буферов задаются макросами `SERIAL_RX_BUFFER_SIZE` и `SERIAL_TX_BUFFER_SIZE` и
 * должны быть степенями двойки (8, 16, 32, 64, ...).
 *
 * @warning
 * `hal::uart0_write()` использует активное ожидание наличия места в TX-буфере. Если глобальные
 * прерывания запрещены и буфер заполнен, функция будет ждать бесконечно.
 */

/* ===================== Настройки буферов (power-of-two) ===================== */
#ifndef SERIAL_RX_BUFFER_SIZE
#  define SERIAL_RX_BUFFER_SIZE 64u
#endif

#ifndef SERIAL_TX_BUFFER_SIZE
#  define SERIAL_TX_BUFFER_SIZE 64u
#endif

#if ((SERIAL_RX_BUFFER_SIZE & (SERIAL_RX_BUFFER_SIZE - 1u)) != 0u) || \
    ((SERIAL_TX_BUFFER_SIZE & (SERIAL_TX_BUFFER_SIZE - 1u)) != 0u)
#  error "SERIAL_*_BUFFER_SIZE must be power of two"
#endif

#define RX_MASK (SERIAL_RX_BUFFER_SIZE - 1u)
#define TX_MASK (SERIAL_TX_BUFFER_SIZE - 1u)

/* ===================== Глобальные буферы/индексы ===================== */
/** @brief RX кольцевой буфер (приём). */
static volatile uint8_t rx_buf[SERIAL_RX_BUFFER_SIZE];
/** @brief TX кольцевой буфер (передача). */
static volatile uint8_t tx_buf[SERIAL_TX_BUFFER_SIZE];

/** @brief Индексы головы/хвоста RX-буфера. */
static volatile uint8_t rx_head = 0u;
static volatile uint8_t rx_tail = 0u;
/** @brief Индексы головы/хвоста TX-буфера. */
static volatile uint8_t tx_head = 0u;
static volatile uint8_t tx_tail = 0u;

/**
 * @brief Признак включённого паритета.
 * @details Используется в ISR приёма: если паритет включён, байты с UPE отбрасываются.
 */
static volatile uint8_t s_parity_enabled = 0u;

/* ===================== Локальные утилиты буферов ===================== */
/** @brief Количество байт, доступных в RX. */
static inline uint8_t rx_count(void)
{
    return (uint8_t)((rx_head - rx_tail) & RX_MASK);
}

/** @brief Количество байт, находящихся в TX. */
static inline uint8_t tx_count(void)
{
    return (uint8_t)((tx_head - tx_tail) & TX_MASK);
}

/**
 * @brief Свободное место в TX.
 * @details Одно место намеренно резервируется, чтобы отличать "пусто" от "полно".
 */
static inline uint8_t tx_space(void)
{
    return (uint8_t)(TX_MASK - tx_count());
}

/* ===================== Настройка UART0: baudrate и формат кадра ===================== */

/**
 * @brief Абсолютная разница (|a-b|) для 32-битных беззнаковых.
 */
static inline uint32_t u32_abs_diff(uint32_t a, uint32_t b)
{
    return (a > b) ? (a - b) : (b - a);
}

/**
 * @brief Рассчитать UBRR и фактическую скорость для заданного режима (U2X on/off).
 * @param baud Запрошенная скорость.
 * @param use_u2x 1 — режим U2X, 0 — обычный режим.
 * @param[out] ubrr Рассчитанное значение UBRR (12 бит).
 * @param[out] actual_baud Фактическая скорость при таком UBRR.
 * @return true, если расчёт валиден (UBRR в диапазоне), иначе false.
 */
static bool uart0_calc_ubrr(uint32_t baud, uint8_t use_u2x, uint16_t* ubrr, uint32_t* actual_baud)
{
    if (baud == 0u) {
        return false;
    }

    const uint32_t denom = use_u2x ? 8u : 16u;

    /* UBRR = round(F_CPU/(denom*baud) - 1) */
    const uint32_t div = denom * baud;
    const uint32_t q = (uint32_t)(F_CPU / div);
    const uint32_t r = (uint32_t)(F_CPU % div);

    if (q == 0u) {
        return false;
    }

    uint32_t ubrr_u32 = q - 1u;

    /* округление к ближайшему */
    if ((2u * r) >= div) {
        ubrr_u32 += 1u;
    }

    if (ubrr_u32 > 4095u) {
        return false;
    }

    *ubrr = (uint16_t)ubrr_u32;
    *actual_baud = (uint32_t)(F_CPU / (denom * ((uint32_t)(*ubrr) + 1u)));
    return true;
}

/**
 * @brief Установить скорость UART0 и режим U2X с минимизацией ошибки baudrate.
 * @param baud Скорость в бодах.
 *
 * @details
 * Выбирается вариант (U2X включён/выключен) с меньшей абсолютной ошибкой по скорости.
 * Для F_CPU=16 МГц допускается проектное исключение: 57600 бод — принудительно без U2X.
 */
static void uart0_set_baud(uint32_t baud)
{
    uint16_t ubrr_norm = 0u, ubrr_u2x = 0u;
    uint32_t act_norm = 0u, act_u2x = 0u;

    const bool ok_norm = uart0_calc_ubrr(baud, 0u, &ubrr_norm, &act_norm);
    const bool ok_u2x  = uart0_calc_ubrr(baud, 1u, &ubrr_u2x,  &act_u2x);

    uint8_t use_u2x = 0u;
    uint16_t ubrr = 0u;

#if (F_CPU == 16000000UL)
    /* Проектное исключение: 57600 бод на 16 МГц обычно лучше без U2X */
    if (baud == 57600u && ok_norm) {
        use_u2x = 0u;
        ubrr = ubrr_norm;
    } else
#endif
    {
        if (ok_norm && ok_u2x) {
            const uint32_t err_norm = u32_abs_diff(act_norm, baud);
            const uint32_t err_u2x  = u32_abs_diff(act_u2x,  baud);
            use_u2x = (err_u2x < err_norm) ? 1u : 0u;
            ubrr = use_u2x ? ubrr_u2x : ubrr_norm;
        } else if (ok_u2x) {
            use_u2x = 1u;
            ubrr = ubrr_u2x;
        } else {
            /* fallback: если расчёт невалиден, оставляем нормальный режим с ubrr=0 */
            use_u2x = 0u;
            ubrr = ok_norm ? ubrr_norm : 0u;
        }
    }

    if (use_u2x) {
        UCSR0A = _BV(U2X0);
    } else {
        UCSR0A = 0u;
    }

    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)(ubrr & 0xFFu);
}

/**
 * @brief Применить формат кадра UART из конфигурационного байта SERIAL_*.
 * @param cfg Константа формата (SERIAL_*).
 *
 * @details
 * Поддерживается 5..8 бит данных, 1/2 стоп-бита, none/even/odd паритет.
 * Биты в cfg:
 * - 0x06 — длина слова (5..8 бит),
 * - 0x08 — 2 стоп-бита,
 * - 0x30 — паритет (0x20 even, 0x30 odd).
 */
static void uart0_set_frame_from_cfg(uint8_t cfg)
{
    uint8_t ucsrc = 0u;

    /* Длина слова (5..8 бит) — UCSZ01:0 */
    switch (cfg & 0x06u) {
        case 0x00u: /* 5 */                                   break;                       /* 00 */
        case 0x02u: /* 6 */ ucsrc |= _BV(UCSZ00);             break;                       /* 01 */
        case 0x04u: /* 7 */ ucsrc |= _BV(UCSZ01);             break;                       /* 10 */
        case 0x06u: /* 8 */ ucsrc |= (uint8_t)(_BV(UCSZ01) | _BV(UCSZ00)); break;          /* 11 */
        default:                                                break;
    }

    /* Стоп-биты: USBS0 = 1 -> 2 стоп-бита */
    if ((cfg & 0x08u) != 0u) {
        ucsrc |= _BV(USBS0);
    }

    /* Паритет: UPM01:0 */
    const uint8_t parity = (uint8_t)(cfg & 0x30u);
    if (parity == 0x20u) {                                    /* even */
        ucsrc |= _BV(UPM01);
        s_parity_enabled = 1u;
    } else if (parity == 0x30u) {                             /* odd */
        ucsrc |= (uint8_t)(_BV(UPM01) | _BV(UPM00));
        s_parity_enabled = 1u;
    } else {                                                  /* none */
        s_parity_enabled = 0u;
    }

    UCSR0C = ucsrc;
}

/** @brief Включить RX/TX и прерывание приёма RXC. */
static inline void uart0_enable_rx_tx_irq(void)
{
    UCSR0B = (uint8_t)(_BV(RXEN0) | _BV(TXEN0) | _BV(RXCIE0));
}

/** @brief Полностью отключить UART0 (RX/TX и связанные прерывания). */
static inline void uart0_disable_rx_tx_irq(void)
{
    UCSR0B = 0u;
}

/* ===================== Реализация HAL ===================== */
namespace hal
{
    void uart0_init(uint32_t baud, uint8_t cfg)
    {
        /* На время перенастройки отключаем периферию */
        uart0_disable_rx_tx_irq();

        uart0_set_baud(baud);
        uart0_set_frame_from_cfg(cfg);

        /* Сброс буферов */
        rx_head = 0u;
        rx_tail = 0u;
        tx_head = 0u;
        tx_tail = 0u;

        /* Включаем RX/TX/IRQ */
        uart0_enable_rx_tx_irq();
    }

    uint8_t uart0_available(void)
    {
        return rx_count();
    }

    int16_t uart0_read(void)
    {
        if (rx_head == rx_tail) {
            return -1;
        }

        const uint8_t t = rx_tail;
        const uint8_t v = rx_buf[t];
        rx_tail = (uint8_t)((t + 1u) & RX_MASK);
        return (int16_t)v;
    }

    void uart0_write(uint8_t b)
    {
        /* Ожидаем освобождения места (см. предупреждение в заголовке файла). */
        while (tx_space() == 0u) {
            /* ожидание: ISR(UDRE) выгружает данные */
        }

        const uint8_t was_empty = (tx_head == tx_tail);

        const uint8_t h = tx_head;
        tx_buf[h] = b;
        tx_head = (uint8_t)((h + 1u) & TX_MASK);

        /* Разрешаем прерывание UDRE для запуска/продолжения передачи. */
        if (was_empty) {
            /* TXC сбрасывается записью '1' (подготовка к корректной фиксации завершения кадра). */
            UCSR0A |= _BV(TXC0);
        }
        UCSR0B |= _BV(UDRIE0);
    }
} // namespace hal

/* ===================== Обработчики прерываний UART0 ===================== */

#if defined(USART_RX_vect)
#  define UART0_RX_VECTOR   USART_RX_vect
#elif defined(USART0_RX_vect)
#  define UART0_RX_VECTOR   USART0_RX_vect
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

/**
 * @brief ISR приёма байта (RX Complete).
 *
 * @details
 * - Считывает статус до чтения UDR0 (это важно для анализа FE/DOR/UPE).
 * - Отбрасывает байты с FE (frame error) или DOR (data overrun).
 * - При включённом паритете отбрасывает байты с UPE (parity error).
 * - Кладёт байт в RX-кольцо, если есть место; иначе байт теряется.
 */
ISR(UART0_RX_VECTOR)
{
    const uint8_t status = UCSR0A;
    const uint8_t d = UDR0; /* чтение UDR0 сбрасывает флаг RXC */

    if ((status & (uint8_t)(_BV(FE0) | _BV(DOR0))) != 0u) {
        return;
    }

    if ((s_parity_enabled != 0u) && ((status & _BV(UPE0)) != 0u)) {
        return;
    }

    const uint8_t next = (uint8_t)((rx_head + 1u) & RX_MASK);
    if (next != rx_tail) {
        rx_buf[rx_head] = d;
        rx_head = next;
    } else {
        /* RX overflow: буфер полон, байт теряется. */
    }
}

/**
 * @brief ISR UDRE (регистр данных пуст): выгрузка байта из TX-кольца.
 *
 * @details
 * При опустошении буфера отключает прерывание UDRE, чтобы не генерировать лишние ISR.
 */
ISR(UART0_UDRE_VECTOR)
{
    if (tx_head == tx_tail) {
        UCSR0B &= (uint8_t)~_BV(UDRIE0);
        return;
    }

    const uint8_t t = tx_tail;
    UDR0 = tx_buf[t];
    tx_tail = (uint8_t)((t + 1u) & TX_MASK);
}

/* ===================== Глобальный объект Serial ===================== */
/**
 * @brief Глобальный потоковый интерфейс к UART0.
 * @note Объявление находится в compat.hpp, определение — в этом модуле.
 */
SerialPort Serial;
