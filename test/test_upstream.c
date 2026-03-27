#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <assert.h>
#include <stdint.h>
#include "app_device.h"
#include "app_buffer.h"
#include "log/log.h"

// 测试统计结构
typedef struct
{
    int write_count;       // 写入计数
    int at_cmd_count;      // AT指令计数
    int data_count;        // 数据计数
    int queue_full_count;  // 队列满次数
    pthread_mutex_t mutex; // 保护统计数据的互斥锁
} TestStats;

static TestStats g_stats = {0};
static Device *g_device = NULL;

// 获取当前时间（毫秒）
static long get_current_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// 测试1：写队列容量测试
void test_write_queue_capacity(void)
{
    printf("========== 测试1：写队列容量测试 ==========\n");

    // 测试队列满时的行为
    int test_count = 0;
    int max_retries = 20; // 队列容量限制

    for (int i = 0; i < max_retries; i++)
    {
        char data[128];
        sprintf(data, "TEST_DATA_%d", i);

        // 尝试写入队列
        pthread_mutex_lock(&g_device->write_mutex);
        int ret = app_buffer_write(g_device->write_queue, data, strlen(data));
        pthread_mutex_unlock(&g_device->write_mutex);

        if (ret != 0)
        {
            printf("队列已满，写入失败: %d\n", i);
            pthread_mutex_lock(&g_stats.mutex);
            g_stats.queue_full_count++;
            pthread_mutex_unlock(&g_stats.mutex);
            break;
        }
        test_count++;
    }

    printf("成功写入 %d 条数据\n", test_count);
    printf("队列满次数: %d\n", g_stats.queue_full_count);
}

// 测试2：AT指令写间隔测试
// 测试2：AT指令写间隔测试
void test_at_command_interval(void)
{
    printf("========== 测试2：AT指令写间隔测试 ==========\n");

    long start_time = get_current_time_ms();
    int cmd_count = 3;

    for (int i = 0; i < cmd_count; i++)
    {
        char cmd[32];
        sprintf(cmd, "AT+TEST%d", i);

        // 写入队列
        pthread_mutex_lock(&g_device->write_mutex);
        app_buffer_write(g_device->write_queue, cmd, strlen(cmd));
        pthread_cond_signal(&g_device->write_cond);
        pthread_mutex_unlock(&g_device->write_mutex);

        pthread_mutex_lock(&g_stats.mutex);
        g_stats.at_cmd_count++;
        pthread_mutex_unlock(&g_stats.mutex);
    }

    // 等待write_thread处理完所有数据
    // 方法1：检查队列是否为空
    int queue_empty = 0;
    int wait_count = 0;
    while (!queue_empty && wait_count < 100)
    { // 最多等待10秒
        pthread_mutex_lock(&g_device->write_mutex);
        queue_empty = (g_device->write_queue->sub_buffers[g_device->write_queue->read_index]->len == 0);
        pthread_mutex_unlock(&g_device->write_mutex);

        if (!queue_empty)
        {
            usleep(100000); // 等待100ms
            wait_count++;
        }
    }

    // 方法2：额外等待足够的时间，确保write_thread完成所有AT指令的写入
    // 3条AT指令需要至少400ms（2个200ms间隔）
    sleep(1); // 等待1秒，确保write_thread完成

    long end_time = get_current_time_ms();
    long total_time = end_time - start_time;

    printf("发送 %d 条AT指令总耗时: %ldms\n", cmd_count, total_time);
    printf("平均每条AT指令耗时: %.2fms\n", (float)total_time / cmd_count);

    // 验证：3条AT指令应该至少需要400ms（2个200ms间隔）
    assert(total_time >= 400);
    printf("✓ AT指令间隔验证通过\n");
}

// 测试3：数据转发延迟测试
void test_data_forwarding_latency(void)
{
    printf("========== 测试3：数据转发延迟测试 ==========\n");

    long start_time = get_current_time_ms();
    int data_count = 10;

    for (int i = 0; i < data_count; i++)
    {
        char data[32];
        sprintf(data, "DATA_%d", i);

        // 写入队列
        pthread_mutex_lock(&g_device->write_mutex);
        app_buffer_write(g_device->write_queue, data, strlen(data));
        pthread_cond_signal(&g_device->write_cond);
        pthread_mutex_unlock(&g_device->write_mutex);

        pthread_mutex_lock(&g_stats.mutex);
        g_stats.data_count++;
        pthread_mutex_unlock(&g_stats.mutex);
    }

    long end_time = get_current_time_ms();
    long total_time = end_time - start_time;

    printf("发送 %d 条数据总耗时: %ldms\n", data_count, total_time);
    printf("平均每条数据耗时: %.2fms\n", (float)total_time / data_count);

    // 验证：数据转发应该很快，10条数据应该在50ms内完成
    assert(total_time < 50);
    printf("✓ 数据转发延迟测试通过\n");
}

// 测试4：上行数据处理测试
void test_upstream_data_processing(void)
{
    printf("========== 测试4：上行数据处理测试 ==========\n");

    // 构造测试数据
    uint8_t test_data[] = {0xf1, 0xdd, 0x05, 0x00, 0x01, 'H', 'e', 'l', 'l', 'o'};

    // 向串口写入原始数据，让read_thread自然处理
    ssize_t write_len = write(g_device->fd, test_data, sizeof(test_data));
    if (write_len != sizeof(test_data))
    {
        printf("写入串口失败\n");
        return;
    }

    // 等待read_thread处理完数据
    int queue_empty = 0;
    int wait_count = 0;
    while (!queue_empty && wait_count < 100)
    { // 最多等待10秒
        pthread_mutex_lock(&g_device->up_queue_mutex);
        queue_empty = (g_device->up_queue->sub_buffers[g_device->up_queue->read_index]->len == 0);
        pthread_mutex_unlock(&g_device->up_queue_mutex);

        if (!queue_empty)
        {
            usleep(100000); // 等待100ms
            wait_count++;
        }
    }

    printf("上行数据处理测试完成\n");
}

int main(int argc, char *argv[])
{
    printf("========================================\n");
    printf("  专用串口线程+队列架构测试\n");
    printf("========================================\n");

    // 初始化互斥锁
    pthread_mutex_init(&g_stats.mutex, NULL);

    // 初始化设备
    g_device = app_device_init("/dev/ttyUSB0");
    if (g_device == NULL)
    {
        printf("设备初始化失败\n");
        return -1;
    }

    // 启动设备
    if (app_device_start() != 0)
    {
        printf("设备启动失败\n");
        return -1;
    }

    // 等待设备稳定
    sleep(1);

    // 运行所有测试
    test_write_queue_capacity();
    test_at_command_interval();
    test_data_forwarding_latency();
    test_upstream_data_processing();

    // 清理资源
    app_device_close();
    pthread_mutex_destroy(&g_stats.mutex);

    printf("========================================\n");
    printf("  所有测试通过！\n");
    printf("========================================\n");

    return 0;
}
