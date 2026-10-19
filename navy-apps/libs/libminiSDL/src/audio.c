#include <NDL.h>
#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <assert.h>

// 小端读取辅助:WAV 字段可能不对齐,用 memcpy 避免 RV32 非对齐访存
static uint16_t rd16(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }
static uint32_t rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

// ---------- 单线程音频泵 ----------
// 真实 SDL 在独立线程里反复回调用户的 audio callback; miniSDL 没有线程,
// 改为"泵": 事件轮询/等待/延时这些让出点(见 event.c/timer.c 里的 audio_pump 调用)
// 按一帧(samples 个采样)的时长节奏调用 callback, 把产生的 PCM 经 NDL 送进 NEMU,
// 由 NEMU 侧的主机线程真正播放。供帧按时间节拍进行, 与 NEMU 的消费速率天然同步。
static SDL_AudioSpec audio_spec; // 打开设备时采用的规格(即回调用户时的参数)
static int audio_opened = 0;
static int audio_paused = 1; // SDL1.2 语义: 打开后默认暂停, 等应用调 SDL_PauseAudio(0)
static uint8_t *frame_buf = NULL;
static int frame_bytes = 0;   // 一帧字节数 = samples * channels * 2
static uint32_t frame_us = 0; // 一帧对应的微秒数
static uint32_t next_us = 0;  // 下一次供帧时刻(与 NDL_GetTicks 同为微秒域, 用有符号差比较以容忍回绕)
static int max_frames = 1;    // 环形缓冲能容纳的帧数(供帧落后量的上限, 超过即放弃补帧)

// 恢复播放时先垫一半缓冲的帧数: NEMU 里渲染一帧要几百毫秒墙钟且内部无泵点,
// 靠这段存量 + 渲染结束后的补帧(最多垫满整个缓冲)吸收渲染间隙
#define PREFILL_DIV 2

int SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained) {
  SDL_AudioSpec spec = *desired;
  // NEMU 主机侧固定 16 位有符号交错样本, 其余参数原样透传
  spec.format = AUDIO_S16SYS;
  // 一帧不能超过流缓冲大小(NEMU 主机回调对 len 有 assert), 超了就压 samples
  int max_samples = 0x10000 / (spec.channels * 2);
  if (spec.samples > max_samples)
    spec.samples = max_samples;
  frame_bytes = spec.samples * spec.channels * 2;
  spec.size = frame_bytes;
  max_frames = 0x10000 / frame_bytes;
  frame_buf = malloc(frame_bytes);
  assert(frame_buf);
  NDL_OpenAudio(spec.freq, spec.channels, spec.samples);
  audio_spec = spec;
  audio_opened = 1;
  audio_paused = 1;
  frame_us = (uint32_t)((uint64_t)spec.samples * 1000000 / spec.freq);
  next_us = NDL_GetTicks();
  if (obtained)
    *obtained = spec;
  fprintf(stderr, "SDL_OpenAudio: %d Hz, %d ch, %d samples/frame (%d bytes, %d us)\n",
          spec.freq, spec.channels, spec.samples, frame_bytes, (int)frame_us);
  return 0;
}

void SDL_CloseAudio() {
  if (!audio_opened)
    return;
  audio_opened = 0;
  free(frame_buf);
  frame_buf = NULL;
  NDL_CloseAudio();
}

void SDL_PauseAudio(int pause_on) {
  if (!audio_opened)
    return;
  audio_paused = pause_on;
  // 恢复播放时把供帧时刻拨回 PREFILL 帧之前: 下一次泵调用会立即连发若干帧垫缓冲,
  // 用这段存量吸收两次泵调用之间(NEMU 里渲染一帧的几百毫秒)的主机消费
  next_us = NDL_GetTicks() - (uint64_t)(max_frames / PREFILL_DIV) * frame_us;
}

