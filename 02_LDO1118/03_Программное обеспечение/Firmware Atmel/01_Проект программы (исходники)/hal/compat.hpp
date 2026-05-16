/**
 * @file compat.hpp
 * @brief Минимальная обёртка совместимости и HAL для 8-битных AVR.
 *
 * @details
 * Заголовок предоставляет:
 * - совместимые базовые типы (`byte`, `word`, `boolean`);
 * - константы формата UART-кадра (`SERIAL_*`);
 * - утилиты отображения диапазонов и ограничения значений (`map_long`, `constrain`);
 * - пространство имён `hal` (тикер, задержки, GPIO, UART0, SPI);
 * - лёгкий потоковый класс `SerialPort` поверх `hal::uart0_*`;
 * - инлайн-обёртки для наиболее используемых функций (глобальные `pinMode()`, `delay()` и т.п.).
 *
 * @note
 * Для корректной работы таймингов требуется корректное значение `F_CPU` на этапе компиляции.
 */

#pragma once

#ifndef F_CPU
#  define F_CPU 16000000UL
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <avr/io.h>

/*----------------------------------------------------------------------------
 * Очистка возможных макро-замещений из сторонних заголовков
 *---------------------------------------------------------------------------*/
#ifdef byte
#  undef byte
#endif
#ifdef word
#  undef word
#endif
#ifdef boolean
#  undef boolean
#endif

/** @brief 8-битное беззнаковое значение (проектный алиас). */
typedef uint8_t  byte;
/** @brief 16-битное беззнаковое значение (проектный алиас). */
typedef uint16_t word;
/** @brief Логический тип (проектный алиас). */
typedef bool     boolean;

#ifndef HIGH
/** @brief Логическая единица для цифровых линий. */
#  define HIGH 0x1
#endif
#ifndef LOW
/** @brief Логический ноль для цифровых линий. */
#  define LOW  0x0
#endif

/*----------------------------------------------------------------------------
 * Форматы кадра UART (набор конфигураций SERIAL_*)
 *---------------------------------------------------------------------------*/
/**
 * @name UART frame config (SERIAL_*)
 * @brief Константы формата кадра для инициализации UART.
 *
 * @details
 * Значения соответствуют кодированию (data bits / parity / stop bits),
 * принятому в совместимом слое проекта.
 * @{
 */
#ifndef SERIAL_5N1
#  define SERIAL_5N1 0x00
#  define SERIAL_6N1 0x02
#  define SERIAL_7N1 0x04
#  define SERIAL_8N1 0x06
#  define SERIAL_5N2 0x08
#  define SERIAL_6N2 0x0A
#  define SERIAL_7N2 0x0C
#  define SERIAL_8N2 0x0E
#  define SERIAL_5E1 0x20
#  define SERIAL_6E1 0x22
#  define SERIAL_7E1 0x24
#  define SERIAL_8E1 0x26
#  define SERIAL_5E2 0x28
#  define SERIAL_6E2 0x2A
#  define SERIAL_7E2 0x2C
#  define SERIAL_8E2 0x2E
#  define SERIAL_5O1 0x30
#  define SERIAL_6O1 0x32
#  define SERIAL_7O1 0x34
#  define SERIAL_8O1 0x36
#  define SERIAL_5O2 0x38
#  define SERIAL_6O2 0x3A
#  define SERIAL_7O2 0x3C
#  define SERIAL_8O2 0x3E
#endif
/** @} */

/**
 * @brief Режимы конфигурации линии ввода-вывода.
 */
enum PinMode
{
    INPUT = 0,        /**< Вход без подтяжки. */
    OUTPUT = 1,       /**< Цифровой выход. */
    INPUT_PULLUP = 2  /**< Вход с внутренней подтяжкой к Vcc (если поддерживается). */
};

/*----------------------------------------------------------------------------
 * Утилиты
 *---------------------------------------------------------------------------*/
/**
 * @brief Линейное отображение значения из одного диапазона в другой (целочисленное).
 *
 * @details
 * Вычисление выполняется в целочисленной арифметике. При `in_max == in_min`
 * возникает деление на ноль; вызывающая сторона обязана обеспечивать корректные диапазоны.
 *
 * @param x       Входное значение.
 * @param in_min  Нижняя граница входного диапазона.
 * @param in_max  Верхняя граница входного диапазона.
 * @param out_min Нижняя граница выходного диапазона.
 * @param out_max Верхняя граница выходного диапазона.
 * @return Значение, отображённое в выходной диапазон.
 */
