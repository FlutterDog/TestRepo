
/**
 * @file newlib_stubs.c
 * @brief Минимальные системные заглушки newlib для bare-metal сборки.
 *
 * Файл закрывает POSIX-вызовы, которые требуются стандартной библиотеке C
 * при использовании abort(), signal() и части stdio-кода.
 */

#include <errno.h>

/**
 * @brief Завершение процесса для bare-metal среды.
 *
 * В микроконтроллере нет операционной системы и процесса, который можно
 * завершить. Вызов переводит выполнение в бесконечный цикл.
 *
 * @param status Код завершения.
 */
void _exit(int status)
{
    (void)status;

    __asm__ volatile ("cpsid i");

    for (;;)
    {
        __asm__ volatile ("nop");
    }
}

/**
 * @brief Возвращает идентификатор процесса для newlib.
 *
 * В bare-metal среде используется фиксированный идентификатор.
 *
 * @return Фиксированный идентификатор процесса.
 */
int _getpid(void)
{
    return 1;
}

/**
 * @brief Заглушка отправки сигнала процессу.
 *
 * В bare-metal среде сигналы процессов не поддерживаются.
 *
 * @param pid Идентификатор процесса.
 * @param sig Номер сигнала.
 *
 * @return Всегда возвращает ошибку.
 */
int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;

    errno = EINVAL;
    return -1;
}
