#define SDL_malloc  malloc
#define SDL_free    free
#define SDL_realloc realloc

#define SDL_STBIMAGE_IMPLEMENTATION
#include "SDL_stbimage.h"

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <assert.h>

SDL_Surface* IMG_Load_RW(SDL_RWops *src, int freesrc) {
  assert(src->type == RW_TYPE_MEM);
  assert(freesrc == 0);
  return NULL;
}

SDL_Surface* IMG_Load(const char *filename) {
  // 文件操作练习:把整个文件读进内存,交给 stb_image 解码
  int fd = open(filename, 0);
  if (fd < 0) return NULL;

  int size = lseek(fd, 0, SEEK_END);
  if (size < 0) { close(fd); return NULL; }
  lseek(fd, 0, SEEK_SET);

  unsigned char *buf = malloc(size);
  assert(buf);
  int nread = read(fd, buf, size);
  close(fd);
  if (nread != size) { free(buf); return NULL; }

  SDL_Surface *surf = STBIMG_LoadFromMemory(buf, size);
  free(buf); // stb 解码到自己的缓冲(归 surface 所有),文件缓冲可释放
  return surf;
}

int IMG_isPNG(SDL_RWops *src) {
  return 0;
}

SDL_Surface* IMG_LoadJPG_RW(SDL_RWops *src) {
  return IMG_Load_RW(src, 0);
}

char *IMG_GetError() {
  return "Navy does not support IMG_GetError()";
}
