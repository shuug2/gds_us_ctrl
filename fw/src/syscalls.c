/* fw/src/syscalls.c — bare-metal newlib stubs to keep link output warning-free. */
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

extern char _end;

static char *s_heap_end;

int _close(int file)
{
    (void)file;
    errno = EBADF;
    return -1;
}

void _exit(int status)
{
    (void)status;
    for (;;) {
    }
}

int _fstat(int file, struct stat *st)
{
    (void)file;
    st->st_mode = S_IFCHR;
    return 0;
}

int _getpid(void)
{
    return 1;
}

int _isatty(int file)
{
    (void)file;
    return 1;
}

int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

off_t _lseek(int file, off_t ptr, int dir)
{
    (void)file;
    (void)ptr;
    (void)dir;
    return 0;
}

ssize_t _read(int file, void *ptr, size_t len)
{
    (void)file;
    (void)ptr;
    (void)len;
    return 0;
}

ssize_t _write(int file, const void *ptr, size_t len)
{
    (void)file;
    (void)ptr;
    return (ssize_t)len;
}

caddr_t _sbrk(int incr)
{
    char *prev_heap_end;

    if (s_heap_end == 0) {
        s_heap_end = &_end;
    }

    prev_heap_end = s_heap_end;
    s_heap_end += incr;
    return (caddr_t)prev_heap_end;
}