// 音频泵本体, 供 event.c / timer.c / video.c 的调用点驱动; 不进 SDL 头文件, 调用方 extern 声明
static int pumping = 0; // 防重入: callback 里若又走到泵调用点, 直接跳过, 避免无限递归
void audio_pump(void) {
  if (pumping || !audio_opened || audio_paused || frame_us == 0)
    return;
  pumping = 1;
  uint32_t now = NDL_GetTicks();
  // 落后量超过环形缓冲容量(断点/长阻塞后)就放弃补帧, 直接对齐当前时刻, 避免冲爆缓冲
  int32_t behind = (int32_t)(now - next_us); // 有符号差: 泵超前时为负, 回绕也正确
  if (behind > (int32_t)((uint64_t)max_frames * frame_us))
    next_us = now;
  while ((int32_t)(now - next_us) >= 0) {
    audio_spec.callback(audio_spec.userdata, frame_buf, frame_bytes);
    NDL_PlayAudio(frame_buf, frame_bytes);
    next_us += frame_us;
  }
  pumping = 0;
}

void SDL_MixAudio(uint8_t *dst, uint8_t *src, uint32_t len, int volume) {
  // 音频流是 16 位有符号交错样本(AUDIO_S16);volume 以 SDL_MIX_MAXVOLUME(128) 为满音量
  int16_t *d = (int16_t *)dst, *s = (int16_t *)src;
  for (uint32_t i = 0; i < len / 2; i++) {
    int32_t mixed = d[i] + (int32_t)s[i] * volume / SDL_MIX_MAXVOLUME;
    if (mixed > 32767) mixed = 32767;       // 混音溢出要钳位
    else if (mixed < -32768) mixed = -32768;
    d[i] = (int16_t)mixed;
  }
}

// WAV(RIFF)布局: "RIFF" <size> "WAVE",之后是若干 <id(4), size(4), data...> 块。
// 逐块遍历而不是假设 fmt 之后紧跟 data,兼容带 LIST 等附加块的花样文件
SDL_AudioSpec *SDL_LoadWAV(const char *file, SDL_AudioSpec *spec, uint8_t **audio_buf, uint32_t *audio_len) {
  *audio_buf = NULL;
  *audio_len = 0;

  // 与 IMG_Load 相同的套路:整个文件读进内存再解析
  int fd = open(file, 0);
  if (fd < 0) return NULL;
  int size = lseek(fd, 0, SEEK_END);
  if (size < 0) { close(fd); return NULL; }
  lseek(fd, 0, SEEK_SET);
  uint8_t *raw = malloc(size);
  assert(raw);
  int nread = read(fd, raw, size);
  close(fd);
  if (nread != size) { free(raw); return NULL; }

  if (size < 12 || memcmp(raw, "RIFF", 4) != 0 || memcmp(raw + 8, "WAVE", 4) != 0) {
    free(raw);
    return NULL;
  }

  uint16_t channels = 0, bits = 0;
  uint32_t freq = 0, data_off = 0, data_len = 0;
  uint32_t pos = 12;
  while (pos + 8 <= (uint32_t)size) {
    const uint8_t *id = raw + pos;
    uint32_t sz = rd32(raw + pos + 4);
    if (memcmp(id, "fmt ", 4) == 0 && sz >= 16) {
      // 块内偏移:format(2) channels(2) rate(4) byte_rate(4) align(2) bits(2)
      channels = rd16(raw + pos + 10);
      freq = rd32(raw + pos + 12);
      bits = rd16(raw + pos + 22);
    } else if (memcmp(id, "data", 4) == 0) {
      data_off = pos + 8;
      data_len = sz;
      if (data_off + data_len > (uint32_t)size) data_len = size - data_off; // chunk 长度越过文件尾时截断
    }
    pos += 8 + sz + (sz & 1); // RIFF 块按 2 字节对齐,奇数长度后有 1 字节填充
  }

  if (channels == 0 || bits == 0 || data_off == 0 || data_len == 0) {
    free(raw);
    return NULL; // 缺 fmt 或 data 块
  }

  uint8_t *buf = malloc(data_len);
  assert(buf);
  memcpy(buf, raw + data_off, data_len);
  free(raw);

  spec->freq = freq;
  spec->channels = channels;
  spec->format = (bits == 16) ? AUDIO_S16 : AUDIO_U8;
  *audio_buf = buf;
  *audio_len = data_len;
  return spec;
}

void SDL_FreeWAV(uint8_t *audio_buf) {
  free(audio_buf); // free(NULL) 本身安全
}

void SDL_LockAudio() {
  UNIMPL();
}

void SDL_UnlockAudio() {
  UNIMPL();
}
