#ifndef __APP_SERIAL_H__
#define __APP_SERIAL_H__

#include <termios.h>

#include "app_device.h"

// 波特率
typedef enum {
    BR_9600 = 9600,
    BR_115200 = 115200,
} BaudRate;
// 校验位
typedef enum {
    Parity_NONE = 0,
    Parity_ODD = PARENB | PARODD,
    Parity_EVEN = PARENB,
} Parity;
// 停止位
typedef enum {
    SB_1 = 0,
    SB_2 = CSTOPB,
} StopBits;

/**
 * @brief    初始化串口
 *
 * @param Device    设备
 * @param baudRate   波特率
 * @return int
 */
int app_serial_setBaudRate(Device *device, BaudRate baudRate);

/**
 * @brief   设置串口数据位
 *
 * @param Device    设备
 * @param parity    校验位
 * @return int
 */
int app_serial_setParity(Device *device, Parity parity);

/**
 * @brief   设置串口停止位
 *
 * @param Device    设备
 * @param stopBits   停止位
 * @return int
 */
int app_serial_setStopBits(Device *device, StopBits stopBits);

/**
 * @brief   设置串口阻塞模式
 *
 * @param Device    设备
 * @param block     是否阻塞 1 - 阻塞 0 - 非阻塞
 * @return int
 */
int app_serial_setBlockMode(Device *device, int is_block);

/**
 * @brief  设置串口原始模式
 *
 * @param Device
 * @return int
 */
int app_serial_setRaw(Device *device);

/**
 * @brief   串口初始化
 *
 * @param Device    设备
 * @return int
 */
int app_serial_init(Device *device);
#endif  // !_APP_SERIAL_H__
