#***************************************************************************************
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
# Log.mk —— 可复用的 Makefile 调试日志工具箱
#
# 任何 Makefile 只要 include 本文件即可获得四样东西:
#   1) 五要素日志宏: $(call log_debug/info/warn/error, 消息)
#        输出 = 来源($(warning) 自动加 文件:行号:) + 时间戳 + 级别 + 上下文 + 信息
#   2) 构建留痕目标: build-log —— 每次构建自动生成 trace/dryrun/database
#        三份快照 x (.txt 纯净 + .ansi 彩色) 两种格式
#   3) 日志渲染器:   logview  —— 构建时自动写出 .ansi 彩色版(cat 即渲染,
#        VS Code 终端里 文件:行号 可 Ctrl+点击 跳转); 查看时也可 make logview
#        对 .txt 即时上色, 原始 .txt 始终保持无颜色码的纯净格式
#   4) 演示目标:     logdemo  —— recipe 阶段日志样例
#
# 用法(其它 Makefile 调用):
#   include <本文件路径>/nemu/scripts/Log.mk
#   LOG_GOAL := all          # 按需覆盖参数
#   my-target: build-log     # 把留痕挂进自己的构建链
#   $(call log_info, hello)  # 打日志
#
# 可调参数(均 ?= 定义, include 方可覆盖):
#   LOG_DIR   快照输出目录         默认 log
#   LOG_GOAL  build-log 追踪的目标 默认 run
#   LOG_CTX   日志的"上下文"字段   默认 goal=<命令行目标>
#   DBG       级别开关: 1=INFO及以上, 2=+DEBUG; WARN/ERROR 永远输出
#
# 环境变量 LOG_NESTED=1: build-log 的防递归标记(子 make 自动设置),
# 手动设置即为逃生口 —— 临时关闭留痕.
#==============================================================================

LOG_DIR  ?= log
LOG_GOAL ?= run

#------------------------------------------------------------------------------
# 五要素日志宏: 时间戳 + 级别 + 来源 + 上下文 + 信息
#
#   时间戳: LOG_TS 用递归变量(=)定义, 每次展开重新执行 date => 每行都新鲜;
#           若写成 := 会在定义时定格, 之后所有日志共用同一时间(经典坑).
#   级别:   DBG=1 开 INFO 及以上; DBG=2 连 DEBUG 一起; ERROR 永远生效.
#   来源:   底层用 $(warning) 输出, make 自动在行首加 "文件:行号:" 前缀,
#           白赚一个精确定位(自己拼只能拿 MAKEFILE_LIST, 拿不到行号).
#   上下文: LOG_CTX, 默认给 goal, 项目可在 include 后用 = 重新定义加料.
#   信息:   $(1). 注意宏体不做嵌套 call —— $(call) 按逗号切参数,
#           消息里含逗号(如内存区间 [0x80000000, 0x87ffffff])会被截断.
#------------------------------------------------------------------------------
LOG_TS   = $(shell date '+%F %T.%3N')
LOG_CTX ?= goal=$(if $(MAKECMDGOALS),$(MAKECMDGOALS),<default>)

log_debug = $(if $(filter 2,$(DBG)),$(warning [$(LOG_TS)] [DEBUG] [$(LOG_CTX)] $(1)))
log_info  = $(if $(DBG),$(warning [$(LOG_TS)] [INFO ] [$(LOG_CTX)] $(1)))
log_warn  =          $(warning [$(LOG_TS)] [WARN ] [$(LOG_CTX)] $(1))
log_error =          $(error  [$(LOG_TS)] [ERROR] [$(LOG_CTX)] $(1)))

