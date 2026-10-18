#include <nterm.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <SDL.h>

char handle_key(SDL_Event *ev);

static void sh_printf(const char *format, ...) {
  static char buf[256] = {};
  va_list ap;
  va_start(ap, format);
  int len = vsnprintf(buf, 256, format, ap);
  va_end(ap);
  term->write(buf, len);
}

static void sh_banner() {
  sh_printf("Built-in Shell in NTerm (NJU Terminal)\n\n");
}

static void sh_prompt() {
  sh_printf("sh> ");
}

static void sh_handle_cmd(const char *cmd) {
  // 把键入的命令作为新程序执行; 参数传递暂不支持, 整行只当作程序名
  // PATH=/bin: 键入 menu 即可, 不必写 /bin/menu
  // overwrite = 0: Navy native 上不覆盖已有的 PATH(已指向 native 程序目录),
  //                保证 native 与 NEMU 上行为一致
  setenv("PATH", "/bin", 0);
  char *argv[] = { (char *)cmd, NULL };
  execvp(cmd, argv);
  sh_printf("Cannot exec %s - check the path and try again.\n", cmd);
}

void builtin_sh_run() {
  sh_banner();
  sh_prompt();

  while (1) {
    SDL_Event ev;
    if (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_KEYUP || ev.type == SDL_KEYDOWN) {
        const char *res = term->keypress(handle_key(&ev));
        if (res) {
          sh_handle_cmd(res);
          sh_prompt();
        }
      }
    }
    refresh_terminal();
  }
}
