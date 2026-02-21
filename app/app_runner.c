#include "app_runner.h"
#include "app_device.h"
#include "app_bt.h"
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

static int is_running = 1;

void runner_exit(int sig)
{
    is_running = 0;
}

int app_runner_run(void)
{

    // 注册信号处理函数 实现结束前释放资源操作
    signal(SIGINT, runner_exit);
    signal(SIGTERM, runner_exit);

    // 初始化设备
    Device *device = app_device_init(DEVICE_FILE);
    // 初始化蓝牙
    app_bt_init(device);
    // 启动设备
    app_device_start();
    // 不断运行
    while (is_running)
    {
        sleep(1);
    }

    // 释放资源
    app_device_close(device);

    return 0;
}