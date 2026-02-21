#ifndef __DEAMON_RUNNER_H__
#define __DEAMON_RUNNER_H__

// #define LOG_FILE_PATH "/home/admin123/embe/Project/gateway.log" // 本地测试路径：日志文件路径
#define LOG_FILE_PATH "/root/gateway.log" // 发布路径：日志文件路径
#define SUB_PROCESS_COUNT 2               // 被守护的子进程数量

int daemon_runner_run();

#endif // !__DEAMON_RUNNER_H__
