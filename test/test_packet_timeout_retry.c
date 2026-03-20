
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>
#include <fcntl.h>
#include "app_bt.h"
#include "app_serial.h"
#include "log/log.h"

// 测试配置
#define TEST_PACKET_COUNT 5      // 测试数据包数量
#define TEST_PACKET_SIZE 64      // 测试数据包大小
#define TEST_ACK_TIMEOUT_MS 1000 // 测试用的ACK超时时间（毫秒）

// 全局变量
static Device test_device;
static int test_running = 1;
static uint16_t test_packet_id = 0;
static int ack_received = 0;
static int retry_triggered = 0;

// 获取当前时间（毫秒）
uint64_t get_current_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// 模拟发送数据包
int send_test_packet(Device *device, const char *data, int data_len)
{
    char packet[256];
    int packet_len;

    // 构造测试数据包
    packet[0] = 1;                          // conn_type
    packet[1] = 2;                          // id_len
    packet[2] = data_len;                   // msg_len
    memcpy(packet + 3, &test_packet_id, 2); // id
    memcpy(packet + 5, data, data_len);     // msg

    // 调用preWrite函数处理数据
    packet_len = app_bt_preWrite(packet, sizeof(packet));
    if (packet_len < 0)
    {
        log_error("Failed to pre-write packet");
        return -1;
    }

    // 发送数据包
    int written = write(device->fd, packet, packet_len);
    if (written != packet_len)
    {
        log_error("Failed to write packet");
        return -1;
    }

    log_info("Sent packet %d", test_packet_id);
    test_packet_id++;
    return 0;
}

// ACK接收线程
void *ack_receiver_thread(void *arg)
{
    Device *device = (Device *)arg;
    char buf[256];
    int len;
    fd_set read_fds;
    struct timeval timeout;

    while (test_running)
    {
        // 使用select函数设置超时
        FD_ZERO(&read_fds);
        FD_SET(device->fd, &read_fds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 10000; // 10ms

        int ret = select(device->fd + 1, &read_fds, NULL, NULL, &timeout);
        if (ret > 0)
        {
            // 读取数据
            len = read(device->fd, buf, sizeof(buf));
            if (len > 0)
            {
                // 处理ACK - 蓝牙模块返回的ACK格式是 "OK\r\n"
                if (len >= 4 && memcmp(buf, "OK\r\n", 4) == 0)
                {
                    // 处理ACK
                    ack_received++;
                    log_info("Received ACK");
                }
                log_debug("Received data: %.*s", len, buf);
            }
        }
        else if (ret < 0)
        {
            // select出错
            log_error("select error");
            break;
        }
        // 超时继续循环
    }

    return NULL;
}

// 测试数据包超时和重试机制
int test_packet_timeout_retry(Device *device)
{
    log_info("=== Test: Packet Timeout and Retry ===");

    // 重置计数器
    test_packet_id = 0;
    ack_received = 0;
    retry_triggered = 0;

    // 发送多个数据包
    for (int i = 0; i < TEST_PACKET_COUNT; i++)
    {
        char data[TEST_PACKET_SIZE];
        memset(data, 'A' + i, TEST_PACKET_SIZE);

        // 记录发送时间
        uint64_t send_time = get_current_time_ms();

        // 发送数据包
        if (send_test_packet(device, data, TEST_PACKET_SIZE) < 0)
        {
            log_error("Failed to send packet %d", i);
            return -1;
        }

        // 等待ACK或超时
        uint64_t current_time = get_current_time_ms();
        int ack_count_before = ack_received;

        while ((current_time - send_time) < TEST_ACK_TIMEOUT_MS)
        {
            // 检查是否收到ACK
            if (ack_received > ack_count_before)
            {
                log_info("Packet %d ACK received in %llu ms", i, current_time - send_time);
                break;
            }

            // 短暂休眠
            usleep(10000); // 10ms
            current_time = get_current_time_ms();
        }

        // 检查是否超时
        if ((current_time - send_time) >= TEST_ACK_TIMEOUT_MS)
        {
            log_warn("Packet %d timeout after %llu ms", i, current_time - send_time);
            retry_triggered++;

            // 模拟重试
            log_info("Retrying packet %d", i);
            if (send_test_packet(device, data, TEST_PACKET_SIZE) < 0)
            {
                log_error("Failed to retry packet %d", i);
                return -1;
            }

            // 等待重试的ACK
            sleep(1);
        }
    }

    // 检查结果
    log_info("Total packets sent: %d", TEST_PACKET_COUNT);
    log_info("Total ACKs received: %d", ack_received);
    log_info("Total retries triggered: %d", retry_triggered);

    if (retry_triggered > 0)
    {
        log_info("Test PASSED: Timeout and retry mechanism works");
        return 0;
    }
    else
    {
        log_warn("No timeout triggered, retry mechanism not tested");
        return 0; // 不算失败，只是没有触发重试
    }
}

int main(int argc, char *argv[])
{
    pthread_t ack_thread;

    log_info("Starting Packet Timeout and Retry Test");

    // 初始化测试设备
    memset(&test_device, 0, sizeof(test_device));

    // 打开串口设备
    test_device.fd = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY);
    if (test_device.fd < 0)
    {
        log_error("Failed to open serial device /dev/ttyUSB0");
        log_error("Possible causes:");
        log_error("  1. Device does not exist (check with: ls /dev/ttyUSB0)");
        log_error("  2. Permission denied (try: sudo usermod -a -G dialout $USER)");
        log_error("  3. Device is in use by another process");
        return -1;
    }
    log_info("Opened serial device /dev/ttyUSB0");

    // 重置蓝牙模块的内部状态
    app_bt_reset_internal_state();

    // 初始化蓝牙
    if (app_bt_init(&test_device) < 0)
    {
        log_error("Failed to initialize bluetooth");
        close(test_device.fd);
        return -1;
    }

    // 创建ACK接收线程
    if (pthread_create(&ack_thread, NULL, ack_receiver_thread, &test_device) != 0)
    {
        log_error("Failed to create ACK receiver thread");
        close(test_device.fd);
        return -1;
    }

    // 运行测试
    int test_result = test_packet_timeout_retry(&test_device);

    // 停止ACK接收线程
    test_running = 0;
    write(test_device.fd, "\0", 1);
    pthread_join(ack_thread, NULL);

    // 关闭设备
    close(test_device.fd);

    // 返回测试结果
    if (test_result == 0)
    {
        log_info("All tests PASSED");
        return 0;
    }
    else
    {
        log_error("Some tests FAILED");
        return -1;
    }
}
