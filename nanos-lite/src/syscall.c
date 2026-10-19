#include <common.h>
#include <fs.h>
#include <proc.h>
#include <sys/time.h>
#include "syscall.h"

/* kernel-side syscall handlers */

uintptr_t mm_brk(uintptr_t incr); // implemented in mm.c

/* man 2 write: on success, return the number of bytes written; -1 on error */
static ssize_t sys_write(int fd, const void *buf, size_t count)
{
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

/* 批处理系统: 程序结束后不关机, 把 NTerm 拉起到当前进程的 PCB 上。
 * 旧程序镜像已被 context_uload 覆盖, 必须返回新上下文, 不能回到旧程序 */
static Context *sys_exit(void)
{
  context_uload(current, "/bin/nterm", NULL, NULL);
  return current->cp;
}

/* man 2 execve: 结束当前程序, 加载并运行 fname 指定的新程序, 带 argv/envp。
 * pcb 参数传 current: execve 的语义是"当前进程换新程序",
 * 新上下文记在当前 PCB 上, 对 schedule() 的轮转完全透明 */
static Context *sys_execve(Context *c)
{
  context_uload(current, (const char *)c->GPR2, (char *const *)c->GPR3, (char *const *)c->GPR4);
  return current->cp; // 成功时不返回旧程序; 加载失败会在 fs_open 处 panic
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

static int sys_gettimeofdy(struct timeval *tv, struct timezone *tz)
{
  uint64_t us = io_read(AM_TIMER_UPTIME).us;
  tv->tv_sec = us / 1000000;
  tv->tv_usec = us % 1000000;
  return 0;
}
// ==================== strace: 系统调用跟踪 ====================
// 开关在 Makefile: make ... STRACE=1 时通过 -DCONFIG_STRACE 打开, 默认关闭

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
// 注意: SYS_exit(转去重新拉起程序)和加载成功的 SYS_execve都不会返回,
// 走不到末尾的 "= 返回值", 属预期
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
  case SYS_execve: // (fname, argv, envp): fname 是路径字符串, 打印内容而非指针
    strace_str((const char *)a);
    printf(", argv@0x%x, envp@0x%x", b, c);
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

/* 返回值: 正常情况下原样返回 c(内核把返回值写进 c->GPRx);
 * SYS_exit/SYS_execve 则换成了新程序的上下文 —— 旧程序已被覆盖, 回不去了 */
Context *do_syscall(Context *c)
{
  uintptr_t a[4];
  a[0] = c->GPR1;
  a[1] = c->GPR2;
  a[2] = c->GPR3;
  a[3] = c->GPR4;

#ifdef CONFIG_STRACE
  strace_begin(a[0], a[1], a[2], a[3]); // 前半段: name(args
#endif

#ifdef CONFIG_STRACE
  int switched = 0; // exit/execve 换走了上下文, 调用方拿不到"返回值"
#endif
  switch (a[0])
  {
  case SYS_exit:
    c = sys_exit();
#ifdef CONFIG_STRACE
    switched = 1;
#endif
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
  case SYS_execve:
    c = sys_execve(c);
#ifdef CONFIG_STRACE
    switched = 1;
#endif
    break;
  case SYS_gettimeofday:
    c->GPRx = sys_gettimeofdy((struct timeval *)a[1], (struct timezone *)a[2]);
    break;
  default:
    panic("Unhandled syscall ID = %d", a[0]);
  }

#ifdef CONFIG_STRACE
  if (switched)
    printf(") = <switched to new context>\n"); // 换了程序, 不会有返回值
  else
    printf(") = 0x%x\n", c->GPRx); // 后半段: 返回值接在同一行
#endif

  return c;
}
