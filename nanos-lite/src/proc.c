#include <proc.h>

#define MAX_NR_PROC 4

static PCB pcb[MAX_NR_PROC] __attribute__((used)) = {};
static PCB pcb_boot = {};
PCB *current = NULL;

void switch_boot_pcb()
{
  current = &pcb_boot;
  yield();
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
void context_uload(PCB *p, const char *filename, char *const argv[], char *const envp[])
{
  uintptr_t entry = loader(p, filename); // 先加载 ELF 拿到真实入口, 不能把文件名当地址

  // argv/envp 按 NULL 结尾约定数个数(没有显式的 envc), 允许传 NULL
  int argc = 0;
  while (argv != NULL && argv[argc] != NULL)
    argc++;
  int envc = 0;
  while (envp != NULL && envp[envc] != NULL)
    envc++;

  // 用户栈从内核堆顶向下切一块: heap.end 是 RAM 顶(0x88000000), 往上放整块越界;
  // 向下切还保证多次调用时各进程栈不重叠
  heap.end = (char *)heap.end - STACK_SIZE;
  char *ustack_top = (char *)heap.end + STACK_SIZE;
  p->as.area = (Area){heap.end, ustack_top};

  // 在栈顶摆参数块(自高向低): 字符串本体 | envp[] NULL | argv[] NULL | argc
  // 与进程入口的标准栈布局一致, crt0 的 call_main 按 uintptr_t 解析
  size_t strsz = 0;
  for (int i = 0; i < argc; i++)
    strsz += (strlen(argv[i]) + sizeof(uintptr_t)) & ~(sizeof(uintptr_t) - 1);
  for (int i = 0; i < envc; i++)
    strsz += (strlen(envp[i]) + sizeof(uintptr_t)) & ~(sizeof(uintptr_t) - 1);
  int nwords = 1 + argc + 1 + envc + 1; // argc + argv[] + NULL + envp[] + NULL

  uintptr_t *bp = (uintptr_t *)(ustack_top - strsz) - nwords; // 块底, argc 放这里
  char *sp = ustack_top;
  int k = 0;
  bp[k++] = (uintptr_t)argc;
  for (int i = 0; i < argc; i++)
  {
    size_t len = (strlen(argv[i]) + sizeof(uintptr_t)) & ~(sizeof(uintptr_t) - 1);
    sp -= len;
    memcpy(sp, argv[i], strlen(argv[i]) + 1);
    bp[k++] = (uintptr_t)sp;
  }
  bp[k++] = 0;
  for (int i = 0; i < envc; i++)
  {
    size_t len = (strlen(envp[i]) + sizeof(uintptr_t)) & ~(sizeof(uintptr_t) - 1);
    sp -= len;
    memcpy(sp, envp[i], strlen(envp[i]) + 1);
    bp[k++] = (uintptr_t)sp;
  }
  bp[k++] = 0;

  // 栈顶收到块底: 初始 sp 指向 argc, 之后应用压栈只会写到块底以下, 参数块不会被破坏
  p->cp = ucontext(&p->as, (Area){p->as.area.start, (char *)bp}, (void *)entry, bp);
  Log("Load %s: entry = %p, argc = %d", filename, entry, argc);
}

void init_proc()
{
  context_kload(&pcb[0], hello_fun, (void *)1L);
  context_kload(&pcb[1], hello_fun, (void *)2L);

  switch_boot_pcb();

  // 测试参数传递: 仙剑带 --skip 启动, 跳过片头商标动画
  // argv[0] 按惯例是程序路径; 测试用不到环境变量, 给一个仅含 NULL 的空 envp
  char *const pal_argv[] = {"/bin/pal", NULL, NULL};
  char *const pal_envp[] = {NULL};
  context_uload(&pcb[0], "/bin/pal", pal_argv, pal_envp);
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
