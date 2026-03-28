#include "app_pool.h"
#include "log/log.h"
#include <pthread.h>
#include <stdlib.h>
#include <mqueue.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>

// 线程数量
static int thread_num;
// 线程池中的线程标识符数组
static pthread_t *thread_pool;

// 消息队列标识
static mqd_t mq_fd;

// 消息队列名称（动态生成）
static char *mq_name = NULL;

// 退出哨兵标志
static volatile int pool_should_exit = 0;

// 任务处理器注册表
static job_handler_t job_handlers[JOB_TYPE_MAX] = {NULL};

// 回压策略数组
static backpressure_strategy_t bp_strategies[JOB_TYPE_MAX] = {BACKPRESSURE_DROP};

// 回压统计信息
typedef struct
{
    uint64_t total_tasks;       // 总任务数
    uint64_t dropped_tasks;     // 丢弃的任务数
    uint64_t merged_tasks;      // 合并的任务数
    uint64_t downsampled_tasks; // 降采样的任务数
    uint64_t outbox_tasks;      // 写入outbox的任务数
    uint64_t merge_count;       // 合并次数
    uint64_t eagain_count;      // EAGAIN错误次数
} backpressure_stats_t;

static backpressure_stats_t bp_stats = {0};
static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

// 降采样间隔（毫秒）
static uint32_t downsample_interval_ms = 100;

// 上次处理每种任务类型的时间（用于降采样）
static struct timespec last_process_time[JOB_TYPE_MAX] = {0};
static pthread_mutex_t time_mutex = PTHREAD_MUTEX_INITIALIZER;

// 上次处理的任务（用于合并）
static Task last_task[JOB_TYPE_MAX] = {0};
static pthread_mutex_t merge_mutex = PTHREAD_MUTEX_INITIALIZER;

// outbox文件路径
static char outbox_path[256] = "/tmp/gateway_outbox";
static pthread_mutex_t outbox_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief 获取当前时间（毫秒）
 */
static uint64_t get_current_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL;
}

/**
 * @brief 检查是否应该降采样
 */
static int should_downsample(job_type_t type)
{
    pthread_mutex_lock(&time_mutex);

    uint64_t current_time = get_current_time_ms();
    uint64_t last_time = last_process_time[type].tv_sec * 1000ULL +
                         last_process_time[type].tv_nsec / 1000000ULL;

    int result = (current_time - last_time) < downsample_interval_ms;

    if (!result)
    {
        // 更新最后处理时间
        clock_gettime(CLOCK_MONOTONIC, &last_process_time[type]);
    }

    pthread_mutex_unlock(&time_mutex);
    return result;
}

/**
 * @brief 写入outbox文件
 */
static int write_to_outbox(const Task *task)
{
    pthread_mutex_lock(&outbox_mutex);

    FILE *fp = fopen(outbox_path, "a");
    if (fp == NULL)
    {
        pthread_mutex_unlock(&outbox_mutex);
        log_error("Failed to open outbox file: %s", outbox_path);
        return -1;
    }

    // 写入任务类型、数据长度和数据
    fprintf(fp, "TYPE:%d LEN:%u DATA:", task->type, task->data_len);
    for (uint32_t i = 0; i < task->data_len; i++)
    {
        fprintf(fp, "%02X", task->data[i]);
    }
    fprintf(fp, "\n");

    fclose(fp);
    pthread_mutex_unlock(&outbox_mutex);
    return 0;
}

/**
 * @brief 更新回压统计信息
 */
static void update_backpressure_stats(job_type_t type, const char *action)
{
    pthread_mutex_lock(&stats_mutex);
    bp_stats.total_tasks++;

    if (strcmp(action, "drop") == 0)
    {
        bp_stats.dropped_tasks++;
    }
    else if (strcmp(action, "merge") == 0)
    {
        bp_stats.merged_tasks++;
        bp_stats.merge_count++;
    }
    else if (strcmp(action, "downsample") == 0)
    {
        bp_stats.downsampled_tasks++;
    }
    else if (strcmp(action, "outbox") == 0)
    {
        bp_stats.outbox_tasks++;
    }

    pthread_mutex_unlock(&stats_mutex);
}

