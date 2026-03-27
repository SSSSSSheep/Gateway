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

// 上行处理线程函数
static void *up_thread_fun(void *arg)
{
    Device *dev = (Device *)arg;
    uint8_t data_buf[128];

    while (dev->up_thread_running)
    {
        pthread_mutex_lock(&dev->up_queue_mutex);

        // 等待队列中有数据
        while (dev->up_queue->sub_buffers[dev->up_queue->read_index]->len == 0 && dev->up_thread_running)
        {
            pthread_cond_wait(&dev->up_queue_cond, &dev->up_queue_mutex);
        }

        if (!dev->up_thread_running)
        {
            pthread_mutex_unlock(&dev->up_queue_mutex);
            break;
        }

        // 从up_queue读取数据
        int data_len = app_buffer_read(dev->up_queue, data_buf, sizeof(data_buf));

        pthread_mutex_unlock(&dev->up_queue_mutex);

        if (data_len <= 0)
        {
            continue;
        }

        // data_buf 已经是解析好的应用层数据（格式为 conn_type + id_len + msg_len + id + msg）
        // 直接传递给 MQTT 发布，无需再调用 app_bt_postRead
        if (app_pool_add_task(JOB_TYPE_MQTT_PUBLISH, data_buf, data_len) != 0)
        {
            log_error("add task to pool error");
            continue;
        }
        log_debug("Processed data added to MQTT queue, len=%d", data_len);
    }
    return NULL;
}

static void *write_thread_fun(void *arg)
{
    Device *dev = (Device *)arg;
    char data_buf[128];

    while (dev->write_thread_running)
    {
        pthread_mutex_lock(&dev->write_mutex);

        // 等待队列中有数据
        while (dev->write_queue->sub_buffers[dev->write_queue->read_index]->len == 0 && dev->write_thread_running)
        {
            pthread_cond_wait(&dev->write_cond, &dev->write_mutex);
        }

        if (!dev->write_thread_running)
        {
            pthread_mutex_unlock(&dev->write_mutex);
            break;
        }

        // 从write_queue读取数据
        int data_len = app_buffer_read(dev->write_queue, data_buf, sizeof(data_buf));

        pthread_mutex_unlock(&dev->write_mutex);

        if (data_len <= 0)
            continue;

        // 判断消息类型
        msg_type_t msg_type = MSG_TYPE_DATA;
        if (data_len >= 2 && strncmp(data_buf, "AT", 2) == 0)
        {
            msg_type = MSG_TYPE_AT_CMD;
        }

        // AT指令间隔控制
        if (msg_type == MSG_TYPE_AT_CMD)
        {
            long distance = app_common_getCurrentTime() - dev->last_write_time;
            if (distance < 200)
            {
                // 简单的延时优化：如果延时期间收到退出信号，能更快响应
                long delay = (200 - distance) * 1000;
                long sleep_step = 10000; // 10ms
                while (delay > 0 && dev->write_thread_running)
                {
                    usleep(sleep_step);
                    delay -= sleep_step;
                }
                if (!dev->write_thread_running)
                {
                    break;
                }
            }
        }

        // 添加跟踪
        int need_track = 0;
        if (data_len > 0 && strncmp(data_buf, "AT+MESH", 7) == 0)
        {
            need_track = 1;
        }
        else if (data_len > 0 && strncmp(data_buf, "AT+MESHSTATISTICS", 16) == 0)
        {
            need_track = 1;
        }

        // 处理非阻塞写入
        ssize_t write_len = 0;
        int retry_count = 0;
        const int max_retry = 3;

        while (retry_count < max_retry)
        {
            write_len = write(dev->fd, data_buf, data_len);

            if (write_len == data_len)
            {
                // 写入完整，跳出重试循环
                break;
            }
            else if (write_len < 0)
            {
                // 发生错误
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    // 缓冲区满，等待一小会儿重试
                    log_warn("Serial write buffer full, retrying...");
                    usleep(20000); // 20ms
                    retry_count++;
                }
                else if (errno == EBADF)
                {
                    // 文件描述符已关闭，退出循环
                    log_debug("File descriptor closed, exiting write thread");
                    break;
                }
                else
                {
                    // 真正的错误（如设备断开）
                    log_error("Serial write error: %s", strerror(errno));
                    retry_count = max_retry; // 强制退出
                    break;
                }
            }
            else
            {
                // 部分写入 (处理逻辑较复杂，通常建议重试整个包或丢弃)
                // 这里简单处理：视为失败，重试
                log_warn("Serial partial write: %d/%d, retrying...", (int)write_len, data_len);
                retry_count++;
            }
        }

        // 检查最终写入结果
        if (write_len != data_len)
        {
            log_error("Failed to write to serial after retries. Dropping packet.");
            continue;
        }

        // 写入成功，添加追踪
        if (need_track)
        {
            int ret = app_bt_add_tracker(dev, data_buf, data_len);
            if (ret != 0)
            {
                log_error("CRITICAL: Message sent but tracker failed to add!");
            }
        }

        // 安全打印：确保不越界
        if (data_len > 0)
        {
            // 临时截断字符串以便打印，防止 %s 越界
            char temp_buf[128];
            int print_len = data_len < sizeof(temp_buf) - 1 ? data_len : sizeof(temp_buf) - 1;
            memcpy(temp_buf, data_buf, print_len);
            temp_buf[print_len] = '\0';
            log_debug("write to bluetooth serial success, len=%d, data=%.32s...", data_len, temp_buf);
        }

        // 更新时间和类型
        dev->last_write_time = app_common_getCurrentTime();
        dev->last_msg_type = msg_type;
    }

    return NULL;
}

