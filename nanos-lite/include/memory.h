#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <common.h>
#include <elf.h>

#ifndef PGSIZE
#define PGSIZE 4096
#endif

#define PG_ALIGN __attribute((aligned(PGSIZE)))

void *new_page(size_t);
void set_zero(Elf32_Word begin, Elf32_Word end);
void *pg_alloc(size_t);

#endif
