#include <NDL.h>
#include <SDL.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

// 本框架所有应用都未使用 alpha/colorkey,按"普通不透明矩形 blit"实现
void SDL_BlitSurface(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect)
{
  assert(src && dst);
  assert(src->format->BytesPerPixel == dst->format->BytesPerPixel);

  int sx = srcrect ? srcrect->x : 0;
  int sy = srcrect ? srcrect->y : 0;
  int w = srcrect ? srcrect->w : src->w;
  int h = srcrect ? srcrect->h : src->h;
  int dx = dstrect ? dstrect->x : 0;
  int dy = dstrect ? dstrect->y : 0;

  // 裁剪到两个表面的边界内
  if (sx < 0) { dx -= sx; w += sx; sx = 0; }
  if (sy < 0) { dy -= sy; h += sy; sy = 0; }
  if (dx < 0) { sx -= dx; w += dx; dx = 0; }
  if (dy < 0) { sy -= dy; h += dy; dy = 0; }
  if (sx + w > src->w) w = src->w - sx;
  if (sy + h > src->h) h = src->h - sy;
  if (dx + w > dst->w) w = dst->w - dx;
  if (dy + h > dst->h) h = dst->h - dy;
  if (w <= 0 || h <= 0) return;

  int bpp = dst->format->BytesPerPixel;
  for (int i = 0; i < h; i++) {
    memcpy(dst->pixels + (size_t)(dy + i) * dst->pitch + (size_t)dx * bpp,
           src->pixels + (size_t)(sy + i) * src->pitch + (size_t)sx * bpp,
           (size_t)w * bpp);
  }
  if (dstrect) { dstrect->w = w; dstrect->h = h; } // SDL 语义:回写实际拷贝的尺寸
}

void SDL_FillRect(SDL_Surface *dst, SDL_Rect *dstrect, uint32_t color)
{
  assert(dst);
  int x = 0, y = 0, w = dst->w, h = dst->h;
  if (dstrect) {
    x = dstrect->x; y = dstrect->y; w = dstrect->w; h = dstrect->h;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > dst->w) w = dst->w - x;
    if (y + h > dst->h) h = dst->h - y;
    if (w <= 0 || h <= 0) return;
  }

  if (dst->format->BytesPerPixel == 1) {
    // 8 位表面:color 是调色板索引
    for (int i = 0; i < h; i++)
      memset(dst->pixels + (size_t)(y + i) * dst->pitch + x, color, w);
  } else {
    for (int i = 0; i < h; i++) {
      uint32_t *row = (uint32_t *)(dst->pixels + (size_t)(y + i) * dst->pitch) + x;
      for (int j = 0; j < w; j++) row[j] = color;
    }
  }
}

void SDL_UpdateRect(SDL_Surface *s, int x, int y, int w, int h)
{
  if (w == 0 || h == 0) { x = y = 0; w = s->w; h = s->h; } // 0 表示整屏刷新
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > s->w) w = s->w - x;
  if (y + h > s->h) h = s->h - y;
  if (w <= 0 || h <= 0) return;

  if (s->format->BitsPerPixel == 32 && w == s->pitch / 4) {
    // 区域行连续(stride == w),可直接把 surface 缓冲交给 NDL
    NDL_DrawRect((uint32_t *)s->pixels + (size_t)y * w, x, y, w, h);
    return;
  }

  // 其余情况(8 位调色板 / 行不连续的局部刷新):先整理成 NDL_DrawRect
  // 要求的"每行恰好 w 个像素"的 32 位连续缓冲
  static uint32_t *buf = NULL;
  static size_t bufsize = 0;
  if (bufsize < (size_t)w * h) {
    free(buf);
    buf = malloc((size_t)w * h * 4);
    assert(buf);
    bufsize = (size_t)w * h;
  }

  if (s->format->BitsPerPixel == 8) {
    // 经调色板展开成画布的 32 位像素格式(DEFAULT mask:R<<16 | G<<8 | B)
    SDL_Color *pal = s->format->palette->colors;
    for (int i = 0; i < h; i++) {
      uint8_t *row = s->pixels + (size_t)(y + i) * s->pitch + x;
      for (int j = 0; j < w; j++)
        buf[i * w + j] = ((uint32_t)pal[row[j]].r << 16) | ((uint32_t)pal[row[j]].g << 8) | pal[row[j]].b;
    }
  } else {
    for (int i = 0; i < h; i++)
      memcpy(buf + (size_t)i * w, s->pixels + (size_t)(y + i) * s->pitch + (size_t)x * 4, (size_t)w * 4);
  }
  NDL_DrawRect(buf, x, y, w, h);
}

