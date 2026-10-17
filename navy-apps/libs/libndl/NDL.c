#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static int evtdev = -1;
static int fbdev = -1;
static int screen_w = 0, screen_h = 0;
static int canvas_w = 0, canvas_h = 0;
static int canvas_x = 0, canvas_y = 0;

// timer
uint32_t NDL_GetTicks()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  uint32_t now = (uint32_t)tv.tv_sec * 1000000 + tv.tv_usec;
  return now;
}

// keyboard
int NDL_PollEvent(char *buf, int len)
{
  int fd = open("/dev/events", 0);
  if (fd < 0)
    return 0;
  int n = read(fd, buf, len - 1);
  close(fd);
  if (n > 0)
    buf[n] = '\0'; // 文件读出的是裸字节,补 C 字符串结尾
  return n;
}

// gpu
// set Canvas w/h and x/y
void NDL_OpenCanvas(int *w, int *h)
{
  if (getenv("NWM_APP"))
  {
    int fbctl = 4;
    fbdev = 5;
    screen_w = *w;
    screen_h = *h;
    char buf[64];
    int len = sprintf(buf, "%d %d", screen_w, screen_h);
    // let NWM resize the window and create the frame buffer
    write(fbctl, buf, len);
    while (1)
    {
      // 3 = evtdev
      int nread = read(3, buf, sizeof(buf) - 1);
      if (nread <= 0)
        continue;
      buf[nread] = '\0';
      if (strcmp(buf, "mmap ok") == 0)
        break;
    }
    close(fbctl);
  }
  else
  {
    // 从 /proc/dispinfo 拿屏幕宽高
    int fd = open("/proc/dispinfo", 0);
    char buf[128];
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n < 0)
      n = 0;
    buf[n] = '\0';
    sscanf(buf, "WIDTH : %d\nHEIGHT : %d", &screen_w, &screen_h);

    // 0 表示全屏画布;不允许超过屏幕
    if (*w == 0 || *w > screen_w)
      *w = screen_w;
    if (*h == 0 || *h > screen_h)
      *h = screen_h;
    canvas_w = *w;
    canvas_h = *h;
    canvas_x = (screen_w - canvas_w) / 2; // 画布在屏幕上居中
    canvas_y = (screen_h - canvas_h) / 2;
  }
}

// gpu
void NDL_DrawRect(uint32_t *pixels, int x, int y, int w, int h)
{
  // 必须逐行写:矩形行与行之间在屏幕上隔着 screen_w - w 的间隙,
  int fb = open("/dev/fb", 0);
  // 一把梭连续写会把矩形抹成斜条
  if (fb < 0)
    return;
  for (int i = 0; i < h; i++)
  {
    size_t offset = ((size_t)(canvas_y + y + i) * screen_w + (canvas_x + x)) * 4;
    lseek(fb, offset, SEEK_SET);
    write(fb, pixels + (size_t)i * w, w * 4);
  }
  close(fb);
}

void NDL_OpenAudio(int freq, int channels, int samples)
{
}

void NDL_CloseAudio()
{
}

int NDL_PlayAudio(void *buf, int len)
{
  return 0;
}

int NDL_QueryAudio()
{
  return 0;
}

int NDL_Init(uint32_t flags)
{
  if (getenv("NWM_APP"))
  {
    evtdev = 3;
  }
  return 0;
}

void NDL_Quit()
{
}
