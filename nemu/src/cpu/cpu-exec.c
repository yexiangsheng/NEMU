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

#include <cpu/cpu.h>
#include <cpu/decode.h>
#include <cpu/difftest.h>
#include <memory/vaddr.h>
#include <memory/paddr.h>
#ifdef CONFIG_WATCHPOINT
#include "../monitor/sdb/sdb.h"
#endif
#ifdef CONFIG_FTRACE
#include "../monitor/ftrace.h"
#endif
#include <locale.h>

/* The assembly code of instructions executed is only output to the screen
 * when the number of instructions executed is less than this value.
 * This is useful when you use the `si' command.
 * You can modify this value as you want.
 */
#define MAX_INST_TO_PRINT 10

#ifdef CONFIG_ITRACE
#define IRINGBUF_CAPACITY 16 // iringbuf 最多记录的指令条数

typedef struct ring_buf
{
  char *buffer;     // 存储区起始地址
  char *buffer_end; // 存储区结束地址
  char *head;       // 指向最旧的指令
  char *tail;       // 指向下一条指令的写入位置
  char *current;    // 指向最新写入的指令(即当前正在执行的指令)
  size_t capacity;  // ring_buf 的最大容量
  size_t size;      // 每条记录占用的字节数
  bool full;        // 是否已经写满一圈
} ring_buf;

static ring_buf iringbuf;

// 让指针 p 前移一条记录, 到达末尾后回到开头, 形成环
static void ring_buf_next(ring_buf *rb, char **p)
{
  *p += rb->size;
  if (*p == rb->buffer_end)
  {
    *p = rb->buffer;
  }
}

static void init_ring_buf(ring_buf *rb, size_t capacity, size_t size)
{
  rb->buffer = malloc(capacity * size);
  Assert(rb->buffer != NULL, "malloc for iringbuf failed");
  memset(rb->buffer, 0, capacity * size);
  rb->buffer_end = rb->buffer + capacity * size;
  rb->capacity = capacity;
  rb->size = size;
  rb->head = rb->tail = rb->current = rb->buffer;
  rb->full = false;
}

// 把当前指令的 trace 行写入环形缓冲区
static void ring_buf_record(ring_buf *rb, const char *logbuf)
{
  if (rb->buffer == NULL)
  {
    init_ring_buf(rb, IRINGBUF_CAPACITY, sizeof(((Decode *)0)->logbuf));
  }
  snprintf(rb->tail, rb->size, "%s", logbuf);

  rb->current = rb->tail; // 当前正在执行的指令

  ring_buf_next(rb, &rb->tail);
  if (rb->full)
  {
    // 已写满一圈: 最旧的记录被覆盖, head 跟着前移一条
    ring_buf_next(rb, &rb->head);
  }
  else if (rb->tail == rb->head)
  {
    rb->full = true; // tail 追上 head, 说明已经写满一圈
  }
}

// 从旧到新输出缓冲区中的所有指令, 最新的一条(当前指令)用 --> 标出
static void trace_output_ringbuf(ring_buf *rb)
{
  if (rb->buffer == NULL || (!rb->full && rb->head == rb->tail))
  {
    return; // 还没有记录过指令
  }
  size_t count = rb->full ? rb->capacity : (size_t)(rb->tail - rb->buffer) / rb->size;
  char *p = rb->head;
  for (size_t i = 0; i < count; i++)
  {
    printf("%s%s\n", p == rb->current ? "--> " : "    ", p);
    ring_buf_next(rb, &p);
  }
}

// 错误发生后, 从出错指令的下一条开始, 把内存中"还没执行到"的几条指令打印出来, 便于观察上下文
// (按定长 4 字节顺序读内存, 适用于 RISC-V; x86 变长指令不适用)
static void peek_next_insts(vaddr_t pc, int n)
{
  bool try_disassemble(char *str, int size, uint64_t pc, uint8_t *code, int nbyte);
  printf("Instructions after the error (not executed yet):\n");
  for (int k = 0; k < n; k++, pc += 4)
  {
    if (pc < PMEM_LEFT || pc + 3 > PMEM_RIGHT)
    {
      break; // 越出物理内存范围, 不再继续读
    }
    word_t inst = vaddr_read(pc, 4);
    uint8_t *b = (uint8_t *)&inst;
    char text[48];
    bool ok = try_disassemble(text, sizeof(text), pc, b, 4);
    printf(".... " FMT_WORD ": %02x %02x %02x %02x  %s\n",
           pc, b[3], b[2], b[1], b[0], ok ? text : "(invalid)");
  }
}
#endif // CONFIG_ITRACE

CPU_state cpu = {};
uint64_t g_nr_guest_inst = 0;
static uint64_t g_timer = 0; // unit: us
static bool g_print_step = false;

void device_update();

