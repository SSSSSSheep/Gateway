
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

// 全局变量
static Device test_device;
static int test_running = 1;
static int ack_received = 0;

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

// 测试AT指令ACK
int test_at_ack(Device *device)
{
    log_info("=== Test: AT Command ACK ===");

    // 重置计数器
    ack_received = 0;

    // 发送AT指令
    const char *at_cmd = "AT\r\n";
    int written = write(device->fd, at_cmd, strlen(at_cmd));
    if (written != strlen(at_cmd))
    {
        log_error("Failed to send AT command");
        return -1;
    }
    log_info("Sent AT command");

    // 等待ACK
    sleep(1);

    // 检查结果
    if (ack_received > 0)
    {
        log_info("Test PASSED: Received %d ACK(s)", ack_received);
        return 0;
    }
    else
    {
        log_error("Test FAILED: No ACK received");
        return -1;
    }
}

int main(int argc, char *argv[])
{
    pthread_t ack_thread;

    log_info("Starting AT ACK Test");

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
    int test_result = test_at_ack(&test_device);

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
