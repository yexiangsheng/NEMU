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
  // /proc/dispinfo 是文本文件: WIDTH:400\nHEIGHT:300\n
  // NDL 靠它拿到屏幕宽高(乘积拆不开,必须分别给出)
  AM_GPU_CONFIG_T info = io_read(AM_GPU_CONFIG);
  char tmp[64];
  int n = sprintf(tmp, "WIDTH:%d\nHEIGHT:%d\n", info.width, info.height);
  if (offset >= (size_t)n)
    return 0; // 已读过文件尾
  if (len > (size_t)(n - offset))
    len = n - offset;
  memcpy(buf, tmp + offset, len);
  return len;
}

size_t fb_write(const void *buf, size_t offset, size_t len)
{
  if (len == 0)
    return 0;
  int screen_w = io_read(AM_GPU_CONFIG).width;
  size_t px = offset / 4; // 字节偏移 → 像素序号(每像素 4 字节)
  // 一条 h=1 的水平带:行尾越界由 fbdraw 的线性下标自然回卷到下一行
  io_write(AM_GPU_FBDRAW,
           .x = px % screen_w, .y = px / screen_w,
           .w = len / 4, .h = 1,
           .sync = 1, .pixels = (uint32_t *)buf); // AM 接口无 const,显式跨界
  return len;
}

void init_device()
{
  Log("Initializing devices...");
  ioe_init();
}
