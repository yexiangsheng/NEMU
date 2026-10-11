#include <common.h>
#include <fs.h>
#include "syscall.h"

/* kernel-side syscall handlers */

/* man 2 write: on success, return the number of bytes written; -1 on error */
static ssize_t sys_write(int fd, const void *buf, size_t count)
{
  // if (fd == 1 || fd == 2)
  //{ // stdout / stderr only
  // const char *p = buf;
  //// for (size_t i = 0; i < count; i++)
  //// putch(p[i]); // output to the serial port, byte by byte
  //// return count;
  return fs_write(fd, buf, count);
  //}
  // return -1; // other fds are not supported yet
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

static int sys_brk()
{
  return 0;
}

void do_syscall(Context *c)
{
  uintptr_t a[4];
  a[0] = c->GPR1;
  // uintptr_t sys_ret = c->GPRx;
  a[1] = c->GPR2;
  a[2] = c->GPR3;
  a[3] = c->GPR4;

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
}
