#include <common.h>
#include <device.h>

#if defined(MULTIPROGRAM) && !defined(TIME_SHARING)
#define MULTIPROGRAM_YIELD() yield()
#else
#define MULTIPROGRAM_YIELD()
#endif

#define NAME(key) \
  [AM_KEY_##key] = #key,

static const char *keyname[256] __attribute__((used)) = {
    [AM_KEY_NONE] = "NONE",
    AM_KEYS(NAME)};

size_t serial_write(const void *buf, size_t offset, size_t len)
{
  for (size_t i = 0; i < len; i++)
  {
    putch(((const char *)buf)[i]);
  }
  return len;
}

#if 0  
// size_t events_read(void *buf, size_t offset, size_t len)
//{
// AM_INPUT_KEYBRD_T ev = io_read(AM_INPUT_KEYBRD);
// if (ev.keycode == AM_KEY_NONE)
//{
// return 0;
//}
// char tmp[64];
// int n = sprintf();
// return 0;
//}
#endif

size_t events_read(void *buf, size_t offset, size_t len)
{
  AM_INPUT_KEYBRD_T ev = io_read(AM_INPUT_KEYBRD);
  if (ev.keycode == AM_KEY_NONE)
    return 0; // 无有效按键 → 0
  char tmp[64];
  int n = sprintf(tmp, "%s %s\n", ev.keydown ? "kd" : "ku",
                  keyname[ev.keycode]); // 你已有的 keyname 数组
  if (n > (int)len)
    n = len; // 最长写 len 字节
  memcpy(buf, tmp, n);
  return n;
}

size_t dispinfo_read(void *buf, size_t offset, size_t len)
{
  AM_GPU_FBDRAW_T gpu = io_read(AM_GPU_FBDRAW);
  int x = gpu.x, y = gpu.y, w = gpu.w, h = gpu.h;
  uint32_t *pixels = gpu.pixels;

  char tmp[x][y];
  for (int i = y; i < y + h; i++)
  {
    for (int j = x; j < x + w; j++)
    {
      tmp[i][j] = pixels[w * (i - y) + (j - x)];
    }
  }

  int size = w * h;
  if (size > len)
  {
    size = len;
  }
  memcpy(buf, tmp, size);
  return 0;
}

size_t fb_write(const void *buf, size_t offset, size_t len)
{
  return 0;
}

void init_device()
{
  Log("Initializing devices...");
  ioe_init();
}
