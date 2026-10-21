/***************************************************************************************
 * Copyright (c) 2014-2024 Zihao Yu, Nanjing University
 *
 * NEMU is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 * See the Mulan PSL v2 for more details.
 ***************************************************************************************/

#include <isa.h>
#include <memory/paddr.h>

// 三类访存共用的地址求解: 问 isa_mmu_check 要不要过 MMU ——
// DIRECT 则虚地址就是物理地址; TRANSLATE 则先走页表翻译
static inline paddr_t va2pa(vaddr_t addr, int len, int type)
{
  int m = isa_mmu_check(addr, len, type);
  if (m == MMU_TRANSLATE)
  {
    paddr_t paddr = isa_mmu_translate(addr, len, type);
    assert(paddr != MEM_RET_FAIL); // 页表里没有这个映射
    return paddr;
  }
  assert(m == MMU_DIRECT);
  return addr;
}

// ifetch 与 read 除 MEM_TYPE 外完全同构, 用 ## 拼接批量生成
#define DEF_VADDR(name, pfunc, mtype)                       \
  word_t vaddr_##name(vaddr_t addr, int len)                \
  {                                                         \
    return paddr_##pfunc(va2pa(addr, len, mtype), len);     \
  }

DEF_VADDR(ifetch, read, MEM_TYPE_IFETCH)
DEF_VADDR(read, read, MEM_TYPE_READ)

void vaddr_write(vaddr_t addr, int len, word_t data)
{
  paddr_write(va2pa(addr, len, MEM_TYPE_WRITE), len, data);
}
