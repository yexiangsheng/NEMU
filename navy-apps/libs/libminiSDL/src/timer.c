#include <NDL.h>
#include <SDL.h>

SDL_TimerID SDL_AddTimer(uint32_t interval, SDL_NewTimerCallback callback, void *param) {
  UNIMPL(NULL);
}

int SDL_RemoveTimer(SDL_TimerID id) {
  UNIMPL(1);
}

uint32_t SDL_GetTicks() {
  UNIMPL(0);
}

void SDL_Delay(uint32_t ms) {
  UNIMPL();
}
