#ifndef __APP_BUFFER_H__
#define __APP_BUFFER_H__

#include <pthread.h>

// 子缓冲区结构体，用于存储数据
typedef struct
{
    unsigned char *ptr; // 内存指针
    int total_size;     // 总大小
    int len;            // 当前长度
} SubBuffer;

// 缓冲区结构体，用于管理多个子缓冲区
typedef struct
{
    SubBuffer *sub_buffers[2];  // 两个子缓冲区
    int read_index;             // 当前读缓冲区索引
    int write_index;            // 当前写缓冲区索引
    pthread_mutex_t read_lock;  // 读锁
    pthread_mutex_t write_lock; // 写锁
} Buffer;

/**
 * @brief 初始化缓冲区
 *
 * @param size 缓冲区大小
 * @return Buffer* 缓冲区指针
 */
Buffer *app_buffer_init(int size); // 初始化缓冲区

/**
 * @brief 释放缓冲区
 *
 * @param buffer
 */
void app_buffer_free(Buffer *buffer); // 释放缓冲区

/**
 * @brief 写入数据
 *
 * @param buffer 缓冲区指针
 * @param data  要写入的数据
 * @param len    数据长度
 * @return int
 */
int app_buffer_write(Buffer *buffer, char *data, int data_len); // 写入数据

/**
 * @brief 读取数据
 *
 * @param buffer    缓冲区指针
 * @param data    存储读取的数据
 * @param len    读取的数据长度
 * @return int
 */
int app_buffer_read(Buffer *buffer, char *data_buf, int buf_size); // 读取数据

#endif // !__APP_BUFFER_H__