// 写线程函数：从下行缓冲区读取数据并写入设备文件
// 注意：AT控制指令写时间间隔需要 >= 200ms，数据转发无限制
static int write_thread_handler(const uint8_t *data, uint32_t len)
{
    if (!device || !data || len == 0)
        return -1;

    char data_buf[128];
    int data_len = (len > sizeof(data_buf)) ? sizeof(data_buf) : len;
    memcpy(data_buf, data, data_len);

    // pre_write处理
    if (device->pre_write)
    {
        data_len = device->pre_write(data_buf, data_len);
        if (data_len < 0)
            return -1;
    }

    // 写入write_queue
    pthread_mutex_lock(&device->write_mutex);
    app_buffer_write(device->write_queue, data_buf, data_len);
    pthread_cond_signal(&device->write_cond);
    pthread_mutex_unlock(&device->write_mutex);

    return 0;
}

// 上行消息处理函数：从上行缓冲区读取数据并发送给远程
// 线程池的某个线程函数会被调用
static int send_message_handler(const uint8_t *data, uint32_t len)
{
    // 参数data和len已经处理过

    // 1. 将字符数据message 转换为json
    char *json = app_message_chars2Json((char *)data, len);
    if (!json)
    {
        log_error("chars2Json returned NULL");
        return -1;
    }

    // 2. 将json数据发送给远程
    int res = app_mqtt_send(json);
    if (res == -1)
    {
        log_error("mqtt send error");
        free(json);
        return -1;
    }
    log_debug("mqtt send success:%s", json);
    free(json);
    return 0;
}

// 专门从设备文件中读取数据的线程函数
static void *read_thread_fun(void *arg)
{
    Device *dev = (Device *)arg;
    while (dev->is_running)
    {
        // 从设备文件中读取数据到缓冲区
        char data_buf[128];
        ssize_t data_len = read(dev->fd, data_buf, sizeof(data_buf));

        if (data_len <= 0)
        {
            if (data_len == 0)
            {
                log_error("read return 0, device closed");
                break; // 设备已关闭，退出循环
            }
            else
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    // 非阻塞模式下没有数据，这是正常的
                    usleep(10000); // 10ms 延迟
                }
                else if (errno == EBADF)
                {
                    // 文件描述符已关闭，退出循环
                    log_debug("File descriptor closed, exiting read thread");
                    break;
                }
                else
                {
                    log_error("read error:%s", strerror(errno));
                    usleep(100000);
                }
            }
            // 检查退出标志
            if (!dev->is_running)
            {
                break;
            }
            continue;
        }

        // 将数据转换为字符数组Message
        if (data_len > 0 && dev->post_read)
        {
            data_len = dev->post_read(data_buf, data_len);
            if (data_len < 0)
            {
                log_error("post_read returned negative length: %d", data_len);
                continue;
            }
        }

        if (data_len > 0)
        {
            // 将原始消息写入上行缓冲区
            int ret = app_buffer_write(dev->up_buffer, data_buf, data_len);
            if (ret != 0)
            {
                log_error("Failed to write to up buffer: %d", ret);
                continue;
            }
            // 将数据从app_buffer移动到up_queue
            pthread_mutex_lock(&dev->up_queue_mutex);
            ret = app_buffer_read(dev->up_buffer, data_buf, sizeof(data_buf));
            if (ret > 0)
            {
                app_buffer_write(dev->up_queue, data_buf, ret);
                pthread_cond_signal(&dev->up_queue_cond);
            }
            pthread_mutex_unlock(&dev->up_queue_mutex);
        }
    }
}

