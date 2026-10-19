# NEMU — RISC-V 32 全系统模拟器

从零实现的 RISC-V 32 指令集模拟器与简易计算机系统:指令执行、中断/异常、系统调用、文件系统、多道程序、双级 Cache,直至在其上完整运行《仙剑奇侠传》。

基于 [NJU ICS PA 2024](https://nju-projectn.github.io/ics-pa-gitbook/ics2024/) 框架,核心实现由本人独立完成。

![C](https://img.shields.io/badge/C-11-A8B9CC?style=flat-square&logo=c&logoColor=white)
![RISC-V](https://img.shields.io/badge/ISA-RISC--V%2032-293272?style=flat-square&logo=riscv&logoColor=blue)
![Linux](https://img.shields.io/badge/platform-Linux%20%2F%20WSL2-FCC624?style=flat-square&logo=linux&logoColor=black)
![Make](https://img.shields.io/badge/build-Make-427819?style=flat-square)

---

## 演示

<!-- TODO: 录一张《仙剑奇侠传》在 NEMU 上运行的 GIF,存为 screenshots/pal.gif -->
![PAL on NEMU](screenshots/pal.gif)

## 系统结构

```
┌─────────────────────────────────────────────┐
│            《仙剑奇侠传》(PAL)               │  Navy 应用
├─────────────────────────────────────────────┤
│  Navy-apps:libminiSDL / Newlib / NTerm      │  应用运行时
├─────────────────────────────────────────────┤
│  Nanos-lite:中断/异常 · 系统调用 · 文件系统  │  简易内核
├─────────────────────────────────────────────┤
│  Abstract-Machine:TRM / IOE / CTE           │  硬件抽象层
├─────────────────────────────────────────────┤
│  NEMU:RV32 取指·译码·执行 · 双级 Cache · sdb │  模拟器核心
└─────────────────────────────────────────────┘
```

## 实现内容(对应分支)

| 阶段 | 分支 | 实现内容 |
|------|------|----------|
| PA1 | [`pa1`](../../tree/pa1) | sdb 调试器:单步执行、断点、监视点、表达式求值(词法分析 + 递归下降) |
| PA2 | [`pa2`](../../tree/pa2) | RV32 指令系统(算术、访存、分支跳转),AM 环境与 I/O 扩展 |
| PA3 | [`pa3`](../../tree/pa3) | CTE 中断/异常、系统调用、简易文件系统、批处理与多道程序调度 |
| PA4 | [`pa4`](../../tree/pa4) | 双级 Cache(L1-I / L1-D),命中率统计与性能分析 |

> 各分支保留了完整的开发提交历史;本仓库默认分支 `pa4` 为最终状态。

## 构建与运行

```bash
# 编译并运行模拟器(需要 riscv32 工具链与 SDL2)
make -C nemu menuconfig    # 架构选择 riscv32
make -C nemu               # 编译
make -C nemu run           # 运行

# sdb 调试器常用命令
#   c / si [N] / info r / x [N] ADDR / w EXPR / b ADDR / p EXPR
```

## 开发环境

- Linux(WSL2)· GCC · Make(kconfig + menuconfig 配置系统)
- Git:按 pa1 → pa4 分支推进,提交历史完整保留

## 致谢

框架与课程指导来自 [NJU ProjectN](https://github.com/NJU-ProjectN) / [ICS PA](https://nju-projectn.github.io/ics-pa-gitbook/ics2024/),本项目为个人学习实践。
