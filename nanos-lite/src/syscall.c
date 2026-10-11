#include <common.h>
#include <fs.h>
#include "syscall.h"

/* kernel-side syscall handlers */

uintptr_t mm_brk(uintptr_t incr); // implemented in mm.c

/* man 2 write: on success, return the number of bytes written; -1 on error */
static ssize_t sys_write(int fd, const void *buf, size_t count)
{
  if (fd == 1 || fd == 2)
  { // stdout / stderr: output to the serial port, byte by byte
    const char *p = buf;
    for (size_t i = 0; i < count; i++)
      putch(p[i]);
    return count;
  }
  return fs_write(fd, buf, count); // regular files: write to ramdisk
}

/* no input source in this stage: return 0 = EOF */
static ssize_t sys_read(int fd, void *buf, size_t count)
{
  if (fd >= 0)
  {
    return fs_read(fd, buf, count);
  }
  return -1;
}

static ssize_t sys_lseek(int fd, size_t offset, int whence)
{
  return fs_lseek(fd, offset, whence);
}

static int sys_open(const char *pathname, int flags, int mode)
{
  return fs_open(pathname, flags, mode);
}

static int sys_close(int fd)
{
  return fs_close(fd);
}

static int sys_exit(int status)
{
  halt(status); // never returns
}

static int sys_yield(void)
{
  yield();
  return 0;
}

static uintptr_t sys_brk(uintptr_t incr)
{
  return mm_brk(incr); // 返回旧 break,作为 _sbrk 的返回值
}

// ==================== strace: 系统调用跟踪 ====================
#define CONFIG_STRACE 1 // 改成 0 可一键关闭跟踪

#ifdef CONFIG_STRACE

#define PRINT_CHAR_MAX 32 // 字符串/缓冲区参数最多打印的字符数

static const char *syscall_name[] = {
    [SYS_exit] = "exit",
    [SYS_yield] = "yield",
    [SYS_open] = "open",
    [SYS_read] = "read",
    [SYS_write] = "write",
    [SYS_kill] = "kill",
    [SYS_getpid] = "getpid",
    [SYS_close] = "close",
    [SYS_lseek] = "lseek",
    [SYS_brk] = "brk",
    [SYS_fstat] = "fstat",
    [SYS_time] = "time",
    [SYS_signal] = "signal",
    [SYS_execve] = "execve",
    [SYS_fork] = "fork",
    [SYS_link] = "link",
    [SYS_unlink] = "unlink",
    [SYS_wait] = "wait",
    [SYS_times] = "times",
    [SYS_gettimeofday] = "gettimeofday",
};

// 打印字符串参数: 带引号, 超过 PRINT_CHAR_MAX 字节用 "..." 截断
static void strace_str(const char *s)
{
  putch('"');
  int i;
  for (i = 0; s[i] != '\0' && i < PRINT_CHAR_MAX; i++)
    putch(s[i]);
  if (s[i] != '\0')
    putstr("...");
  putch('"');
}

// 打印缓冲区参数: 不可见字符(如 '\n')用 '.' 代替, 避免搅乱一行的结构
static void strace_buf(const void *buf, size_t len)
{
  const char *p = buf;
  putch('"');
  for (int i = 0; i < len && i < PRINT_CHAR_MAX; i++)
    putch((p[i] >= 32 && p[i] < 127) ? p[i] : '.');
  if (len > PRINT_CHAR_MAX)
    putstr("...");
  putch('"');
}

// 打印一行跟踪的前半段 "[时间] name(args"
// 注意: SYS_exit 的 handler 会 halt, 永远走不到末尾的 "= 返回值", 属预期
static void strace_begin(uintptr_t id, uintptr_t a, uintptr_t b, uintptr_t c)
{
  const char *name = (id < LENGTH(syscall_name)) ? syscall_name[id] : NULL;
  if (name == NULL)
  {
    printf("[strace] unknown syscall %d(0x%x, 0x%x, 0x%x", id, a, b, c);
    return;
  }
  printf("%s(", name);
  switch (id)
  {
  case SYS_open: // (pathname, flags, mode): pathname 是字符串, 打印内容而非指针
    strace_str((const char *)a);
    printf(", 0x%x, 0x%x", b, c);
    break;
  case SYS_write: // (fd, buf, count): buf 是待写出的内容
    printf("%d, ", a);
    strace_buf((const void *)b, c);
    printf(", %d", c);
    break;
  case SYS_read: // (fd, buf, count): 调用时刻 buf 还没有数据, 只打印地址
    printf("%d, buf@0x%x, %d", a, b, c);
    break;
  case SYS_close:
    printf("%d", a);
    break;
  case SYS_lseek:
    printf("%d, 0x%x, %d", a, b, c);
    break;
  case SYS_brk:
    printf("incr=0x%x", a);
    break;
  case SYS_yield:
  case SYS_getpid:
  case SYS_exit: // 无参数
    break;
  default: // 其余暂未使用: 统一按整数打印
    printf("0x%x, 0x%x, 0x%x", a, b, c);
    break;
  }
}

#endif // CONFIG_STRACE

void do_syscall(Context *c)
{
  uintptr_t a[4];
  a[0] = c->GPR1;
  a[1] = c->GPR2;
  a[2] = c->GPR3;
  a[3] = c->GPR4;

#ifdef CONFIG_STRACE
  strace_begin(a[0], a[1], a[2], a[3]); // 前半段: name(args
#endif

  switch (a[0])
  {
  case SYS_exit:
    c->GPRx = sys_exit(a[1]);
    break;
  case SYS_yield:
    c->GPRx = sys_yield();
    break;
  case SYS_read:
    c->GPRx = sys_read(a[1], (void *)a[2], a[3]);
    break;
  case SYS_write:
    c->GPRx = sys_write(a[1], (const void *)a[2], a[3]);
    break;
  case SYS_open:
    c->GPRx = sys_open((const char *)a[1], a[2], a[3]);
    break;
  case SYS_close:
    c->GPRx = sys_close(a[1]);
    break;
  case SYS_lseek:
    c->GPRx = sys_lseek(a[1], a[2], a[3]);
    break;
  case SYS_brk:
    c->GPRx = sys_brk(a[1]);
    break;
  default:
    panic("Unhandled syscall ID = %d", a[0]);
  }

#ifdef CONFIG_STRACE
  printf(") = 0x%x\n", c->GPRx); // 后半段: 返回值接在同一行
#endif
}
