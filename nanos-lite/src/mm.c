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

uintptr_t mm_brk(uintptr_t incr)
{
  if (program_break == 0)                  // 首次调用:把堆起点定在内核空闲区开头
    program_break = (uintptr_t)heap.start; // PA3 无虚存,用户堆与内核共用物理内存
  uintptr_t old = program_break;
  program_break += incr; // 返回旧 break = 新增内存的起点(malloc 依赖这一点)
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
