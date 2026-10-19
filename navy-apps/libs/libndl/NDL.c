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

// ---------- audio ----------
// NEMU 音频设备(nemu/src/device/audio.c)直接 mmio 访问:
//   寄存器组 0xa0000200(0x18 字节): [0]=freq [1]=channels [2]=samples
//                                    [3]=sbuf_size [4]=init [5]=count
//   流缓冲   0xa1200000, 环形, 大小 0x10000(CONFIG_SB_SIZE)
// 协议要点(RTFSC 得出):
//   * 写 init 非 0: NEMU 打开主机音频, 此后每次回调拉 samples*channels*2 字节
//   * 写 count=N:   表示"追加了 N 字节待播", 主机消费 min(N, 拉取量) 并递减 count
//   * 读 count:     副作用是先把 count 改成 SB_SIZE 再返回 —— 会毁掉 NEMU 侧记账,
//                   且永远只能读到 SB_SIZE, 客机绝不能读它!
//   * 流缓冲的读指针在 NEMU 内部维护, 客机只维护自己的写指针, 两侧按同一速率推进
#ifndef __GLIBC__ // NEMU 环境: 应用运行在 M 模式, 直接访问 mmio 合法

#define AUDIO_REG_BASE 0xa0000200
#define AUDIO_SBUF_BASE 0xa1200000
// = NEMU 的 CONFIG_SB_SIZE; reg_sbuf_size 从未被 NEMU 写入(恒 0), 读不到真实值。
// 64KB 只够 0.37s 音频, 而 NEMU 里渲染一帧就要几百毫秒墙钟, 缓冲会被打空(音乐断续),
// 已把 NEMU 的 CONFIG_SB_SIZE 调到 256KB(约 1.5s), 两处需保持一致
#define AUDIO_SB_SIZE 0x40000

enum { AREG_FREQ, AREG_CHANNELS, AREG_SAMPLES, AREG_SBUF_SIZE, AREG_INIT, AREG_COUNT };

static int audio_inited = 0; // init 只写一次: 重复写只会让 NEMU 重复打开主机音频
static uint32_t sbuf_w = 0;  // 客机侧环形写指针; 关闭再打开不清零, 保持与 NEMU 读指针对齐

static inline void audio_reg_write(int reg, uint32_t val)
{
  *(volatile uint32_t *)(AUDIO_REG_BASE + reg * 4) = val;
}

void NDL_OpenAudio(int freq, int channels, int samples)
{
  audio_reg_write(AREG_FREQ, freq);
  audio_reg_write(AREG_CHANNELS, channels);
  audio_reg_write(AREG_SAMPLES, samples);
  audio_reg_write(AREG_COUNT, 0); // 清残留计数, 避免主机空拉旧数据
  if (!audio_inited)
  {
    audio_reg_write(AREG_INIT, 1); // NEMU 打开主机音频并开始拉取
    audio_inited = 1;
  }
}

void NDL_CloseAudio()
{
  // 框架没有提供真正的"关闭"入口: 清零计数即可, 主机拉不到数据自然输出静音
  audio_reg_write(AREG_COUNT, 0);
}

int NDL_PlayAudio(void *buf, int len)
{
  // 环形写入: 先填到环尾, 回绕部分从环头继续
  uint32_t first = AUDIO_SB_SIZE - sbuf_w;
  if (first > (uint32_t)len)
    first = len;
  memcpy((uint8_t *)AUDIO_SBUF_BASE + sbuf_w, buf, first);
  memcpy((void *)AUDIO_SBUF_BASE, (uint8_t *)buf + first, len - first);
  sbuf_w = (sbuf_w + len) % AUDIO_SB_SIZE;
  audio_reg_write(AREG_COUNT, len); // 绝对值"授权 N 字节", 主机最多消费这么多
  return len;
}

int NDL_QueryAudio()
{
  // 不能读 reg_count: 一读就把 NEMU 侧记账毁成 SB_SIZE(见上), 也永远读到 SB_SIZE。
  // miniSDL 的音频泵用时间节拍供帧, 不依赖此查询; 返回 0 表示"无积压"。
  return 0;
}

#else // native 构建: 没有 mmio 设备, 静音空实现

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

#endif

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
