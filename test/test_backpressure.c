
#include "app/app_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>

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

// 测试任务处理函数
static int test_handler(const uint8_t *data, uint32_t len)
{
    test_handler_called++;
    if (data != NULL && len > 0)
    {
        memcpy(test_received_data, data, len);
        test_received_len = len;
    }
    // 添加延迟以模拟较慢的处理速度
    usleep(100000); // 100ms延迟
    return 0;
}

// 测试1: 验证丢弃策略
void test_drop_strategy(void)
{
    TEST_START("Drop Strategy");

    // 设置丢弃策略
    int result = app_pool_set_backpressure_strategy(JOB_TYPE_BT_SCAN, BACKPRESSURE_DROP);
    TEST_ASSERT(result == 0, "Set drop strategy");

    // 重置统计信息
    app_pool_reset_backpressure_stats();

    // 发送大量任务以触发回压策略
    test_handler_called = 0;
    for (int i = 0; i < 100; i++)
    {
        uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04};
        app_pool_add_task(JOB_TYPE_BT_SCAN, test_data, sizeof(test_data));
    }

    // 等待任务处理完成
    sleep(2);

    // 报告统计信息
    app_pool_report_backpressure_stats();

    // 验证丢弃的任务数
    TEST_ASSERT(test_handler_called <= 50, "Expected at most 50 tasks to be processed");
    printf("  Handler called: %d times\n", test_handler_called);
}

// 测试2: 验证合并策略
void test_merge_strategy(void)
{
    TEST_START("Merge Strategy");

    // 设置合并策略
    int result = app_pool_set_backpressure_strategy(JOB_TYPE_MQTT_PUBLISH, BACKPRESSURE_MERGE);
    TEST_ASSERT(result == 0, "Set merge strategy");

    // 重置统计信息
    app_pool_reset_backpressure_stats();

    // 发送大量任务以触发回压策略
    test_handler_called = 0;
    for (int i = 0; i < 100; i++)
    {
        uint8_t test_data[] = {0x05, 0x06, 0x07, 0x08};
        app_pool_add_task(JOB_TYPE_MQTT_PUBLISH, test_data, sizeof(test_data));
    }

    // 等待任务处理完成
    sleep(2);

    // 报告统计信息
    app_pool_report_backpressure_stats();

    // 验证合并的任务数
    printf("  Handler called: %d times\n", test_handler_called);
    TEST_ASSERT(test_handler_called >= 10, "Expected at least 10 tasks to be processed");
}

// 测试3: 验证降采样策略
void test_downsample_strategy(void)
{
    TEST_START("Downsample Strategy");

    // 设置降采样策略
    int result = app_pool_set_backpressure_strategy(JOB_TYPE_SERIAL_SEND, BACKPRESSURE_DOWNSAMPLE);
    TEST_ASSERT(result == 0, "Set downsample strategy");

    // 设置降采样间隔为50ms
    result = app_pool_set_downsample_interval(50);
    TEST_ASSERT(result == 0, "Set downsample interval");

    // 重置统计信息
    app_pool_reset_backpressure_stats();

    // 发送大量任务以触发回压策略
    test_handler_called = 0;
    for (int i = 0; i < 100; i++)
    {
        uint8_t test_data[] = {0x09, 0x0A, 0x0B, 0x0C};
        app_pool_add_task(JOB_TYPE_SERIAL_SEND, test_data, sizeof(test_data));
        usleep(10000); // 10ms延迟
    }

    // 等待任务处理完成
    sleep(2);

    // 报告统计信息
    app_pool_report_backpressure_stats();

    // 验证降采样的任务数
    printf("  Handler called: %d times\n", test_handler_called);
    TEST_ASSERT(test_handler_called >= 5, "Expected at least 5 tasks to be processed");
}

