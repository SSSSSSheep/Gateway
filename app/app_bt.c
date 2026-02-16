#define _GNU_SOURCE
#include "app_bt.h"
#include "app_serial.h"
#include "log/log.h"
#include <string.h>
#include <unistd.h>

static int init_bluetooth(Device *device)
{
    // 初始化串口
    app_serial_init(device);

    // 设置串口为非阻塞模式
    app_serial_setBlockMode(device, 0);
    app_serial_flush(device);

    // 串口的波特率当前是9600
    // 如果当前蓝牙可用，说明当前的波特率为9600
    if (app_bt_status(device) == 0)
    {
        // 将蓝牙的波特率设置为115200
        app_bt_setBaudRate(device, BT_BR_115200);
        // 重启蓝牙设备
        app_bt_reset(device);
        // 等待蓝牙设备重启完成
        sleep(2);
    }
    // 将串口的波特率设置为115200
    app_serial_setBaudRate(device, BR_115200);
    app_serial_flush(device);

    // 判断蓝牙是否可用 如果不可用 返回-1
    if (app_bt_status(device) != 0)
    {
        log_error("bluetooth is not available");
        return -1;
    }
    // 设置组网ID: 组内相同 组间不同
    app_bt_setNetId(device, "1111");
    // 设置MAC地址: 组内不同 组间可以相同
    app_bt_setMaddr(device, "0001");
    // 将串口设置为阻塞模式
    app_serial_setBlockMode(device, 1);
    app_serial_flush(device);

    log_debug("bluetooth is available");
    return 0;
}

/**
 * 蓝牙模块初始化
 * 1. 给设备指定perWrite和postRead两个函数
 * 2. 蓝牙连接初始化配置
 */
int app_bt_init(Device *device)
{
    device->post_read = app_bt_postRead;
    device->pre_write = app_bt_preWrite;

    // 初始化蓝牙
    return init_bluetooth(device);
}

int app_bt_preWrite(char *data, int data_len)
{
    // 检查data数据受否合法
    if (data_len < 6)
    {
        log_error("data_len is too short");
        return -1;
    }

    // 计算蓝牙数据的长度
    int blue_len = 8 + 2 + data[2] + 2;
    // 创建蓝牙数据数组
    char blue_data[blue_len];
    // 根据data中的数据组装蓝牙数据
    // data: 1 2 3 XX abc
    // blue_data: AT+MESH XX abc \r\n
    // 拷贝AT+MESH
    memcpy(blue_data, "AT+MESH", 8);
    // 拷贝id
    memcpy(blue_data + 8, data + 3, 2);
    // 拷贝message
    memcpy(blue_data + 10, data + 5, data[2]);
    // 拷贝\r\n
    memcpy(blue_data + 10 + data[2], "\r\n", 2);

    // 清空data中的数据
    memset(data, 0, data_len);
    // 将蓝牙数据拷贝到data中
    memcpy(data, blue_data, blue_len);
    // 返回蓝牙数据的长度
    return blue_len;
}

static char read_buf[1024];                 // 缓存读取的蓝牙数据
static int read_len = 0;                    // 已读取数据的长度
static char fixed_header[2] = {0xf1, 0xdd}; // 固定头部

// 删除缓存中指定长度数据
static void remove_data(int len)
{
    memmove(read_buf, read_buf + len, read_len - len);
    read_len -= len;
}

int app_bt_postRead(char *data, int data_len)
{
    // 将当前数据添加到缓存中
    memcpy(read_buf + read_len, data, data_len);
    read_len += data_len;

    // 如果当前已读数据的长度小于8，读到的蓝牙数据不完整，直接返回0
    if (read_len < 8)
    {
        log_debug("current read bluetooth data is too short(less than 8 bytes), contiue read");
        return 0;
    }

    // 遍历查找完整的蓝牙数据
    int i;
    for (i = 0; i < read_len - 7; i++)
    {
        // 查找开头fixed_header
        if (memcmp(read_buf + i, fixed_header, 2) == 0)
        {
            // 之前的数据都是无效数据，删除他们
            if (i > 0)
            {
                remove_data(i);
                // 如果缓存数据长度 小于8，继续读取
                if (read_len < 8)
                {
                    log_debug("current read bluetooth data is too short(less than 8 bytes)2, contiue read");
                    return 0;
                }
            }

            // 有8位不代表当前蓝牙数据是完整的
            int blue_len = 3 + read_buf[2];
            if (read_len < blue_len)
            {
                log_debug("current read bluetooth data is too short 3, contiue read");
                return 0;
            }
            // 根据缓存中蓝牙数据生成字符数组消息
            memset(data, 0, data_len);
            data[0] = 1;                             // conn_type;
            data[1] = 2;                             // id_len;
            data[2] = blue_len - 7;                  // msg_len;
            memcpy(data + 3, read_buf + 3, 2);       // id;
            memcpy(data + 5, read_buf + 5, data[2]); // msg;

            // 删除缓存中已处理的蓝牙数据
            remove_data(blue_len);

            // 返回消息的长度
            return data[2] + 5;
        }
    }

    // 遍历的数据都是无效数据，删除他们
    remove_data(i);

    return 0;
}

// 判断是否收到ACK指令
int wait_ack(int fd)
{
    // 等待一定时间
    usleep(50 * 1000);
    // 读取数据
    char data_buf[4];
    read(fd, data_buf, 4);
    // 判断是否是OK\r\n
    if (memcmp(data_buf, "OK\r\n", 4) == -1)
    {
        log_error("wait_ack failed");
        return -1;
    }
    log_debug("wait_ack success");
}

int app_bt_status(Device *device)
{
    // 向蓝牙串口文件中写入"AT\r\n"的指令数据
    write(device->fd, "AT\r\n", 4);
    // 通过读取"OK\r\n"数据，判断蓝牙是否可用
    return wait_ack(device->fd);
}

int app_bt_rename(Device *device, char *name)
{
    // 拼接指令
    char cmd[20];
    sprintf(cmd, "AT+NAME%s\r\n", name);
    // 写入指令
    write(device->fd, cmd, strlen(cmd));
    // 等待ACK
    return wait_ack(device->fd);
}

int app_bt_setBaudRate(Device *device, BT_BaudRate baudRate)
{
    // 拼接指令
    char cmd[20];
    sprintf(cmd, "AT+BAUD%c\r\n", baudRate);
    // 写入指令
    write(device->fd, cmd, strlen(cmd));
    // 等待ACK
    return wait_ack(device->fd);
}

int app_bt_reset(Device *device)
{
    // 拼接指令
    char *cmd = "AT+RESET\r\n";
    // 写入指令
    write(device->fd, cmd, strlen(cmd));
    // 等待ACK
    return wait_ack(device->fd);
}

int app_bt_setNetId(Device *device, char *netid)
{
    // 拼接指令
    char cmd[20];
    sprintf(cmd, "AT+NETID%s\r\n", netid);
    // 写入指令
    write(device->fd, cmd, strlen(cmd));
    // 等待ACK
    return wait_ack(device->fd);
}

int app_bt_setMaddr(Device *device, char *maddr)
{
    // 拼接指令
    char cmd[20];
    sprintf(cmd, "AT+MADDR%c\r\n", maddr);
    // 写入指令
    write(device->fd, cmd, strlen(cmd));
    // 等待ACK
    return wait_ack(device->fd);
}
