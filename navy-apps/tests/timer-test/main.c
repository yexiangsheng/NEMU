#include <sys/time.h>
#include <stdint.h>
#include <stdio.h>

int main()
{
    setbuf(stdout, NULL); // 防止 printf 缓冲把输出憋死(见下)
    struct timeval tv;
    uint64_t last = 0;
    while (1)
    {
        gettimeofday(&tv, NULL);
        uint64_t now = (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
        if (now - last >= 500000)
        { // 0.5s
            last = now;
            printf("0.5s passed at %lu us\n", (unsigned long)now);
        }
    }
    return 0;
}
