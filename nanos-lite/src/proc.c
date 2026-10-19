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

// 加载用户程序 ELF 并创建可被调度的上下文, 并把 argv/envp 传给新程序。
// 注意: Navy 所有程序都链接在 0x83000000, 没实现 VME(分页)之前,
// 多个用户程序(含 naive_uload 的程序)镜像会互相覆盖, 同一时刻只能存在一个!
void context_uload(PCB *p, const char *filename, char *const argv[], char *const envp[])
{
  // filename/argv/envp 目前还躺在调用者的内存里(旧程序镜像或内核栈),
  // 而 loader 会把旧镜像整个覆盖 —— 所以必须先数个数、拷字符串, 最后才跑 loader

  // argv/envp 按 NULL 结尾约定数个数(没有显式的 envc), 允许传 NULL;
  // 惯例 argv[0] 是程序路径: 调用方没给(argv==NULL 或空)时用 filename 补上,
  // 保证 main 拿到的 argv[0] 永远是合法字符串
  char *const argv_default[2] = {(char *)filename, NULL};
  char *const *av = argv;
  if (av == NULL || av[0] == NULL)
    av = argv_default;
  int argc = 0;
  while (av[argc] != NULL)
    argc++;
  int envc = 0;
  while (envp != NULL && envp[envc] != NULL)
    envc++;

  // filename 也拷到内核栈上备用: loader 覆盖旧镜像后, 原指针就是野指针了
  char kname[128];
  int flen = 0;
  while (filename[flen] != '\0' && flen < (int)sizeof(kname) - 1)
    flen++;
  memcpy(kname, filename, flen);
  kname[flen] = '\0';

  // 用户栈从内核堆顶向下切一块: heap.end 是 RAM 顶(0x88000000), 往上放整块越界;
  // 向下切保证反复 execve 时新栈不与旧栈重叠 —— 但旧栈不回收, 属已知内存泄漏
  heap.end = (char *)heap.end - STACK_SIZE;
  char *ustack_top = (char *)heap.end + STACK_SIZE;
  p->as.area = (Area){heap.end, ustack_top};

  // 在栈顶摆参数块(自高向低): 字符串本体 | envp[] NULL | argv[] NULL | argc
  // crt0 的 call_main 按此布局解析: argc = args[0], argv = args+1, envp = argv+argc+1
  size_t strsz = 0;
  for (int i = 0; i < argc; i++)
    strsz += (strlen(av[i]) + sizeof(uintptr_t)) & ~(sizeof(uintptr_t) - 1);
  for (int i = 0; i < envc; i++)
    strsz += (strlen(envp[i]) + sizeof(uintptr_t)) & ~(sizeof(uintptr_t) - 1);
  int nwords = 1 + argc + 1 + envc + 1; // argc + argv[] + NULL + envp[] + NULL

  uintptr_t *bp = (uintptr_t *)(ustack_top - strsz) - nwords; // 块底, argc 放这里
  bp = (uintptr_t *)((uintptr_t)bp & ~0xf);                   // 16 字节对齐(riscv 栈 ABI)
  char *sp = ustack_top;
  int k = 0;
  bp[k++] = (uintptr_t)argc;
  for (int i = 0; i < argc; i++)
  {
    size_t len = (strlen(av[i]) + sizeof(uintptr_t)) & ~(sizeof(uintptr_t) - 1);
    sp -= len;
    memcpy(sp, av[i], strlen(av[i]) + 1); // 字符串拷到新栈, 从此不再依赖旧镜像
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

  // 现在才加载 ELF: 参数已全部安全落在新的用户栈上, 旧镜像随便覆盖
  uintptr_t entry = loader(p, kname);

  // 初始 sp 收在参数块底: 应用首次压栈只写到块底以下, 参数块不会被破坏;
  // GPRx(a0) = 参数块地址, start.S 原样传给 call_main
  p->cp = ucontext(&p->as, (Area){p->as.area.start, (char *)bp}, (void *)entry, bp);
  Log("Load %s: entry = 0x%x, argc = %d, argv-block = 0x%x", kname, entry, argc, (uintptr_t)bp);
}

void init_proc()
{
  // 开机即进仙剑(--skip 跳过商标动画): 顺带验证 execve 的参数传递链路;
  // 退出后 sys_exit 会把 NTerm 拉起到 pcb[0] 上, 之后从內建 Shell 启动其它程序
  char *const pal_argv[] = {"/bin/pal", "--skip", NULL};
  char *const pal_envp[] = {NULL};
  context_uload(&pcb[0], "/bin/pal", pal_argv, pal_envp);

  // 一个 hello 内核线程: 用户程序 yield 时穿插打印, 证明调度仍在轮转
  context_kload(&pcb[1], hello_fun, (void *)1L);

  switch_boot_pcb(); // current = &pcb_boot 并 yield, 进入 boot(闲逛) <-> pal <-> hello 轮转
}

Context *schedule(Context *prev)
{
  current->cp = prev;
  // boot(闲逛) <-> pcb[0](用户程序) <-> pcb[1](hello) 三方轮转:
  // 用户程序的 SYS_yield 会经内核 yield() 到这里,必须能让 boot 转回来
  if (current == &pcb_boot)
    current = &pcb[0];
  else if (current == &pcb[0])
    current = &pcb[1];
  else
    current = &pcb_boot;
  return current->cp;
}
