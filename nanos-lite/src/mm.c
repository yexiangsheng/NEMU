#include <memory.h>

static void *pf = NULL;

void *new_page(size_t nr_page)
{
  assert(pf != NULL);
  void *old_pf = pf;
  pf += nr_page * PGSIZE;
  return old_pf;
}

#ifdef HAS_VME
static void *pg_alloc(int n)
{
  // AM 通过回调申请的空间保证是页面大小的整数倍;
  // 页表内容不能有脏数据, 分配出来的页面必须清零
  assert(n % PGSIZE == 0);
  void *p = new_page(n / PGSIZE);
  memset(p, 0, n);
  return p;
}
#endif

void free_page(void *p)
{
  panic("not implement yet");
}

/* The brk() system call handler. */
static uintptr_t program_break = 0;
static uintptr_t break_committed = 0; // 已向页池提交到的堆顶(页对齐)

uintptr_t mm_brk(uintptr_t incr)
{
  // 用户堆必须和 pg_alloc 的页表共用同一个页池, 否则堆起点(=heap.start)会与
  // 页表本体重叠: PA3 时代堆从 heap.start 线性生长没有冲突, VME 一开,
  // pg_alloc 同样从 heap.start 起分配页表, 堆一写就把根页表抹了。
  // 按页提交: break 越过已提交页就继续向页池要新页(与页表互斥, 永不重叠)
  if (program_break == 0)
  {
    program_break = (uintptr_t)new_page(1);
    break_committed = program_break + PGSIZE;
  }
  uintptr_t old = program_break;
  program_break += incr; // 返回旧 break = 新增内存的起点(malloc 依赖这一点)
  while (program_break > break_committed)
  {
    new_page(1);
    break_committed += PGSIZE;
  }
  return old;
}

void init_mm()
{
  // 空闲物理页就是从heap开始
  pf = (void *)ROUNDUP(heap.start, PGSIZE);
  Log("free physical pages starting from %p", pf);

#ifdef HAS_VME
  vme_init(pg_alloc, free_page);
#endif
}
