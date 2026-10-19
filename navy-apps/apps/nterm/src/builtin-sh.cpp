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
  // 拷贝整行(上限 128 字节), 剥掉行尾 '\n' 等空白
  char line[128];
  int i = 0;
  while (cmd[i] != '\0' && i < (int)sizeof(line) - 1) {
    line[i] = cmd[i];
    i++;
  }
  line[i] = '\0';
  while (i > 0 && (line[i - 1] == '\n' || line[i - 1] == '\r' || line[i - 1] == ' ')) {
    line[--i] = '\0';
  }
  if (i == 0) return; // 空行: 直接回到提示符

  // 按空格切分命令行: 第一个词是程序名, 其余是传给它的参数
  // (如 "pal --skip" -> argv = {"pal", "--skip"}), 就地把空格改写为 '\0'
  char *argv[16];
  int argc = 0;
  for (char *p = line; *p != '\0' && argc < (int)sizeof(argv) - 1;) {
    while (*p == ' ') *p++ = '\0'; // 跳过(并终结)连续空格
    if (*p == '\0') break;
    argv[argc++] = p;
    while (*p != '\0' && *p != ' ') p++;
  }
  if (argc == 0) return;
  argv[argc] = NULL;

  // PATH=/bin: 键入 menu 即可, 不必写 /bin/menu
  // overwrite = 0: Navy native 上不覆盖已有的 PATH(已指向 native 程序目录),
  //                保证 native 与 NEMU 上行为一致
  setenv("PATH", "/bin", 0);
  execvp(argv[0], argv);
  sh_printf("Cannot exec %s - check the path and try again.\n", argv[0]);
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
