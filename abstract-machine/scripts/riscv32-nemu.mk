include $(AM_HOME)/scripts/isa/riscv.mk
include $(AM_HOME)/scripts/platform/nemu.mk
CFLAGS  += -DISA_H=\"riscv/riscv.h\" 
NEMU_CONFIG ?= $(NEMU_HOME)/.config
ifneq ($(shell grep -s "^CONFIG_ETRACE=y" $(NEMU_CONFIG)),)
CFLAGS += -UCONFIG_ETRACE -DCONFIG_ETRACE=1
endif
COMMON_CFLAGS += -march=rv32im_zicsr -mabi=ilp32   # overwrite
LDFLAGS       += -melf32lriscv                     # overwrite

AM_SRCS += riscv/nemu/start.S \
           riscv/nemu/cte.c \
           riscv/nemu/trap.S \
           riscv/nemu/vme.c