static inline long map_long(long x, long in_min, long in_max, long out_min, long out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

#ifdef __cplusplus
/**
 * @brief Ограничение значения в пределах [a; b] (C++-вариант).
 *
 * @tparam T Любой упорядочиваемый тип.
 * @param x Значение.
 * @param a Нижняя граница.
 * @param b Верхняя граница.
 * @return Значение, ограниченное диапазоном [a; b].
 */
template <typename T>
static inline T constrainT(T x, T a, T b)
{
    return (x < a) ? a : (x > b) ? b : x;
}
#else
/* C-совместимый вариант: без шаблонов, только как макрос. */
#  define constrainT(x, a, b) (((x) < (a)) ? (a) : (((x) > (b)) ? (b) : (x)))
#endif

/*----------------------------------------------------------------------------
 * HAL API
 *---------------------------------------------------------------------------*/
#ifdef __cplusplus
/**
 * @namespace hal
 * @brief Низкоуровневые аппаратные вызовы (hardware abstraction layer).
 *
 * @details
 * Функции предназначены для прямого управления периферией и не опираются на динамическую память.
 */
namespace hal
{
    /* ---- Тикер/таймер ---- */

    /**
     * @brief Инициализация базового тикера миллисекунд.
     * @post Дальнейшие вызовы hal::millis()/delay() становятся валидными.
     */
    void     tick_init(void);

    /**
     * @brief Количество миллисекунд с момента старта.
     * @return Время в миллисекундах (переполнение по модулю 2^32).
     */
    uint32_t millis(void);

    /**
     * @brief Блокирующая задержка в миллисекундах.
     * @param ms Время ожидания.
     */
    void     delay(uint32_t ms);

    /**
     * @brief Блокирующая задержка в микросекундах.
     * @param us Время ожидания.
     */
    void     delayMicroseconds(uint16_t us);

    /* ---- GPIO ---- */

    /**
     * @brief Настройка режима цифрового пина.
     * @param pin Номер пина (проектная нумерация).
     * @param mode Режим из @ref PinMode.
     */
    void     pinMode(uint8_t pin, PinMode mode);

    /**
     * @brief Установить уровень на цифровом выходе.
     * @param pin Номер пина.
     * @param hi true — лог.1, false — лог.0.
     */
    void     digitalWrite(uint8_t pin, bool hi);

    /**
     * @brief Прочитать уровень с цифрового входа.
     * @param pin Номер пина.
     * @return true — лог.1, false — лог.0.
     */
    bool     digitalRead(uint8_t pin);

    /* ---- UART0 (TX/RX) ---- */

    /**
     * @brief Инициализация UART0.
     * @param baud Скорость, бод.
     * @param cfg  Формат кадра (см. @ref SERIAL_8N1 и др.).
     * @post UART готов к обмену; приёмный буфер очищен (если реализовано в драйвере).
     */
    void     uart0_init(uint32_t baud, uint8_t cfg);

    /**
     * @brief Количество байт, доступных для чтения из UART0.
     * @return Количество байт в приёмном буфере (0..255).
     */
    uint8_t  uart0_available(void);

    /**
     * @brief Прочитать байт из UART0.
     * @return 0..255 — байт; -1 — данных нет.
     */
    int16_t  uart0_read(void);

    /**
     * @brief Передать байт по UART0.
     * @param b Байт.
     */
    void     uart0_write(uint8_t b);

    /* ---- SPI (низкоуровневый) ---- */

    /**
     * @brief Инициализация SPI в режиме ведущего.
     * @post MOSI/SCK настроены на выход, MISO — на вход; SS — по месту использования.
     */
    void     spi_init(void);

    /**
     * @brief Передача байта по SPI с одновременным приёмом.
     * @param v Отправляемый байт.
     * @return Принятый байт.
     */
    uint8_t  spi_txrx(uint8_t v);
}
#endif /* __cplusplus */

/*----------------------------------------------------------------------------
 * Потоковый интерфейс UART0
 *---------------------------------------------------------------------------*/
#ifdef __cplusplus
/**
 * @class SerialPort
 * @brief Минимальный потоковый интерфейс поверх `hal::uart0_*`.
 *
 * @details
 * Реализует базовые операции чтения/записи и печать строк/чисел.
 * Динамическая память не используется.
 */
class SerialPort
{
public:
    /** @brief Инициализация UART0 со стандартным форматом 8N1. */
    void   begin(uint32_t baud)              { hal::uart0_init(baud, SERIAL_8N1); }
    /** @brief Инициализация UART0 с явным форматом кадра. */
    void   begin(uint32_t baud, uint8_t cfg) { hal::uart0_init(baud, cfg); }
    /** @brief Завершение работы (совместимость; фактического выключения периферии не выполняет). */
    void   end()                             { /* no-op */ }

    /** @brief Количество доступных байт для чтения. */
    int    available()                       { return (int)hal::uart0_available(); }
    /** @brief Прочитать один байт (или -1, если данных нет). */
    int    read()                            { return (int)hal::uart0_read(); }
    /** @brief Записать один байт. @return Количество отправленных байт (1). */
    size_t write(uint8_t b)                  { hal::uart0_write(b); return 1u; }

    /** @brief Вывести ASCIIZ-строку. @return Количество отправленных символов. */
    size_t print(const char* s)
    {
        size_t n = 0u;
        while (s && *s)
        {
            hal::uart0_write((uint8_t)(*s++));
            ++n;
        }
        return n;
    }

    /** @brief Вывести строку и CRLF. @return Количество отправленных символов. */
    size_t println(const char* s)
    {
        size_t n = print(s);
        hal::uart0_write('\r');
        hal::uart0_write('\n');
        return n + 2u;
    }

    /** @brief Вывести беззнаковое 32-бит число в десятичном виде. */
    size_t print(uint32_t v)
    {
        /* Буфер достаточен для 10 цифр uint32_t + '\0'. */
        char buf[11];
        char* p = buf + sizeof(buf);
        *--p = '\0';

        if (v == 0u)
        {
            hal::uart0_write((uint8_t)'0');
            return 1u;
        }

        while (v != 0u)
        {
            *--p = (char)('0' + (v % 10u));
            v /= 10u;
        }

        size_t n = 0u;
        while (*p)
        {
            hal::uart0_write((uint8_t)*p++);
            ++n;
        }
        return n;
    }

    /** @brief Вывести число и CRLF. */
    size_t println(uint32_t v)
    {
        size_t n = print(v);
        hal::uart0_write('\r');
        hal::uart0_write('\n');
        return n + 2u;
    }
};

/**
 * @brief Глобальный объект последовательного порта.
 *
 * @details
 * Экземпляр должен быть определён ровно в одном .cpp файле:
 * `SerialPort Serial;`
 */
extern SerialPort Serial;
#endif /* __cplusplus */

/*----------------------------------------------------------------------------
 * Инлайн-обёртки (глобальные имена для совместимости)
 *---------------------------------------------------------------------------*/
#ifdef __cplusplus
/** @brief Настройка режима пина. @see hal::pinMode */
static inline void pinMode(uint8_t p, PinMode m) { hal::pinMode(p, m); }

/**
 * @brief Установить уровень на пине.
 * @param p Номер пина.
 * @param v Ненулевое значение трактуется как HIGH.
 * @see hal::digitalWrite
 */
static inline void digitalWrite(uint8_t p, int v) { hal::digitalWrite(p, (v != 0)); }

/**
 * @brief Прочитать уровень с пина.
 * @return HIGH или LOW.
 * @see hal::digitalRead
 */
static inline int digitalRead(uint8_t p) { return hal::digitalRead(p) ? HIGH : LOW; }

/** @brief Текущее время в миллисекундах. @see hal::millis */
static inline uint32_t millis() { return hal::millis(); }

/** @brief Блокирующая задержка в миллисекундах. @see hal::delay */
static inline void delay(uint32_t ms) { hal::delay(ms); }

/** @brief Блокирующая задержка в микросекундах. @see hal::delayMicroseconds */
static inline void delayMicroseconds(uint16_t us) { hal::delayMicroseconds(us); }
#endif /* __cplusplus */

/*----------------------------------------------------------------------------
 * Совместимые утилиты (короткие имена)
 *---------------------------------------------------------------------------*/
#ifdef __cplusplus
/**
 * @brief Ограничение значения в пределах [a; b] (по значению).
 * @note Возвращается значение, а не ссылка — безопасно при передаче временных объектов.
 */
template <typename T>
static inline T constrain(T x, T a, T b)
{
    return constrainT<T>(x, a, b);
}
#else
#  define constrain(x, a, b) constrainT((x), (a), (b))
#endif

/**
 * @brief Линейное отображение значения из одного диапазона в другой (короткое имя).
 * @see map_long
 */
static inline long map(long x, long a, long b, long c, long d)
{
    return map_long(x, a, b, c, d);
}
