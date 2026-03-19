
#include "app/app_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <pthread.h>
#include <sys/time.h>
#include <math.h>

// 测试计数器
static int test_counter = 0;
static int test_passed = 0;
static int test_failed = 0;

// 测试结果记录
#define TEST_START(name)                                         \
    do                                                           \
    {                                                            \
        printf("\n=== Test %d: %s ===\n", ++test_counter, name); \
    } while (0)

#define TEST_ASSERT(condition, message)       \
    do                                        \
    {                                         \
        if (condition)                        \
        {                                     \
            printf("  [PASS] %s\n", message); \
            test_passed++;                    \
        }                                     \
        else                                  \
        {                                     \
            printf("  [FAIL] %s\n", message); \
            test_failed++;                    \
        }                                     \
    } while (0)

// 测试任务处理器
static int test_handler_called = 0;
static uint8_t test_received_data[256];
static uint32_t test_received_len = 0;

// 延迟统计
static uint64_t total_latency_ns = 0;
static uint64_t max_latency_ns = 0;
static uint64_t min_latency_ns = UINT64_MAX;
static uint64_t latency_samples = 0;
static pthread_mutex_t latency_mutex = PTHREAD_MUTEX_INITIALIZER;

// 任务发送时间记录
typedef struct {
    struct timespec send_time;
    uint8_t data[256];
    uint32_t data_len;
} task_record_t;

static task_record_t task_records[10000];
static uint32_t task_record_index = 0;
static pthread_mutex_t record_mutex = PTHREAD_MUTEX_INITIALIZER;

// 获取当前时间（纳秒）
static uint64_t get_current_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

// 更新延迟统计
static void update_latency_stats(uint64_t latency_ns)
{
    pthread_mutex_lock(&latency_mutex);
    total_latency_ns += latency_ns;
    if (latency_ns > max_latency_ns)
    {
        max_latency_ns = latency_ns;
    }
    if (latency_ns < min_latency_ns)
    {
        min_latency_ns = latency_ns;
    }
    latency_samples++;
    pthread_mutex_unlock(&latency_mutex);
}

// 计算百分位数
static uint64_t calculate_percentile(uint64_t *latencies, uint64_t count, int percentile)
{
    // 对延迟数组进行排序
    for (uint64_t i = 0; i < count; i++)
    {
        for (uint64_t j = i + 1; j < count; j++)
        {
            if (latencies[i] > latencies[j])
            {
                uint64_t temp = latencies[i];
                latencies[i] = latencies[j];
                latencies[j] = temp;
            }
        }
    }

    // 计算百分位数
    uint64_t index = (count * percentile) / 100;
    if (index >= count)
    {
        index = count - 1;
    }
    return latencies[index];
}

// 测试任务处理函数
static int test_handler(const uint8_t *data, uint32_t len)
{
    test_handler_called++;
    if (data != NULL && len > 0)
    {
        memcpy(test_received_data, data, len);
        test_received_len = len;

        // 计算延迟
        uint64_t current_time = get_current_time_ns();
        uint64_t send_time;
        memcpy(&send_time, data, sizeof(send_time));
        uint64_t latency_ns = current_time - send_time;
        update_latency_stats(latency_ns);
    }
    // 添加延迟以模拟较慢的处理速度
    usleep(10000); // 10ms延迟
    return 0;
}

// 测试1: 并发测试（多生产者）
typedef struct {
    int thread_id;
    int task_count;
    job_type_t task_type;
} producer_thread_arg_t;

void *producer_thread(void *arg)
{
    producer_thread_arg_t *thread_arg = (producer_thread_arg_t *)arg;

    for (int i = 0; i < thread_arg->task_count; i++)
    {
        uint8_t test_data[256];
        uint64_t send_time = get_current_time_ns();
        memcpy(test_data, &send_time, sizeof(send_time));

        app_pool_add_task(thread_arg->task_type, test_data, sizeof(send_time));

        // 添加少量延迟以模拟真实场景
        usleep(100); // 0.1ms延迟
    }

    return NULL;
}

