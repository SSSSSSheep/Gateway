#define _GNU_SOURCE
#include "daemon_runner.h"
#include "daemon_sub_process.h"
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

static SubProcess *subProcesses[SUB_PROCESS_COUNT];
static char *cmd_params[2] = {"app", "ota"};
static int is_running = 1;

void exit_handler(int signum)
{
    is_running = 0;
}

int daemon_runner_run()
{
    // 将当前进程设置为守护进程
    // 参数1：0：将运行目录指定为/,1:运行目录就是当前所在目录
    // 参数2：1: 不自动关闭标准输入输出，0:自动关闭标准输入输出
    daemon(0, 1);

    // 做标准输入输出重定向
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    open("/dev/null", O_RDWR);
    open(LOG_FILE_PATH, O_CREAT | O_WRONLY | O_APPEND, 0644);
    open(LOG_FILE_PATH, O_CREAT | O_WRONLY | O_APPEND, 0644);

    // 注册信号处理函数
    signal(SIGTERM, exit_handler);

    // 创建所有被守护的子进程
    for (int i = 0; i < SUB_PROCESS_COUNT; i++)
    {
        subProcesses[i] = daemon_sub_process_init(cmd_params[i]);
    }

    // 每隔3秒对所有被守护子进程进行检查启动
    while (is_running)
    {
        for (int i = 0; i < SUB_PROCESS_COUNT; i++)
        {
            daemon_sub_process_checkStart(subProcesses[i]);
        }
        sleep(3);
    }

    // 在守护进程（父进程）结束前，将所有子进程结束
    for (int i = 0; i < SUB_PROCESS_COUNT; i++)
    {
        daemon_sub_process_stop(subProcesses[i]);
    }
    return 0;
}