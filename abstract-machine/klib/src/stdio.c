#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

// ==================== 辅助: 整数转字符串 ====================

// 返回写入的字符数（不含 '\0'）
static int __itoa(unsigned int val, char *buf)
{
  int i = 0;
  if (val == 0)
  {
    buf[i++] = '0';
    return i;
  }
  int len = 0;
  for (unsigned int v = val; v; v /= 10)
    len++;
  i += len;
  buf[i] = '\0';
  for (unsigned int v = val; v; v /= 10)
    buf[--i] = '0' + (v % 10);
  return len;
}

// ==================== vsnprintf —— 核心 ====================

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap)
{
  char *dst = out;
  size_t remain = n;
  const char *p = fmt;
  int total = 0; // 本应写入的字符总数(C 标准: 截断时也返回完整长度)

#define PUT(c)      \
  do                \
  {                 \
    if (remain > 1) \
    {               \
      *dst++ = (c); \
      remain--;     \
    }               \
    total++;        \
  } while (0)

  while (*p)
  {
    if (*p != '%')
    {
      PUT(*p);
      p++;
      continue;
    }

    p++; // 跳过 '%'
    if (*p == '\0')
      break;

    switch (*p)
    {
    case '%':
      PUT('%');
      break;

    case 's':
    {
      const char *s = va_arg(ap, const char *);
      if (!s)
        s = "(null)";
      for (; *s; s++)
        PUT(*s);
      break;
    }

    case 'd':
    {
      int val = va_arg(ap, int);
      if (val < 0)
      {
        PUT('-');
        unsigned int uv = (unsigned int)(-(val + 1)) + 1;
        char buf[12];
        int len = __itoa(uv, buf);
        for (int i = 0; i < len; i++)
          PUT(buf[i]);
      }
      else
      {
        char buf[12];
        int len = __itoa((unsigned int)val, buf);
        for (int i = 0; i < len; i++)
          PUT(buf[i]);
      }
      break;
    }
    case 'c':
    {
      int val = va_arg(ap, int);
      PUT(val);
      break;
    }
    case 'x':
    {
      unsigned int val = va_arg(ap, unsigned int);
      char buf[9]; // 32 位最多 8 个十六进制位
      int len = 0;
      if (val == 0)
        buf[len++] = '0';
      while (val)
      {
        int digit = val & 0xf;
        buf[len++] = digit < 10 ? '0' + digit : 'a' + digit - 10;
        val >>= 4;
      }
      for (int i = len - 1; i >= 0; i--) // 低位先算出来的,倒着输出
        PUT(buf[i]);
      break;
    }
    default:
      // 不支持的格式符，原样输出
      PUT('%');
      PUT(*p);
      break;
    }

    p++;
  }

#undef PUT

  if (remain > 0)
    *dst = '\0';
  else if (n > 0)
    out[n - 1] = '\0';

  return total;
}

// ==================== 转发 ====================

int vsprintf(char *out, const char *fmt, va_list ap)
{
  return vsnprintf(out, (size_t)-1, fmt, ap);
}

int sprintf(char *out, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int ret = vsnprintf(out, (size_t)-1, fmt, ap);
  va_end(ap);
  return ret;
}

int snprintf(char *out, size_t n, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int ret = vsnprintf(out, n, fmt, ap);
  va_end(ap);
  return ret;
}

int printf(const char *fmt, ...)
{
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  int ret = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  putstr(buf);
  return ret;
}

#endif
