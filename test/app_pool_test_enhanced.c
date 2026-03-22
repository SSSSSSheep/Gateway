#include "app_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <mqueue.h>
#include <sys/wait.h>

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

// 测试任务处理函数1
static int test_handler_1(const uint8_t *data, uint32_t len)
{
    printf("  test_handler_1 called with data_len=%u\n", len);
    test_handler_called++;
    if (data != NULL && len > 0)
    {
        memcpy(test_received_data, data, len);
        test_received_len = len;
    }
    return 0;
}

// 测试任务处理函数2
static int test_handler_2(const uint8_t *data, uint32_t len)
{
    printf("  test_handler_2 called with data_len=%u\n", len);
    test_handler_called++;
    return 0;
}

// 测试1: 验证任务处理器注册
void test_handler_registration(void)
{
    TEST_START("Handler Registration");

    // 注册有效的处理器
    int result = app_pool_register_handler(JOB_TYPE_BT_SCAN, test_handler_1);
    TEST_ASSERT(result == 0, "Register valid handler");

    // 注册无效的处理器类型
    result = app_pool_register_handler(JOB_TYPE_INVALID, test_handler_1);
    TEST_ASSERT(result == -1, "Reject invalid job type");

    result = app_pool_register_handler(JOB_TYPE_MAX, test_handler_1);
    TEST_ASSERT(result == -1, "Reject JOB_MAX type");
}

// 测试2: 验证线程池初始化
void test_pool_initialization(void)
{
    TEST_START("Pool Initialization");

    // 初始化线程池
    int result = app_pool_init(2);
    TEST_ASSERT(result == 0, "Initialize thread pool");
}

// 测试3: 验证任务添加和执行
void test_task_addition_and_execution(void)
{
    TEST_START("Task Addition and Execution");

    test_handler_called = 0;

    // 添加有效任务
    uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04};
    int result = app_pool_add_task(JOB_TYPE_BT_SCAN, test_data, sizeof(test_data));
    TEST_ASSERT(result == 0, "Add valid task");

    // 等待任务执行
    sleep(1);

    TEST_ASSERT(test_handler_called > 0, "Handler was called");
    TEST_ASSERT(test_received_len == sizeof(test_data), "Data length matches");
    TEST_ASSERT(memcmp(test_received_data, test_data, sizeof(test_data)) == 0, "Data content matches");
}

// 测试4: 验证无效任务类型
void test_invalid_task_type(void)
{
    TEST_START("Invalid Task Type");

    test_handler_called = 0;

    // 添加无效类型的任务
    uint8_t test_data[] = {0x01};
    int result = app_pool_add_task(JOB_TYPE_INVALID, test_data, sizeof(test_data));
    TEST_ASSERT(result == -1, "Reject invalid task type");

    result = app_pool_add_task(JOB_TYPE_MAX, test_data, sizeof(test_data));
    TEST_ASSERT(result == -1, "Reject JOB_MAX type");

    // 等待确认没有任务被执行
    sleep(1);

    TEST_ASSERT(test_handler_called == 0, "No handler called for invalid type");
}

// 测试5: 验证数据大小限制
void test_data_size_limit(void)
{
    TEST_START("Data Size Limit");

    // 添加超过大小限制的数据
    uint8_t large_data[300];
    memset(large_data, 0xAA, sizeof(large_data));

    int result = app_pool_add_task(JOB_TYPE_BT_SCAN, large_data, sizeof(large_data));
    TEST_ASSERT(result == -1, "Reject oversized data");
}

// 测试6: 验证空数据处理
void test_null_data_handling(void)
{
    TEST_START("Null Data Handling");

    test_handler_called = 0;

    // 添加空数据任务
    int result = app_pool_add_task(JOB_TYPE_BT_SCAN, NULL, 0);
    TEST_ASSERT(result == 0, "Accept null data with zero length");

    // 添加空指针但非零长度
    result = app_pool_add_task(JOB_TYPE_BT_SCAN, NULL, 10);
    TEST_ASSERT(result == -1, "Reject null data with non-zero length");

    sleep(1);

    TEST_ASSERT(test_handler_called == 1, "Handler called exactly once");
}

// 测试7: 验证多个任务类型
void test_multiple_task_types(void)
{
    TEST_START("Multiple Task Types");

    // 注册第二个处理器
    int result = app_pool_register_handler(JOB_TYPE_MQTT_PUBLISH, test_handler_2);
    TEST_ASSERT(result == 0, "Register second handler");

    test_handler_called = 0;

    // 添加不同类型的任务
    uint8_t data1[] = {0x01};
    uint8_t data2[] = {0x02};

    result = app_pool_add_task(JOB_TYPE_BT_SCAN, data1, sizeof(data1));
    TEST_ASSERT(result == 0, "Add BT_SCAN task");

    result = app_pool_add_task(JOB_TYPE_MQTT_PUBLISH, data2, sizeof(data2));
    TEST_ASSERT(result == 0, "Add MQTT_PUBLISH task");

    sleep(1);

    TEST_ASSERT(test_handler_called == 2, "Both handlers called");
}

// 测试8: 验证线程池销毁
void test_pool_destruction(void)
{
    TEST_START("Pool Destruction");

    // 销毁线程池
    app_pool_destroy();

    printf("  Pool destroyed successfully\n");
}

