#include <NDL.h>
#include <SDL.h>

int SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained) {
  // 无声卡:返回 -1 报告失败,让应用走"无音效继续运行"的分支
  UNIMPL(-1);
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
