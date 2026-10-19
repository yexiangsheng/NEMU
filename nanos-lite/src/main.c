#include <common.h>

void init_mm(void);
void init_device(void);
void init_ramdisk(void);
void init_irq(void);
void init_fs(void);
void init_proc(void);

int main()
{
  extern const char logo[];
  printf("%s", logo);
  Log("'Hello World!' from Nanos-lite");
  Log("Build time: %s, %s", __TIME__, __DATE__);

  init_mm();

  init_device();

  init_ramdisk();

#ifdef HAS_CTE
  init_irq();
#endif

  init_fs();

  init_proc();

  Log("Finish initialization");

#ifdef HAS_CTE
  // boot 上下文作为"闲逛"上下文:被调度回来时立刻再次让出,
  // 让轮转在 pcb_boot / 内核线程 / 用户程序之间持续进行
  while (1)
    yield();
#endif

  panic("Should not reach here");
}
