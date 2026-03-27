#ifndef __APP_DEVICE_H__
#define __APP_DEVICE_H__

#include <pthread.h>
#include <mqueue.h>
#include "app_buffer.h"

// 消息类型枚举
typedef enum
{
    MSG_TYPE_DATA = 0, // 数据转发消息（无写间隔限制）
    MSG_TYPE_AT_CMD,   // AT指令消息（有200ms写间隔限制）
    MSG_TYPE_MAX
} msg_type_t;

// 设备结构体
typedef struct
{
    char *filename;                        // 设备文件名
    int fd;                                // 设备文件描述符
    Buffer *up_buffer;                     // 上行缓冲区
    Buffer *down_buffer;                   // 下行缓冲区
    pthread_t read_thread;                 // 读线程
    pthread_t write_thread;                // 专用写线程
    Buffer *write_queue;                   // 写入队列
    pthread_mutex_t write_mutex;           // 写入队列互斥锁
    pthread_cond_t write_cond;             // 写入队列条件变量
    volatile int write_thread_running;     // 专用写线程是否运行
    volatile int is_running;               // 是否运行
    int (*post_read)(char *data, int len); // 读取后的回调函数 (data => message)
    int (*pre_write)(char *data, int len); // 写入前的回调函数 (message => data)
    Buffer *up_queue;                      // 上行消息队列
    pthread_mutex_t up_queue_mutex;        // 上行消息队列互斥锁
    pthread_cond_t up_queue_cond;          // 上行消息队列条件变量
    volatile int up_thread_running;        // 上行处理线程运行标志
    pthread_t up_thread;                   // 上行处理线程
    long last_write_time;                  // 上次写入时间
    msg_type_t last_msg_type;              // 上次写入的消息类型
} Device;

/**
 * @brief 初始化设备
 *
 * @param filename 设备文件名
 * @return Device* 设备指针
 */
Device *app_device_init(char *filename);

/**
 * @brief 启动设备
 *
 * @return int 0 成功，-1 失败
 */
int app_device_start();

/**
 * @brief 关闭设备
 *
 * @return int 0 成功，-1 失败
 */
int app_device_close();

/**
 * @brief 初始化写消息队列
 *
 * @return int 0 成功，-1 失败
 */
int app_device_init_write_mq(void);

/**
 * @brief 向消息队列发送数据
 *
 * @param data  数据
 * @param len    数据长度
 * @return int   0 成功，-1 失败
 */
int app_device_send_to_mq(const char *data, int len);

#endif // !__APP_DEVICE_H__