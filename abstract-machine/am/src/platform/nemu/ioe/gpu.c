#include <am.h>
#include <nemu.h>

#define SCREEN_SIZE_ADDR (VGACTL_ADDR)
#define SYNC_ADDR (VGACTL_ADDR + 4)
#define SCREEN_W_MASK 0xf0
#define SCREEN_H_MASK 0x0f

void __am_gpu_init()
{
  int i;

  uint32_t screen = intl(SCREEN_SIZE_ADDR);
  uint32_t screen_w = screen & SCREEN_W_MASK >> 16;
  uint32_t screen_h = screen & SCREEN_H_MASK;
  int w = screen_w; // TODO: get the correct width
  int h = screen_h; // TODO: get the correct height
  uint32_t *fb = (uint32_t *)(uintptr_t)FB_ADDR;
  for (i = 0; i < w * h; i++)
    fb[i] = i;
  outl(SYNC_ADDR, 1);
}

void __am_gpu_config(AM_GPU_CONFIG_T *cfg)
{
  uint32_t screen = intl(SCREEN_SIZE_ADDR);
  uint32_t screen_w = screen & SCREEN_W_MASK >> 16;
  uint32_t screen_h = screen & SCREEN_H_MASK;
  *cfg = (AM_GPU_CONFIG_T){
      .present = true, .has_accel = false, .width = screen_w, .height = screen_h, .vmemsz = 0};
}

void __am_gpu_fbdraw(AM_GPU_FBDRAW_T *ctl)
{
  uint32_t screen_w = intl(SCREEN_SIZE_ADDR) & SCREEN_W_MASK >> 16;
  uint32_t begin_x = ctl->x;
  uint32_t begin_y = ctl->y * screen_w;
  for (int x = 0; x < ctl->w; x++)
  {
    for (int y = 0; ctl->h; y++)
    {
      outl(FB_ADDR + begin_y + begin_x, ctl->pixels);
      begin_x++;
    }
    begin_y++;
  }
  if (ctl->sync)
  {
    outl(SYNC_ADDR, 1);
  }
}

void __am_gpu_status(AM_GPU_STATUS_T *status)
{
  status->ready = true;
}
