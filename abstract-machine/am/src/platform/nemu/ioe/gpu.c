#include <am.h>
#include <nemu.h>

#define SCREEN_SIZE_ADDR (VGACTL_ADDR)
#define SYNC_ADDR (VGACTL_ADDR + 4)
#define SCREEN_MASK 0xffff

void __am_gpu_init()
{
}

void __am_gpu_config(AM_GPU_CONFIG_T *cfg)
{
  uint32_t screen = inl(SCREEN_SIZE_ADDR);
  *cfg = (AM_GPU_CONFIG_T){
      .present = true, .has_accel = false, .vmemsz = 0, .width = screen >> 16, .height = screen & SCREEN_MASK};
}

void __am_gpu_fbdraw(AM_GPU_FBDRAW_T *ctl)
{
  uint32_t screen = inl(SCREEN_SIZE_ADDR);
  int screen_w = screen >> 16;
  int screen_h = screen & SCREEN_MASK;
  uint32_t *fb = (uint32_t *)(uintptr_t)FB_ADDR;
  uint32_t *pixels = ctl->pixels;

  for (int y = 0; y < ctl->h; y++)
  {
    int py = ctl->y + y;
    if (py >= screen_h)
      break; // 超出屏幕下边界，剩余的行整行跳过
    for (int x = 0; x < ctl->w; x++)
    {
      int px = ctl->x + x;
      if (px >= screen_w)
        break; // 本行超出右边界的像素跳过
      fb[py * screen_w + px] = pixels[y * ctl->w + x];
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
