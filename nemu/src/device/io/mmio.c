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

#include <device/map.h>
#include <memory/paddr.h>
#include <isa.h>

#define NR_MAP 16

static IOMap maps[NR_MAP] = {};
static int nr_map = 0;

// 遍历所有maps，获取并返回
static IOMap *fetch_mmio_map(paddr_t addr)
{
  //  printf("Addr is " FMT_PADDR "\n", addr);
  int mapid = find_mapid_by_addr(maps, nr_map, addr);
  return (mapid == -1 ? NULL : &maps[mapid]);
}

static void report_mmio_overlap(const char *name1, paddr_t l1, paddr_t r1,
                                const char *name2, paddr_t l2, paddr_t r2)
{
  panic("MMIO region %s@[" FMT_PADDR ", " FMT_PADDR "] is overlapped "
        "with %s@[" FMT_PADDR ", " FMT_PADDR "]",
        name1, l1, r1, name2, l2, r2);
}

/* device interface */
// 添加map到注册表中
void add_mmio_map(const char *name, paddr_t addr, void *space, uint32_t len, io_callback_t callback)
{
  assert(nr_map < NR_MAP);
  paddr_t left = addr, right = addr + len - 1;
  // check this device's io space whether overlap with pmem and other devices' io space
  if (in_pmem(left) || in_pmem(right))
  {
    report_mmio_overlap(name, left, right, "pmem", PMEM_LEFT, PMEM_RIGHT);
  }
  for (int i = 0; i < nr_map; i++)
  {
    if (left <= maps[i].high && right >= maps[i].low)
    {
      report_mmio_overlap(name, left, right, maps[i].name, maps[i].low, maps[i].high);
    }
  }

  // if this io map pass check, sign up this map into array
  maps[nr_map] = (IOMap){.name = name, .low = addr, .high = addr + len - 1, .space = space, .callback = callback};
  Log("Add mmio map '%s' at [" FMT_PADDR ", " FMT_PADDR "]",
      maps[nr_map].name, maps[nr_map].low, maps[nr_map].high);

  nr_map++;
}

#ifdef CONFIG_DTRACE
static void dtrace_log(const char *op, const char *name, paddr_t addr, int len, word_t data)
{
  if (addr >= CONFIG_DTRACE_START && addr <= CONFIG_DTRACE_END)
  {
    log_write("dtrace: %s map_name= %s addr=" FMT_PADDR " len=%d data=" FMT_WORD " pc=" FMT_WORD "\n",
              op, name, addr, len, data, cpu.pc);
  }
}
#endif

/* bus interface */
// 用户访问device时候不用知道设备空间·
word_t mmio_read(paddr_t addr, int len)
{
  IOMap *map = fetch_mmio_map(addr);
  word_t ret = map_read(addr, len, map);
  IFDEF(CONFIG_MTRACE, dtrace_log("read", map->name, addr, len, ret));
  return ret;
}

void mmio_write(paddr_t addr, int len, word_t data)
{
  IOMap *map = fetch_mmio_map(addr);
  map_write(addr, len, data, map);
  IFDEF(CONFIG_MTRACE, dtrace_log("write", map->name, addr, len, data));
}
