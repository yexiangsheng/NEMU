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
    Log("Hello World from Nanos-lite with arg '%p' for the %dth time!", (uintptr_t)arg, j);
    j++;
    yield();
  }
}

void init_proc()
{
  switch_boot_pcb();

  // NTerm 作为批处理系统的第一个程序: 程序结束后由 SYS_exit 再次拉起 nterm
  naive_uload(NULL, "/bin/hello"); // TODO(测试): 验证 SYS_exit -> nterm 链路后改回 /bin/nterm

  Log("Initializing processes...");

  // load program here
}

Context *schedule(Context *prev)
{
  return NULL;
}
