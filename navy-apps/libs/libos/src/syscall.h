#ifndef __SYSCALL_H__
#define __SYSCALL_H__
#include <unistd.h>

// ???
// how to log strace 's name, args and ret
#define Log_syscall(name, args, ret) Log("syscall name = [%s], args = [%s], ret = [%d]\n", \
                                         name, args, ret)

// syscall number table (will be keep in OS kernel mem if in real machine)
enum
{
    SYS_exit,
    SYS_yield,
    SYS_open,
    SYS_read,
    SYS_write,
    SYS_kill,
    SYS_getpid,
    SYS_close,
    SYS_lseek,
    SYS_brk,
    SYS_fstat,
    SYS_time,
    SYS_signal,
    SYS_execve,
    SYS_fork,
    SYS_link,
    SYS_unlink,
    SYS_wait,
    SYS_times,
    SYS_gettimeofday
};

void do_syscall(Context *c);

// why must static inline
// static: tell compiler every TU has this function's definition
// inline: tell compiler this function gave to inline function, every TU has a copy is right
static inline int sys_exit(int status)
{
    halt(status);
    return 0;
}

static inline int sys_yield()
{
    yield();
    return 0;
}

static inline ssize_t sys_read(int fd, void *buf, size_t conunt)
{
    return 0;
}

static inline ssize_t sys_write(int fd, void *buf, size_t count)
{
    if (fd == 1 || fd == 2)
    {
        for (int i = 0; i < count; i++)
        {
            putch(((char *)buf)[i]);
        }
    }
    return 0;
}

#endif