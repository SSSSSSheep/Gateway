#ifndef __APP_DEVICE_H__
#define __APP_DEVICE_H__

#include <pthread.h>

#include "app_buffer.h"

// 设备结构体
typedef struct
{
    char *filename;                        // 串口设备文件名
    int fd;                                // 串口文件描述符
    Buffer *up_buffer;                     // 上行缓冲区
    Buffer *down_buffer;                   // 下行缓冲区
    pthread_t read_thread;                 // 读取串口文件中数据的线程
    int is_running;                        // 设备是否正在运行
    int (*post_read)(char *data, int len); // 读取串口数据后的回调函数 （蓝牙数据）=> message
    int (*pre_write)(char *data, int len); // 写入串口数据前的回调函数 (message => 蓝牙数据)
    long last_write_time;                  // 上次写入数据的时间
} Device;

/**
 * @brief 初始化设备
 *
 * @param filename 串口设备文件名
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

#endif // !__APP_DEVICE_H__