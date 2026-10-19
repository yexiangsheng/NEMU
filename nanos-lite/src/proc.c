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

// 加载用户程序 ELF 并创建可被调度的上下文。
// 注意: Navy 所有程序都链接在 0x83000000, 没实现 VME(分页)之前,
// 多个用户程序(含 naive_uload 的程序)镜像会互相覆盖, 同一时刻只能存在一个!
void context_uload(PCB *p, const char *filename)
{
  uintptr_t entry = loader(p, filename); // 先加载 ELF 拿到真实入口, 不能把文件名当地址
  // 用户栈从内核堆顶向下切一块: heap.end 是 RAM 顶(0x88000000), 往上放整块越界;
  // 向下切还保证多次调用时各进程栈不重叠
  heap.end = (char *)heap.end - STACK_SIZE;
  p->as.area = (Area){heap.end, (char *)heap.end + STACK_SIZE};
  p->cp = ucontext(&p->as, p->as.area, (void *)entry);
}

void init_proc()
{
  context_kload(&pcb[0], hello_fun, (void *)1L);
  context_kload(&pcb[1], hello_fun, (void *)2L);

  switch_boot_pcb();

  // NTerm 作为批处理系统的第一个程序: 程序结束后由 SYS_exit 再次拉起 nterm。
  // 没有 VME 时整个系统只允许这一个用户程序, 多道程序用上面的内核线程演示
  naive_uload(NULL, "/bin/nterm");

  // Log("Initializing processes...");

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
