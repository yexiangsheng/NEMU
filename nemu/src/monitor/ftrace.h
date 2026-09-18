#ifndef __FTRACE_H__
#define __FTRACE_H__

#include <common.h>

#ifdef CONFIG_FTRACE
void init_ftrace(const char *elf_file);
void ftrace_step(struct Decode *s);
#endif

#endif
