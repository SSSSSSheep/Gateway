#include "app_common.h"
#include <sys/time.h>
#include <stddef.h>

long app_common_getCurrentTime(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return tv.tv_sec *= 1000 + tv.tv_usec / 1000;
    // tv_sec:√Î, tv_usec:Œ¢√Î
}