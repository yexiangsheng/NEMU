HAS_NAVY = 1
RAMDISK_FILE = build/ramdisk.img


NAME = nanos-lite
SRCS = $(shell find -L ./src/ -name "*.c" -o -name "*.cpp" -o -name "*.S")

# strace 开关: 在 make 命令行加 STRACE=1 打开系统调用跟踪, 默认关闭
# 例: make ARCH=riscv32-nemu STRACE=1 run
ifeq ($(strip $(STRACE)),1)
CFLAGS += -DCONFIG_STRACE
endif

# 彩色日志开关: 终端看日志保持彩色, 重定向到文件时用 COLOR=0 去掉 ANSI 颜色码
# 例: make ARCH=riscv32-nemu COLOR=0 run > run-log.txt
COLOR = 1
ifeq ($(strip $(COLOR)),0)
CFLAGS += -DCONFIG_NO_LOG_COLOR
endif

include $(AM_HOME)/Makefile

ifeq ($(ARCH),native)
ISA = am_native
else
INC_PATH += include $(NAVY_HOME)/libs/libc/include
endif

./src/resources.S: $(RAMDISK_FILE)
	@touch $@

ifeq ($(HAS_NAVY),)
files = $(RAMDISK_FILE) src/files.h src/syscall.h
# create an empty file if it does not exist
$(foreach f,$(files),$(if $(wildcard $f),, $(shell touch $f)))
else

ifeq ($(wildcard $(NAVY_HOME)/libs/libos/src/syscall.h),)
  $(error $$NAVY_HOME must be a Navy-apps repo)
endif

update:
	$(MAKE) -s -C $(NAVY_HOME) ISA=$(ISA) ramdisk
	@ln -sf $(NAVY_HOME)/build/ramdisk.img $(RAMDISK_FILE)
	@ln -sf $(NAVY_HOME)/build/ramdisk.h src/files.h
	@ln -sf $(NAVY_HOME)/libs/libos/src/syscall.h src/syscall.h

.PHONY: update
endif
