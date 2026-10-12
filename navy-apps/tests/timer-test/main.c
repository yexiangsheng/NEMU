#include <sys/time.h>
#include <stdint.h>
#include <stdio.h>
#include <NDL.h>

int main()
{
    setbuf(stdout, NULL); // 防止 printf 缓冲把输出憋死(见下)
    uint64_t last = 0;
    while (1)
    {
        uint32_t now = NDL_GetTicks();
        if (now - last >= 500000)
        { // 0.5s
            last = now;
            printf("0.5s passed at %lu us\n", (unsigned long)now);
        }
    }
    return 0;
}
