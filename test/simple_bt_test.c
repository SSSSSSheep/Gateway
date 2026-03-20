
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <pthread.h>
#include "app_bt.h"
#include "app_serial.h"
#include "log/log.h"

// 测试设备
static Device test_device;
static int test_running = 1;

// 模拟蓝牙响应的线程
void *bluetooth_simulator_thread(void *arg)
{
    int fd = *(int *)arg;
    char buf[256];
    int len;

    // 设置为非阻塞模式
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    while (test_running)
    {
        // 读取数据
        len = read(fd, buf, sizeof(buf) - 1);
        if (len > 0)
        {
            buf[len] = '\0';
            printf("Received: %s\n", buf);

            // 模拟蓝牙模块的响应
            if (strncmp(buf, "AT\r\n", 4) == 0)
            {
                write(fd, "OK\r\n", 4);
                printf("Sent: OK\r\n");
            }
            else if (strncmp(buf, "AT+NAME", 7) == 0)
            {
                write(fd, "OK\r\n", 4);
                printf("Sent: OK\r\n");
            }
            else if (strncmp(buf, "AT+BAUD", 7) == 0)
            {
                write(fd, "OK\r\n", 4);
                printf("Sent: OK\r\n");
            }
            else if (strncmp(buf, "AT+RESET", 8) == 0)
            {
                write(fd, "OK\r\n", 4);
                printf("Sent: OK\r\n");
            }
            else if (strncmp(buf, "AT+NETID", 8) == 0)
            {
                write(fd, "OK\r\n", 4);
                printf("Sent: OK\r\n");
            }
            else if (strncmp(buf, "AT+MADDR", 8) == 0)
            {
                write(fd, "OK\r\n", 4);
                printf("Sent: OK\r\n");
            }
        }
        else if (len < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            // 读取错误
            printf("Read error: %s\n", strerror(errno));
            break;
        }
        usleep(10000); // 10ms
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    int pty_master, pty_slave;
    char pty_name[256];
    pthread_t simulator_thread;

    log_info("Starting Simple Bluetooth Test");

    // 创建伪终端
    if (openpty(&pty_master, &pty_slave, pty_name, NULL, NULL) == -1)
    {
        log_error("Failed to create pseudo terminal: %s", strerror(errno));
        return -1;
    }

    log_info("Created pseudo terminal: %s", pty_name);

    // 初始化测试设备
    memset(&test_device, 0, sizeof(test_device));
    test_device.fd = pty_slave;

    // 创建模拟蓝牙响应的线程
    if (pthread_create(&simulator_thread, NULL, bluetooth_simulator_thread, &pty_master) != 0)
    {
        log_error("Failed to create simulator thread");
        close(pty_master);
        close(pty_slave);
        return -1;
    }

    // 初始化串口
    if (app_serial_init(&test_device) < 0)
    {
        log_error("Failed to initialize serial");
        test_running = 0;
        pthread_join(simulator_thread, NULL);
        close(pty_master);
        close(pty_slave);
        return -1;
    }

    log_info("Serial initialized successfully");

    // 测试AT指令
    log_info("Testing AT command");
    write(test_device.fd, "AT\r\n", 4);
    sleep(1);

    // 测试AT+NAME指令
    log_info("Testing AT+NAME command");
    write(test_device.fd, "AT+NAMEtest\r\n", 13);
    sleep(1);

    // 测试AT+BAUD指令
    log_info("Testing AT+BAUD command");
    write(test_device.fd, "AT+BAUD8\r\n", 11);
    sleep(1);

    // 停止测试
    test_running = 0;
    pthread_join(simulator_thread, NULL);

    // 关闭设备
    close(pty_master);
    close(pty_slave);

    log_info("Test completed");

    return 0;
}
