
#include "../app/app_mqtt.h"
#include "log/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

// 测试配置
#define TEST_MESSAGES 10
#define TEST_MESSAGE_SIZE 256

// 全局变量
static volatile int running = 1;
static int messages_sent = 0;
static int messages_received = 0;
static int messages_confirmed = 0;

// 信号处理函数
void signal_handler(int signum)
{
    log_info("Received signal %d, stopping...", signum);
    running = 0;
}

// 接收回调函数
int recv_callback(char *json)
{
    messages_received++;
    log_info("Received message %d: %s", messages_received, json);
    return 0;
}

// 生成测试消息
void generate_test_message(char *buffer, int size, int message_num)
{
    snprintf(buffer, size, "{\"test\":true,\"message_num\":%d,\"timestamp\":%ld}",
             message_num, time(NULL));
}

// 测试1：基本发送和接收
int test_basic_send_receive(void)
{
    log_info("=== Test 1: Basic send and receive ===");

    char message[TEST_MESSAGE_SIZE];
    int success = 0;

    // 发送测试消息
    for (int i = 0; i < 5; i++)
    {
        generate_test_message(message, sizeof(message), i + 1);
        if (app_mqtt_send(message) == 0)
        {
            success++;
            messages_sent++;
            log_info("Sent message %d", i + 1);
        }
        else
        {
            log_error("Failed to send message %d", i + 1);
        }
        sleep(1);
    }

    log_info("Test 1: %d/%d messages sent successfully", success, 5);
    return success == 5 ? 0 : -1;
}

// 测试2：断线重连测试
int test_reconnect(void)
{
    log_info("=== Test 2: Reconnect test ===");

    char message[TEST_MESSAGE_SIZE];
    int success = 0;

    // 发送一些消息
    for (int i = 0; i < 3; i++)
    {
        generate_test_message(message, sizeof(message), i + 1);
        if (app_mqtt_send(message) == 0)
        {
            success++;
            messages_sent++;
            log_info("Sent message %d before disconnect", i + 1);
        }
        sleep(1);
    }

    // 模拟断线（关闭MQTT连接）
    log_info("Simulating disconnect...");
    app_mqtt_close();
    sleep(3);

    // 重新初始化MQTT
    log_info("Reinitializing MQTT...");
    if (app_mqtt_init() != 0)
    {
        log_error("Failed to reinitialize MQTT");
        return -1;
    }

    // 注册接收回调
    app_mqtt_registerRecvCallback(recv_callback);

    // 发送更多消息
    for (int i = 0; i < 3; i++)
    {
        generate_test_message(message, sizeof(message), i + 4);
        if (app_mqtt_send(message) == 0)
        {
            success++;
            messages_sent++;
            log_info("Sent message %d after reconnect", i + 4);
        }
        sleep(1);
    }

    log_info("Test 2: %d/6 messages sent successfully", success);
    return success == 6 ? 0 : -1;
}

// 测试3：未确认消息检查
int test_unconfirmed_messages(void)
{
    log_info("=== Test 3: Unconfirmed messages check ===");

    char message[TEST_MESSAGE_SIZE];
    int success = 0;

    // 发送大量消息
    for (int i = 0; i < 10; i++)
    {
        generate_test_message(message, sizeof(message), i + 1);
        if (app_mqtt_send(message) == 0)
        {
            success++;
            messages_sent++;
            log_info("Sent message %d", i + 1);
        }
        sleep(1);
    }

    // 检查未确认消息
    app_mqtt_check_unconfirmed_messages();

    log_info("Test 3: %d/10 messages sent", success);
    return success == 10 ? 0 : -1;
}

// 测试4：长时间运行测试
int test_long_running(void)
{
    log_info("=== Test 4: Long running test ===");

    char message[TEST_MESSAGE_SIZE];
    int success = 0;
    int duration = 30; // 运行30秒
    time_t start_time = time(NULL);

    while (time(NULL) - start_time < duration && running)
    {
        generate_test_message(message, sizeof(message), messages_sent + 1);
        if (app_mqtt_send(message) == 0)
        {
            success++;
            messages_sent++;
            log_info("Sent message %d", messages_sent);
        }

        // 检查未确认消息
        app_mqtt_check_unconfirmed_messages();

        sleep(2);
    }

    log_info("Test 4: %d messages sent in %d seconds", success, duration);
    return 0;
}

// 打印统计信息
void print_statistics(void)
{
    log_info("=== Statistics ===");
    log_info("Messages sent: %d", messages_sent);
    log_info("Messages received: %d", messages_received);
    log_info("Messages confirmed: %d", messages_confirmed);
}

int main(int argc, char *argv[])
{
    int test_result = 0;

    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    log_info("Starting MQTT reliability test");

    // 初始化MQTT
    if (app_mqtt_init() != 0)
    {
        log_error("Failed to initialize MQTT");
        return -1;
    }

    // 注册接收回调
    app_mqtt_registerRecvCallback(recv_callback);

    // 运行测试
    if (test_basic_send_receive() != 0)
    {
        log_error("Test 1 failed");
        test_result = -1;
    }

    sleep(5);

    if (test_reconnect() != 0)
    {
        log_error("Test 2 failed");
        test_result = -1;
    }

    sleep(5);

    if (test_unconfirmed_messages() != 0)
    {
        log_error("Test 3 failed");
        test_result = -1;
    }

    sleep(5);

    if (test_long_running() != 0)
    {
        log_error("Test 4 failed");
        test_result = -1;
    }

    // 打印统计信息
    print_statistics();

    // 清理
    app_mqtt_close();

    log_info("MQTT reliability test completed with result: %d", test_result);
    return test_result;
}
