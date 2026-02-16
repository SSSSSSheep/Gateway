#ifndef __APP_BT_H
#define __APP_BT_H

#include "app_device.h"

typedef enum
{
    BT_BR_9600 = '4',
    BT_BR_115200 = '8',
} BT_BaudRate;

/**
 * @brief 蓝牙模块初始化
 * 1. 给设备指定preWrite和postRead两个函数
 * 2. 蓝牙连接初始化配置
 * @param device
 * @return int
 */
int app_bt_init(Device *device);

/**
 * @brief
 *
 * @param data
 * @param data_len
 * @return int
 */
int app_bt_preWrite(char *data, int data_len);

/**
 * @brief
 *
 * @param data
 * @param data_len
 * @return int
 */
int app_bt_postRead(char *data, int data_len);

/**
 * @brief 获取蓝牙状态
 *
 * @param device
 * @return int
 */
int app_bt_status(Device *device);

/**
 * @brief 修改名称
 *
 * @param device
 * @param name
 * @return int
 */
int app_bt_rename(Device *device, char *name);

/**
 * @brief 修改波特率
 *
 * @param device
 * @param baudRate
 * @return int
 */
int app_bt_setBaudRate(Device *device, BT_BaudRate baudRate);

/**
 * @brief 重启蓝牙
 *
 * @param device
 * @return int
 */
int app_bt_reset(Device *device);

/**
 * @brief 设置网络ID
 *
 * @param device
 * @param netid
 * @return int
 */
int app_bt_setNetId(Device *device, char *netid);

/**
 * @brief 设置蓝牙地址
 *
 * @param device
 * @param maddr
 * @return int
 */
int app_bt_setMaddr(Device *device, char *maddr);
#endif // !__APP_BT_H
