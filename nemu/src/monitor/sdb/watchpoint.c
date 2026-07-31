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
      printf("Hardware watchpoint %d: %s\n\n", ite->next->NO, ite->next->watch_num);
      printf("Old value = %u\n", ite->next->prev_val);
      printf("New value = %u\n\n", new_val);
      ite->next->prev_val = new_val;
      nemu_state.state = NEMU_STOP;
    }
    ite = ite->next;
  }
  return;
}