// 当收到远程消息的回调函数
static int recv_msg_callback(char *json)
{
    // json消息转换为字符数组Message
    char data_buf[128];
    int data_len = app_message_json2Chars(json, data_buf, sizeof(data_buf));
    if (data_len <= 0)
    {
        log_error("json2Chars returned invalid length: %d", data_len);
        return -1;
    }
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
    if (!device)
    {
        log_error("Failed to allocate memory for device");
        return NULL;
    }

    // 初始化Device指针
    device->filename = filename;
    device->fd = open(filename, O_RDWR | O_NOCTTY | O_NONBLOCK);
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
    // 在app_device_init或类似的初始化函数中添加

    // 初始化MQTT模块
    app_mqtt_init();

    // 初始化write_queue
    device->write_queue = app_buffer_init(BUFFER_SIZE);

    // 初始化互斥锁和条件变量
    pthread_mutex_init(&device->write_mutex, NULL);
    pthread_cond_init(&device->write_cond, NULL);

    // 初始化写入线程运行标志
    device->write_thread_running = 0;

    // 初始化up_queue
    device->up_queue = app_buffer_init(BUFFER_SIZE);

    // 初始化互斥锁和条件变量
    pthread_mutex_init(&device->up_queue_mutex, NULL);
    pthread_cond_init(&device->up_queue_cond, NULL);

    // 初始化上行处理线程运行标志
    device->up_thread_running = 0;

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
    pthread_create(&device->read_thread, NULL, read_thread_fun, device);
    // 启动专用写入线程
    device->write_thread_running = 1;
    pthread_create(&device->write_thread, NULL, write_thread_fun, device);
    // 启动上行处理线程
    device->up_thread_running = 1;
    pthread_create(&device->up_thread, NULL, up_thread_fun, device);
    // 启动MQTT模块，注册一个接收远程消息的回调函数
    app_mqtt_registerRecvCallback(recv_msg_callback);

    return 0;
}

int app_device_close()
{
    if (!device)
    {
        return 0;
    }

    log_debug("app_device_close: 开始关闭设备");

    // 设置退出标志，通知读取线程退出
    log_debug("app_device_close: 设置退出标志");
    device->is_running = 0;
    device->write_thread_running = 0;
    device->up_thread_running = 0;

    // 唤醒有可能等待在条件变量上的线程
    log_debug("app_device_close: 唤醒write_thread");
    pthread_mutex_lock(&device->write_mutex);
    pthread_cond_broadcast(&device->write_cond);
    pthread_mutex_unlock(&device->write_mutex);

    // 唤醒上行处理线程
    log_debug("app_device_close: 唤醒up_thread");
    pthread_mutex_lock(&device->up_queue_mutex);
    pthread_cond_broadcast(&device->up_queue_cond);
    pthread_mutex_unlock(&device->up_queue_mutex);

    // 关闭文件描述符，这会中断阻塞在read()上的线程
    log_debug("app_device_close: 关闭文件描述符");
    if (device->fd >= 0)
    {
        close(device->fd);
        device->fd = -1;
    }

    // 等待线程结束
    log_debug("app_device_close: 等待read_thread结束");
    if (device->read_thread)
    {
        pthread_join(device->read_thread, NULL);
    }
    log_debug("app_device_close: read_thread已结束");

    log_debug("app_device_close: 等待write_thread结束");
    if (device->write_thread)
    {
        pthread_join(device->write_thread, NULL);
    }
    log_debug("app_device_close: write_thread已结束");

    log_debug("app_device_close: 等待up_thread结束");
    if (device->up_thread)
    {
        pthread_join(device->up_thread, NULL);
    }
    log_debug("app_device_close: up_thread已结束");

    // 销毁线程池（这会关闭消息队列）
    log_debug("app_device_close: 销毁线程池");
    app_pool_destroy();
    log_debug("app_device_close: 线程池已销毁");

    // 关闭MQTT
    log_debug("app_device_close: 关闭MQTT");
    app_mqtt_close();
    log_debug("app_device_close: MQTT已关闭");

    // 释放内存
    log_debug("app_device_close: 释放内存");
    if (device->up_buffer)
    {
        app_buffer_free(device->up_buffer);
        device->up_buffer = NULL;
    }
    if (device->down_buffer)
    {
        app_buffer_free(device->down_buffer);
        device->down_buffer = NULL;
    }
    if (device->write_queue)
    {
        app_buffer_free(device->write_queue);
        device->write_queue = NULL;
    }

    // 销毁锁和条件变量
    pthread_mutex_destroy(&device->write_mutex);
    pthread_cond_destroy(&device->write_cond);

    pthread_mutex_destroy(&device->up_queue_mutex);
    pthread_cond_destroy(&device->up_queue_cond);

    free(device);

    device = NULL;
    log_debug("app_device_close: 设备已关闭");
    return 0;
}
