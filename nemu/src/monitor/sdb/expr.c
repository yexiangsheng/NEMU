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

#include <isa.h>
#include <memory/paddr.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum
{
  TK_NOTYPE = 256,

  /* 操作数 */
  TK_DEC = 257,   // 十进制数字
  TK_HEX = 258,   // 十六进制数字 (0x...)
  TK_REG = 259,   // 寄存器 ($0, ra, sp, ...)

  /* 二元运算符（优先级从低到高） */
  TK_AND = 260,   // &&
  TK_EQ  = 261,   // ==
  TK_NEQ = 262,   // !=
  TK_ADD = 263,   // +
  TK_SUB = 264,   // -
  TK_MUL = 265,   // *
  TK_DIV = 266,   // /

  /* 一元运算符 */
  TK_DEREF = 267, // * 解引用
  TK_NEG   = 268, // - 取反

  /* 括号 */
  TK_LEF = 269,   // (
  TK_RIG = 270,   // )
};

static struct rule
{
  const char *regex;
  int token_type;
} rules[] = {
    /* 规则按顺序尝试——长匹配/特化模式放前面 */

    {" +", TK_NOTYPE},                       // 空格（跳过）

    /* 多字符运算符优先 */
    {"==", TK_EQ},                           // 相等
    {"!=", TK_NEQ},                          // 不等
    {"&&", TK_AND},                          // 逻辑与

    /* 十六进制优先于十进制（否则 0x1 的 "0" 先被 TK_DEC 匹配） */
    {"0[xX][0-9a-fA-F]+", TK_HEX},           // 十六进制

    /* 十进制 */
    {"[0-9]+", TK_DEC},                      // 十进制

    /* 寄存器：必须放在 [0-9]+ 等可能部分匹配的规则之后，但实际上
     * 寄存器名以 $ 或字母开头，不会与数字规则冲突。放在数字之后
     * 可以避免类似 "s0" 在 [a-z]+ 中意外匹配（现在没有这条规则）。 */
    {"\\$0|ra|sp|gp|tp|t[0-6]|s[0-9]+|a[0-9]+", TK_REG},

    /* 算术 */
    {"\\+", TK_ADD},
    {"\\-", TK_SUB},
    {"\\*", TK_MUL},
    {"\\/", TK_DIV},

    /* 括号 */
    {"\\(", TK_LEF},
    {"\\)", TK_RIG},
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex()
{
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i++)
  {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0)
    {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token
{
  int type;
  char str[32];
} Token;

static Token tokens[64] __attribute__((used)) = {};
static int nr_token __attribute__((used)) = 0;

/* ---- 词法分析 ---- */

static bool make_token(char *e)
{
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0')
  {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i++)
    {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0)
      {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        if (nr_token >= 64)
        {
          printf("too many tokens\n");
          return false;
        }

        switch (rules[i].token_type)
        {
        /* ---- 跳过的类型 ---- */
        case TK_NOTYPE:
          break; /* 空格不产生 token */

        /* ---- 运算符（只需记录 type） ---- */
        case TK_EQ:
        case TK_NEQ:
        case TK_AND:
        case TK_ADD:
        case TK_SUB:
        case TK_MUL:
        case TK_DIV:
        case TK_LEF:
        case TK_RIG:
          tokens[nr_token].type = rules[i].token_type;
          nr_token++;
          break;

        /* ---- 操作数（需要保存字符串以便求值） ---- */
        case TK_DEC:
        case TK_HEX:
        case TK_REG:
          assert(substr_len < 32);
          tokens[nr_token].type = rules[i].token_type;
          strncpy(tokens[nr_token].str, substr_start, substr_len);
          tokens[nr_token].str[substr_len] = '\0';
          nr_token++;
          break;

        default:
          panic("unhandled token type %d", rules[i].token_type);
        }

        break; /* 跳出规则循环，继续扫描下一个位置 */
      }
    }

    if (i == NR_REGEX)
    {
      printf("no match at position %d\n%s\n%*s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

/* ---- 辅助判断 ---- */

/* 操作数类型：* 和 - 前面是这些类型时，应视为二元运算符 */
static bool is_operand(int type)
{
  return type == TK_DEC || type == TK_HEX || type == TK_REG || type == TK_RIG;
}

/* 二元运算符优先级：数字越大优先级越高。返回 0 表示不是二元运算符。 */
static int precedence(int type)
{
  switch (type)
  {
  case TK_AND:
    return 1; // 最低
  case TK_EQ:
  case TK_NEQ:
    return 2;
  case TK_ADD:
  case TK_SUB:
    return 3;
  case TK_MUL:
  case TK_DIV:
    return 4; // 最高
  default:
    return 0; // 非二元运算符
  }
}

/* ---- 括号匹配检查 ---- */

static bool check_parentheses(int p, int q)
{
  if (tokens[p].type != TK_LEF || tokens[q].type != TK_RIG)
    return false;

  int depth = 0;
  for (int i = p; i <= q; i++)
  {
    if (tokens[i].type == TK_LEF) depth++;
    if (tokens[i].type == TK_RIG) depth--;
    if (depth == 0 && i < q) return false; /* 过早闭合 */
  }
  return depth == 0;
}

/* ---- 递归求值 ---- */

static uint32_t eval(int p, int q, bool *success)
{
  if (!*success) return 0;

  /* 非法区间 */
  if (p > q)
  {
    *success = false;
    return 0;
  }

  /* ---- 单 token ---- */
  if (p == q)
  {
    switch (tokens[p].type)
    {
    case TK_DEC:
      return (uint32_t)strtoul(tokens[p].str, NULL, 10);
    case TK_HEX:
      return (uint32_t)strtoul(tokens[p].str, NULL, 16);
    case TK_REG:
      return isa_reg_str2val(tokens[p].str, success);
    default:
      *success = false;
      return 0;
    }
  }

  /* ---- 剥离匹配的外层括号 ---- */
  if (check_parentheses(p, q))
  {
    return eval(p + 1, q - 1, success);
  }

  /* ---- 一元运算符（出现在表达式最左端） ---- */
  if (tokens[p].type == TK_DEREF)
  {
    uint32_t addr = eval(p + 1, q, success);
    if (!*success) return 0;
    return paddr_read(addr, 4);
  }
  if (tokens[p].type == TK_NEG)
  {
    return -(int32_t)eval(p + 1, q, success);
  }

  /* ---- 寻找主二元运算符（最低优先级、最右） ---- */
  int op = -1;
  int min_prec = 999;
  int depth = 0;

  for (int i = q; i >= p; i--)
  {
    if (tokens[i].type == TK_RIG) { depth++; continue; }
    if (tokens[i].type == TK_LEF) { depth--; continue; }
    if (depth > 0) continue; /* 跳过括号内部 */

    int prec = precedence(tokens[i].type);
    if (prec > 0 && prec <= min_prec)
    {
      min_prec = prec;
      op = i;
    }
  }

  if (op == -1)
  {
    *success = false;
    return 0;
  }

  uint32_t val1 = eval(p, op - 1, success);
  uint32_t val2 = eval(op + 1, q, success);
  if (!*success) return 0;

  switch (tokens[op].type)
  {
  case TK_AND:
    return (val1 != 0 && val2 != 0) ? 1 : 0;
  case TK_EQ:
    return (val1 == val2) ? 1 : 0;
  case TK_NEQ:
    return (val1 != val2) ? 1 : 0;
  case TK_ADD:
    return val1 + val2;
  case TK_SUB:
    return val1 - val2;
  case TK_MUL:
    return val1 * val2;
  case TK_DIV:
    if (val2 == 0) { *success = false; return 0; }
    return (int32_t)val1 / (int32_t)val2; /* 有符号除法 */
  default:
    *success = false;
    return 0;
  }
}

/* ---- 对外入口 ---- */

word_t expr(char *e, bool *success)
{
  *success = true;

  /* 1. 词法分析 */
  if (!make_token(e))
  {
    *success = false;
    return 0;
  }

  /* 2. 二次标记：区分 * (乘法/解引用) 和 - (减法/取反) */
  for (int i = 0; i < nr_token; i++)
  {
    bool at_start = (i == 0);
    bool prev_is_op = !at_start && !is_operand(tokens[i - 1].type);

    if (tokens[i].type == TK_MUL && (at_start || prev_is_op))
    {
      tokens[i].type = TK_DEREF;
    }

    if (tokens[i].type == TK_SUB && (at_start || prev_is_op))
    {
      tokens[i].type = TK_NEG;
    }
  }

  /* 3. 空表达式 */
  if (nr_token == 0)
  {
    *success = false;
    return 0;
  }

  /* 4. 递归求值 */
  return eval(0, nr_token - 1, success);
}

/* ================================================================
 *  expr 测试函数
 *  GDB 中调用:
 *    call test_expr("1+2", 3)    — 单测
 *    call test_expr_all()        — 全测
 * ================================================================ */

static int test_one(const char *s, uint32_t expected)
{
  bool success = false;
  uint32_t result = expr((char *)s, &success);

  printf("expr(\"%s\") = %u (0x%08x), success=%s, expect=%u",
         s, result, result,
         success ? "true" : "false",
         expected);

  if (!success)
  {
    printf("  [FAIL] expr returned failure\n");
    return 1;
  }
  if (result != expected)
  {
    printf("  [FAIL] result mismatch\n");
    return 1;
  }
  printf("  [PASS]\n");
  return 0;
}

int test_expr(const char *s, uint32_t expected)
{
  printf("=== test_expr ===\n");
  int fail = test_one(s, expected);
  printf("=== %s ===\n", fail ? "FAILED" : "PASSED");
  return fail;
}

int test_expr_all(void)
{
  printf("=== test_expr_all ===\n");
  int fail = 0;

  /* 单数字 */
  fail += test_one("0",   0);
  fail += test_one("1",   1);
  fail += test_one("123", 123);

  /* 加减乘除 */
  fail += test_one("1+2",   3);
  fail += test_one("5-3",   2);
  fail += test_one("2*3",   6);
  fail += test_one("8/2",   4);

  /* 括号 */
  fail += test_one("(1+2)",     3);
  fail += test_one("(1+2)*3",   9);
  fail += test_one("2*(3+4)",  14);
  fail += test_one("((1+2))",   3);

  /* 优先级 */
  fail += test_one("1+2*3",   7);   // 1+(2*3)
  fail += test_one("2*3+4",  10);   // (2*3)+4
  fail += test_one("2+3*4",  14);
  fail += test_one("1-2+3",   2);   // left assoc

  /* 空格 */
  fail += test_one(" 1 + 2 ",       3);
  fail += test_one("( 1 + 2 ) * 3", 9);

  /* 比较 */
  fail += test_one("1==1",   1);
  fail += test_one("1==2",   0);
  fail += test_one("1!=2",   1);
  fail += test_one("2!=2",   0);

  /* 逻辑与 */
  fail += test_one("1&&1",   1);
  fail += test_one("1&&0",   0);
  fail += test_one("0&&1",   0);
  fail += test_one("2&&3",   1);

  /* 一元取反 */
  fail += test_one("-1",     0xffffffff);
  fail += test_one("-0",     0);
  fail += test_one("1+-2",   0xffffffff);
  fail += test_one("-(1+2)", 0xfffffffd);

  /* 组合 */
  fail += test_one("1+2==3",     1);
  fail += test_one("(1+2)*3!=9", 0);
  fail += test_one("1+2*3==7",   1);

  /* 解引用 & 寄存器（需要 NEMU 运行中才能正确测试，这里留空框架） */
  // fail += test_one("*0x80000000", ???);  // 依赖实际内存
  // fail += test_one("$0", 0);             // RISC-V zero 寄存器

  printf("=== total failures: %d ===\n", fail);
  return fail;
}
