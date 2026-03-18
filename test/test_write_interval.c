#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <assert.h>

// 模拟消息类型
typedef enum
{
    MSG_TYPE_DATA = 0, // 数据转发消息（无写间隔限制）
    MSG_TYPE_AT_CMD,   // AT控制指令（需要200ms写间隔）
    MSG_TYPE_MAX
} msg_type_t;

// 模拟设备结构体
typedef struct
{
    long last_write_time;     // 上次写入时间
    msg_type_t last_msg_type; // 上次写入的消息类型
    int write_count;          // 写入计数
    int at_cmd_count;         // AT指令计数
    int data_count;           // 数据计数
} MockDevice;

// 获取当前时间（毫秒）
long mock_getCurrentTime()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// 模拟写入函数
int mock_write(MockDevice *device, const char *data, msg_type_t msg_type)
{
    long current_time = mock_getCurrentTime();

    // 写文件前，检查写时间间隔
    // 只有AT控制指令需要 >= 200ms间隔
    if (msg_type == MSG_TYPE_AT_CMD)
    {
        long distance = current_time - device->last_write_time;
        if (distance < 200)
        {
            printf("[AT] 需要等待 %ldms\n", 200 - distance);
            usleep((200 - distance) * 1000);
        }
    }
    // 数据转发路径无写间隔限制，直接写入

    // 模拟写入操作
    printf("[%s] 写入数据: %s, 时间间隔: %ldms\n", msg_type == MSG_TYPE_AT_CMD ? "AT" : "DATA",
           data,
           device->last_write_time > 0 ? current_time - device->last_write_time : 0);

    // 更新计数
    device->write_count++;
    if (msg_type == MSG_TYPE_AT_CMD)
    {
        device->at_cmd_count++;
    }
    else
    {
        device->data_count++;
    }

    // 保存当前时间和消息类型
    device->last_write_time = mock_getCurrentTime();
    device->last_msg_type = msg_type;

    return 0;
}

// 测试1：连续发送AT指令
void test_continuous_at_commands(MockDevice *device)
{
    printf("========== 测试1：连续发送AT指令 ==========\n");
    MockDevice test_device = *device;

    for (int i = 0; i < 3; i++)
    {
        char cmd[32];
        sprintf(cmd, "AT+CMD%d", i);
        mock_write(&test_device, cmd, MSG_TYPE_AT_CMD);
    }

    printf("AT指令总数: %d\n", test_device.at_cmd_count);
    printf("数据总数: %d\n", test_device.data_count);
    printf("写入总数: %d\n", test_device.write_count);
}

// 测试2：连续发送数据
void test_continuous_data(MockDevice *device)
{
    printf("========== 测试2：连续发送数据 ==========\n");
    MockDevice test_device = *device;

    for (int i = 0; i < 5; i++)
    {
        char data[32];
        sprintf(data, "DATA%d", i);
        mock_write(&test_device, data, MSG_TYPE_DATA);
    }

    printf("AT指令总数: %d\n", test_device.at_cmd_count);
    printf("数据总数: %d\n", test_device.data_count);
    printf("写入总数: %d\n", test_device.write_count);
}

// 测试3：混合发送AT指令和数据
void test_mixed_messages(MockDevice *device)
{
    printf("========== 测试3：混合发送AT指令和数据 ==========\n");
    MockDevice test_device = *device;

    // AT指令
    mock_write(&test_device, "AT+CMD1", MSG_TYPE_AT_CMD);

    // 数据（应该立即发送）
    mock_write(&test_device, "DATA1", MSG_TYPE_DATA);
    mock_write(&test_device, "DATA2", MSG_TYPE_DATA);

    // AT指令（应该等待200ms）
    mock_write(&test_device, "AT+CMD2", MSG_TYPE_AT_CMD);

    // 数据（应该立即发送）
    mock_write(&test_device, "DATA3", MSG_TYPE_DATA);

    printf("AT指令总数: %d\n", test_device.at_cmd_count);
    printf("数据总数: %d\n", test_device.data_count);
    printf("写入总数: %d\n", test_device.write_count);
}

// 测试4：性能测试 - 数据转发的延迟
void test_data_forwarding_latency(MockDevice *device)
{
    printf("========== 测试4：数据转发延迟测试 ==========\n");
    MockDevice test_device = *device;

    long start_time = mock_getCurrentTime();

    // 连续发送10条数据
    for (int i = 0; i < 10; i++)
    {
        char data[32];
        sprintf(data, "DATA%d", i);
        mock_write(&test_device, data, MSG_TYPE_DATA);
    }

    long end_time = mock_getCurrentTime();
    long total_time = end_time - start_time;

    printf("发送10条数据总耗时: %ldms\n", total_time);
    printf("平均每条数据耗时: %.2fms\n", (float)total_time / 10);
    printf("数据总数: %d", test_device.data_count);

    // 验证：数据转发应该很快，10条数据应该在50ms内完成
    assert(total_time < 50);
    printf("✓ 数据转发延迟测试通过\n");
}

// 测试5：AT指令间隔验证
void test_at_command_interval(MockDevice *device)
{
    printf("========== 测试5：AT指令间隔验证 ==========\n");
    MockDevice test_device = *device;

    long start_time = mock_getCurrentTime();

    // 连续发送3条AT指令
    for (int i = 0; i < 3; i++)
    {
        char cmd[32];
        sprintf(cmd, "AT+CMD%d", i);
        mock_write(&test_device, cmd, MSG_TYPE_AT_CMD);
    }

    long end_time = mock_getCurrentTime();
    long total_time = end_time - start_time;

    printf("发送3条AT指令总耗时: %ldms\n", total_time);
    printf("平均每条AT指令耗时: %.2fms\n", (float)total_time / 3);
    printf("AT指令总数: %d", test_device.at_cmd_count);

    // 验证：3条AT指令应该至少需要400ms（2个200ms间隔）
    assert(total_time >= 400);
    printf("✓ AT指令间隔验证通过\n");
}

int main()
{
    printf("========================================\n");
    printf("  写间隔细致化功能测试\n");
    printf("========================================\n");

    // 初始化模拟设备
    MockDevice device = {
        .last_write_time = 0,
        .last_msg_type = MSG_TYPE_DATA,
        .write_count = 0,
        .at_cmd_count = 0,
        .data_count = 0};

    // 运行所有测试
    test_continuous_at_commands(&device);
    test_continuous_data(&device);
    test_mixed_messages(&device);
    test_data_forwarding_latency(&device);
    test_at_command_interval(&device);

    printf("========================================\n");
    printf("  所有测试通过！\n");
    printf("========================================\n");

    return 0;
}
