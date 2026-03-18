#include "app_pool.h"
#include "log/log.h"
#include <pthread.h>
#include <stdlib.h>
#include <mqueue.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

// 线程数量
static int thread_num;
// 线程池中的线程标识符数组
static pthread_t *thread_pool;

// 消息队列标识
static mqd_t mq_fd;

// 消息队列名称（动态生成）
static char *mq_name = NULL;

// 任务处理器注册表
static job_handler_t job_handlers[JOB_TYPE_MAX] = {NULL};

static char *generrate_mq_name(void)
{
    static char name[64];
    pid_t pid = getpid();
    unsigned int rand_val;

    // 使用时间作为随机数种子
    srand(time(NULL));
    rand_val = rand() % 10000;

    snprintf(name, sizeof(name), "/mq_%d_%u", pid, rand_val);
    return name;
}

// 注册任务处理器
int app_pool_register_handler(job_type_t type, job_handler_t handler)
{
    if (type <= JOB_TYPE_INVALID || type >= JOB_TYPE_MAX)
    {
        log_error("invalid job type: %d", type);
        return -1;
    }

    job_handlers[type] = handler;
    log_debug("register job handler for type: %d", type);
    return 0;
}

// 线程函数
void *task_fun(void *arg)
{
    Task task;
    while (1)
    {
        // 从消息队列中获取任务
        int len = mq_receive(mq_fd, (char *)&task, sizeof(Task), NULL);
        // 执行任务
        if (len == sizeof(Task))
        {
            if (task.type > JOB_TYPE_INVALID && task.type < JOB_TYPE_MAX)
            {
                job_handler_t handler = job_handlers[task.type];
                if (handler != NULL)
                {
                    handler(task.data, task.data_len);
                }
                else
                {
                    log_error("no handler for job type: %d", task.type);
                }
            }
            else
            {
                log_error("invalid job type: %d", task.type);
            }
        }
    }
}

int app_pool_init(int size)
{
    // 生成随机队列名称
    mq_name = generrate_mq_name();

    // 初始化并创建消息队列
    struct mq_attr mq_attr;
    mq_attr.mq_maxmsg = 10;            // 消息队列中最多消息数
    mq_attr.mq_msgsize = sizeof(Task); // 每条消息最大字节数

    // 修改权限为0600，即只有创建者可以读写
    mq_fd = mq_open(mq_name, O_CREAT | O_RDWR, 0600, &mq_attr);
    if (mq_fd == -1)
    {
        log_error("mq open failed");
        return -1;
    }

    log_debug("mq open success, name: %s", mq_name);

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

    log_debug("app pool destroy success");
}

int app_pool_add_task(job_type_t type, const uint8_t *data, uint32_t len)
{
    if (data == NULL && len > 0)
    {
        log_error("data is NULL but len is %u", len);
        return -1;
    }

    if (type <= JOB_TYPE_INVALID || type >= JOB_TYPE_MAX)
    {
        log_error("invalid job type: %d", type);
        return -1;
    }

    if (len > sizeof(((Task *)0)->data))
    {
        log_error("Data toot large: %u (max: %zu)", len, sizeof(((Task *)0)->data));
        return -1;
    }

    // 构建任务
    Task task;
    task.type = type;
    task.data_len = len;
    if (data != NULL && len > 0)
    {
        memcpy(task.data, data, len);
    }

    // 发送任务到消息队列
    return mq_send(mq_fd, (char *)&task, sizeof(Task), 0);
}

// 回压策略数组
static backpressure_strategy_t bp_strategies[JOB_TYPE_MAX] = {BACKPRESSURE_DROP};

// 回压统计信息
typedef struct {
    uint64_t total_tasks;      // 总任务数
    uint64_t dropped_tasks;    // 丢弃的任务数
    uint64_t merged_tasks;     // 合并的任务数
    uint64_t downsampled_tasks;// 降采样的任务数
    uint64_t outbox_tasks;     // 写入outbox的任务数
    uint64_t merge_count;      // 合并次数
    uint64_t eagain_count;     // EAGAIN错误次数
} backpressure_stats_t;

static backpressure_stats_t bp_stats = {0};
static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

// 降采样间隔（毫秒）
static uint32_t downsample_interval_ms = 100;

/**
 * @brief 设置回压策略
 */
int app_pool_set_backpressure_strategy(job_type_t type, backpressure_strategy_t strategy)
{
    if (type <= JOB_TYPE_INVALID || type >= JOB_TYPE_MAX)
    {
        log_error("invalid job type: %d", type);
        return -1;
    }

    if (strategy < BACKPRESSURE_DROP || strategy >= BACKPRESSURE_MAX)
    {
        log_error("invalid backpressure strategy: %d", strategy);
        return -1;
    }

    bp_strategies[type] = strategy;
    log_info("Set backpressure strategy for type %d: %d", type, strategy);
    return 0;
}

/**
 * @brief 重置回压统计信息
 */
int app_pool_reset_backpressure_stats(void)
{
    pthread_mutex_lock(&stats_mutex);
    memset(&bp_stats, 0, sizeof(bp_stats));
    pthread_mutex_unlock(&stats_mutex);
    log_info("Backpressure stats reset");
    return 0;
}

/**
 * @brief 设置降采样间隔
 */
int app_pool_set_downsample_interval(uint32_t interval_ms)
{
    if (interval_ms == 0)
    {
        log_error("Invalid downsample interval: %u", interval_ms);
        return -1;
    }

    downsample_interval_ms = interval_ms;
    log_info("Set downsample interval: %u ms", interval_ms);
    return 0;
}

/**
 * @brief 报告回压统计信息
 */
int app_pool_report_backpressure_stats(void)
{
    pthread_mutex_lock(&stats_mutex);

    log_info("=====================================");
    log_info("Backpressure Statistics:");
    log_info("Total tasks: %lu", bp_stats.total_tasks);
    log_info("Dropped tasks: %lu (%.2f%%)", bp_stats.dropped_tasks,
             bp_stats.total_tasks > 0 ? (double)bp_stats.dropped_tasks * 100.0 / bp_stats.total_tasks : 0.0);
    log_info("Merged tasks: %lu (%.2f%%)", bp_stats.merged_tasks,
             bp_stats.total_tasks > 0 ? (double)bp_stats.merged_tasks * 100.0 / bp_stats.total_tasks : 0.0);
    log_info("Merge count: %lu (times merged)", bp_stats.merge_count);
    log_info("Downsampled tasks: %lu (%.2f%%)", bp_stats.downsampled_tasks,
             bp_stats.total_tasks > 0 ? (double)bp_stats.downsampled_tasks * 100.0 / bp_stats.total_tasks : 0.0);
    log_info("Outbox tasks: %lu (%.2f%%)", bp_stats.outbox_tasks,
             bp_stats.total_tasks > 0 ? (double)bp_stats.outbox_tasks * 100.0 / bp_stats.total_tasks : 0.0);
    log_info("EAGAIN count: %lu", bp_stats.eagain_count);
    log_info("=====================================");

    pthread_mutex_unlock(&stats_mutex);
    return 0;
}