void test_concurrent_producers(void)
{
    TEST_START("Concurrent Producers Test");

    // 重置统计信息
    app_pool_reset_backpressure_stats();
    test_handler_called = 0;
    total_latency_ns = 0;
    max_latency_ns = 0;
    min_latency_ns = UINT64_MAX;
    latency_samples = 0;

    // 创建多个生产者线程
    const int thread_count = 10;
    const int tasks_per_thread = 1000;
    pthread_t threads[thread_count];
    producer_thread_arg_t thread_args[thread_count];

    // 设置回压策略
    app_pool_set_backpressure_strategy(JOB_TYPE_BT_SCAN, BACKPRESSURE_DROP);

    // 创建并启动生产者线程
    for (int i = 0; i < thread_count; i++)
    {
        thread_args[i].thread_id = i;
        thread_args[i].task_count = tasks_per_thread;
        thread_args[i].task_type = JOB_TYPE_BT_SCAN;

        if (pthread_create(&threads[i], NULL, producer_thread, &thread_args[i]) != 0)
        {
            printf("  [FAIL] Failed to create thread %d\n", i);
            return;
        }
    }

    // 等待所有生产者线程完成
    for (int i = 0; i < thread_count; i++)
    {
        pthread_join(threads[i], NULL);
    }

    // 等待任务处理完成
    sleep(15);

    // 报告统计信息
    app_pool_report_backpressure_stats();

    // 计算平均延迟
    double avg_latency_ms = latency_samples > 0 ? 
        (double)total_latency_ns / latency_samples / 1000000.0 : 0.0;
    double max_latency_ms = (double)max_latency_ns / 1000000.0;
    double min_latency_ms = (double)min_latency_ns / 1000000.0;

    printf("  Latency Statistics:\n");
    printf("    Average: %.3f ms\n", avg_latency_ms);
    printf("    Min: %.3f ms\n", min_latency_ms);
    printf("    Max: %.3f ms\n", max_latency_ms);
    printf("    Samples: %lu\n", latency_samples);

    // 验证P99延迟是否≤100ms
    TEST_ASSERT(max_latency_ms <= 100.0, "P100 latency <= 100ms");
    TEST_ASSERT(avg_latency_ms <= 50.0, "Average latency <= 50ms");
    TEST_ASSERT(test_handler_called > 0, "Tasks were processed");
}

// 测试2: 性能测试（计算P99延迟）
void test_performance_p99_latency(void)
{
    TEST_START("Performance Test (P99 Latency)");

    // 重置统计信息
    app_pool_reset_backpressure_stats();
    test_handler_called = 0;
    total_latency_ns = 0;
    max_latency_ns = 0;
    min_latency_ns = UINT64_MAX;
    latency_samples = 0;

    // 设置回压策略
    app_pool_set_backpressure_strategy(JOB_TYPE_MQTT_PUBLISH, BACKPRESSURE_MERGE);

    // 发送大量任务
    const int task_count = 10000;
    uint64_t *latencies = malloc(task_count * sizeof(uint64_t));
    uint32_t latency_index = 0;

    for (int i = 0; i < task_count; i++)
    {
        uint8_t test_data[256];
        uint64_t send_time = get_current_time_ns();
        memcpy(test_data, &send_time, sizeof(send_time));

        app_pool_add_task(JOB_TYPE_MQTT_PUBLISH, test_data, sizeof(send_time));

        // 添加延迟以模拟真实场景
        usleep(100); // 0.1ms延迟
    }

    // 等待任务处理完成
    sleep(30);

    // 报告统计信息
    app_pool_report_backpressure_stats();

    // 计算平均延迟
    double avg_latency_ms = latency_samples > 0 ? 
        (double)total_latency_ns / latency_samples / 1000000.0 : 0.0;
    double max_latency_ms = (double)max_latency_ns / 1000000.0;
    double min_latency_ms = (double)min_latency_ns / 1000000.0;

    printf("  Latency Statistics:\n");
    printf("    Average: %.3f ms\n", avg_latency_ms);
    printf("    Min: %.3f ms\n", min_latency_ms);
    printf("    Max: %.3f ms\n", max_latency_ms);
    printf("    Samples: %lu\n", latency_samples);

    // 验证P99延迟是否≤100ms
    TEST_ASSERT(max_latency_ms <= 100.0, "P100 latency <= 100ms");
    TEST_ASSERT(avg_latency_ms <= 50.0, "Average latency <= 50ms");
    TEST_ASSERT(test_handler_called > 0, "Tasks were processed");

    free(latencies);
}

