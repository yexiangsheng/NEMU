#include <common.h>
#include "syscall.h"

// ???
// I don't nanos-lite and navy-apps how to communicate
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
  case SYS_yield:
    // ???
    // sys_ret = SYS_yield();
    sys_yield();
    Log_syscall("yield", "null", "0");
  case SYS_exit:
    // ???
    // how to transit machine state from nemu to nanos-lite?
    sys_exit(0);
    Log_syscall("exit", "status", "0");
    break;
  case SYS_read:
    ssize_t read_ret = sys_read(a[1], &a[2], a[3]);
    Log_syscall("read", "null", read_ret);
    break;
  case SYS_write:
    ssize_t write_ret = sys_write(a[1], &a[2], a[3]);
    Log_syscall("write", "null", write_ret);
    break;
  default:
    panic("Unhandled syscall ID = %d", a[0]);
  }
}
