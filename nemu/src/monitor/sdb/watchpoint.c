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
#include <utils.h>

#define NR_WP 32

typedef struct watchpoint
{
  int NO;
  struct watchpoint *next;

  /* TODO: Add more members if necessary */
  char *watch_num;
  word_t prev_val;
  bool is_breakpoint;

} WP;

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;
static uint32_t head_num = 0, free_num = 0;

void init_wp_pool()
{
  int i;
  for (i = 0; i < NR_WP; i++)
  {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
    wp_pool[i].watch_num = NULL;
    free_num++;
  }

  head = NULL;
  free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */

WP *new_wp()
{
  assert(free_ != NULL);
  WP *temp = free_;
  free_ = free_->next;
  // Head is not NULL
  if (head != NULL)
  {
    free_->next = head;
    head = free_;
  }
  else // Head is NULL
  {
    head = temp;
  }
  head_num++;
  head->NO = head_num;
  free_num--;

  return head;
}

void free_wp(WP *wp)
{
  assert(head != NULL);

  WP *temp = head;
  head = head->next;
  // Head is not NULL
  if (free_ != NULL)
  {
    head->next = free_;
    free_ = head;
  }
  else // Head is NULL
  {
    free_ = temp;
  }
  free_->NO = 0;
  free_->watch_num = NULL;
  head_num--;
  free_num++;
}

void get_new_wp(char *e)
{
  bool success = true;
  head = new_wp();
  head->watch_num = e;
  head->prev_val = expr(e, &success);
  printf("Hardware watchpoint %d: %s\n\n", head->NO, e);
}

void watchpoint_check()
{
  WP *ite = head;
  for (int i = 0; i < head_num; i++)
  {
    bool success = true;
    word_t new_val = expr(ite->next->watch_num, &success);
    if (new_val != ite->next->prev_val)
    {
      ite->next->prev_val = new_val;
      if (ite->next->is_breakpoint)
      {
        /* breakpoint: only stop on 0->1 transition */
        if (new_val == 1)
          nemu_state.state = NEMU_STOP;
      }
      else
      {
        nemu_state.state = NEMU_STOP;
      }
    }
    ite = ite->next;
  }
  return;
}

void set_breakpoint(char *addr_str)
{
  /* construct expression "$pc == ADDR" */
  char expr_str[64];
  snprintf(expr_str, sizeof(expr_str), "$pc == %s", addr_str);

  bool success = true;
  head = new_wp();
  head->watch_num = strdup(expr_str);
  head->prev_val = expr(expr_str, &success);
  head->is_breakpoint = true;
  printf("Breakpoint %d at %s\n", head->NO, addr_str);
}

void info_watchpoints()
{
  if (head == NULL)
  {
    printf("No watchpoints.\n");
    return;
  }
  printf("Num     Type           What\n");
  WP *ite = head;
  for (int i = 0; i < head_num; i++)
  {
    printf("%-8d%-15s%s\n", ite->next->NO,
           ite->next->is_breakpoint ? "breakpoint" : "hw watchpoint",
           ite->next->watch_num);
    ite = ite->next;
  }
}

void delete_wp(int n)
{
  WP *ite = head;
  for (int i = 0; i < head_num; i++)
  {
    if (head->NO == n)
    {
      head = head->next;
      ite->next = free_;
      free_ = ite;
      Log("Free");
    }
    else if (ite->next->NO == n)
    {
      WP *temp = ite->next;
      ite->next->next = temp->next;
      temp->next = free_;
      free_ = temp;
      Log("Free");
    }
    else
    {
      ite = ite->next;
    }
  }
  return;
}