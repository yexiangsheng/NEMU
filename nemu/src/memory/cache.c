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

#include <memory/cache.h>

#ifdef CONFIG_CACHE

#include <memory/paddr.h>
#include <isa.h>

/* Cache geometry: 8B lines, L1 64B (8 lines), L2 256B (32 lines), inclusive.
 * The associativity comes from Kconfig: WAYS = 1 gives a direct-mapped cache,
 * WAYS = line number gives a fully associative one. */
#define CACHE_LINE_SIZE    8
#define CACHE_L1_SIZE      64
#define CACHE_L2_SIZE      256
#define CACHE_L1_LINE_NUM  (CACHE_L1_SIZE / CACHE_LINE_SIZE)
#define CACHE_L2_LINE_NUM  (CACHE_L2_SIZE / CACHE_LINE_SIZE)

#define CACHE_L1_WAYS      CONFIG_CACHE_L1_WAYS
#define CACHE_L2_WAYS      CONFIG_CACHE_L2_WAYS
#define CACHE_L1_SETS      (CACHE_L1_LINE_NUM / CACHE_L1_WAYS)
#define CACHE_L2_SETS      (CACHE_L2_LINE_NUM / CACHE_L2_WAYS)

/* log2(CACHE_LINE_SIZE) */
#define OFFSET_BITS        3

_Static_assert(CACHE_L1_LINE_NUM % CACHE_L1_WAYS == 0, "CONFIG_CACHE_L1_WAYS must divide 8");
_Static_assert(CACHE_L2_LINE_NUM % CACHE_L2_WAYS == 0, "CONFIG_CACHE_L2_WAYS must divide 32");
_Static_assert(CACHE_L1_SETS > 0 && (CACHE_L1_SETS & (CACHE_L1_SETS - 1)) == 0, "L1 sets must be a power of 2");
_Static_assert(CACHE_L2_SETS > 0 && (CACHE_L2_SETS & (CACHE_L2_SETS - 1)) == 0, "L2 sets must be a power of 2");

typedef struct {
  bool valid;
  bool dirty;
  uint32_t tag;                    // addr >> (OFFSET_BITS + index_bits)
  uint64_t stamp;                  // LRU timestamp, 0 = never used
  uint8_t data[CACHE_LINE_SIZE];   // byte array keeps partial merges endian-safe
} cache_line_t;

typedef struct {
  const char *name;                // "L1" / "L2"
  uint32_t sets, ways, index_bits; // index_bits differs between the two levels
  uint64_t tick;                   // LRU clock
  struct {
    uint64_t access, hit, miss, evict, writeback, back_inval;
  } stat;
  cache_line_t *lines;             // set i, way w -> &lines[i * ways + w]
} cache_t;

static cache_line_t l1_lines[CACHE_L1_LINE_NUM];
static cache_line_t l2_lines[CACHE_L2_LINE_NUM];
static cache_t L1, L2;

static inline uint32_t set_index(cache_t *c, paddr_t addr) {
  return (addr >> OFFSET_BITS) & (c->sets - 1);
}

static inline uint32_t set_tag(cache_t *c, paddr_t addr) {
  return addr >> (OFFSET_BITS + c->index_bits);
}

/* Reconstruct the line base address (8B aligned) from a set index and a tag */
static inline paddr_t line_addr(cache_t *c, uint32_t idx, uint32_t tag) {
  return ((paddr_t)tag << (OFFSET_BITS + c->index_bits)) | ((paddr_t)idx << OFFSET_BITS);
}

static inline cache_line_t *set_first(cache_t *c, uint32_t idx) {
  return &c->lines[idx * c->ways];
}

/* Probe one cache for the line containing la. Statistics and the LRU stamp
 * are updated here, so only real hierarchy lookups may call this. */
static cache_line_t *cache_lookup(cache_t *c, paddr_t la) {
  Assert((la & (CACHE_LINE_SIZE - 1)) == 0, "unaligned line address " FMT_PADDR, la);
  uint32_t idx = set_index(c, la);
  uint32_t tag = set_tag(c, la);
  c->stat.access++;
  cache_line_t *first = set_first(c, idx);
  for (uint32_t w = 0; w < c->ways; w++) {
    if (first[w].valid && first[w].tag == tag) {
      c->stat.hit++;
      first[w].stamp = ++c->tick;
      return &first[w];
    }
  }
  c->stat.miss++;
  return NULL;
}

