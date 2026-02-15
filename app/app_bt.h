#ifndef __APP_BT_H
#define __APP_BT_H

#include "app_device.h"

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

#endif  // !__APP_BT_H
