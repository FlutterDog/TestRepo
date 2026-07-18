/**
 * @file syscalls.c
 * @brief Системные вызовы для минимального C/C++ runtime.
 *
 * Модуль задаёт границу heap и предоставляет системные функции,
 * требуемые стандартной библиотекой при компоновке bare-metal проекта.
 */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

extern char _end;

static char* g_heap_end = 0;

static char* platform_get_stack_pointer(void)
{
    register char* stack_pointer __asm("sp");
    return stack_pointer;
}

void* _sbrk(ptrdiff_t increment)
{
    char* previous_heap_end;
    char* next_heap_end;
    char* stack_pointer;

    if (g_heap_end == 0)
    {
        g_heap_end = &_end;
    }

    previous_heap_end = g_heap_end;
    next_heap_end = g_heap_end + increment;
    stack_pointer = platform_get_stack_pointer();

    if (next_heap_end >= stack_pointer)
    {
        errno = ENOMEM;
        return (void*)-1;
    }

    g_heap_end = next_heap_end;

    return previous_heap_end;
}

int _close(int file)
{
    (void)file;
    errno = EBADF;
    return -1;
}

int _fstat(int file, struct stat* status)
{
    (void)file;

    if (status == 0)
    {
        errno = EINVAL;
        return -1;
    }

    status->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file)
{
    (void)file;
    return 1;
}

int _lseek(int file, int offset, int whence)
{
    (void)file;
    (void)offset;
    (void)whence;

    errno = ESPIPE;
    return -1;
}

int _read(int file, char* buffer, int length)
{
    (void)file;
    (void)buffer;
    (void)length;

    errno = EAGAIN;
    return -1;
}

int _write(int file, const char* buffer, int length)
{
    (void)file;
    (void)buffer;

    if (length < 0)
    {
        errno = EINVAL;
        return -1;
    }

    return length;
}
