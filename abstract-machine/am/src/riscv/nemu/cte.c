#include <riscv/riscv.h>
#include <am.h>
#include <klib.h>
#define CONFIG_ETRACE 0

// Hardware Exception
// ecall --> event --> exception handler

static Context *(*user_handler)(Event, Context *) = NULL;

void printf_context(Context *c)
{
  printf("CSR register: mcause = 0x%x, mstatus = 0x%x, mepc = 0x%x\n",
         c->mcause, c->mstatus, c->mepc);
}

// Interrupt request
Context *__am_irq_handle(Context *c)
{
  if (user_handler)
  {
#if CONFIG_ETRACE
    printf_context(c);
#endif
    Event ev = {0};
    switch (c->mcause)
    {
    case 11:
      c->mepc += 4;
      if (c->GPR1 == (uintptr_t)-1)
      {
        ev.event = EVENT_YIELD;
      }
      // else
      //{
      // ev.event = EVENT_SYSCALL;
      // ev.cause = c->GPR1;
      //}
      if (c->GPR1 >= (uintptr_t)0 && c->GPR1 <= 256)
      {
        ev.event = EVENT_SYSCALL;
        ev.cause = c->GPR1;
      }
      break;
    default:
      ev.event = EVENT_ERROR;
      break;
    }

    // more than interrupt request
    c = user_handler(ev, c);
    assert(c != NULL);
  }

  return c;
}

// keep Context(register)
// this func will be call in start.S
extern void __am_asm_trap(void);

bool cte_init(Context *(*handler)(Event, Context *))
{
  // initialize exception entry
  asm volatile("csrw mtvec, %0" : : "r"(__am_asm_trap));

  // regi ster event handler
  user_handler = handler;

  return true;
}

Context *kcontext(Area kstack, void (*entry)(void *), void *arg)
{
  return NULL;
}

// trap function
void yield()
{
#ifdef __riscv_e
  asm volatile("li a5, -1; ecall");
#else
  asm volatile("li a7, -1; ecall");
#endif
}

bool ienabled()
{
  return false;
}

void iset(bool enable)
{
}
