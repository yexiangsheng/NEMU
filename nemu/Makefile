#**************************************************************************************
# Copyright (c) 2014-2024 Zihao Yu, Nanjing University
#
# NEMU is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
#
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
#
# See the Mulan PSL v2 for more details.
#**************************************************************************************/

#==============================================================================
# NEMU 主 Makefile —— 一套 Make 函数的"组合拳", 典型链路:
#
#   shell+find 收集文件 -> wildcard 检查文件存在 -> if 做条件展开
#        -> call+patsubst 处理配置值 -> error 兜底报错
#
# 按本文件出现顺序:
#   1) wildcard + error  : 入口合法性检查
#   2) call + patsubst   : 去掉 menuconfig 配置值自带的引号
#   3) shell + find      : 递归收集 filelist.mk 和 .c 源文件
#   4) if                : 按 menuconfig 开关条件展开编译选项
#
# 注意区分"指令"与"函数":
#   ifeq/ifneq/ifdef 是指令, 控制 make 读取哪些行;
#   $(if) 是函数, 控制哪段文本被展开. 两者作用层面不同.
#==============================================================================

#------------------------------------------------------------------------------
# [1] 合法性检查: wildcard 检查文件存在 + error 兜底报错
#
# $(wildcard $(NEMU_HOME)/src/nemu-main.c)
#   wildcard 把模式展开为"实际存在的文件列表", 文件不存在时返回空串;
#   这里正是利用"不存在则返回空串"来探测文件是否存在.
# ifeq (...,)
#   展开结果为空串 => NEMU_HOME 下缺少 src/nemu-main.c, 不是合法的 NEMU 仓库.
# $(error ...)
#   在展开处向 stderr 打印信息并立即终止 make (warning 只警告不终止),
#   作为构建流程的第一道防线.
#------------------------------------------------------------------------------
ifeq ($(wildcard $(NEMU_HOME)/src/nemu-main.c),)
	$(error NEMU_HOME=$(NEMU_HOME) is not a NEMU repo)
endif

#------------------------------------------------------------------------------
# 载入 menuconfig 生成的配置文件.
# auto.conf 内容形如 CONFIG_ISA="riscv32" —— 配置值自带双引号.
# 前置 '-' 表示文件不存在时也不报错(首次构建前尚未执行过 menuconfig).
#------------------------------------------------------------------------------
-include $(NEMU_HOME)/include/config/auto.conf
-include $(NEMU_HOME)/include/config/auto.conf.cmd

#------------------------------------------------------------------------------
# [2] 配置值处理: call + patsubst 去掉引号
#
# remove_quote 是自定义"函数"(宏), $(1) 为形参:
#   patsubst(模式替换) 把匹配 "%"(前后带引号) 的词替换为 %(仅内容),
#   即剥掉双引号、保留里面的值.
# 调用方式: $(call remove_quote,$(CONFIG_ISA))
#   call 先把宏体中的 $(1) 替换为实参, 再整体展开, 相当于一次函数调用.
#------------------------------------------------------------------------------
remove_quote = $(patsubst "%",%,$(1))

#------------------------------------------------------------------------------
# 从 menuconfig 提取基础变量:
#   ?= 仅在变量未定义时赋值(允许命令行覆盖);
#   若不去引号, NAME 会拼出 "riscv32"-nemu-interpreter 这样的脏值.
#------------------------------------------------------------------------------
GUEST_ISA ?= $(call remove_quote,$(CONFIG_ISA))
ENGINE ?= $(call remove_quote,$(CONFIG_ENGINE))
NAME    = $(GUEST_ISA)-nemu-$(ENGINE)

#------------------------------------------------------------------------------
# [3] 源文件收集: shell + find
#
# $(shell find -L ./src -name "filelist.mk")
#   shell 把命令交给 /bin/sh 执行, stdout 作为返回值(换行替换为空格);
#   它在 make 解析阶段执行, 而不是执行 recipe 时.
# 各子目录通过自己的 filelist.mk 登记源文件/目录, include 之后合并进全局
# 变量 SRCS-y / DIRS-y / SRCS-BLACKLIST / DIRS-BLACKLIST,
# 因此主 Makefile 不必逐个手写文件清单.
#------------------------------------------------------------------------------
FILELIST_MK = $(shell find -L ./src -name "filelist.mk")
include $(FILELIST_MK)

#------------------------------------------------------------------------------
# 黑名单过滤: if 守卫 + shell/find 收集 + filter-out 剔除
#
# $(if $(DIRS-y), $(shell ...),)
#   if 函数: 第 1 参数非空 => 展开第 2 参数, 否则展开第 3 参数(此处为空).
#   守卫作用: 变量为空时不执行 find, 避免 find 收到空参数而报错.
# filter-out 从 SRCS-y 中剔除黑名单里的文件, 得到最终参与编译的源文件集合.
#------------------------------------------------------------------------------
DIRS-BLACKLIST-y += $(DIRS-BLACKLIST)
SRCS-BLACKLIST-y += $(if $(DIRS-BLACKLIST-y), $(shell find -L $(DIRS-BLACKLIST-y) -name "*.c"),)
SRCS-BLACKLIST-y += $(SRCS-BLACKLIST)
SRCS-y += $(if $(DIRS-y), $(shell find -L $(DIRS-y) -name "*.c"),)
SRCS = $(filter-out $(SRCS-BLACKLIST-y),$(SRCS-y))

#------------------------------------------------------------------------------
# [4] 编译选项组装: call/patsubst 去引号 + if 条件附加
#
# ifneq(指令) 控制"哪些行被读取": 仅当 menuconfig 指定了编译器时才覆盖 CC.
# 之后每个 $(if ...) 对应一个开关项: 勾选(非空)才展开出相应 flag.
# ITRACE_COND 未配置时兜底为 true.
#------------------------------------------------------------------------------
ifneq ($(CONFIG_CC),)
	CC = $(call remove_quote,$(CONFIG_CC))
endif
CFLAGS_BUILD += $(call remove_quote,$(CONFIG_CC_OPT))
CFLAGS_BUILD += $(if $(CONFIG_CC_LTO),-flto,)
CFLAGS_BUILD += $(if $(CONFIG_CC_DEBUG),-Og -ggdb3,)
CFLAGS_BUILD += $(if $(CONFIG_CC_ASAN),-fsanitize=address,)
CFLAGS_TRACE += -DITRACE_COND=$(if $(CONFIG_ITRACE_COND),$(call remove_quote,$(CONFIG_ITRACE_COND)),true)
CFLAGS  += $(CFLAGS_BUILD) $(CFLAGS_TRACE) -D__GUEST_ISA__=$(GUEST_ISA)
LDFLAGS += $(CFLAGS_BUILD)

#------------------------------------------------------------------------------
# 引入 menuconfig 相关规则(配置界面本身).
#------------------------------------------------------------------------------
include $(NEMU_HOME)/scripts/config.mk

#------------------------------------------------------------------------------
# 按构建目标分流(此处 ifdef 是指令, 控制 include 哪个文件):
#   定义了 CONFIG_TARGET_AM : 构建"跑在 AM 之上"的 NEMU, 复用 AM 的构建规则;
#   否则(默认)            : 构建原生(native)版 NEMU.
#------------------------------------------------------------------------------
ifdef CONFIG_TARGET_AM
	include $(AM_HOME)/Makefile
	LINKAGE += $(ARCHIVES)
else
	include $(NEMU_HOME)/scripts/native.mk
endif
