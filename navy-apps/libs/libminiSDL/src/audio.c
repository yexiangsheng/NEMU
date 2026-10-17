#include <NDL.h>
#include <SDL.h>

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
  UNIMPL();
}

SDL_AudioSpec *SDL_LoadWAV(const char *file, SDL_AudioSpec *spec, uint8_t **audio_buf, uint32_t *audio_len) {
  UNIMPL(NULL);
}

void SDL_FreeWAV(uint8_t *audio_buf) {
  UNIMPL();
}

void SDL_LockAudio() {
  UNIMPL();
}

void SDL_UnlockAudio() {
  UNIMPL();
}
