#pragma once

/**
 * @file compat.hpp
 * @brief Общий аппаратный интерфейс и базовые системные объявления для AVR.
 *
 * @details
 * Заголовок содержит:
 * - базовые проектные типы;
 * - константы формата кадра UART;
 * - служебные функции ограничения и линейного отображения;
 * - объявления низкоуровневых функций пространства имён hal;
 * - минимальный потоковый интерфейс UART0;
 * - короткие inline-функции для часто используемых системных операций.
 *
 * @note
 * Для корректной работы таймингов значение F_CPU должно соответствовать
 * фактической частоте тактирования микроконтроллера.
 *
 * @warning
 * Этот файл не должен содержать карту выводов конкретной платы.
 * Назначение прикладных сигналов описывается в аппаратном слое платы.
 */

#ifndef F_CPU
#  define F_CPU 16000000UL
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <avr/io.h>
#include <avr/interrupt.h>

/*----------------------------------------------------------------------------
 * Проектные типы
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

/**
 * @brief Восьмибитное беззнаковое значение.
 */
typedef uint8_t byte;

/**
 * @brief Шестнадцатибитное беззнаковое значение.
 */
typedef uint16_t word;

/**
 * @brief Логический тип.
 */
typedef bool boolean;

/*----------------------------------------------------------------------------
 * Логические уровни цифровых линий
 *---------------------------------------------------------------------------*/

#ifndef HIGH
/**
 * @brief Логическая единица цифровой линии.
 */
#  define HIGH 0x1
#endif

#ifndef LOW
/**
 * @brief Логический ноль цифровой линии.
 */
#  define LOW 0x0
#endif

/*----------------------------------------------------------------------------
 * Форматы кадра UART
 *---------------------------------------------------------------------------*/

/**
 * @defgroup serial_config Форматы кадра UART
 * @brief Коды форматов кадра для настройки UART0.
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

/*----------------------------------------------------------------------------
 * Режимы цифровых линий
 *---------------------------------------------------------------------------*/

/**
 * @brief Режим конфигурации цифровой линии ввода-вывода.
 */
enum PinMode
{
    INPUT = 0,        /**< Вход без внутренней подтяжки. */
    OUTPUT = 1,       /**< Цифровой выход. */
    INPUT_PULLUP = 2  /**< Вход с внутренней подтяжкой к VCC. */
};

/*----------------------------------------------------------------------------
 * Служебные функции
 *---------------------------------------------------------------------------*/

/**
 * @brief Линейно отображает значение из одного диапазона в другой.
 *
 * @details
 * Вычисление выполняется в целочисленной арифметике.
 *
 * @param x       Входное значение.
 * @param in_min  Нижняя граница входного диапазона.
 * @param in_max  Верхняя граница входного диапазона.
 * @param out_min Нижняя граница выходного диапазона.
 * @param out_max Верхняя граница выходного диапазона.
 *
 * @return Значение, отображённое в выходной диапазон.
 *
 * @warning
 * При in_max == in_min произойдёт деление на ноль.
 * Вызывающая сторона обязана передавать корректные границы диапазона.
 */
static inline long map_long(
    long x,
    long in_min,
    long in_max,
    long out_min,
    long out_max
)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

#ifdef __cplusplus

/**
 * @brief Ограничивает значение диапазоном [a; b].
 *
 * @tparam T Тип значения.
 *
 * @param x Значение.
 * @param a Нижняя граница.
 * @param b Верхняя граница.
 *
 * @return Значение, ограниченное диапазоном [a; b].
 */
template <typename T>
static inline T constrainT(T x, T a, T b)
{
    return (x < a) ? a : (x > b) ? b : x;
}

#else

/**
 * @brief Ограничивает значение диапазоном [a; b].
 */
#  define constrainT(x, a, b) (((x) < (a)) ? (a) : (((x) > (b)) ? (b) : (x)))

#endif

/*----------------------------------------------------------------------------
 * HAL API
 *---------------------------------------------------------------------------*/

#ifdef __cplusplus

/**
 * @namespace hal
 * @brief Низкоуровневый аппаратный интерфейс проекта.
 *
 * @details
 * Пространство имён содержит минимальные аппаратные сервисы:
 * - системный миллисекундный тикер;
 * - задержки;
 * - GPIO;
 * - UART0;
 * - SPI;
 * - TWI/I2C.
 *
 * Функции не используют динамическую память.
 */
namespace hal
{
    /* ---------------------------------------------------------------------
     * Тикер / задержки
     * ------------------------------------------------------------------ */

    /**
     * @brief Инициализирует базовый миллисекундный тикер.
     *
     * @details
     * После вызова этой функции становятся валидными hal::millis()
     * и функции задержек, завязанные на системный тик.
     *
     * Обычно вызывается один раз из main() до setup().
     */
    void tick_init(void);

