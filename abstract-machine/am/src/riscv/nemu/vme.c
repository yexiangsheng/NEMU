#include <am.h>
#include <nemu.h>
#include <klib.h>

// Sv32 页表项标志位(PTE_V/R/W/X)用框架 riscv.h 里的定义

static AddrSpace kas = {};
static void *(*pgalloc_usr)(int) = NULL;
static void (*pgfree_usr)(void *) = NULL;
static int vme_enable = 0;

static Area segments[] = {// Kernel memory mappings
                          NEMU_PADDR_SPACE};

#define USER_SPACE RANGE(0x40000000, 0x80000000)

static inline void set_satp(void *pdir)
{
  uintptr_t mode = 1ul << (__riscv_xlen - 1);
  asm volatile("csrw satp, %0" : : "r"(mode | ((uintptr_t)pdir >> 12)));
}

static inline uintptr_t get_satp()
{
  uintptr_t satp;
  asm volatile("csrr %0, satp" : "=r"(satp));
  return satp << 12;
}

bool vme_init(void *(*pgalloc_f)(int), void (*pgfree_f)(void *))
{
  pgalloc_usr = pgalloc_f;
  pgfree_usr = pgfree_f;

  // malloc a mem PAGSIZE(4KB)
  kas.ptr = pgalloc_f(PGSIZE);

  // 填写内核空间的Page table
  int i;
  for (i = 0; i < LENGTH(segments); i++)
  {
    void *va = segments[i].start;
    for (; va < segments[i].end; va += PGSIZE)
    {
      map(&kas, va, va, 0);
    }
  }

  set_satp(kas.ptr);
  vme_enable = 1;

  return true;
}

void protect(AddrSpace *as)
{
  // PTE in kernel space
  // every proc has a page table by itself
  // page table in proc heap
  PTE *updir = (PTE *)(pgalloc_usr(PGSIZE));
  as->ptr = updir;
  // data and text in user space
  as->area = USER_SPACE;
  as->pgsize = PGSIZE;
  // map kernel space
  memcpy(updir, kas.ptr, PGSIZE);
}

void unprotect(AddrSpace *as)
{
}

void __am_get_cur_as(Context *c)
{
  c->pdir = (vme_enable ? (void *)get_satp() : NULL);
}

void __am_switch(Context *c)
{
  if (vme_enable && c->pdir != NULL)
  {
    set_satp(c->pdir);
  }
}

void map(AddrSpace *as, void *va, void *pa, int prot)
{
  uintptr_t uva = (uintptr_t)va, upa = (uintptr_t)pa;
  PTE *root = (PTE *)as->ptr;

  // 第一级: VPN1 = va[31:22] 作下标查根表, 每项覆盖 4MB。
  // 表项无效说明该 4MB 区间还没有二级页表, 通过回调向 Nanos-lite 要一页
  // (pg_alloc 已保证清零), 挂成非叶子项(只置 V, 不置 R/W/X)
  PTE *pde = &root[(uva >> 22) & 0x3ff];
  if (!(*pde & PTE_V))
  {
    PTE *pt = pgalloc_usr(PGSIZE);
    *pde = ((uintptr_t)pt >> 12 << 10) | PTE_V;
  }

  // 第二级: VPN0 = va[21:12] 作下标查二级表, 叶子项写入目标物理页 PPN。
  // PA 简化不区分权限, 一律可读可写可执行
  PTE *pt = (PTE *)((*pde >> 10) << 12);
  pt[(uva >> 12) & 0x3ff] = (upa >> 12 << 10) | PTE_V | PTE_R | PTE_W | PTE_X;
}

Context *ucontext(AddrSpace *as, Area kstack, void *entry, void *arg)
{
  // Context 放在栈顶一格: trap.S 不保存/恢复 gpr[2],
  // 而是用 Context 的地址 + CONTEXT_SIZE 推算出 sp, 因此首次调度时 sp = kstack.end
  Context *c = (Context *)kstack.end - 1;
  *c = (Context){0};
  c->pdir = (vme_enable ? as->ptr : NULL); // 地址空间,供 __am_switch 切换 satp
  c->GPRx = (uintptr_t)arg;                // a0 = 参数块地址,crt0 的 call_main 由此取 argc/argv/envp
  c->mepc = (uintptr_t)entry;
  c->mstatus = 0x1800; // MPP = M:mret 之后仍处于机器模式

  return c;
}