// trace : output instruction
static void trace_and_difftest(Decode *_this, vaddr_t dnpc)
{
#ifdef CONFIG_ITRACE_COND
  if (ITRACE_COND)
  {
    log_write("%s\n", _this->logbuf);
    IFDEF(CONFIG_ITRACE, ring_buf_record(&iringbuf, _this->logbuf));
  }
#endif
#ifdef CONFIG_WATCHPOINT
  watchpoint_check();
#endif
  if (g_print_step)
  {
    IFDEF(CONFIG_ITRACE, puts(_this->logbuf));
  }
  IFDEF(CONFIG_DIFFTEST, difftest_step(_this->pc, dnpc));
  IFDEF(CONFIG_FTRACE, ftrace_step(_this));
}

// Fetch, Decode, Execute, Refresh PC
static void exec_once(Decode *s, vaddr_t pc)
{
  s->pc = pc;
  s->snpc = pc;
  isa_exec_once(s);
  cpu.pc = s->dnpc; // refresh PC
#ifdef CONFIG_ITRACE
  char *p = s->logbuf;
  p += snprintf(p, sizeof(s->logbuf), FMT_WORD ":", s->pc);
  int ilen = s->snpc - s->pc;
  int i;
  uint8_t *inst = (uint8_t *)&s->isa.inst;
#ifdef CONFIG_ISA_x86
  for (i = 0; i < ilen; i++)
  {
#else
  for (i = ilen - 1; i >= 0; i--)
  {
#endif
    p += snprintf(p, 4, " %02x", inst[i]);
  }
  int ilen_max = MUXDEF(CONFIG_ISA_x86, 8, 4);
  int space_len = ilen_max - ilen;
  if (space_len < 0)
    space_len = 0;
  space_len = space_len * 3 + 1;
  memset(p, ' ', space_len);
  p += space_len;

  void disassemble(char *str, int size, uint64_t pc, uint8_t *code, int nbyte);
  // 指令非法(INV)时跳过反汇编: 非法编码 capstone 解不出来,
  // 否则 disassemble 里的 assert 会在 iringbuf 输出之前就把 NEMU 杀掉
  if (nemu_state.state != NEMU_ABORT)
  {
    disassemble(p, s->logbuf + sizeof(s->logbuf) - p,
                MUXDEF(CONFIG_ISA_x86, s->snpc, s->pc), (uint8_t *)&s->isa.inst, ilen);
  }
#endif
}

static void execute(uint64_t n)
{
  Decode s;
  for (; n > 0; n--)
  {
    exec_once(&s, cpu.pc);
    g_nr_guest_inst++;
    trace_and_difftest(&s, cpu.pc);
    if (nemu_state.state != NEMU_RUNNING)
      break;
    IFDEF(CONFIG_DEVICE, device_update());
  }
}

static void statistic()
{
  IFNDEF(CONFIG_TARGET_AM, setlocale(LC_NUMERIC, ""));
#define NUMBERIC_FMT MUXDEF(CONFIG_TARGET_AM, "%", "%'") PRIu64
  Log("host time spent = " NUMBERIC_FMT " us", g_timer);
  Log("total guest instructions = " NUMBERIC_FMT, g_nr_guest_inst);
  if (g_timer > 0)
    Log("simulation frequency = " NUMBERIC_FMT " inst/s", g_nr_guest_inst * 1000000 / g_timer);
  else
    Log("Finish running in less than 1 us and can not calculate the simulation frequency");
}

void assert_fail_msg()
{
  IFDEF(CONFIG_ITRACE, trace_output_ringbuf(&iringbuf));
  isa_reg_display();
  statistic();
}

/* Simulate how the CPU works. */
void cpu_exec(uint64_t n)
{
  g_print_step = (n < MAX_INST_TO_PRINT);
  switch (nemu_state.state)
  {
  case NEMU_END:
  case NEMU_ABORT:
  case NEMU_QUIT:
    printf("Program execution has ended. To restart the program, exit NEMU and run again.\n");
    return;
  default:
    nemu_state.state = NEMU_RUNNING;
  }

  uint64_t timer_start = get_time();

  execute(n);

  uint64_t timer_end = get_time();
  g_timer += timer_end - timer_start;

  switch (nemu_state.state)
  {
  case NEMU_RUNNING:
    nemu_state.state = NEMU_STOP;
    break;

  case NEMU_END:
  case NEMU_ABORT:
    IFDEF(CONFIG_ITRACE, trace_output_ringbuf(&iringbuf));
    // 出错时(ABORT 或 BAD TRAP)再顺带打印出错位置之后未执行的几条指令
    IFDEF(CONFIG_ITRACE, if (nemu_state.state == NEMU_ABORT || nemu_state.halt_ret != 0) { peek_next_insts(nemu_state.halt_pc + 4, 4); });
    Log("nemu: %s at pc = " FMT_WORD,
        (nemu_state.state == NEMU_ABORT ? ANSI_FMT("ABORT", ANSI_FG_RED) : (nemu_state.halt_ret == 0 ? ANSI_FMT("HIT GOOD TRAP", ANSI_FG_GREEN) : ANSI_FMT("HIT BAD TRAP", ANSI_FG_RED))),
        nemu_state.halt_pc);
    // fall through
  case NEMU_QUIT:
    statistic();
  }
}