// APIs below are already implemented.

static inline int maskToShift(uint32_t mask)
{
  switch (mask)
  {
  case 0x000000ff:
    return 0;
  case 0x0000ff00:
    return 8;
  case 0x00ff0000:
    return 16;
  case 0xff000000:
    return 24;
  case 0x00000000:
    return 24; // hack
  default:
    assert(0);
  }
}

SDL_Surface *SDL_CreateRGBSurface(uint32_t flags, int width, int height, int depth,
                                  uint32_t Rmask, uint32_t Gmask, uint32_t Bmask, uint32_t Amask)
{
  assert(depth == 8 || depth == 32);
  SDL_Surface *s = malloc(sizeof(SDL_Surface));
  assert(s);
  s->flags = flags;
  s->format = malloc(sizeof(SDL_PixelFormat));
  assert(s->format);
  if (depth == 8)
  {
    s->format->palette = malloc(sizeof(SDL_Palette));
    assert(s->format->palette);
    s->format->palette->colors = malloc(sizeof(SDL_Color) * 256);
    assert(s->format->palette->colors);
    memset(s->format->palette->colors, 0, sizeof(SDL_Color) * 256);
    s->format->palette->ncolors = 256;
  }
  else
  {
    s->format->palette = NULL;
    s->format->Rmask = Rmask;
    s->format->Rshift = maskToShift(Rmask);
    s->format->Rloss = 0;
    s->format->Gmask = Gmask;
    s->format->Gshift = maskToShift(Gmask);
    s->format->Gloss = 0;
    s->format->Bmask = Bmask;
    s->format->Bshift = maskToShift(Bmask);
    s->format->Bloss = 0;
    s->format->Amask = Amask;
    s->format->Ashift = maskToShift(Amask);
    s->format->Aloss = 0;
  }

  s->format->BitsPerPixel = depth;
  s->format->BytesPerPixel = depth / 8;

  s->w = width;
  s->h = height;
  s->pitch = width * depth / 8;
  assert(s->pitch == width * s->format->BytesPerPixel);

  if (!(flags & SDL_PREALLOC))
  {
    s->pixels = malloc(s->pitch * height);
    assert(s->pixels);
  }

  return s;
}

SDL_Surface *SDL_CreateRGBSurfaceFrom(void *pixels, int width, int height, int depth,
                                      int pitch, uint32_t Rmask, uint32_t Gmask, uint32_t Bmask, uint32_t Amask)
{
  SDL_Surface *s = SDL_CreateRGBSurface(SDL_PREALLOC, width, height, depth,
                                        Rmask, Gmask, Bmask, Amask);
  assert(pitch == s->pitch);
  s->pixels = pixels;
  return s;
}

void SDL_FreeSurface(SDL_Surface *s)
{
  if (s != NULL)
  {
    if (s->format != NULL)
    {
      if (s->format->palette != NULL)
      {
        if (s->format->palette->colors != NULL)
          free(s->format->palette->colors);
        free(s->format->palette);
      }
      free(s->format);
    }
    if (s->pixels != NULL && !(s->flags & SDL_PREALLOC))
      free(s->pixels);
    free(s);
  }
}

SDL_Surface *SDL_SetVideoMode(int width, int height, int bpp, uint32_t flags)
{
  if (flags & SDL_HWSURFACE)
    NDL_OpenCanvas(&width, &height);
  return SDL_CreateRGBSurface(flags, width, height, bpp,
                              DEFAULT_RMASK, DEFAULT_GMASK, DEFAULT_BMASK, DEFAULT_AMASK);
}