    /**
     * @brief Возвращает количество миллисекунд с момента старта тикера.
     *
     * @return Время в миллисекундах.
     *
     * @note
     * Значение переполняется по модулю 2^32.
     * Интервалы времени следует вычислять через разность беззнаковых значений.
     */
    uint32_t millis(void);

    /**
     * @brief Выполняет блокирующую задержку в миллисекундах.
     *
     * @param ms Время задержки в миллисекундах.
     */
    void delay(uint32_t ms);

    /**
     * @brief Выполняет блокирующую задержку в микросекундах.
     *
     * @param us Время задержки в микросекундах.
     */
    void delayMicroseconds(uint16_t us);

    /* ---------------------------------------------------------------------
     * GPIO
     * ------------------------------------------------------------------ */

    /**
     * @brief Настраивает режим цифровой линии.
     *
     * @param pin Номер линии в проектной нумерации.
     * @param mode Режим линии из enum PinMode.
     *
     * @note
     * Для сигналов конкретной платы предпочтительно использовать аппаратный
     * слой платы, а не прямую проектную нумерацию линий.
     */
    void pinMode(uint8_t pin, PinMode mode);

    /**
     * @brief Устанавливает уровень цифрового выхода.
     *
     * @param pin Номер линии в проектной нумерации.
     * @param hi true — логическая единица; false — логический ноль.
     */
    void digitalWrite(uint8_t pin, bool hi);

    /**
     * @brief Читает уровень цифрового входа.
     *
     * @param pin Номер линии в проектной нумерации.
     *
     * @return true — логическая единица; false — логический ноль.
     */
    bool digitalRead(uint8_t pin);

    /* ---------------------------------------------------------------------
     * UART0
     * ------------------------------------------------------------------ */

    /**
     * @brief Инициализирует UART0.
     *
     * @param baud Скорость обмена, бод.
     * @param cfg Формат кадра SERIAL_*.
     *
     * @post UART0 готов к передаче и приёму.
     */
    void uart0_init(uint32_t baud, uint8_t cfg);

    /**
     * @brief Возвращает количество байт, доступных для чтения из UART0.
     *
     * @return Количество байт в приёмном буфере.
     */
    uint8_t uart0_available(void);

    /**
     * @brief Читает один байт из UART0.
     *
     * @return Значение от 0 до 255, если байт прочитан; -1, если данных нет.
     */
    int16_t uart0_read(void);

    /**
     * @brief Передаёт один байт через UART0.
     *
     * @param b Передаваемый байт.
     */
    void uart0_write(uint8_t b);

    /**
     * @brief Тип callback-функции окончания физической передачи UART0.
     *
     * @details
     * Callback вызывается из обработчика прерывания UART TX Complete.
     * Функция должна быть короткой: не выполнять длительных операций,
     * не вызывать передачу в UART и не зависеть от UART-прерываний.
     */
    typedef void (*Uart0TxCompleteCallback)(void);

    /**
     * @brief Назначает callback окончания физической передачи UART0.
     *
     * @param callback Указатель на функцию или 0.
     *
     * @details
     * Указатель сохраняется атомарно относительно прерываний.
     */
    void uart0_set_tx_complete_callback(Uart0TxCompleteCallback callback);

    /**
     * @brief Очищает TXC0 и разрешает прерывание UART0 TX Complete.
     *
     * @details
     * Используется перед отправкой кадра, когда вызывающему коду требуется
     * получить событие после физического окончания передачи последнего байта.
     *
     * TXC0 очищается записью логической единицы.
     */
    void uart0_enable_tx_complete_interrupt(void);

    /**
     * @brief Запрещает прерывание UART0 TX Complete.
     */
    void uart0_disable_tx_complete_interrupt(void);

    /* ---------------------------------------------------------------------
     * SPI
     * ------------------------------------------------------------------ */

    /**
     * @brief Инициализирует SPI в режиме ведущего.
     *
     * @post
     * MOSI и SCK настроены как выходы, MISO — как вход.
     * Линии выбора ведомых устройств должны настраиваться аппаратным
     * слоем платы.
     */
    void spi_init(void);

    /**
     * @brief Передаёт байт по SPI и одновременно принимает ответный байт.
     *
     * @param value Передаваемый байт.
     *
     * @return Принятый байт.
     */
    uint8_t spi_txrx(uint8_t value);

    /* ---------------------------------------------------------------------
     * TWI / I2C
     * ------------------------------------------------------------------ */

    /**
     * @brief Инициализирует TWI/I2C в режиме ведущего.
     *
     * @param scl_hz Частота линии SCL, Гц.
     *
     * @post
     * Аппаратный модуль TWI включён и готов к обмену.
     */
    void twi_init(uint32_t scl_hz);

