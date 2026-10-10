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

// 简化的异常响应
// 直接给出exception number & exception handler entry
word_t isa_raise_intr(word_t NO, vaddr_t epc)
{
  /* Trigger an interrupt/exception with ``NO''.
   * Then return the address of the interrupt/exception vector.
   */
  cpu.csr.mepc = epc;     // pc of the instruction that triggered the exception
  cpu.csr.mcause = NO;    // record the exception number
  cpu.pc = cpu.csr.mtvec; // jump to the trap entry

  return cpu.pc;
}

// word_t isa_trap(word_t NO, vaddr_t epc)
//{
// cpu.csr.mepc = epc;
// cpu.csr.mcause = NO;
// cpu.pc = cpu.csr.mtvec;
// return cpu.pc;
//}

word_t isa_query_intr()
{
  return INTR_EMPTY;
}
