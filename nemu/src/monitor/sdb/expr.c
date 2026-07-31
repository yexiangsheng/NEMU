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

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum
{
  TK_NOTYPE = 256,
  TK_EQ = 255,

  /* TODO: Add more token types */
  TK_DEC = 10,
  TK_ADD = 43,
  TK_SUB = 45,
  TK_MUL = 42,
  TK_DIV = 47,
  TK_LEF = 40,
  TK_RIG = 41,

  TK_EQQ = 254,
  TK_NEQ = 253,
  TK_AND = 252,
  TK_REF = 251,

  TK_NEG = 200
};

static struct rule
{
  const char *regex;
  int token_type;
} rules[] = {

    /* TODO: Add more rules.
     * Pay attention to the precedence level of different rules.
     */

    {" +", TK_NOTYPE},  // spaces
    {"==", TK_EQ},      // equal
    {"\\+", TK_ADD},    // plus
    {"[0-9]+", TK_DEC}, // How to express a decimel??
    {"\\-", TK_SUB},
    {"\\*", TK_MUL},
    {"\\/", TK_DIV},
    {"\\(", TK_LEF},
    {"\\)", TK_RIG},
    {"\\==", TK_EQQ},
    {"\\!=", TK_NEQ},
    {"\\&&", TK_AND}};

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

static Token tokens[32] __attribute__((used)) = {};
static int nr_token __attribute__((used)) = 0;

static bool make_token(char *e)
{
  int position = 0;
  int i;
  regmatch_t pmatch; //(struct ->int, int)

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

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        switch (rules[i].token_type)
        {
        case TK_ADD:
          tokens[nr_token].type = TK_ADD;
          nr_token += 1;
          break;
        case TK_SUB:
          tokens[nr_token].type = TK_SUB;
          nr_token += 1;
          break;
        case TK_MUL:
          tokens[nr_token].type = TK_MUL;
          nr_token += 1;
          break;
        case TK_DIV:
          tokens[nr_token].type = TK_DIV;
          nr_token += 1;
          break;
        case TK_LEF:
          tokens[nr_token].type = TK_LEF;
          nr_token += 1;
          break;
        case TK_RIG:
          tokens[nr_token].type = TK_RIG;
          nr_token += 1;
          break;
        case TK_DEC:
          tokens[nr_token].type = TK_DEC;
          assert(substr_len < 32);
          strncpy(tokens[nr_token].str, substr_start, substr_len);
          tokens[nr_token].str[substr_len] = '\0';
          nr_token += 1;
          break;
        default:
          printf("It is space!!");
        }

        break;
      }
    }

    // Match failture !!
    if (i == NR_REGEX)
    {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

// Counter: "(" --> +1; ")" --> -1 ; if finally "counter" == 0, return true
bool check_parentheses(int p, int q)
{
  int counter = 0;
  for (int i = 0; i < q; i++)
  {
    if (tokens[i].type == TK_LEF)
    {
      counter += 1;
    }
    if (tokens[i].type == TK_RIG)
    {
      counter -= 1;
    }
    p++;
    if (p < q && counter == 0)
    {
      return false;
    }
  }
  return true;
}

// /* GDB 测试辅助: call test_paren("(1+2)", 0, 4) 一步完成 tokenize + 检查 */
// int test_paren(char *expr_str, int p, int q)
// {
//   if (!make_token(expr_str))
//   {
//     printf("make_token failed!\n");
//     return -1;
//   }
//   printf("nr_token = %d\n", nr_token);
//   for (int i = 0; i < nr_token; i++)
//   {
//     printf("  tokens[%d]: type=%d, str=\"%s\"\n", i, tokens[i].type, tokens[i].str);
//   }
//   bool result = check_parentheses(p, q);
//   printf("check_parentheses(%d, %d) = %s\n", p, q, result ? "true" : "false");
//   return result;
// }

uint32_t eval(int p, int q)
{
  if (p > q)
  {
    /* Bad expression */
    Log("Bad expression");
    assert(0);
  }
  else if (p == q)
  {
    /* Single token.
     * For now this token should be a number.
     * Return the value of the number.
     */
    Log("Single token");
    uint32_t res = (uint32_t)strtoul(tokens[p].str, NULL, 16);
    return res;
  }
  else if (check_parentheses(p, q) == true)
  {
    /* The expression is surrounded by a matched pair of parentheses.
     * If that is the case, just throw away the parentheses.
     */
    return eval(p + 1, q - 1);
  }
  else
  {
    int op_type = TK_NOTYPE;
    int op = 0;
    uint32_t val1 = 0;
    uint32_t val2 = 0;

    for (int i = q; i > p; i--)
    {
      if (tokens[i].type == (int)'(')
      {
        while (tokens[i].type == (int)')')
        {
          i--;
        }
      }
      if (tokens[i].type == (int)'+' || tokens[i].type == (int)'-')
      {
        op_type = tokens[i].type;
        op = i;
        val1 = eval(p, op - 1);
        val2 = eval(op + 1, q);
      }
    }
    // op = the position of 主运算符 in the token expression;
    // val1 = eval(p, op - 1);
    // val2 = eval(op + 1, q);

    switch (op_type)
    {
    case TK_ADD:
      return val1 + val2;
    case TK_SUB: /* ... */
      return val1 - val2;
    case TK_MUL: /* ... */
      return val1 * val2;
    case TK_DIV: /* ... */
      assert(val2 == 0);
      return val1 / val2;
    case TK_EQQ:
      return val1 == val2 ? 1 : 0;
    case TK_NEQ:
      return val1 != val2 ? 1 : 0;
    case TK_AND:
      if (val1 == 0)
      {
        return 0;
      }
      return val2 != 0 ? 1 : 0;
    case TK_REF:

    default:
      assert(0);
    }
    assert("No return!!");
  }
}

/* 判断一个 token 类型是否属于"操作数"（或右括号）。
 * * 和 - 只有当前面不是操作数/右括号时，才被视为一元运算符。 */
static bool is_operand(int type)
{
  return type == TK_DEC || type == TK_RIG;
}

word_t expr(char *e, bool *success)
{
  /* 1. 词法分析 */
  if (!make_token(e))
  {
    *success = false;
    return 0;
  }

  /* 2. 二次标记：区分 *（乘法/解引用）和 -（减法/取反）
   *    规则：如果 * 或 - 出现在表达式开头，或者前面不是操作数/右括号，
   *    则视为一元运算符（TK_REF / TK_NEG）。*/
  for (int i = 0; i < nr_token; i++)
  {
    bool at_start = (i == 0);
    bool prev_is_non_operand = !at_start && !is_operand(tokens[i - 1].type);

    if (tokens[i].type == TK_MUL && (at_start || prev_is_non_operand))
    {
      tokens[i].type = TK_REF;
    }

    if (tokens[i].type == TK_SUB && (at_start || prev_is_non_operand))
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
  *success = true;
  return eval(0, nr_token - 1);
}