    /**
     * @brief Записывает шестнадцатибитный регистр I2C-устройства.
     *
     * @param address Семибитный I2C-адрес устройства.
     * @param reg Адрес регистра внутри устройства.
     * @param value Записываемое значение, старший байт передаётся первым.
     *
     * @return true — запись выполнена успешно; false — ошибка обмена.
     */
    bool twi_write_reg16(uint8_t address, uint8_t reg, uint16_t value);

    /**
     * @brief Читает шестнадцатибитный регистр I2C-устройства.
     *
     * @param address Семибитный I2C-адрес устройства.
     * @param reg Адрес регистра внутри устройства.
     * @param value Указатель для сохранения прочитанного значения.
     *
     * @return true — чтение выполнено успешно; false — ошибка обмена.
     */
    bool twi_read_reg16(uint8_t address, uint8_t reg, uint16_t* value);
}

#endif /* __cplusplus */

/*----------------------------------------------------------------------------
 * Потоковый интерфейс UART0
 *---------------------------------------------------------------------------*/

#ifdef __cplusplus

/**
 * @class SerialPort
 * @brief Минимальный потоковый интерфейс поверх UART0.
 *
 * @details
 * Класс предоставляет базовые операции UART0:
 * - инициализация порта;
 * - проверка количества принятых байт;
 * - чтение одного байта;
 * - запись одного байта;
 * - вывод строк;
 * - вывод целых чисел.
 *
 * Динамическая память не используется.
 */
class SerialPort
{
public:
    /**
     * @brief Инициализирует UART0 со стандартным форматом 8N1.
     *
     * @param baud Скорость обмена, бод.
     */
    void begin(uint32_t baud)
    {
        hal::uart0_init(baud, SERIAL_8N1);
    }

    /**
     * @brief Инициализирует UART0 с явным форматом кадра.
     *
     * @param baud Скорость обмена, бод.
     * @param cfg Формат кадра SERIAL_*.
     */
    void begin(uint32_t baud, uint8_t cfg)
    {
        hal::uart0_init(baud, cfg);
    }

    /**
     * @brief Завершает работу потокового интерфейса.
     *
     * @details
     * Функция оставлена как нейтральная операция.
     * Выключение аппаратного UART0 здесь не выполняется.
     */
    void end(void)
    {
        /* no-op */
    }

    /**
     * @brief Возвращает количество доступных байт для чтения.
     *
     * @return Количество байт в приёмном буфере.
     */
    int available(void)
    {
        return static_cast<int>(hal::uart0_available());
    }

    /**
     * @brief Читает один байт.
     *
     * @return Значение от 0 до 255, если байт прочитан; -1, если данных нет.
     */
    int read(void)
    {
        return static_cast<int>(hal::uart0_read());
    }

    /**
     * @brief Передаёт один байт.
     *
     * @param b Передаваемый байт.
     *
     * @return Количество байт, поставленных в передачу.
     */
    size_t write(uint8_t b)
    {
        hal::uart0_write(b);
        return 1u;
    }

    /**
     * @brief Выводит строку с завершающим нулём.
     *
     * @param s Указатель на строку.
     *
     * @return Количество переданных символов.
     */
    size_t print(const char* s)
    {
        size_t n = 0u;

        while (s && *s)
        {
            hal::uart0_write(static_cast<uint8_t>(*s++));
            ++n;
        }

        return n;
    }

    /**
     * @brief Выводит строку и перевод строки CRLF.
     *
     * @param s Указатель на строку.
     *
     * @return Количество переданных символов, включая CRLF.
     */
    size_t println(const char* s)
    {
        size_t n = print(s);

        hal::uart0_write(static_cast<uint8_t>('\r'));
        hal::uart0_write(static_cast<uint8_t>('\n'));

        return n + 2u;
    }

    /**
     * @brief Выводит беззнаковое 32-битное число в десятичном виде.
     *
     * @param v Число.
     *
     * @return Количество переданных символов.
     */
    size_t print(uint32_t v)
    {
        char buf[11];
        char* p = buf + sizeof(buf);

        *--p = '\0';

        if (v == 0u)
        {
            hal::uart0_write(static_cast<uint8_t>('0'));
            return 1u;
        }

        while (v != 0u)
        {
            *--p = static_cast<char>('0' + (v % 10u));
            v /= 10u;
        }

        size_t n = 0u;

        while (*p)
        {
            hal::uart0_write(static_cast<uint8_t>(*p++));
            ++n;
        }

        return n;
    }

