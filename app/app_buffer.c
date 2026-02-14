#include "app_buffer.h"
#include "log/log.h"
#include <stdlib.h>
#include <string.h>

// 初始化子缓冲区
static Buffer *init_sub_buffer(int size)
{
    SubBuffer *sub_buffer = (SubBuffer *)malloc(sizeof(SubBuffer));
    sub_buffer->ptr = (char *)malloc(size);
    sub_buffer->total_size = size;
    sub_buffer->len = 0;

    return sub_buffer;
}

static void swap_sub_buffer(Buffer *buffer)
{
    // 如果正在写，不能切换 -> 切换操作要加写锁
    // 加写锁
    log_debug("Ready to switch buffer add write lock");
    pthread_mutex_lock(&buffer->write_lock);

    int temp = buffer->read_index;
    buffer->read_index = buffer->write_index;
    buffer->write_index = temp;

    // 解写锁
    log_debug("After switching buffer release write lock");
    pthread_mutex_unlock(&buffer->write_lock);
}

Buffer *app_buffer_init(int size)
{
    // 申请Buffer内存
    Buffer *buffer = (Buffer *)malloc(sizeof(Buffer));
    // 初始化Buffer的各个字段
    buffer->sub_buffers[0] = init_sub_buffer(size);
    buffer->sub_buffers[1] = init_sub_buffer(size);
    buffer->read_index = 0;
    buffer->write_index = 1;
    // 初始化读、写锁
    pthread_mutex_init(&buffer->read_lock, NULL);
    pthread_mutex_init(&buffer->write_lock, NULL);

    return buffer;
}

void app_buffer_free(Buffer *buffer)
{
    free(buffer->sub_buffers[0]->ptr);
    free(buffer->sub_buffers[0]);
    free(buffer->sub_buffers[1]->ptr);
    free(buffer->sub_buffers[1]);
    free(buffer);
}

int app_buffer_write(Buffer *buffer, char *data, int data_len)
{
    // 限制数据长度不要超过一个字节的大小
    if (data_len > 255)
    {
        log_error("data_len is too long");
        return -1;
    }

    // 加写锁
    log_debug("Before writing data add write lock");
    pthread_mutex_lock(&buffer->write_lock);
    // 得到写缓冲区
    SubBuffer *w_buffer = buffer->sub_buffers[buffer->write_index];

    // 判断写缓冲区剩余空间是否足够
    if (w_buffer->total_size - w_buffer->len < data_len + 1)
    {
        log_error("write buffer is not enough");
        pthread_mutex_unlock(&buffer->write_lock);
        return -1;
    }

    // 向写缓冲区写入数据：先写长度，再写数据
    w_buffer->ptr[w_buffer->len] = data_len;
    memcpy(w_buffer->ptr + w_buffer->len + 1, data, data_len);
    // 更新写缓冲区的len
    w_buffer->len += data_len + 1;

    // 写解锁
    log_debug("After writing the data release write lock");
    pthread_mutex_unlock(&buffer->write_lock);
    return 0;
}

int app_buffer_read(Buffer *buffer, char *data_buf, int buf_size)
{
    // 加读锁
    log_debug("Before reading data add read lock");
    pthread_mutex_lock(&buffer->read_lock);
    // 得到读缓冲区
    SubBuffer *r_buffer = buffer->sub_buffers[buffer->read_index];
    // 如果当前缓冲区没有数据，切换读写缓冲区
    if (r_buffer->len == 0)
    {
        swap_sub_buffer(buffer);
        r_buffer = buffer->sub_buffers[buffer->read_index];
        // 如果切换后仍然没有数据，返回-1
        if (r_buffer->len == 0)
        {
            log_error("no data in buffer");
            pthread_mutex_unlock(&buffer->read_lock);
            return -1;
        }
    }
    // 读取一份数据保存到data_buf中： 先读长度，再度数据
    int data_len = r_buffer->ptr[0];
    log_debug("------data_len: %d", data_len);
    // 判断data_buf是否足够大
    if (buf_size < data_len)
    {
        log_error("data_buf is not enough");
        pthread_mutex_unlock(&buffer->read_lock);
        return -1;
    }
    memcpy(data_buf, r_buffer->ptr + 1, data_len);
    // 将所有未读取的数据移动到最左边
    memmove(r_buffer->ptr, r_buffer->ptr + 1 + data_len, r_buffer->len - 1 - data_len);
    // 更新读缓冲区的len
    r_buffer->len -= 1 + data_len;

    // 读解锁
    log_debug("After reading the data release read lock");
    pthread_mutex_unlock(&buffer->read_lock);

    // 返回读取的数据长度
    return data_len;
}
