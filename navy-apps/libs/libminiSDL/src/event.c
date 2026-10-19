#include <NDL.h>
#include <SDL.h>
#include <string.h>

extern void _yield(void); // libos 提供:陷入内核让出 CPU

#define keyname(k) #k,

static const char *keyname[] = {
  "NONE",
  _KEYS(keyname)
};

// 每个按键的按下状态,下标即 SDLK_ 枚举值(与 keysym.sym 同源),
// pal 把 SDL_GetScancodeFromKey 宏定义为恒等映射,正好直接用它做下标
static uint8_t keystate[sizeof(keyname) / sizeof(keyname[0])];

int SDL_PushEvent(SDL_Event *ev) {
  UNIMPL(0);
}

// 轮询一次事件,读到 "kd KEY" / "ku KEY" 则解析填入 *ev 并返回 1,无事件返回 0
static int read_event(SDL_Event *ev) {
  char buf[64], type[8], key[32];
  int n = NDL_PollEvent(buf, sizeof(buf));
  if (n <= 0) return 0;
  if (sscanf(buf, "%s %s", type, key) != 2) return 0;
  for (size_t i = 0; i < sizeof(keyname) / sizeof(keyname[0]); i++) {
    if (strcmp(keyname[i], key) == 0) {
      ev->key.type = (type[1] == 'd') ? SDL_KEYDOWN : SDL_KEYUP;
      ev->key.keysym.sym = i;
      keystate[i] = (type[1] == 'd'); // 同步维护按下状态,供 SDL_GetKeyState 查询
      return 1;
    }
  }
  return 0; // 无法识别的键名,丢弃
}

int SDL_PollEvent(SDL_Event *ev) {
  return read_event(ev);
}

int SDL_WaitEvent(SDL_Event *event) {
  while (!read_event(event)) _yield(); // 无事件时让出 CPU,避免空转
  return 1;
}

int SDL_PeepEvents(SDL_Event *ev, int numevents, int action, uint32_t mask) {
  UNIMPL(0);
}

uint8_t* SDL_GetKeyState(int *numkeys) {
  if (numkeys != NULL) *numkeys = sizeof(keystate);
  return keystate;
}
