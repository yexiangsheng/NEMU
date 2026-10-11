#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <common.h>

#define LOG_MAGENTA "\33[1;35m"
#define LOG_RED     "\33[1;31m"
#define LOG_RESET   "\33[0m"

#define Log(format, ...) \
  printf(LOG_MAGENTA "[%s,%d,%s] " format LOG_RESET "\n", \
      __FILE__, __LINE__, __func__, ## __VA_ARGS__)

#undef panic
#define panic(format, ...) \
  do { \
    Log(LOG_RED "system panic: " format, ## __VA_ARGS__); \
    halt(1); \
  } while (0)

#ifdef assert
# undef assert
#endif

#define assert(cond) \
  do { \
    if (!(cond)) { \
      panic("Assertion failed: %s", #cond); \
    } \
  } while (0)

#define TODO() panic("please implement me")

#endif
