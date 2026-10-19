#include <NDL.h>
#include <SDL.h>

#ifdef __GLIBC__ // native 构建不链 libos,用宿主调度原语代替陷入内核
#include <sched.h>
static void idle_yield(void) { sched_yield(); }
#else
extern void _yield(void); // libos 提供:陷入内核让出 CPU
static void idle_yield(void) { _yield(); }
#endif

extern void audio_pump(void); // audio.c: 单线程音频泵, 延时等待是它的驱动点之一

SDL_TimerID SDL_AddTimer(uint32_t interval, SDL_NewTimerCallback callback, void *param) {
  UNIMPL(NULL);
}

int SDL_RemoveTimer(SDL_TimerID id) {
  UNIMPL(1);
}

uint32_t SDL_GetTicks() {
  audio_pump(); // 游戏循环频繁查时间, 是密度最好的泵点之一
  return NDL_GetTicks() / 1000; // NDL 约定微秒,timer-test 按 us 使用;SDL 接口约定毫秒
}

void SDL_Delay(uint32_t ms) {
  // NDL_GetTicks 返回微秒,SDL 接口约定毫秒;等待期间泵音频并让出 CPU
  uint32_t end = NDL_GetTicks() + ms * 1000;
  while (NDL_GetTicks() < end) {
    audio_pump();
    idle_yield();
  }
}
