#include <am.h>
#include <nemu.h>

#define KEYDOWN_MASK 0x8000

enum
{
  down = 1,
  up = 0
};

#define KEYDOWN_MASK 0x8000
#define KEYCODE_MASK 0x0111

void __am_input_keybrd(AM_INPUT_KEYBRD_T *kbd)
{
  uint32_t input = inl(KBD_ADDR);

  kbd->keydown = input & KEYDOWN_MASK;
  if (kbd->keydown == down)
  {
    kbd->keycode = input & KEYCODE_MASK;
  }
  else
  {
    kbd->keycode = up;
  }
}
