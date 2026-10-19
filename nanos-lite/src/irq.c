#include <common.h>
#include <proc.h>
#include "syscall.h"

Context *do_syscall(Context *c); // implemented in syscall.c

// Syscall event request
static Context *do_event(Event e, Context *c)
{
  switch (e.event)
  {
  case EVENT_YIELD:
    // 注意不能在这里调 yield(): yield() 本身就是 ecall(a7=-1),
    // 会导致 EVENT_YIELD -> yield() -> EVENT_YIELD 的无限递归, 直到栈溢出砸穿程序镜像
    c = schedule(c);
    break;
  case EVENT_SYSCALL:
    c = do_syscall(c); // execve/exit 可能换成新程序的上下文
    break;
  default:
    panic("Unhandled event ID = %d", e.event);
  }

  return c;
}

void init_irq(void)
{
  Log("Initializing interrupt/exception handler...");
  cte_init(do_event);
}