/* Probe one cache without touching statistics. Used by back-invalidation
 * and L1 write-back, which must look a line up in the *other* level with
 * that level's own set/tag mapping (the two levels have different sets). */
static cache_line_t *find_line(cache_t *c, paddr_t la) {
  uint32_t idx = set_index(c, la);
  uint32_t tag = set_tag(c, la);
  cache_line_t *first = set_first(c, idx);
  for (uint32_t w = 0; w < c->ways; w++) {
    if (first[w].valid && first[w].tag == tag) return &first[w];
  }
  return NULL;
}

/* Pick a victim slot in a set: an invalid way first, else the LRU way */
static cache_line_t *pick_victim(cache_t *c, uint32_t idx) {
  cache_line_t *first = set_first(c, idx);
  cache_line_t *victim = &first[0];
  for (uint32_t w = 0; w < c->ways; w++) {
    if (!first[w].valid) return &first[w];
    if (first[w].stamp < victim->stamp) victim = &first[w];
  }
  return victim;
}

/* Fill a line into L2 from pmem. An L2 victim is written back to pmem, and
 * its L1 copy (inclusive property) is back-invalidated *before* that, so a
 * dirty L1 copy can be merged into the victim instead of being lost. */
static cache_line_t *l2_fill(paddr_t la) {
  uint32_t idx = set_index(&L2, la);
  cache_line_t *v = pick_victim(&L2, idx);
  if (v->valid) {
    paddr_t va = line_addr(&L2, idx, v->tag);
    cache_line_t *l1l = find_line(&L1, va);
    if (l1l != NULL) {
      if (l1l->dirty) {
        memcpy(v->data, l1l->data, CACHE_LINE_SIZE);
        v->dirty = true;
      }
      l1l->valid = false;
      L2.stat.back_inval++;
    }
    if (v->dirty) {
      memcpy(guest_to_host(va), v->data, CACHE_LINE_SIZE);
      L2.stat.writeback++;
    }
    L2.stat.evict++;
  }
  v->tag = set_tag(&L2, la);
  v->valid = true;
  v->dirty = false;
  memcpy(v->data, guest_to_host(la), CACHE_LINE_SIZE);
  v->stamp = ++L2.tick;
  return v;
}

/* Fill a line into L1 from L2; the caller must ensure the line is in L2.
 * A dirty L1 victim is written back into its L2 copy (which must exist by
 * inclusion), never directly to pmem. */
static cache_line_t *l1_fill(paddr_t la) {
  uint32_t idx = set_index(&L1, la);
  cache_line_t *v = pick_victim(&L1, idx);
  if (v->valid) {
    if (v->dirty) {
      paddr_t wa = line_addr(&L1, idx, v->tag);
      cache_line_t *l2l = find_line(&L2, wa);
      Assert(l2l != NULL, "L1 victim " FMT_PADDR " has no L2 copy", wa);
      memcpy(l2l->data, v->data, CACHE_LINE_SIZE);
      l2l->dirty = true;
      L1.stat.writeback++;
    }
    L1.stat.evict++;
  }
  cache_line_t *l2l = find_line(&L2, la);
  Assert(l2l != NULL, "line " FMT_PADDR " is not in L2", la);
  v->tag = set_tag(&L1, la);
  v->valid = true;
  v->dirty = false;
  memcpy(v->data, l2l->data, CACHE_LINE_SIZE);
  v->stamp = ++L1.tick;
  return v;
}

/* Make la resident in L1 and return its line. Shared by reads and writes
 * (write-allocate). The L2 fill runs before the L1 fill on purpose: the
 * back-invalidation inside it frees an L1 slot that pick_victim can then
 * reuse, and it can never evict the line that is being filled. */