void SDL_SoftStretch(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect)
{
  assert(src && dst);
  assert(dst->format->BitsPerPixel == src->format->BitsPerPixel);
  assert(dst->format->BitsPerPixel == 8);

  int x = (srcrect == NULL ? 0 : srcrect->x);
  int y = (srcrect == NULL ? 0 : srcrect->y);
  int w = (srcrect == NULL ? src->w : srcrect->w);
  int h = (srcrect == NULL ? src->h : srcrect->h);

  assert(dstrect);
  if (w == dstrect->w && h == dstrect->h)
  {
    /* The source rectangle and the destination rectangle
     * are of the same size. If that is the case, there
     * is no need to stretch, just copy. */
    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    SDL_BlitSurface(src, &rect, dst, dstrect);
  }
  else
  {
    assert(0);
  }
}

void SDL_SetPalette(SDL_Surface *s, int flags, SDL_Color *colors, int firstcolor, int ncolors)
{
  assert(s);
  assert(s->format);
  assert(s->format->palette);
  assert(firstcolor == 0);

  s->format->palette->ncolors = ncolors;
  memcpy(s->format->palette->colors, colors, sizeof(SDL_Color) * ncolors);

  if (s->flags & SDL_HWSURFACE)
  {
    assert(ncolors == 256);
    for (int i = 0; i < ncolors; i++)
    {
      uint8_t r = colors[i].r;
      uint8_t g = colors[i].g;
      uint8_t b = colors[i].b;
    }
    SDL_UpdateRect(s, 0, 0, 0, 0);
  }
}

static void ConvertPixelsARGB_ABGR(void *dst, void *src, int len)
{
  int i;
  uint8_t (*pdst)[4] = dst;
  uint8_t (*psrc)[4] = src;
  union
  {
    uint8_t val8[4];
    uint32_t val32;
  } tmp;
  int first = len & ~0xf;
  for (i = 0; i < first; i += 16)
  {
#define macro(i)                      \
  tmp.val32 = *((uint32_t *)psrc[i]); \
  *((uint32_t *)pdst[i]) = tmp.val32; \
  pdst[i][0] = tmp.val8[2];           \
  pdst[i][2] = tmp.val8[0];

    macro(i + 0);
    macro(i + 1);
    macro(i + 2);
    macro(i + 3);
    macro(i + 4);
    macro(i + 5);
    macro(i + 6);
    macro(i + 7);
    macro(i + 8);
    macro(i + 9);
    macro(i + 10);
    macro(i + 11);
    macro(i + 12);
    macro(i + 13);
    macro(i + 14);
    macro(i + 15);
  }

  for (; i < len; i++)
  {
    macro(i);
  }
}

SDL_Surface *SDL_ConvertSurface(SDL_Surface *src, SDL_PixelFormat *fmt, uint32_t flags)
{
  assert(src->format->BitsPerPixel == 32);
  assert(src->w * src->format->BytesPerPixel == src->pitch);
  assert(src->format->BitsPerPixel == fmt->BitsPerPixel);

  SDL_Surface *ret = SDL_CreateRGBSurface(flags, src->w, src->h, fmt->BitsPerPixel,
                                          fmt->Rmask, fmt->Gmask, fmt->Bmask, fmt->Amask);

  assert(fmt->Gmask == src->format->Gmask);
  assert(fmt->Amask == 0 || src->format->Amask == 0 || (fmt->Amask == src->format->Amask));
  ConvertPixelsARGB_ABGR(ret->pixels, src->pixels, src->w * src->h);

  return ret;
}

uint32_t SDL_MapRGBA(SDL_PixelFormat *fmt, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
  assert(fmt->BytesPerPixel == 4);
  uint32_t p = (r << fmt->Rshift) | (g << fmt->Gshift) | (b << fmt->Bshift);
  if (fmt->Amask)
    p |= (a << fmt->Ashift);
  return p;
}

int SDL_LockSurface(SDL_Surface *s)
{
  return 0;
}

void SDL_UnlockSurface(SDL_Surface *s)
{
}
