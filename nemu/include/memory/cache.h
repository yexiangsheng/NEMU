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

#ifndef __MEMORY_CACHE_H__
#define __MEMORY_CACHE_H__

#include <common.h>

#ifdef CONFIG_CACHE

/* Initialize the L1/L2 cache. Call after init_mem() and before loading the image. */
void init_cache(void);

/* Read/write through the cache hierarchy. len is one of 1, 2, 4 and addr may
 * be unaligned (an access can span two cache lines). MMIO never comes here. */
word_t cache_read(paddr_t addr, int len);
void cache_write(paddr_t addr, int len, word_t data);

/* Print cache statistics. Called from statistic() in cpu-exec.c. */
void cache_statistic(void);

#endif // CONFIG_CACHE

#endif
