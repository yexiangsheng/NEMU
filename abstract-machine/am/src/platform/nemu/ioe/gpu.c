#include <am.h>
#include <nemu.h>

#define SCREEN_SIZE_ADDR (VGACTL_ADDR)
#define SYNC_ADDR (VGACTL_ADDR + 4)
#define SCREEN_MASK 0xffff

void __am_gpu_init()
{
  int i;

  uint32_t screen = inl(SCREEN_SIZE_ADDR);
  uint32_t screen_w = screen >> 16;
  uint32_t screen_h = screen & SCREEN_MASK;
  int w = screen_w; // TODO: get the correct width
  int h = screen_h; // TODO: get the correct height
  uint32_t *fb = (uint32_t *)(uintptr_t)FB_ADDR;
  for (i = 0; i < w * h; i++)
    fb[i] = i;
  outl(SYNC_ADDR, 1);
}

void __am_gpu_config(AM_GPU_CONFIG_T *cfg)
{
  uint32_t screen = inl(SCREEN_SIZE_ADDR);
  uint32_t screen_w = screen >> 16;
  uint32_t screen_h = screen & SCREEN_MASK;
  *cfg = (AM_GPU_CONFIG_T){
      .present = true, .has_accel = false, .width = screen_w, .height = screen_h, .vmemsz = 0};
}

void __am_gpu_fbdraw(AM_GPU_FBDRAW_T *ctl)
{
  int x = ctl->x, y = ctl->y, w = ctl->w, h = ctl->h;
  if (!ctl->sync && (w == 0 || h == 0))
  {
    return;
  }
  uint32_t *pixels = ctl->pixels;
  uint32_t *fb = (uint32_t *)(uintptr_t)FB_ADDR;
  uint32_t screen_w = inl(VGACTL_ADDR) >> 16;
  // uint32_t screen_w = inl(SCREEN_SIZE_ADDR) >> 16;
  // uint32_t begin_x = ctl->x;
  // uint32_t begin_y = ctl->y * screen_w;
  /*for (int x = 0; x < ctl->w; x++)
  {
    for (int y = 0; y < ctl->h; y++)
    {
      outl(FB_ADDR + begin_y + begin_x, ctl->pixels[]);
      begin_x++;
    }
    begin_y++;
  }*/

  for (int i = y; i < y + h; i++)
  {
    for (int j = x; j < x + w; j++)
    {
      fb[screen_w * i + j] = pixels[w * (i - y) + (j - x)];
    }
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
