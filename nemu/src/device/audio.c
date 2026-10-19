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

#include <common.h>
#include <device/map.h>
#include <SDL2/SDL.h>
#include <device/mmio.h>

enum
{
  reg_freq,
  reg_channels,
  reg_samples,
  reg_sbuf_size,
  reg_init,
  reg_count,
  nr_reg
};

static uint8_t *sbuf = NULL;
static uint32_t *audio_base = NULL;
static uint32_t sbuf_r = 0;

// TEMP 临时探针: 验证客机喂数据后删除
#include <stdio.h>
static uint64_t dbg_cb = 0, dbg_played = 0, dbg_starve = 0;
__attribute__((destructor)) static void dbg_report(void)
{
  if (dbg_cb)
    fprintf(stderr, "[audio-probe] callbacks=%llu played=%llu bytes starved=%llu\n",
            (unsigned long long)dbg_cb, (unsigned long long)dbg_played, (unsigned long long)dbg_starve);
}

void audio_callback(void *userdata, Uint8 *stream, int len)
{
  SDL_LockAudio();
  assert(len >= 0 && len <= CONFIG_SB_SIZE);
  uint32_t count = audio_base[reg_count];
  uint32_t n = len < count ? len : count;
  dbg_cb++;
  dbg_played += n;
  if (n < (uint32_t)len)
    dbg_starve += len - n;
  uint32_t tail = CONFIG_SB_SIZE - sbuf_r;
  uint32_t first = n < tail ? n : tail;
  memcpy(stream, sbuf + sbuf_r, first);
  memcpy(stream + first, sbuf, n - first);
  sbuf_r = (sbuf_r + n) % CONFIG_SB_SIZE;
  if (n < (uint32_t)len)
  {
    memset(stream + n, 0, len - n);
  }
  audio_base[reg_count] = count - n;
  SDL_UnlockAudio();
}

static void audio_io_handler(uint32_t offset, int len, bool is_write)
{
  if (is_write && offset == 4 * reg_init && audio_base[reg_init])
  {
    SDL_AudioSpec s = {};
    s.freq = audio_base[reg_freq];
    s.format = AUDIO_S16SYS; // 假设系统中音频数据的格式总是使用16位有符号数来表示
    s.channels = audio_base[reg_channels];
    s.samples = audio_base[reg_samples];
    s.userdata = NULL; // 不使用
    s.callback = audio_callback;
    SDL_InitSubSystem(SDL_INIT_AUDIO);
    SDL_OpenAudio(&s, NULL);
    SDL_PauseAudio(0);
  }
  if (!is_write && offset == 4 * reg_count)
  {
    audio_base[reg_count] = CONFIG_SB_SIZE;
  }
}

void init_audio()
{
  uint32_t space_size = sizeof(uint32_t) * nr_reg;
  audio_base = (uint32_t *)new_space(space_size);
#ifdef CONFIG_HAS_PORT_IO
  add_pio_map("audio", CONFIG_AUDIO_CTL_PORT, audio_base, space_size, audio_io_handler);
#else
  add_mmio_map("audio", CONFIG_AUDIO_CTL_MMIO, audio_base, space_size, audio_io_handler);
#endif

  sbuf = (uint8_t *)new_space(CONFIG_SB_SIZE);
  add_mmio_map("audio-sbuf", CONFIG_SB_ADDR, sbuf, CONFIG_SB_SIZE, NULL);
}
