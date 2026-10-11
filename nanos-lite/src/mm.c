#include <memory.h>

static void *pf = NULL;

void *new_page(size_t nr_page)
{
  return NULL;
}

#ifdef HAS_VME
static void *pg_alloc(int n)
{
  return NULL;
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
  program_break += incr;                   // 返回旧 break = 新增内存的起点(malloc 依赖这一点)
  return old;
}

void init_mm()
{
  pf = (void *)ROUNDUP(heap.start, PGSIZE);
  Log("free physical pages starting from %p", pf);

#ifdef HAS_VME
  vme_init(pg_alloc, free_page);
#endif
}