    /**
     * @brief Выводит знаковое 32-битное число в десятичном виде.
     *
     * @param v Число.
     *
     * @return Количество переданных символов.
     */
    size_t print(int32_t v)
    {
        if (v < 0)
        {
            hal::uart0_write(static_cast<uint8_t>('-'));

            /*
             * Преобразование через -(v + 1) защищает от переполнения
             * при INT32_MIN.
             */
            const uint32_t magnitude =
                static_cast<uint32_t>(-(v + 1)) + 1u;

            return print(magnitude) + 1u;
        }

        return print(static_cast<uint32_t>(v));
    }

    /**
     * @brief Выводит беззнаковое число и перевод строки CRLF.
     *
     * @param v Число.
     *
     * @return Количество переданных символов, включая CRLF.
     */
    size_t println(uint32_t v)
    {
        size_t n = print(v);

        hal::uart0_write(static_cast<uint8_t>('\r'));
        hal::uart0_write(static_cast<uint8_t>('\n'));

        return n + 2u;
    }

    /**
     * @brief Выводит знаковое число и перевод строки CRLF.
     *
     * @param v Число.
     *
     * @return Количество переданных символов, включая CRLF.
     */
    size_t println(int32_t v)
    {
        size_t n = print(v);

        hal::uart0_write(static_cast<uint8_t>('\r'));
        hal::uart0_write(static_cast<uint8_t>('\n'));

        return n + 2u;
    }
};

/**
 * @brief Глобальный потоковый объект UART0.
 *
 * @details
 * Экземпляр должен быть определён ровно в одном .cpp-файле.
 */
extern SerialPort Serial;

#endif /* __cplusplus */

/*----------------------------------------------------------------------------
 * Глобальные inline-функции
 *---------------------------------------------------------------------------*/

#ifdef __cplusplus

/**
 * @brief Настраивает режим цифровой линии.
 *
 * @param p Номер линии.
 * @param m Режим линии.
 *
 * @see hal::pinMode
 */
static inline void pinMode(uint8_t p, PinMode m)
{
    hal::pinMode(p, m);
}

/**
 * @brief Устанавливает уровень цифрового выхода.
 *
 * @param p Номер линии.
 * @param v Ненулевое значение трактуется как HIGH.
 *
 * @see hal::digitalWrite
 */
static inline void digitalWrite(uint8_t p, int v)
{
    hal::digitalWrite(p, (v != 0));
}

/**
 * @brief Читает уровень цифрового входа.
 *
 * @param p Номер линии.
 *
 * @return HIGH или LOW.
 *
 * @see hal::digitalRead
 */
static inline int digitalRead(uint8_t p)
{
    return hal::digitalRead(p) ? HIGH : LOW;
}

/**
 * @brief Возвращает текущее время в миллисекундах.
 *
 * @return Время в миллисекундах.
 *
 * @see hal::millis
 */
static inline uint32_t millis(void)
{
    return hal::millis();
}

/**
 * @brief Выполняет блокирующую задержку в миллисекундах.
 *
 * @param ms Время задержки.
 *
 * @see hal::delay
 */
static inline void delay(uint32_t ms)
{
    hal::delay(ms);
}

/**
 * @brief Выполняет блокирующую задержку в микросекундах.
 *
 * @param us Время задержки.
 *
 * @see hal::delayMicroseconds
 */
static inline void delayMicroseconds(uint16_t us)
{
    hal::delayMicroseconds(us);
}

/**
 * @brief Глобально запрещает прерывания.
 */
static inline void noInterrupts(void)
{
    cli();
}

/**
 * @brief Глобально разрешает прерывания.
 */
static inline void interrupts(void)
{
    sei();
}

#endif /* __cplusplus */

/*----------------------------------------------------------------------------
 * Служебные inline-функции
 *---------------------------------------------------------------------------*/

#ifdef __cplusplus

/**
 * @brief Ограничивает значение диапазоном [a; b].
 *
 * @tparam T Тип значения.
 *
 * @param x Значение.
 * @param a Нижняя граница.
 * @param b Верхняя граница.
 *
 * @return Значение, ограниченное диапазоном [a; b].
 *
 * @note
 * Возвращается значение, а не ссылка. Это безопасно при передаче временных
 * объектов.
 */
template <typename T>
static inline T constrain(T x, T a, T b)
{
    return constrainT<T>(x, a, b);
}

#else

/**
 * @brief Ограничивает значение диапазоном [a; b].
 */
#  define constrain(x, a, b) constrainT((x), (a), (b))

#endif

/**
 * @brief Линейно отображает значение из одного диапазона в другой.
 *
 * @param x Входное значение.
 * @param a Нижняя граница входного диапазона.
 * @param b Верхняя граница входного диапазона.
 * @param c Нижняя граница выходного диапазона.
 * @param d Верхняя граница выходного диапазона.
 *
 * @return Значение, отображённое в выходной диапазон.
 *
 * @see map_long
 */
static inline long map(long x, long a, long b, long c, long d)
{
    return map_long(x, a, b, c, d);
}