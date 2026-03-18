/**
 * @file test_app_pool.c
 * @brief 线程池回压策略测试程序
 */

#include "app_pool.h"
#include "log/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>

// 测试任务计数器
static volatile int test_task_count = 0;
static volatile int test_task_processed = 0;

// 测试任务处理器
static int test_task_handler(const uint8_t *data, uint32_t len)
{
    (void)data; // 消除未使用参数警告
    (void)len;  // 消除未使用参数警告

    test_task_processed++;

    // 模拟任务处理时间（10ms）
    usleep(10000);

    log_debug("Task processed: %d", test_task_processed);
    return 0;
}

// 测试线程函数
static void *test_thread_func(void *arg)
{
    int thread_id = *(int *)arg;
    char test_data[64];

    log_info("Test thread %d started", thread_id);

    // 每个线程发送100个任务
    for (int i = 0; i < 100; i++)
    {
        snprintf(test_data, sizeof(test_data), "Thread %d - Task %d", thread_id, i);

        int ret = app_pool_add_task(JOB_TYPE_MQTT_PUBLISH,
                                    (const uint8_t *)test_data,
                                    strlen(test_data) + 1);

        if (ret == 0)
        {
            test_task_count++;
        }
        else
        {
            log_error("Failed to add task: %d", i);
        }

        // 每10ms发送一个任务
        usleep(10000);
    }

    log_info("Test thread %d finished, sent %d tasks", thread_id, 100);
    return NULL;
}

int main(int argc, char *argv[])
{
    int ret;
    int thread_pool_size = 4;  // 默认4个工作线程
    int test_thread_count = 8; // 默认8个测试线程

    // 解析命令行参数
    if (argc > 1)
    {
        thread_pool_size = atoi(argv[1]);
        if (thread_pool_size <= 0 || thread_pool_size > 16)
        {
            printf("Invalid thread pool size: %s (valid range: 1-16)\n", argv[1]);
            return -1;
        }
    }

    if (argc > 2)
    {
        test_thread_count = atoi(argv[2]);
        if (test_thread_count <= 0 || test_thread_count > 32)
        {
            printf("Invalid test thread count: %s (valid range: 1-32)\n", argv[2]);
            return -1;
        }
    }

    printf("=== Thread Pool Backpressure Test ===\n");
    printf("Thread pool size: %d\n", thread_pool_size);
    printf("Test thread count: %d\n", test_thread_count);
    printf("====================================\n\n");

    // 初始化日志
    log_set_level(LOG_DEBUG);

    // 初始化线程池
    ret = app_pool_init(thread_pool_size);
    if (ret != 0)
    {
        log_error("Failed to initialize thread pool");
        return -1;
    }

    // 注册任务处理器
    ret = app_pool_register_handler(JOB_TYPE_MQTT_PUBLISH, test_task_handler);
    if (ret != 0)
    {
        log_error("Failed to register task handler");
        app_pool_destroy();
        return -1;
    }

    // 设置回压策略
    printf("\n=== Setting Backpressure Strategies ===\n");

    // MQTT发布任务使用合并策略（队列满时合并）
    ret = app_pool_set_backpressure_strategy(JOB_TYPE_MQTT_PUBLISH, BACKPRESSURE_MERGE);
    if (ret != 0)
    {
        log_error("Failed to set backpressure strategy for MQTT_PUBLISH");
    }
    else
    {
        printf("MQTT_PUBLISH: BACKPRESSURE_MERGE\n");
    }

    // 蓝牙扫描任务使用丢弃策略
    ret = app_pool_set_backpressure_strategy(JOB_TYPE_BT_SCAN, BACKPRESSURE_DROP);
    if (ret != 0)
    {
        log_error("Failed to set backpressure strategy for BT_SCAN");
    }
    else
    {
        printf("BT_SCAN: BACKPRESSURE_DROP\n");
    }

    // 蓝牙连接任务使用outbox策略
    ret = app_pool_set_backpressure_strategy(JOB_TYPE_BT_CONNECT, BACKPRESSURE_OUTBOX);
    if (ret != 0)
    {
        log_error("Failed to set backpressure strategy for BT_CONNECT");
    }
    else
    {
        printf("BT_CONNECT: BACKPRESSURE_OUTBOX\n");
    }

    // 串口发送任务使用合并策略
    ret = app_pool_set_backpressure_strategy(JOB_TYPE_SERIAL_SEND, BACKPRESSURE_MERGE);
    if (ret != 0)
    {
        log_error("Failed to set backpressure strategy for SERIAL_SEND");
    }
    else
    {
        printf("SERIAL_SEND: BACKPRESSURE_MERGE\n");
    }

    printf("========================================\n\n");

    // 创建测试线程
    pthread_t *test_threads = malloc(sizeof(pthread_t) * test_thread_count);
    int *thread_ids = malloc(sizeof(int) * test_thread_count);

    printf("=== Starting Test Threads ===\n");
    for (int i = 0; i < test_thread_count; i++)
    {
        thread_ids[i] = i;
        if (pthread_create(&test_threads[i], NULL, test_thread_func, &thread_ids[i]) != 0)
        {
            log_error("Failed to create test thread %d", i);
            free(test_threads);
            free(thread_ids);
            app_pool_destroy();
            return -1;
        }
    }

    // 等待所有测试线程完成
    for (int i = 0; i < test_thread_count; i++)
    {
        pthread_join(test_threads[i], NULL);
    }

    free(test_threads);
    free(thread_ids);

    printf("\n=== Waiting for Task Completion ===\n");
    // 等待所有任务处理完成，最多等待10秒
    int wait_seconds = 0;
    int max_wait_seconds = 10;
    while (test_task_processed < test_task_count && wait_seconds < max_wait_seconds)
    {
        printf("Processed: %d / %d (waiting %d/%d seconds)\r",
               test_task_processed, test_task_count, wait_seconds, max_wait_seconds);
        fflush(stdout);
        usleep(100000); // 100ms
        wait_seconds++;
    }
    printf("\n");

    // 等待一段时间确保所有任务都处理完成
    sleep(1);

    // 上报统计信息
    printf("\n");
    app_pool_report_backpressure_stats();

    printf("\n=== Test Summary ===\n");
    printf("Total tasks sent: %d\n", test_task_count);
    printf("Total tasks processed: %d\n", test_task_processed);
    printf("================================\n");

    // 清理资源
    app_pool_destroy();

    return 0;
}
