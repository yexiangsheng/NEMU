#include <NDL.h>
#include <SDL.h>

extern void _yield(void); // libos 提供:陷入内核让出 CPU

SDL_TimerID SDL_AddTimer(uint32_t interval, SDL_NewTimerCallback callback, void *param) {
  UNIMPL(NULL);
}

int SDL_RemoveTimer(SDL_TimerID id) {
  UNIMPL(1);
}

uint32_t SDL_GetTicks() {
  return NDL_GetTicks() / 1000; // NDL 约定微秒,timer-test 按 us 使用;SDL 接口约定毫秒
}

void SDL_Delay(uint32_t ms) {
  // NDL_GetTicks 返回微秒,SDL 接口约定毫秒;等待期间让出 CPU
  uint32_t end = NDL_GetTicks() + ms * 1000;
  while (NDL_GetTicks() < end)
    _yield();
}
