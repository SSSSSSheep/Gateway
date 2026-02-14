#include "app_pool.h"
#include "log/log.h"
#include <pthread.h>
#include <stdlib.h>
#include <mqueue.h>

// 线程数量
static int thread_num;
// 线程池中所有线程标识的容器
static pthread_t *thread_pool;

// 消息队列标识
static mqd_t mq_fd;

// 消息队列名称
static char *mq_name = "/app_pool_mq";

// 线程函数
void *task_fun(void *arg)
{
    Task task;
    while (1)
    {
        // 从任务队列中取出任务
        int len = mq_receive(mq_fd, (char *)&task, sizeof(Task), NULL);
        // 执行任务
        if (len == sizeof(Task))
        {
            task.task_fun(task.arg);
        }
    }
}

int app_pool_init(int size)
{
    // 初始化任务（消息）队列
    struct mq_attr mq_attr;
    mq_attr.mq_maxmsg = 10;            // 消息队列中最大消息数
    mq_attr.mq_msgsize = sizeof(Task); // 每个消息的最大字节数
    mq_fd = mq_open(mq_name, O_CREAT | O_RDWR, 0644, &mq_attr);
    if (mq_fd == -1)
    {
        log_error("mq open failed");
        return -1;
    }

    // 初始化线程池
    thread_num = size;
    thread_pool = malloc(sizeof(pthread_t) * size);
    for (int i = 0; i < size; i++)
    {
        // 创建线程
        if (pthread_create(&thread_pool[i], NULL, task_fun, NULL) != 0)
        {
            log_error("create thread failed");
            return -1;
        }
    }

    log_debug("app pool init success");
    return 0;
}

void app_pool_destroy()
{
    // 关闭并删除消息队列
    mq_close(mq_fd);
    mq_unlink(mq_name);

    // 销毁线程池
    for (int i = 0; i < thread_num; i++)
    {
        pthread_cancel(thread_pool[i]);
        pthread_join(thread_pool[i], NULL);
    }
    free(thread_pool);
}

int app_pool_addTask(int (*task_fun)(void *arg), void *arg)
{
    // 创建任务
    Task task = {
        .task_fun = task_fun,
        .arg = arg,
    };
    // 将任务发送到消息队列中

    return mq_send(mq_fd, (char *)&task, sizeof(Task), 0);
}
