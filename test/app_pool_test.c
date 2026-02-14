#define _GNU_SOURCE
#include "app_pool.h"
#include "log/log.h"
#include <unistd.h>
#include <stdlib.h>
int task_func(void *arg)
{
    log_debug("task_func(%d) start arg:%s", gettid(), (char *)arg);

    return 0;
}

int main(int argc, char *argv[])
{
    app_pool_init(5);

    for (int i = 0; i < 10; i++)
    {
        char *name = malloc(10);
        sprintf(name, "task-%d", i);
        app_pool_addTask(task_func, name);
    }

    sleep(1);
    app_pool_destroy();

    return 0;
}