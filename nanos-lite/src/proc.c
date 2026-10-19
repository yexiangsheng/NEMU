#include <proc.h>

#define MAX_NR_PROC 4

static PCB pcb[MAX_NR_PROC] __attribute__((used)) = {};
static PCB pcb_boot = {};
PCB *current = NULL;

void switch_boot_pcb()
{
  current = &pcb_boot;
}

void hello_fun(void *arg)
{
  int j = 1;
  while (1)
  {
    Log("Hello World from Nanos-lite with arg '%d' for the %dth time!", (int)(uintptr_t)arg, j);
    j++;
    yield();
  }
}

// 把内核线程的上下文建在传入 PCB 自己的内核栈上
void context_kload(PCB *p, void (*entry)(void *), void *arg)
{
  p->as.area = (Area){p->stack, p->stack + STACK_SIZE};
  p->cp = kcontext(p->as.area, entry, arg);
}

void context_uload(PCB *p, const char *filename)
{
  p->as.area = (Area){heap.end, heap.end + STACK_SIZE};
  p->cp->GPRx = (uintptr_t)heap.end;
  p->cp = ucontext(&(p->as), p->as.area, (void *)filename);
}

void init_proc()
{
  // context_kload(&pcb[0], hello_fun, (void *)1L);
  context_uload(&pcb[0], "/bin/bird");
  context_uload(&pcb[1], "/bin/hello");

  switch_boot_pcb();

  // NTerm 作为批处理系统的第一个程序: 程序结束后由 SYS_exit 再次拉起 nterm
  naive_uload(NULL, "/bin/nterm");

  Log("Initializing processes...");

  // load program here
}

Context *schedule(Context *prev)
{
  current->cp = prev;
  // 在 boot(承载 NTerm)与两个 hello 内核线程之间三方轮转:
  // NTerm 的 SYS_yield 会经内核 yield() 到这里,必须能让 boot 转回来
  if (current == &pcb_boot)
    current = &pcb[0];
  else if (current == &pcb[0])
    current = &pcb[1];
  else
    current = &pcb_boot;
  return current->cp;
}