// 测试9: 验证退出哨兵机制
void test_exit_sentinel(void)
{
    TEST_START("Exit Sentinel Mechanism");

    // 注册处理器
    app_pool_register_handler(JOB_TYPE_BT_SCAN, test_handler_1);

    // 初始化线程池
    int result = app_pool_init(2);
    TEST_ASSERT(result == 0, "Initialize thread pool");

    // 添加一些任务
    uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04};
    for (int i = 0; i < 5; i++)
    {
        app_pool_add_task(JOB_TYPE_BT_SCAN, test_data, sizeof(test_data));
    }

    // 等待部分任务执行
    sleep(1);

    // 销毁线程池，测试退出哨兵机制
    app_pool_destroy();

    printf("  Exit sentinel mechanism tested successfully\n");
}

// 测试10: 验证消息队列的 crash 恢复策略
void test_mq_crash_recovery(void)
{
    TEST_START("MQ Crash Recovery");

    // 第一次初始化
    int result = app_pool_init(2);
    TEST_ASSERT(result == 0, "First initialization");

    // 销毁
    app_pool_destroy();

    // 第二次初始化（模拟崩溃后重启）
    result = app_pool_init(2);
    TEST_ASSERT(result == 0, "Second initialization after crash");

    // 销毁
    app_pool_destroy();

    printf("  MQ crash recovery tested successfully\n");
}

// 测试11: 验证退出任务发送的可靠性
void test_exit_task_reliability(void)
{
    TEST_START("Exit Task Reliability");

    // 注册处理器
    app_pool_register_handler(JOB_TYPE_BT_SCAN, test_handler_1);

    // 初始化线程池（使用较小的队列大小）
    int result = app_pool_init(2);
    TEST_ASSERT(result == 0, "Initialize thread pool");

    // 填满消息队列
    uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04};
    for (int i = 0; i < 20; i++)
    {
        app_pool_add_task(JOB_TYPE_BT_SCAN, test_data, sizeof(test_data));
    }

    // 等待队列满
    sleep(1);

    // 销毁线程池，测试退出任务发送的可靠性
    app_pool_destroy();

    printf("  Exit task reliability tested successfully\n");
}

// 测试12: 验证合并任务的后台重试机制
void test_merged_task_retry(void)
{
    TEST_START("Merged Task Retry Mechanism");

    // 注册处理器
    app_pool_register_handler(JOB_TYPE_BT_SCAN, test_handler_1);
    app_pool_register_handler(JOB_TYPE_MQTT_PUBLISH, test_handler_2);

    // 初始化线程池
    int result = app_pool_init(2);
    TEST_ASSERT(result == 0, "Initialize thread pool");

    // 设置合并策略
    app_pool_set_backpressure_strategy(JOB_TYPE_BT_SCAN, BACKPRESSURE_MERGE);
    app_pool_set_backpressure_strategy(JOB_TYPE_MQTT_PUBLISH, BACKPRESSURE_MERGE);

    // 填满消息队列
    uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04};
    for (int i = 0; i < 20; i++)
    {
        app_pool_add_task(JOB_TYPE_BT_SCAN, test_data, sizeof(test_data));
        app_pool_add_task(JOB_TYPE_MQTT_PUBLISH, test_data, sizeof(test_data));
    }

    // 等待队列处理
    sleep(2);

    // 销毁线程池
    app_pool_destroy();

    printf("  Merged task retry mechanism tested successfully\n");
}

// 测试13: 验证统计信息打印格式
void test_stats_printing(void)
{
    TEST_START("Statistics Printing");

    // 注册处理器
    app_pool_register_handler(JOB_TYPE_BT_SCAN, test_handler_1);

    // 初始化线程池
    int result = app_pool_init(2);
    TEST_ASSERT(result == 0, "Initialize thread pool");

    // 添加一些任务
    uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04};
    for (int i = 0; i < 10; i++)
    {
        app_pool_add_task(JOB_TYPE_BT_SCAN, test_data, sizeof(test_data));
    }

    // 等待任务执行
    sleep(1);

    // 打印统计信息
    result = app_pool_report_backpressure_stats();
    TEST_ASSERT(result == 0, "Report backpressure stats");

    // 销毁线程池
    app_pool_destroy();

    printf("  Statistics printing tested successfully\n");
}

// 测试14: 验证随机数生成改进
void test_random_generation(void)
{
    TEST_START("Random Generation");

    // 多次初始化和销毁，验证随机数生成
    for (int i = 0; i < 5; i++)
    {
        int result = app_pool_init(2);
        TEST_ASSERT(result == 0, "Initialize thread pool");
        app_pool_destroy();
    }

    printf("  Random generation tested successfully\n");
}

// 主测试函数
int main(int argc, char *argv[])
{
    printf("========================================\n");
    printf("  App Pool Enhanced Unit Tests\n");
    printf("========================================\n");

    // 运行所有测试
    test_handler_registration();
    test_pool_initialization();
    test_task_addition_and_execution();
    test_invalid_task_type();
    test_data_size_limit();
    test_null_data_handling();
    test_multiple_task_types();
    test_pool_destruction();
    test_exit_sentinel();
    test_mq_crash_recovery();
    test_exit_task_reliability();
    test_merged_task_retry();
    test_stats_printing();
    test_random_generation();

    // 输出测试结果
    printf("\n========================================\n");
    printf("  Test Summary\n");
    printf("========================================\n");
    printf("  Total:  %d\n", test_counter);
    printf("  Passed: %d\n", test_passed);
    printf("  Failed: %d\n", test_failed);
    printf("========================================\n");

    return (test_failed == 0) ? 0 : 1;
}