// 测试4: 验证Outbox策略
void test_outbox_strategy(void)
{
    TEST_START("Outbox Strategy");

    // 设置Outbox策略
    int result = app_pool_set_backpressure_strategy(JOB_TYPE_BT_CONNECT, BACKPRESSURE_OUTBOX);
    TEST_ASSERT(result == 0, "Set outbox strategy");

    // 重置统计信息
    app_pool_reset_backpressure_stats();

    // 清空outbox文件
    system("rm -f /tmp/gateway_outbox");

    // 发送大量任务以触发回压策略
    test_handler_called = 0;
    for (int i = 0; i < 100; i++)
    {
        uint8_t test_data[] = {0x0D, 0x0E, 0x0F, 0x10};
        app_pool_add_task(JOB_TYPE_BT_CONNECT, test_data, sizeof(test_data));
    }

    // 等待任务处理完成
    sleep(2);

    // 报告统计信息
    app_pool_report_backpressure_stats();

    // 检查outbox文件是否存在
    FILE *fp = fopen("/tmp/gateway_outbox", "r");
    TEST_ASSERT(fp != NULL, "Outbox file created");
    if (fp != NULL)
    {
        // 统计outbox文件中的任务数
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
        TEST_ASSERT(outbox_count >= 10, "Expected at least 10 tasks in outbox");
    }

    printf("  Handler called: %d times\n", test_handler_called);
}

// 测试5: 验证混合策略
void test_mixed_strategies(void)
{
    TEST_START("Mixed Strategies");

    // 为不同任务类型设置不同的回压策略
    app_pool_set_backpressure_strategy(JOB_TYPE_BT_SCAN, BACKPRESSURE_DROP);
    app_pool_set_backpressure_strategy(JOB_TYPE_MQTT_PUBLISH, BACKPRESSURE_MERGE);
    app_pool_set_backpressure_strategy(JOB_TYPE_SERIAL_SEND, BACKPRESSURE_DOWNSAMPLE);
    app_pool_set_backpressure_strategy(JOB_TYPE_BT_CONNECT, BACKPRESSURE_OUTBOX);

    // 重置统计信息
    app_pool_reset_backpressure_stats();

    // 清空outbox文件
    system("rm -f /tmp/gateway_outbox");

    // 发送混合任务
    test_handler_called = 0;
    for (int i = 0; i < 100; i++)
    {
        uint8_t test_data[] = {0x11, 0x12, 0x13, 0x14};
        app_pool_add_task(JOB_TYPE_BT_SCAN, test_data, sizeof(test_data));
        app_pool_add_task(JOB_TYPE_MQTT_PUBLISH, test_data, sizeof(test_data));
        app_pool_add_task(JOB_TYPE_SERIAL_SEND, test_data, sizeof(test_data));
        app_pool_add_task(JOB_TYPE_BT_CONNECT, test_data, sizeof(test_data));
        usleep(10000); // 10ms延迟
    }

    // 等待任务处理完成
    sleep(3);

    // 报告统计信息
    app_pool_report_backpressure_stats();

    printf("  Handler called: %d times\n", test_handler_called);
    TEST_ASSERT(test_handler_called >= 20, "Expected at least 20 tasks to be processed");
}

// 测试6: 验证EAGAIN计数
void test_eagain_count(void)
{
    TEST_START("EAGAIN Count");

    // 重置统计信息
    app_pool_reset_backpressure_stats();

    // 发送大量任务以触发EAGAIN
    test_handler_called = 0;
    for (int i = 0; i < 200; i++)
    {
        uint8_t test_data[] = {0x15, 0x16, 0x17, 0x18};
        app_pool_add_task(JOB_TYPE_BT_SCAN, test_data, sizeof(test_data));
    }

    // 等待任务处理完成
    sleep(2);

    // 报告统计信息
    app_pool_report_backpressure_stats();

    printf("  Handler called: %d times\n", test_handler_called);
    TEST_ASSERT(test_handler_called > 0, "Expected some tasks to be processed");
}

// 主函数
int main(void)
{
    printf("=====================================\n");
    printf("Backpressure Strategy Tests\n");
    printf("=====================================\n");

    // 初始化线程池
    int result = app_pool_init(2);
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
    test_drop_strategy();
    test_merge_strategy();
    test_downsample_strategy();
    test_outbox_strategy();
    test_mixed_strategies();
    test_eagain_count();

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
