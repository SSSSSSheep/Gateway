#define _GNU_SOURCE
#include "app_device.h"
#include "app_message.h"
#include "app_common.h"
#include "app_mqtt.h"
#include "app_pool.h"
#include "log/log.h"
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

static Device *device = NULL; // 整个应用中只有一个Device对象

// 将下行缓冲区数据写入蓝牙串口文件的任务函数 （线程池中某个线程函数调用）
// 两次写入的时间差要 >= 200ms
static int write_thread_fun(void *arg)
{
    Device *device = (Device *)arg;
    // 从下行缓冲区中读取一个字符数组message数据
    char data_buf[128];
    int data_len = app_buffer_read(device->down_buffer, data_buf, sizeof(data_buf));
    // 将字符数组message转换为蓝牙数据
    if (device->pre_write)
    {
        data_len = device->pre_write(data_buf, data_len);
    }

    // 写入文件前，限制两次写入的时间差 >= 200
    long distance = app_common_getCurrentTime() - device->last_write_time;
    if (distance < 200)
    {
        usleep((200 - distance) * 1000);
    }

    // 将蓝牙数据写入蓝牙串口文件
    ssize_t len = write(device->fd, data_buf, data_len);
    if (len != data_len)
    {
        log_error("write to bluetooth serial error");
        return -1;
    }
    log_debug("write to bluetooth serial success:%s", data_buf);
    // 保存当前时间
    device->last_write_time = app_common_getCurrentTime();
    return 0;
}
// 发送消息的任务函数 （线程池中某个线程函数调用）
static int send_message_task(void *arg)
{
    // 从上行缓冲区中读取一个字符数组message数据
    char data_buf[128];
    int data_len = app_buffer_read(device->up_buffer, data_buf, sizeof(data_buf));
    // 将字符数组message转换为json
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
// 不断从串口文件中读取数据的线程函数
static void *read_thread_fun(void *arg)
{
    while (device->is_running)
    {
        // 从串口文件中读取数据 蓝牙数据
        char data_buf[128];
        ssize_t data_len = read(device->fd, data_buf, sizeof(data_buf));
        // log_debug("------:%ld", data_len);
        //  将蓝牙数据转换成字符数组Message
        if (data_len > 0 && device->post_read)
        {
            data_len = device->post_read(data_buf, data_len);
        }

        if (data_len > 0)
        {
            // 将Message写入上行缓冲区
            app_buffer_write(device->up_buffer, data_buf, data_len);
            // 将上行缓冲区中的数据发送给远程
            app_pool_addTask(send_message_task, NULL);
        }
    }
}

// 处理接收的远程消息的回调函数
static int recv_msg_callback(char *json)
{
    // json消息转换为字符数组Message
    char data_buf[128];
    int data_len = app_message_json2Chars(json, data_buf, sizeof(data_buf));
    // 将Message写入下行缓冲区
    app_buffer_write(device->down_buffer, data_buf, data_len);
    // 将后面写入蓝牙串口文件的任务交给线程池模块完成 （添加一个将下行缓冲区数据写入蓝牙串口文件的任务）
    app_pool_addTask(write_thread_fun, device);
}
Device *app_device_init(char *filename)
{
    if (device)
    {
        return device;
    }
    // 申请Device内存
    device = (Device *)malloc(sizeof(Device));
    // 初始化Device指针
    device->filename = filename;
    device->fd = open(filename, O_RDWR);
    device->up_buffer = app_buffer_init(BUFFER_SIZE);
    device->down_buffer = app_buffer_init(BUFFER_SIZE);
    device->is_running = 0;
    device->post_read = NULL;
    device->pre_write = NULL;

    // 初始化线程池
    app_pool_init(5);
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
    // 启动上行流程： 启动读线程
    pthread_create(&device->read_thread, NULL, read_thread_fun, NULL);
    // 启动下行流程： 给MQTT模块注册一个接收远程消息的回调函数
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

    // 取消读线程
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