static cache_line_t *obtain_line(paddr_t la) {
  cache_line_t *l = cache_lookup(&L1, la);
  if (l != NULL) return l;
  if (cache_lookup(&L2, la) == NULL) {
    l2_fill(la);
  }
  return l1_fill(la);
}

void init_cache(void) {
  memset(l1_lines, 0, sizeof(l1_lines));
  memset(l2_lines, 0, sizeof(l2_lines));
  L1 = (cache_t){ .name = "L1", .sets = CACHE_L1_SETS, .ways = CACHE_L1_WAYS,
                  .index_bits = __builtin_ctz(CACHE_L1_SETS), .lines = l1_lines };
  L2 = (cache_t){ .name = "L2", .sets = CACHE_L2_SETS, .ways = CACHE_L2_WAYS,
                  .index_bits = __builtin_ctz(CACHE_L2_SETS), .lines = l2_lines };
  Log("cache: line size = %dB, L1 = %dB (%u sets x %u ways), L2 = %dB (%u sets x %u ways),"
      " write-back + write-allocate, inclusive, PIPT, MMIO uncached",
      CACHE_LINE_SIZE, CACHE_L1_SIZE, CACHE_L1_SETS, CACHE_L1_WAYS,
      CACHE_L2_SIZE, CACHE_L2_SETS, CACHE_L2_WAYS);
}

word_t cache_read(paddr_t addr, int len) {
  Assert(len == 1 || len == 2 || len == 4, "invalid read length %d at pc = " FMT_WORD, len, cpu.pc);
  word_t ret = 0;
  paddr_t la = addr & ~(paddr_t)(CACHE_LINE_SIZE - 1);
  uint32_t off = addr & (CACHE_LINE_SIZE - 1);
  uint32_t first = CACHE_LINE_SIZE - off;
  if (first > (uint32_t)len) first = len;
  cache_line_t *l = obtain_line(la);
  memcpy(&ret, &l->data[off], first);
  if (first < (uint32_t)len) {
    // the access spans two lines, e.g. a 4B read at addr % 8 == 6
    l = obtain_line(la + CACHE_LINE_SIZE);
    memcpy((uint8_t *)&ret + first, &l->data[0], len - first);
  }
  return ret;
}

void cache_write(paddr_t addr, int len, word_t data) {
  Assert(len == 1 || len == 2 || len == 4, "invalid write length %d at pc = " FMT_WORD, len, cpu.pc);
  uint32_t done = 0;
  while (done < (uint32_t)len) {
    paddr_t la = (addr + done) & ~(paddr_t)(CACHE_LINE_SIZE - 1);
    uint32_t loff = (addr + done) & (CACHE_LINE_SIZE - 1);
    uint32_t chunk = CACHE_LINE_SIZE - loff;
    if (chunk > (uint32_t)len - done) chunk = len - done;
    cache_line_t *l = obtain_line(la);
    memcpy(&l->data[loff], (uint8_t *)&data + done, chunk);
    l->dirty = true;
    done += chunk;
  }
}

void cache_statistic(void) {
  uint64_t hr1 = L1.stat.access ? L1.stat.hit * 1000 / L1.stat.access : 0;
  uint64_t hr2 = L2.stat.access ? L2.stat.hit * 1000 / L2.stat.access : 0;
  Log("L1 cache: access = %'" PRIu64 ", hit = %'" PRIu64 " (%" PRIu64 ".%" PRIu64 "%%), miss = %'" PRIu64
      ", eviction = %'" PRIu64 ", writeback = %'" PRIu64,
      L1.stat.access, L1.stat.hit, hr1 / 10, hr1 % 10, L1.stat.miss, L1.stat.evict, L1.stat.writeback);
  Log("L2 cache: access = %'" PRIu64 ", hit = %'" PRIu64 " (%" PRIu64 ".%" PRIu64 "%%), miss = %'" PRIu64
      ", eviction = %'" PRIu64 ", writeback = %'" PRIu64 ", back-invalidate = %'" PRIu64,
      L2.stat.access, L2.stat.hit, hr2 / 10, hr2 % 10, L2.stat.miss, L2.stat.evict,
      L2.stat.writeback, L2.stat.back_inval);
}

#endif // CONFIG_CACHE