#------------------------------------------------------------------------------
# 构建留痕: 每次构建自动生成三份调试快照 x 两种格式, 时间戳 = 构建当刻
#   trace-<ts>    : make -n --trace $(LOG_GOAL) —— 本次构建的决策过程
#   dryrun-<ts>   : make -nB $(LOG_GOAL)         —— 全量重建的完整命令清单
#   database-<ts> : make -np $(LOG_GOAL)         —— 完整数据库(全部变量/规则,
#                   含内置) + 干跑命令清单, 即 -p 与 -n 的合体快照
#   每种两份: .txt 原始纯净(grep/工具友好) + .ansi 彩色版(cat 即渲染).
#   同一流用 tee 分叉: 一份原样落盘, 一份过 logview render 染色后落盘,
#   "渲染"完全发生在构建时刻, 无需手动敲任何查看命令.
#
# 挂载方式: 在你项目的入口目标定义之前追加依赖, 如  run-env: build-log,
#   串行 make 从左到右更新依赖 => 先留痕后构建, trace 里才有本次的编译动作.
# 防无限递归: 含 $(MAKE) 的 recipe 行即使父 make 用 -n 也会真的执行,
#   而它触发的子 make 又会走到本目标... 所以用 LOG_NESTED 做嵌套标记:
#   环境变量会被 make 自动导入为同名变量, 子 make 解析到 ifeq 时直接跳过.
#------------------------------------------------------------------------------
build-log:
ifeq ($(LOG_NESTED),)
	@mkdir -p $(LOG_DIR); \
	ts=$$(date +%Y%m%d-%H%M%S); \
	banner() { printf '# ==== build-log snapshot ====\n# kind : %s\n# goal : %s\n# time : %s\n# host : %s\n\n' \
	  "$$1" "$(LOG_GOAL)" "$$(date '+%F %T')" "$$(hostname)"; }; \
	echo "build-log: -> $(LOG_DIR)/trace-$$ts.txt + .ansi (make -n --trace $(LOG_GOAL))"; \
	{ banner 'trace  : make -n --trace'; LOG_NESTED=1 $(MAKE) -n --trace $(LOG_GOAL) 2>&1; } \
	  | tee $(LOG_DIR)/trace-$$ts.txt | $(LOGVIEW) render > $(LOG_DIR)/trace-$$ts.ansi; \
	echo "build-log: -> $(LOG_DIR)/dryrun-$$ts.txt + .ansi (make -nB $(LOG_GOAL))"; \
	{ banner 'dryrun : make -nB'; LOG_NESTED=1 $(MAKE) -nB $(LOG_GOAL) 2>&1; } \
	  | tee $(LOG_DIR)/dryrun-$$ts.txt | $(LOGVIEW) render > $(LOG_DIR)/dryrun-$$ts.ansi; \
	echo "build-log: -> $(LOG_DIR)/database-$$ts.txt + .ansi (make -np $(LOG_GOAL))"; \
	{ banner 'database : make -np'; LOG_NESTED=1 $(MAKE) -np $(LOG_GOAL) 2>&1; } \
	  | tee $(LOG_DIR)/database-$$ts.txt | $(LOGVIEW) render > $(LOG_DIR)/database-$$ts.ansi
else
	@echo "build-log: LOG_NESTED is set, skip (nested or manually disabled)"
endif

#------------------------------------------------------------------------------
# 查看时刻的即时渲染(可选): .ansi 彩色版已在构建时刻自动生成, 直接 cat 即可;
# logview 用于对 .txt 原始版按需上色查看.
#   make logview               查看最新 trace 快照
#   make logview F=<文件>      查看指定快照(如 dryrun-xxx.txt)
# 脚本随 Log.mk 自定位(用 := 立即取 MAKEFILE_LIST 末词 => 本文件所在目录),
# 其它项目 include 本文件后无需配置同样可用.
#------------------------------------------------------------------------------
LOGMK_DIR := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
LOGVIEW   ?= $(LOGMK_DIR)/logview.sh
LATEST    = $(shell ls -t $(LOG_DIR)/trace-*.txt 2>/dev/null | head -1)

logview:
	@if [ -n "$(F)" ]; then $(LOGVIEW) view $(F); \
	elif [ -n "$(LATEST)" ]; then $(LOGVIEW) view $(LATEST); \
	else echo "logview: no snapshot in $(LOG_DIR)/, run 'make build-log' first"; fi

#------------------------------------------------------------------------------
# recipe 阶段的日志演示: 上面的 $(warning) 都在解析阶段输出;
# 构建命令执行期间的日志要写在 recipe 里, 注意展开者差异:
#   $(LOG_TS)  由 make 展开(展开 recipe 时执行一次 date);
#   $@         也是 make 的自动变量, = 正在构建的目标 —— 最有价值的上下文.
#              (写成 $$@ 会交给 shell 展开成 shell 位置参数, 在 recipe 里为空)
#------------------------------------------------------------------------------
logdemo:
	@echo "[$(LOG_TS)] [INFO ] [recipe] [target=$@] goal=$(if $(MAKECMDGOALS),$(MAKECMDGOALS),<default>)"

.PHONY: build-log logview logdemo
