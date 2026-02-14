#ifndef __APP_BUFFER_H__
#define __APP_BUFFER_H__

#include <pthread.h>

// 内部的子缓冲区
typedef struct
{
    unsigned char *ptr; // 指向缓冲区的指针
    int total_size;     // 缓冲区的大小
    int len;            // 缓冲区的数据长度
} SubBuffer;

// 缓冲区
typedef struct
{
    SubBuffer *sub_buffers[2];  // 两个子缓冲区
    int read_index;             // 当前读取的子缓冲区索引
    int write_index;            // 当前写入的子缓冲区索引
    pthread_mutex_t read_lock;  // 读锁
    pthread_mutex_t write_lock; // 写锁
} Buffer;

/**
 * @brief 初始化缓冲区
 *
 * @param size
 * @return Buffer*
 */
Buffer *app_buffer_init(int size); // 创建缓冲区

/**
 * @brief 清空缓冲区
 *
 * @param buffer
 */
void app_buffer_free(Buffer *buffer); // 释放缓冲区

/**
 * @brief 写入数据到缓冲区
 *
 * @param buffer
 * @param data
 * @param len
 * @return int
 */
int app_buffer_write(Buffer *buffer, char *data, int data_len); // 写入数据

/**
 * @brief 从缓冲区读取数据
 *
 * @param buffer
 * @param data
 * @param len
 * @return int
 */
int app_buffer_read(Buffer *buffer, char *data_buf, int buf_size); // 读取数据

#endif // !__APP_BUFFER_H__
