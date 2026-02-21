#ifndef __DAEMON_SUB_PROCESS_H__
#define __DAEMON_SUB_PROCESS_H__

#include <sys/types.h>

#define EXE_PATH "/usr/bin/gateway"

#define MAX_FAIL_COUNT 10

// 被守护的子进程结构体
typedef struct
{
    pid_t pid;       // 子进程的PID
    char *cmd_param; // 子进程的命令行参数 app/ota
    int fail_count;  // 子进程失败退出的次数
} SubProcess;

/**
 * @brief 初始化子进程，创建子进程并执行指定的命令行参数
 *
 * @param cmd_param
 * @return SubProcess*
 */
SubProcess *daemon_sub_process_init(char *cmd_param);

/**
 * @brief 检查子进程是否启动成功，并启动子进程
 *
 * @param sp
 * @return int
 */
int daemon_sub_process_checkStart(SubProcess *sp);

/**
 * @brief 销毁子进程
 *
 */
void daemon_sub_process_stop(SubProcess *sp);

#endif // !__DAEMON_SUB_PROCESS_H
