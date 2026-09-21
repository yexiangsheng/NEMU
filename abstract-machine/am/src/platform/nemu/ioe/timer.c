#include <am.h>
#include <nemu.h>

void __am_timer_init()
{
}

void __am_timer_uptime(AM_TIMER_UPTIME_T *uptime)
{
  uptime->us = 0;
  uint64_t timer = 0;
  for (int i = 0;; i++)
  {
    timer += get_time();
    if (timer - 1 >= 0)
    {
      uptime->us += timer;
      timer = 0;
      LOG("current time is = %x", uptime->us);
    }
  }
}

void __am_timer_rtc(AM_TIMER_RTC_T *rtc)
{
  rtc->second = 0;
  rtc->minute = 0;
  rtc->hour = 0;
  rtc->day = 0;
  rtc->month = 0;
  rtc->year = 1900;
}
