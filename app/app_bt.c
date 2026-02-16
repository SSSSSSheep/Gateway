#include "app_bt.h"
#include <string.h>
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