// 测试3: 长期稳定性测试（短时间版本）
void test_stability_short(void)
{
    TEST_START("Stability Test (Short)");

    // 重置统计信息
    app_pool_reset_backpressure_stats();
    test_handler_called = 0;
    total_latency_ns = 0;
    max_latency_ns = 0;
    min_latency_ns = UINT64_MAX;
    latency_samples = 0;

    // 设置回压策略
    app_pool_set_backpressure_strategy(JOB_TYPE_SERIAL_SEND, BACKPRESSURE_DOWNSAMPLE);

    // 设置降采样间隔
    app_pool_set_downsample_interval(100);

    // 运行测试60秒
    time_t start_time = time(NULL);
    time_t end_time = start_time + 60; // 60秒

    int iteration = 0;
    while (time(NULL) < end_time)
    {
        // 每次迭代发送100个任务
        for (int i = 0; i < 100; i++)
        {
            uint8_t test_data[256];
            uint64_t send_time = get_current_time_ns();
            memcpy(test_data, &send_time, sizeof(send_time));

            app_pool_add_task(JOB_TYPE_SERIAL_SEND, test_data, sizeof(send_time));
        }

        // 每秒报告一次进度
        if (iteration % 10 == 0)
        {
            printf("  Progress: %ld seconds elapsed\n", time(NULL) - start_time);
        }

        iteration++;
        usleep(100000); // 100ms延迟
    }

    // 等待任务处理完成
    sleep(10);

    // 报告统计信息
    app_pool_report_backpressure_stats();

    // 计算平均延迟
    double avg_latency_ms = latency_samples > 0 ? 
        (double)total_latency_ns / latency_samples / 1000000.0 : 0.0;
    double max_latency_ms = (double)max_latency_ns / 1000000.0;
    double min_latency_ms = (double)min_latency_ns / 1000000.0;

    printf("  Latency Statistics:\n");
    printf("    Average: %.3f ms\n", avg_latency_ms);
    printf("    Min: %.3f ms\n", min_latency_ms);
    printf("    Max: %.3f ms\n", max_latency_ms);
    printf("    Samples: %lu\n", latency_samples);

    // 验证P99延迟是否≤100ms
    TEST_ASSERT(max_latency_ms <= 100.0, "P100 latency <= 100ms");
    TEST_ASSERT(avg_latency_ms <= 50.0, "Average latency <= 50ms");
    TEST_ASSERT(test_handler_called > 0, "Tasks were processed");
}