static char *generate_mq_name(void)
{
    static char name[64];
    pid_t pid = getpid();
    unsigned int rand_val;

    // 使用更可靠的随机数生成方式
    // 组合PID、时间和线程地址作为随机源
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    rand_val = (unsigned int)(pid ^ ts.tv_nsec ^ (unsigned long)&ts);

    // 确保随机值在合理范围内
    rand_val = rand_val % 10000;

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
    while (!pool_should_exit)
    {
        // 从消息队列中获取任务
        int len = mq_receive(mq_fd, (char *)&task, sizeof(Task), NULL);

        // 检查是否收到退出信号
        if (pool_should_exit)
        {
            break;
        }

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

                // 处理完任务后，尝试发送所有类型的合并任务
                for (job_type_t type = JOB_TYPE_INVALID + 1; type < JOB_TYPE_MAX; type++)
                {
                    app_pool_try_send_merged_task(type);
                }
            }
            else
            {
                log_error("invalid job type: %d", task.type);
            }
        }
        else if (len == -1 && errno == EINTR)
        {
            // 被信号中断，继续循环检查退出标志
            continue;
        }
    }
    return NULL;
}

int app_pool_init(int size)
{
    // 生成随机队列名称
    mq_name = generate_mq_name();

    // 初始化并创建消息队列
    struct mq_attr mq_attr;
    mq_attr.mq_maxmsg = 10;            // 消息队列中最多消息数
    mq_attr.mq_msgsize = sizeof(Task); // 每条消息最大字节数

    // crash恢复策略：先尝试删除可能存在的旧消息队列
    // 忽略错误，因为队列可能不存在
    mq_unlink(mq_name);

    // 修改权限为0600，即只有创建者可以读写
    mq_fd = mq_open(mq_name, O_CREAT | O_RDWR, 0600, &mq_attr);
    if (mq_fd == -1)
    {
        log_error("mq open failed");
        return -1;
    }

    // 设置O_NONBLOCK模式，使mq_send在队列满时立即返回EAGAIN错误
    int flags = fcntl(mq_fd, F_GETFL);
    if (flags == -1)
    {
        log_error("mq get flags failed");
        mq_close(mq_fd);
        mq_unlink(mq_name);
        return -1;
    }

    if (fcntl(mq_fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        log_error("mq set O_NONBLOCK failed");
        mq_close(mq_fd);
        mq_unlink(mq_name);
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
    // 设置退出哨兵标志，通知所有线程退出
    pool_should_exit = 1;

    // 向消息队列发送退出任务，唤醒可能阻塞在mq_receive的线程
    // 循环尝试，直到成功或队列被清空
    Task exit_task;
    exit_task.type = JOB_TYPE_INVALID;
    exit_task.data_len = 0;

    // 尝试发送退出任务
    int ret = mq_send(mq_fd, (char *)&exit_task, sizeof(Task), 0);
    if (ret == -1 && errno == EAGAIN)
    {
        // 队列满，尝试清空队列
        Task dummy;
        while (mq_receive(mq_fd, (char *)&dummy, sizeof(Task), NULL) > 0)
        {
            log_warn("Dropping task to make room for exit signal");
        }
        // 再次尝试发送退出任务
        ret = mq_send(mq_fd, (char *)&exit_task, sizeof(Task), 0);
        if (ret == -1)
        {
            log_error("Failed to send exit task: %s", strerror(errno));
        }
    }

    // 先删除消息队列，这会中断所有阻塞在mq_receive上的线程
    mq_unlink(mq_name);

    // 关闭消息队列
    mq_close(mq_fd);

    // 销毁线程池
    for (int i = 0; i < thread_num; i++)
    {
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
    int ret = mq_send(mq_fd, (char *)&task, sizeof(Task), 0);
    log_debug("mq_send returned %d, errno=%d", ret, errno);
    // 处理EAGAIN错误（队列满）
    if (ret == -1 && errno == EAGAIN)
    {
        // 更新EAGAIN计数
        pthread_mutex_lock(&stats_mutex);
        bp_stats.eagain_count++;
        pthread_mutex_unlock(&stats_mutex);

        // 根据回压策略处理
        backpressure_strategy_t strategy = bp_strategies[type];

        switch (strategy)
        {
        case BACKPRESSURE_DROP:
            // 丢弃任务
            update_backpressure_stats(type, "drop");
            log_warn("Task dropped (type: %d) due to backpressure", type);
            return 0;

        case BACKPRESSURE_MERGE:
            // 合并任务
            pthread_mutex_lock(&merge_mutex);
            // 更新最后处理的任务
            last_task[type] = task;
            pthread_mutex_unlock(&merge_mutex);

            update_backpressure_stats(type, "merge");
            log_warn("Task merged (type: %d) due to backpressure", type);
            return 0;

        case BACKPRESSURE_DOWNSAMPLE:
            // 降采样：检查是否应该丢弃此任务
            if (should_downsample(type))
            {
                update_backpressure_stats(type, "downsample");
                log_warn("Task downsampled (type: %d) due to backpressure", type);
                return 0;
            }
            // 否则继续尝试发送
            ret = mq_send(mq_fd, (char *)&task, sizeof(Task), 0);
            if (ret == -1 && errno == EAGAIN)
            {
                // 如果仍然失败，则丢弃任务
                update_backpressure_stats(type, "drop");
                log_warn("Task dropped (type: %d) after downsample attempt", type);
                return 0;
            }
            break;

        case BACKPRESSURE_OUTBOX:
            // 写入outbox文件
            if (write_to_outbox(&task) == 0)
            {
                update_backpressure_stats(type, "outbox");
                log_warn("Task written to outbox (type: %d) due to backpressure", type);
                return 0;
            }
            else
            {
                // 写入失败，则丢弃任务
                update_backpressure_stats(type, "drop");
                log_error("Task dropped (type: %d) due to outbox write failure", type);
                return -1;
            }

        default:
            // 未知策略，丢弃任务
            update_backpressure_stats(type, "drop");
            log_error("Task dropped (type: %d) due to unknown backpressure strategy", type);
            return -1;
        }
    }

    return ret;
}

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
    log_info("Total tasks: %" PRIu64, bp_stats.total_tasks);
    log_info("Dropped tasks: %" PRIu64 " (%.2f%%)", bp_stats.dropped_tasks,
             bp_stats.total_tasks > 0 ? (double)bp_stats.dropped_tasks * 100.0 / bp_stats.total_tasks : 0.0);
    log_info("Merged tasks: %" PRIu64 " (%.2f%%)", bp_stats.merged_tasks,
             bp_stats.total_tasks > 0 ? (double)bp_stats.merged_tasks * 100.0 / bp_stats.total_tasks : 0.0);
    log_info("Merge count: %" PRIu64 " (times merged)", bp_stats.merge_count);
    log_info("Downsampled tasks: %" PRIu64 " (%.2f%%)", bp_stats.downsampled_tasks,
             bp_stats.total_tasks > 0 ? (double)bp_stats.downsampled_tasks * 100.0 / bp_stats.total_tasks : 0.0);
    log_info("Outbox tasks: %" PRIu64 " (%.2f%%)", bp_stats.outbox_tasks,
             bp_stats.total_tasks > 0 ? (double)bp_stats.outbox_tasks * 100.0 / bp_stats.total_tasks : 0.0);
    log_info("EAGAIN count: %" PRIu64, bp_stats.eagain_count);
    log_info("=====================================");

    pthread_mutex_unlock(&stats_mutex);
    return 0;
}

/**
 * @brief 尝试发送合并后的任务
 * @brief 这个函数应该在任务处理完成后调用，以尝试发送合并后的任务
 */
int app_pool_try_send_merged_task(job_type_t type)
{
    if (type <= JOB_TYPE_INVALID || type >= JOB_TYPE_MAX)
    {
        log_error("invalid job type: %d", type);
        return -1;
    }

    // 检查是否有合并的任务
    pthread_mutex_lock(&merge_mutex);
    int has_task = (last_task[type].type == type);
    pthread_mutex_unlock(&merge_mutex);

    if (!has_task)
    {
        return 0; // 没有合并的任务
    }

    // 尝试发送合并后的任务
    pthread_mutex_lock(&merge_mutex);
    Task task = last_task[type];
    last_task[type].type = JOB_TYPE_INVALID; // 清除已发送的任务
    pthread_mutex_unlock(&merge_mutex);

    int ret = mq_send(mq_fd, (char *)&task, sizeof(Task), 0);

    if (ret == -1 && errno == EAGAIN)
    {
        // 队列仍然满，将任务放回合并缓冲区
        pthread_mutex_lock(&merge_mutex);
        last_task[type] = task;
        pthread_mutex_unlock(&merge_mutex);

        log_debug("Failed to send merged task (type: %d), queue still full", type);
        return -1;
    }

    log_debug("Sent merged task (type: %d)", type);
    return ret;
}
