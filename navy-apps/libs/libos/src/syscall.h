#ifndef __SYSCALL_H__
#define __SYSCALL_H__
#include <unistd.h>

// syscall number table (will be keep in OS kernel mem if in real machine)
enum
{
    SYS_exit = 0,    // 1
    SYS_yield,       // 2
    SYS_open,        // 3
    SYS_read,        // 4
    SYS_write,       // 5
    SYS_kill,        // 6
    SYS_getpid,      // 7
    SYS_close,       // 8
    SYS_lseek,       // 9
    SYS_brk,         // 10
    SYS_fstat,       // 11
    SYS_time,        // 12
    SYS_signal,      // 13
    SYS_execve,      // 14
    SYS_fork,        // 15
    SYS_link,        // 16
    SYS_unlink,      // 17
    SYS_wait,        // 18
    SYS_times,       // 19
    SYS_gettimeofday // 20
};

#endif