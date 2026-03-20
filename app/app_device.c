#define _GNU_SOURCE
#include "app_device.h"
#include "app_message.h"
#include "app_common.h"
#include "app_mqtt.h"
#include "app_pool.h"
#include "app_bt.h"
#include "log/log.h"
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define BUFFER_SIZE 1024

static Device *device = NULL; // 整个应用只有一个Device对象

// 写线程函数：从下行缓冲区读取数据并写入设备文件
// 注意：AT控制指令写时间间隔需要 >= 200ms，数据转发无限制
static int write_thread_handler(const uint8_t *data, uint32_t len)
{
    // 从下行缓冲区读取一个字符数组message数据
    char data_buf[128];
    int data_len = app_buffer_read(device->down_buffer, data_buf, sizeof(data_buf));

    // 判断消息类型（这里需要根据实际业务逻辑判断）
    msg_type_t msg_type = MSG_TYPE_DATA; // 默认为数据转发

    // 根据实际业务逻辑判断消息类型
    // 例如：如果数据以"AT"开头，则为AT控制指令
    if (data_len >= 2 && strncmp(data_buf, "AT", 2) == 0)
    {
        msg_type = MSG_TYPE_AT_CMD;
    }

    // 字符数组message转换为字节数组
    if (device->pre_write)
    {
        data_len = device->pre_write(data_buf, data_len);
    }

    // 写文件前，检查写时间间隔
    // 只有AT控制指令需要 >= 200ms间隔
    if (msg_type == MSG_TYPE_AT_CMD)
    {
        long distance = app_common_getCurrentTime() - device->last_write_time;
        if (distance < 200)
        {
            usleep((200 - distance) * 1000);
        }
    }
    // 数据转发路径无写间隔限制，直接写入

    // 添加跟踪
    if (data_len > 0 && strncmp(data_buf, "AT+MESH", 7) == 0)
    {
        app_bt_add_tracker(device, data_buf, data_len);
    }

    // 将字节数组写入设备文件
    ssize_t write_len = write(device->fd, data_buf, data_len);
    if (write_len != data_len)
    {
        log_error("write to bluetooth serial error");
        return -1;
    }
    log_debug("write to bluetooth serial success:%s", data_buf);

    // 保存当前时间和消息类型
    device->last_write_time = app_common_getCurrentTime();
    device->last_msg_type = msg_type;

    return 0;
}

// 上行消息处理函数：从上行缓冲区读取数据并发送给远程
// 线程池的某个线程函数会被调用
static int send_message_handler(const uint8_t *data, uint32_t len)
{
    // 从上行缓冲区读取一个字符数组message数据
    char data_buf[128];
    int data_len = app_buffer_read(device->up_buffer, data_buf, sizeof(data_buf));
    // 字符数组message转换为json
    char *json = app_message_chars2Json(data_buf, data_len);
    // 将json数据发送给远程
    int res = app_mqtt_send(json);
    if (res == -1)
    {
        log_error("mqtt send error");
        return -1;
    }
    log_debug("mqtt send success:%s", json);
    return 0;
}

// 专门从设备文件中读取数据的线程函数
static void *read_thread_fun(void *arg)
{
    while (device->is_running)
    {
        // 从设备文件中读取数据到缓冲区
        char data_buf[128];
        ssize_t data_len = read(device->fd, data_buf, sizeof(data_buf));

        if (data_len <= 0)
        {
            if (data_len == 0)
            {
                log_debug("read returen 0, device closed");
            }
            else
            {
                log_debug("read error:%s", strerror(errno));
            }
            continue;
        }

        // 将数据转换为字符数组Message
        if (data_len > 0 && device->post_read)
        {
            data_len = device->post_read(data_buf, data_len);
        }

        if (data_len > 0)
        {
            // 将Message写入上行缓冲区
            app_buffer_write(device->up_buffer, data_buf, data_len);
            // 将上行缓冲区中的数据发送给远程
            app_pool_add_task(JOB_TYPE_MQTT_PUBLISH, (const uint8_t *)data_buf, data_len);
        }
        
        // 定期检查超时的数据包并进行重试
        app_bt_check_and_retry(device);
    }
    return NULL;
}

// 当收到远程消息的回调函数
static int recv_msg_callback(char *json)
{
    // json消息转换为字符数组Message
    char data_buf[128];
    int data_len = app_message_json2Chars(json, data_buf, sizeof(data_buf));
    // 将Message写入下行缓冲区
    app_buffer_write(device->down_buffer, data_buf, data_len);
    // 将写设备文件的任务交给线程池，线程池会调度某个线程来执行
    // 这个线程会从下行缓冲区读取数据并写入设备文件
    app_pool_add_task(JOB_TYPE_SERIAL_SEND, (const uint8_t *)data_buf, data_len);
    return 0;
}

Device *app_device_init(char *filename)
{
    if (device)
    {
        return device;
    }
    // 分配Device内存
    device = (Device *)malloc(sizeof(Device));
    // 初始化Device指针
    device->filename = filename;
    device->fd = open(filename, O_RDWR);
    device->up_buffer = app_buffer_init(BUFFER_SIZE);
    device->down_buffer = app_buffer_init(BUFFER_SIZE);
    device->is_running = 0;
    device->post_read = NULL;
    device->pre_write = NULL;
    device->last_write_time = 0;           // 初始化为0
    device->last_msg_type = MSG_TYPE_DATA; // 初始化为数据类型

    // 初始化线程池
    app_pool_init(5);
    // 注册任务处理器
    app_pool_register_handler(JOB_TYPE_MQTT_PUBLISH, send_message_handler);
    app_pool_register_handler(JOB_TYPE_SERIAL_SEND, write_thread_handler);
    // 初始化MQTT模块
    app_mqtt_init();
    // 返回Device指针
    return device;
}

int app_device_start()
{
    if (device->is_running)
    {
        log_debug("read thread is running");
        return 0;
    }
    device->is_running = 1;
    // 创建读取线程
    pthread_create(&device->read_thread, NULL, read_thread_fun, NULL);
    // 启动MQTT模块，注册一个接收远程消息的回调函数
    app_mqtt_registerRecvCallback(recv_msg_callback);
    return 0;
}

int app_device_close()
{
    // 关闭文件
    close(device->fd);
    // 释放buffer
    app_buffer_free(device->up_buffer);
    app_buffer_free(device->down_buffer);
    // 取消线程
    pthread_cancel(device->read_thread);
    pthread_join(device->read_thread, NULL);
    // 释放Device
    free(device);
    // 关闭线程池
    app_pool_destroy();
    // 关闭MQTT
    app_mqtt_close();
    return 0;
}