// 测试4: 混合策略长期测试
void test_mixed_strategies_long(void)
{
    TEST_START("Mixed Strategies Long Test");

    // 重置统计信息
    app_pool_reset_backpressure_stats();
    test_handler_called = 0;
    total_latency_ns = 0;
    max_latency_ns = 0;
    min_latency_ns = UINT64_MAX;
    latency_samples = 0;

    // 为不同任务类型设置不同的回压策略
    app_pool_set_backpressure_strategy(JOB_TYPE_BT_SCAN, BACKPRESSURE_DROP);
    app_pool_set_backpressure_strategy(JOB_TYPE_MQTT_PUBLISH, BACKPRESSURE_MERGE);
    app_pool_set_backpressure_strategy(JOB_TYPE_SERIAL_SEND, BACKPRESSURE_DOWNSAMPLE);
    app_pool_set_backpressure_strategy(JOB_TYPE_BT_CONNECT, BACKPRESSURE_OUTBOX);

    // 设置降采样间隔
    app_pool_set_downsample_interval(100);

    // 清空outbox文件
    system("rm -f /tmp/gateway_outbox");

    // 运行测试60秒
    time_t start_time = time(NULL);
    time_t end_time = start_time + 60; // 60秒

    int iteration = 0;
    while (time(NULL) < end_time)
    {
        // 每次迭代发送混合任务
        for (int i = 0; i < 25; i++)
        {
            uint8_t test_data[256];
            uint64_t send_time = get_current_time_ns();
            memcpy(test_data, &send_time, sizeof(send_time));

            app_pool_add_task(JOB_TYPE_BT_SCAN, test_data, sizeof(send_time));
            app_pool_add_task(JOB_TYPE_MQTT_PUBLISH, test_data, sizeof(send_time));
            app_pool_add_task(JOB_TYPE_SERIAL_SEND, test_data, sizeof(send_time));
            app_pool_add_task(JOB_TYPE_BT_CONNECT, test_data, sizeof(send_time));
        }

        // 每秒报告一次进度
        if (iteration % 10 == 0)
        {
            printf("  Progress: %ld seconds elapsed\n", time(NULL) - start_time);
        }

        iteration++;
        usleep(100000); // 100ms延迟
    }

    // 等待任务处理完成
    sleep(10);

    // 报告统计信息
    app_pool_report_backpressure_stats();

    // 计算平均延迟
    double avg_latency_ms = latency_samples > 0 ? 
        (double)total_latency_ns / latency_samples / 1000000.0 : 0.0;
    double max_latency_ms = (double)max_latency_ns / 1000000.0;
    double min_latency_ms = (double)min_latency_ns / 1000000.0;

    printf("  Latency Statistics:\n");
    printf("    Average: %.3f ms\n", avg_latency_ms);
    printf("    Min: %.3f ms\n", min_latency_ms);
    printf("    Max: %.3f ms\n", max_latency_ms);
    printf("    Samples: %lu\n", latency_samples);

    // 检查outbox文件
    FILE *fp = fopen("/tmp/gateway_outbox", "r");
    if (fp != NULL)
    {
        int outbox_count = 0;
        char line[1024];
        while (fgets(line, sizeof(line), fp) != NULL)
        {
            if (strstr(line, "TYPE:") != NULL)
            {
                outbox_count++;
            }
        }
        fclose(fp);
        printf("  Tasks written to outbox: %d\n", outbox_count);
    }

    // 验证P99延迟是否≤100ms
    TEST_ASSERT(max_latency_ms <= 100.0, "P100 latency <= 100ms");
    TEST_ASSERT(avg_latency_ms <= 50.0, "Average latency <= 50ms");
    TEST_ASSERT(test_handler_called > 0, "Tasks were processed");
}

// 主函数
int main(void)
{
    printf("=====================================\n");
    printf("Advanced Backpressure Strategy Tests\n");
    printf("=====================================\n");

    // 初始化线程池
    int result = app_pool_init(4);
    if (result != 0)
    {
        printf("Failed to initialize thread pool\n");
        return -1;
    }

    // 注册任务处理器
    app_pool_register_handler(JOB_TYPE_BT_SCAN, test_handler);
    app_pool_register_handler(JOB_TYPE_MQTT_PUBLISH, test_handler);
    app_pool_register_handler(JOB_TYPE_SERIAL_SEND, test_handler);
    app_pool_register_handler(JOB_TYPE_BT_CONNECT, test_handler);

    // 运行测试
    test_concurrent_producers();
    test_performance_p99_latency();
    test_stability_short();
    test_mixed_strategies_long();

    // 销毁线程池
    app_pool_destroy();

    // 打印测试结果
    printf("\n=====================================\n");
    printf("Test Results\n");
    printf("=====================================\n");
    printf("Total tests: %d\n", test_counter);
    printf("Passed: %d\n", test_passed);
    printf("Failed: %d\n", test_failed);
    printf("=====================================\n");

    return (test_failed == 0) ? 0 : 1;
}
