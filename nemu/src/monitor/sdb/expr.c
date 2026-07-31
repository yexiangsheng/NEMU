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

  /* operands */
  TK_DEC,   // 257: decimal number
  TK_HEX,   // 258: hex number (0x...)
  TK_REG,   // 259: register ($0, ra, sp, ...)

  /* binary operators (low to high precedence) */
  TK_AND,   // 260: &&
  TK_EQ,    // 261: ==
  TK_NEQ,   // 262: !=
  TK_ADD,   // 263: +
  TK_SUB,   // 264: -
  TK_MUL,   // 265: *
  TK_DIV,   // 266: /

  /* unary operators */
  TK_DEREF, // 267: * (pointer dereference)
  TK_NEG,   // 268: - (arithmetic negation)
  TK_NOT,   // 269: ! (logical not)

  /* parentheses */
  TK_LEF,   // 270: (
  TK_RIG,   // 271: )
};

static struct rule
{
  const char *regex;
  int token_type;
} rules[] = {
    /* IMPORTANT: rules are tried in order — put longer / more specific
     * patterns FIRST to avoid premature shorter matches.
     */

    {" +", TK_NOTYPE},                       // spaces (skip)

    /* multi-char operators before their single-char counterparts */
    {"==", TK_EQ},                           // equal
    {"!=", TK_NEQ},                          // not equal
    {"&&", TK_AND},                          // logical and

    /* hex before decimal (otherwise 0x1 → "0" matches TK_DEC first) */
    {"0[xX][0-9a-fA-F]+", TK_HEX},           // hex number

    /* decimal */
    {"[0-9]+", TK_DEC},                      // decimal number

    /* register: $0, ra, sp, gp, tp, t0-t6, s0-s11, a0-a7 */
    {"\\$0|ra|sp|gp|tp|t[0-6]|s[0-9]+|a[0-9]+", TK_REG},

    /* arithmetic */
    {"\\+", TK_ADD},                         // plus
    {"\\-", TK_SUB},                         // minus
    {"\\*", TK_MUL},                         // multiply/deref
    {"\\/", TK_DIV},                         // divide

    /* unary */
    {"!", TK_NOT},                           // logical not

    /* parentheses */
    {"\\(", TK_LEF},                         // left paren
    {"\\)", TK_RIG},                         // right paren
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

/* ---- helpers for the secondary pass ---- */

static bool is_operand(int type)
{
  return type == TK_DEC || type == TK_HEX || type == TK_REG || type == TK_RIG;
}

/* ---- tokenizer ---- */

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
        case TK_NOTYPE:
          /* spaces — do not record a token */
          break;

        case TK_ADD:
        case TK_SUB:
        case TK_MUL:
        case TK_DIV:
        case TK_LEF:
        case TK_RIG:
        case TK_EQ:
        case TK_NEQ:
        case TK_AND:
        case TK_NOT:
          /* operators — type is enough, str unused */
          tokens[nr_token].type = rules[i].token_type;
          nr_token++;
          break;

        case TK_DEC:
        case TK_HEX:
        case TK_REG:
          /* operands — need to save the string */
          assert(substr_len < 32);
          tokens[nr_token].type = rules[i].token_type;
          strncpy(tokens[nr_token].str, substr_start, substr_len);
          tokens[nr_token].str[substr_len] = '\0';
          nr_token++;
          break;

        default:
          panic("unhandled token type %d", rules[i].token_type);
        }

        break; /* out of the for-loop, continue scanning */
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

/* ---- check for a matched outermost pair of parentheses ---- */

static bool check_parentheses(int p, int q)
{
  if (tokens[p].type != TK_LEF || tokens[q].type != TK_RIG)
    return false;

  int depth = 0;
  for (int i = p; i <= q; i++)
  {
    if (tokens[i].type == TK_LEF) depth++;
    if (tokens[i].type == TK_RIG) depth--;
    /* depth hits 0 before reaching q → the '(' at p closed too early */
    if (depth == 0 && i < q) return false;
  }
  return depth == 0;
}

/* ---- precedence table (binary operators only; higher = tighter) ---- */

static int precedence(int type)
{
  switch (type)
  {
  case TK_AND:
    return 1; // lowest
  case TK_EQ:
  case TK_NEQ:
    return 2;
  case TK_ADD:
  case TK_SUB:
    return 3;
  case TK_MUL:
  case TK_DIV:
    return 4; // highest
  default:
    return 0; // not a binary operator
  }
}

/* ---- evaluate single token ---- */

static uint32_t eval_single(int p, bool *success)
{
  const Token *t = &tokens[p];
  switch (t->type)
  {
  case TK_DEC:
    return (uint32_t)strtoul(t->str, NULL, 10);
  case TK_HEX:
    return (uint32_t)strtoul(t->str, NULL, 16);
  case TK_REG:
    return isa_reg_str2val(t->str, success);
  default:
    *success = false;
    return 0;
  }
}

/* ---- recursive-descent evaluator ---- */

static uint32_t eval(int p, int q, bool *success)
{
  if (!*success) return 0;

  if (p > q)
  {
    *success = false;
    return 0;
  }

  /* ---- single token ---- */
  if (p == q)
  {
    return eval_single(p, success);
  }

  /* ---- strip matched parentheses ---- */
  if (check_parentheses(p, q))
  {
    return eval(p + 1, q - 1, success);
  }

  /* ---- unary operator at position p ---- */
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
  if (tokens[p].type == TK_NOT)
  {
    uint32_t v = eval(p + 1, q, success);
    if (!*success) return 0;
    return (v == 0) ? 1 : 0;
  }

  /* ---- find the MAIN binary operator (lowest precedence, rightmost) ---- */
  int op = -1;
  int min_prec = 999;
  int depth = 0;

  for (int i = q; i >= p; i--)
  {
    if (tokens[i].type == TK_RIG) { depth++; continue; }
    if (tokens[i].type == TK_LEF) { depth--; continue; }
    if (depth > 0) continue;

    int prec = precedence(tokens[i].type);
    if (prec > 0 && prec < min_prec)
    {
      min_prec = prec;
      op = i;
      if (prec == 1) break; /* && is the lowest — stop early (leftmost wins for &&) */
    }
  }

  if (op == -1)
  {
    *success = false;
    return 0;
  }

  /* ---- evaluate left and right sub-expressions ---- */
  uint32_t val1 = eval(p, op - 1, success);
  uint32_t val2 = eval(op + 1, q, success);
  if (!*success) return 0;

  /* ---- apply the binary operator ---- */
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
    return (int32_t)val1 / (int32_t)val2; // signed division
  default:
    *success = false;
    return 0;
  }
}

/* ---- public entry point ---- */

word_t expr(char *e, bool *success)
{
  *success = true;

  if (!make_token(e))
  {
    *success = false;
    return 0;
  }

  /* ---- secondary pass: disambiguate * and - ---- */
  for (int i = 0; i < nr_token; i++)
  {
    if (tokens[i].type == TK_MUL && (i == 0 || !is_operand(tokens[i - 1].type)))
    {
      /* * at start, after an operator, or after '(' → dereference */
      tokens[i].type = TK_DEREF;
    }

    if (tokens[i].type == TK_SUB && (i == 0 || !is_operand(tokens[i - 1].type)))
    {
      /* - at start, after an operator, or after '(' → negation */
      tokens[i].type = TK_NEG;
    }
  }

  if (nr_token == 0) { *success = false; return 0; }

  return eval(0, nr_token - 1, success);
}
