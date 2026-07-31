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

#include "sdb.h"
#include <cpu/cpu.h>
// #include "include/isa.h"
// #include "reg.c"
#include <isa.h>
#include <memory/paddr.h>
#include <readline/history.h>
#include <readline/readline.h>

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char *rl_gets()
{
  static char *line_read = NULL;

  if (line_read)
  {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read)
  {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args)
{
  cpu_exec(-1);
  return 0;
}

static int cmd_q(char *args)
{
  nemu_state.state = NEMU_QUIT;
  return -1;
}

static int cmd_help(char *args);

static int cmd_si_N(char *args);

static int cmd_info(char *args1);

static int cmd_scanf_mem(char *args);

static int cmd_p_expr(char *args);

static int cmd_w_expr(char *args);

static int cmd_delete(char *args);

static struct
{
  const char *name;
  const char *description;
  int (*handler)(char *);
} cmd_table[] = {
    {"help", "'help',Display information about all supported commands", cmd_help},
    {"c", "'c',Continue the execution of the program", cmd_c},
    {"q", "'q',Exit NEMU", cmd_q},
    {"si", "'si N',Execute N commands step by step and then stop, when N is null, default = 1", cmd_si_N},
    {"info", "'info r', Display register status; 'info w', Display monitor information", cmd_info},
    {"x", "'x N EXPR',Evaluate the expression EXPR, use the result as the starting memory address, and output N consecutive 4-byte values in hexadecimal format.", cmd_scanf_mem},
    {"p", "'p EXPR',Evaluate the expression EXPR. For the supported operations in EXPR, refer to the Expression Evaluation section in the Debugging chapter.", cmd_p_expr},
    {"w", "'w EXPR',Pause program execution when the value of expression EXPR changes.", cmd_w_expr},
    {"d", "Delete the watchpoint with sequence number N.", cmd_delete}
    /* TODO: Add more commands */

};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args)
{
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL)
  {
    /* no argument given */
    for (i = 0; i < NR_CMD; i++)
    {
      Log("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else
  {
    for (i = 0; i < NR_CMD; i++)
    {
      if (strcmp(arg, cmd_table[i].name) == 0)
      {
        Log("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    Log("Unknown command '%s'\n", arg);
  }
  return 0;
}

static int cmd_si_N(char *args)
{
  char *arg = strtok(NULL, " ");

  uint64_t n = 1;
  if (arg != NULL)
  {
    n = (uint64_t)atoi(arg);
  }

  cpu_exec(n);
  return 0;
}

static int cmd_info(char *args)
{
  char *arg = strtok(NULL, " ");
  if (arg == NULL)
  {
    Log("Usage: info SUBCMD\n");
    return 0;
  }
  if (strcmp(arg, "r") == 0)
  {
    isa_reg_display();
    return 0;
  }
  // else if (strcmp(arg, "w") == 0)
  // {
  // }
  Log("Unknown info subcmd '%s'\n", arg);
  return 0;
}

static bool is_hex(const char *s)
{
  if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
  {
    s += 2;
  }
  else
  {
    return false; // 没有 0x 前缀
  }
  if (*s == '\0')
    return false; // 只有 "0x"，不是有效数字
  while (*s)
  {
    if (!isxdigit(*s)) // 逐位检查是否为 0-9, a-f, A-F
      return false;
    s++;
  }
  return true;
}

static int cmd_scanf_mem(char *args)
{
  // Get "n": input n menory vddr
  char *arg1 = strtok(NULL, " ");
  uint32_t n = 1;
  if (arg1 == NULL)
  {
    Log("Usage: x N EXPR\n");
    return 0;
  }
  else
  {
    n = (uint64_t)atoi(arg1);
  }

  // Get start vddr
  char *expr = strtok(NULL, " ");
  if (expr == NULL)
  {
    Log("Usage: x N EXPR\n");
    return 0;
  }
  else if (!is_hex(expr))
  {
    Log("Please usage hexadecimal\n");
    return 0;
  }
  else
  {
    Log("Scan men Succeed!!");
    // Scanning n vddr
    paddr_t addr = (paddr_t)strtoul(expr, NULL, 16);
    for (int i = 0; i < n; i++)
    {
      uint32_t val = paddr_read(addr + i * 4, 4);
      printf("0x%08x: 0x%08x\n", addr + i * 4, val);
    }
    return 0;
  }

  return 0;
}

static int cmd_p_expr(char *args)
{
  char *arg = strtok(NULL, " ");
  bool success = false;
  uint32_t res = expr(arg, &success);
  snprintf(arg, 32, "%u", res);
  return success;
}

static int cmd_w_expr(char *args)
{
  char *arg = strtok(NULL, " ");
  if (arg != NULL)
  {
    get_new_wp(arg);
    // Log("Already set watchpoint in %");
  }
  else
  {
    Log("The EXPR is NULL!!\n");
  }
  return 1;
}

static int cmd_delete(char *args)
{
  char *arg = strtok(NULL, " ");
  int res = atoi(arg);
  if (arg != NULL)
  {
    delete_wp(res);
  }
  else
  {
    Log("The EXPR is NULL!!\n");
  }
  return 1;
}

void sdb_set_batch_mode()
{
  is_batch_mode = true;
}

// Recognize and match commands
void sdb_mainloop()
{
  if (is_batch_mode)
  {
    cmd_c(NULL);
    return;
  }

  // eg: "si 10"
  // 1. rl_gets(): read line command "si 10"
  for (char *str; (str = rl_gets()) != NULL;)
  {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    // 2. strtok(): extract token, cmd="si", args="10"
    char *cmd = strtok(str, " ");
    if (cmd == NULL)
    {
      continue;
    }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end)
    {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    // 3. cmd_table: check table
    // 4. handler: if get the command in table, execute handler function
    int i;
    for (i = 0; i < NR_CMD; i++)
    {
      if (strcmp(cmd, cmd_table[i].name) == 0)
      {
        if (cmd_table[i].handler(args) < 0)
        {
          return;
        }
        break;
      }
    }

    if (i == NR_CMD)
    {
      Log("Unknown command '%s'\n", cmd);
    }
  }
}

void init_sdb()
{
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
