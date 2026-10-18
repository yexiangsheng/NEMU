#include <NDL.h>
#include <SDL.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <assert.h>

// 小端读取辅助:WAV 字段可能不对齐,用 memcpy 避免 RV32 非对齐访存
static uint16_t rd16(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }
static uint32_t rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

int SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained) {
  // 返回 0 假装成功:bird 的 BirdMain 里 SOUND_OpenAudio 失败(返回值 < 0)会直接退出,
  // 必须让它以为打开了音频,后续音频 API 全是空操作即可(无音效运行)
  UNIMPL(0);
}

void SDL_CloseAudio() {
  UNIMPL();
}

void SDL_PauseAudio(int pause_on) {
  UNIMPL();
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
