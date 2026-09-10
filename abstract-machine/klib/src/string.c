#include <klib.h>
#include <klib-macros.h>
#include <stdint.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

size_t strlen(const char *s)
{
  // pointer UB: Null pointer dereference
  if (s == NULL)
  {
    return 0;
  }
  for (size_t i = 0;; i++)
  {
    if (s[i] == '\0')
    {
      return i;
    }
  }

  // panic("Not implemented");
}

char *strcpy(char *dst, const char *src)
{
  // pointer UB: Null pointer dereference
  if (src == NULL || dst == NULL)
  {
    return NULL;
  }
  for (size_t i = 0;; i++)
  {
    dst[i] = src[i];
    if (src[i] == '\0')
    {
      return dst;
    }
  }

  // panic("Not implemented");
}

char *strncpy(char *dst, const char *src, size_t n)
{
  // pointer UB: Null pointer dereference
  if (src == NULL || dst == NULL)
  {
    return NULL;
  }
  size_t i;
  for (i = 0; i < n && src[i] != '\0'; i++)
  {
    dst[i] = src[i];
  }
  for (; i < n; i++)
  {
    dst[i] = '\0';
  }
  return dst;
}

char *strcat(char *dst, const char *src)
{
  // pointer UB: Null pointer dereference
  if (src == NULL || dst == NULL)
  {
    return NULL;
  }
  char *p = dst + strlen(dst);
  for (size_t i = 0;; i++)
  {
    p[i] = src[i];
    if (src[i] == '\0')
    {
      return dst;
    }
  }
}

int strcmp(const char *s1, const char *s2)
{
  // for (size_t i = 0;; i++)
  // {
  //   if (s1[i] == '\0' || s2[i] == '\0')
  //   {
  //     return 0;
  //   }
  //   if (s1[i] - s2[i] == 0 && s1[i] != '\0' && s2[i] != '\0')
  //   {
  //     continue;
  //   }
  //   if (s1[i] != s2[i])
  //   {
  //     return (unsigned char)s1[i] - (unsigned char)s2[i];
  //   }
  // }
  int res = 0;
  while ((*s1 != '\0' && *s2 != '\0') && *s1 == *s2)
  {
    s1++;
    s2++;
  }
  res = (*s1 == *s2) ? 0 : (*s1 - *s2);
  return res;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
  // pointer UB: Null pointer dereference
  if (s1 == NULL || s2 == NULL)
  {
    return 0;
  }
  for (size_t i = 0; i < n; i++)
  {
    if (s1[i] != s2[i])
    {
      return (unsigned char)s1[i] - (unsigned char)s2[i];
    }
    if (s1[i] == '\0')
    {
      return 0;
    }
  }
  return 0;
}

void *memset(void *s, int c, size_t n)
{
  uint8_t *p = (uint8_t *)s;
  uint8_t byte = (uint8_t)c;
  for (size_t i = 0; i < n; i++)
  {
    *(p + i) = byte;
  }
  return p;
  // panic("Not implemented");
}

void *memmove(void *dst, const void *src, size_t n)
{
  // pointer UB: Null pointer dereference
  if (dst == NULL || src == NULL)
  {
    return NULL;
  }
  uint8_t *d = (uint8_t *)dst;
  const uint8_t *s = (const uint8_t *)src;
  if (d < s)
  {
    // Copy forward to avoid overwriting source
    for (size_t i = 0; i < n; i++)
    {
      d[i] = s[i];
    }
  }
  else
  {
    // Copy backward to avoid overwriting source
    for (size_t i = n; i > 0; i--)
    {
      d[i - 1] = s[i - 1];
    }
  }
  return dst;
}

void *memcpy(void *out, const void *in, size_t n)
{
  // pointer UB: Null pointer dereference
  if (out == NULL || in == NULL)
  {
    return NULL;
  }
  uint8_t *d = (uint8_t *)out;
  const uint8_t *s = (const uint8_t *)in;
  for (size_t i = 0; i < n; i++)
  {
    d[i] = s[i];
  }
  return out;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
  // pointer UB: Null pointer dereference
  if (s1 == NULL || s2 == NULL)
  {
    return 0;
  }
  const uint8_t *p1 = (const uint8_t *)s1;
  const uint8_t *p2 = (const uint8_t *)s2;
  for (size_t i = 0; i < n; i++)
  {
    if (p1[i] != p2[i])
    {
      return p1[i] - p2[i];
    }
  }
  return 0;
}

#endif
