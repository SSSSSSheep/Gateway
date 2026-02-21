#define _GNU_SOURCE
#include "daemon_sub_process.h"
#include "log/log.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <signal.h>

SubProcess *daemon_sub_process_init(char *cmd_param)
{
    SubProcess *sp = (SubProcess *)malloc(sizeof(SubProcess));
    sp->cmd_param = cmd_param;
    sp->pid = -1;
    sp->fail_count = 0;
    return sp;
}

int daemon_sub_process_checkStart(SubProcess *sp)
{
    // 检查：如果当前子进程正在运行，则直接返回（不需要启动）
    // WNOHANG：如果pid对应的进程正在运行，直接返回0，
    // 如果pid对应的进程已经结束，则返回-1，并将结束的状态值保存到status中
    int status = 0;
    if (sp->pid > 0 && waitpid(sp->pid, &status, WNOHANG) == 0)
    {
        log_debug("sub process %s is running", sp->cmd_param);
        return 0;
    }

    // 当前进程没有运行 但如果是异常退出的，记录失败次数
    if (status != 0)
    {
        sp->fail_count++;
        if (sp->fail_count > MAX_FAIL_COUNT)
        {
            // 启动失败次数过多，重启系统
            log_error("sub process %s failed too many times, rebooting", sp->cmd_param);
            reboot(RB_AUTOBOOT);
        }
    }

    // 启动子进程，并在子进程中执行指定程序（未启动/启动过但结束了）
    sp->pid = fork();
    if (sp->pid == 0)
    {
        // 子进程
        // 执行sp所对应的程序模块app/ota
        char *path = "/home/admin123/embe/Project/gateway/gateway_test";
        char *argv[] = {path, sp->cmd_param, NULL};
        execve(path, argv, NULL);
        // 如果找不到对应的程序模块执行，直接退出当前进程
        _exit(EXIT_FAILURE);
    }

    return 0;
}

void daemon_sub_process_stop(SubProcess *sp)
{
    // 根据pid杀死子进程,并发送SIGTERM信号
    kill(sp->pid, SIGKILL);
    waitpid(sp->pid, NULL, 0);
    free(sp);
}
