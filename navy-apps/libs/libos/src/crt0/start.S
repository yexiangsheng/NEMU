.globl  _start
_start:

#if defined(__ISA_AM_NATIVE__)

  movq $0, %rbp
  // 进程入口的栈上按 ABI 摆着 argc/argv/envp(布局与 call_main 的约定一致),
  // 先把它记下来再对齐栈,andq 会破坏 rsp
  movq %rsp, %rax
  // (rsp + 8) should be multiple of 16 when
  // control is transfered to the function entry point.
  // See amd64 ABI manual for more details
  andq $0xfffffffffffffff0, %rsp
  movq %rax, %rdi
  call call_main

#elif defined(__ISA_X86_64__)

  movq $0, %rbp
  movq %rax, %rsp
  movq %rax, %rdi
  call call_main

#elif defined(__ISA_X86__)

  movl $0, %ebp
  call call_main

#elif defined(__ISA_MIPS32__)

  move $fp, $zero
  jal call_main

#elif defined(__riscv)
  mv a7, sp
  mv s0, a0
  jal call_main

#elif defined(__ISA_LOONGARCH32R__)

  move $fp, $zero
  bl call_main

#else
#error Unsupported ISA
#endif
