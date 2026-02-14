#include "app_buffer.h"
#include "log/log.h"

int main(int argc, char *argv[])
{
    Buffer *buffer0 = app_buffer_init(1000);

    char data[255];
    for (int i = 0; i < 255; i++)
    {
        data[i] = 'a';
    }

    app_buffer_write(buffer0, data, 255);

    char data_buffer0[300];
    int data_len0 = app_buffer_read(buffer0, data_buffer0, 300);
    log_debug("read data0: %.*s, data_len: %d", data_len0, data_buffer0, data_len0);

    Buffer *buffer = app_buffer_init(13);

    // 写数据1
    app_buffer_write(buffer, "hello", 5);
    // 写数据2
    app_buffer_write(buffer, "world", 5);

    // 读数据1
    char *data_buffer[10];
    int data_len = app_buffer_read(buffer, data_buffer, 10);
    log_debug("read data1: %.*s, data_len: %d", data_len, data_buffer, data_len);

    // 读数据2
    char *data_buffer2[10];
    int data_len2 = app_buffer_read(buffer, data_buffer2, 10);
    log_debug("read data2: %.*s, data_len: %d", data_len2, data_buffer2, data_len2);

    // 读数据3
    char *data_buffer3[10];
    int data_len3 = app_buffer_read(buffer, data_buffer3, 10);
    log_debug("read data3: %.*s, data_len: %d", data_len3, data_buffer3, data_len3);

    // 写数据3
    app_buffer_write(buffer, "1231231231321321", 17);

    app_buffer_free(buffer);
    return 0;
}