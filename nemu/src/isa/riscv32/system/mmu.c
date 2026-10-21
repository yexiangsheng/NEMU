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
#include <memory/vaddr.h>
#include <memory/paddr.h>

int isa_mmu_check(vaddr_t vaddr, int len, int type)
{
  // NEMU 是模拟器: satp 的当前值就是 cpu.csr.satp(AM 侧的 get_satp() 在这里不存在)。
  // satp[31] = Sv32 的 MODE 位: 0 = Bare(虚地址即物理地址), 1 = 开启分页翻译。
  // 内核和用户都跑在机器模式(ucontext 设 MPP=M), 无特权级绕过, type 也无需区分
  word_t satp = cpu.csr.satp;
  int mode = satp >> 31;
  switch (mode)
  {
  case 0:
    return MMU_DIRECT;
  case 1:
    return MMU_TRANSLATE;
  default:
    break;
  }
  return MMU_FAIL;
}

paddr_t isa_mmu_translate(vaddr_t vaddr, int len, int type)
{
  // Sv32 两级查表, 根页表物理地址 = satp[21:0] << 12
  paddr_t root = ((paddr_t)(cpu.csr.satp & 0x3fffff)) << 12;

  // 第一级: vaddr[31:22] 作下标查根表, 表项指向二级页表(非叶子项只看 V 位)
  word_t pte1 = paddr_read(root + (((vaddr >> 22) & 0x3ff) << 2), 4);
  if (!(pte1 & 1))
    return MEM_RET_FAIL;
  paddr_t pt2 = ((pte1 >> 10) & 0x3fffff) << 12;

  // 第二级: vaddr[21:12] 作下标查二级表, 叶子表项给出目标物理页
  word_t pte0 = paddr_read(pt2 + (((vaddr >> 12) & 0x3ff) << 2), 4);
  if (!(pte0 & 1))
    return MEM_RET_FAIL;
  paddr_t paddr = (((pte0 >> 10) & 0x3fffff) << 12) | (vaddr & 0xfff);

  // 内核空间是恒等映射: 落在 pmem 里的虚地址, 翻译前后必须相同。
  // 不满足即说明查表实现或页表内容有 bug
  if (in_pmem(vaddr))
    assert(paddr == vaddr);

  return paddr;
}
