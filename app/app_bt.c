#include "app_bt.h"
#include "log/log.h"

int app_bt_init(Device *device)
{
    device->post_read = app_bt_postRead;
    device->pre_write = app_bt_preWrite;
    return 0;
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

int app_bt_postRead(char *data, int data_len)
{
    return 0;
}
