#ifndef __APP_BT_H__
#define __APP_BT_H__

#include "app_device.h"
#include "stdint.h"

#define MAX_PENDING_ACKS 16
#define ACK_TIMEOUT_MS 1000

#define MAXX_BLUE_DATA_LEN 256

// 接收缓冲区大小上限
#define MAX_RECV_BUFFER_SIZE 1024

// 最大重试次数
#define MAX_RETRY_COUNT 3

// 重试超时时间（毫秒）
#define RETRY_TIMEOUT_MS 1000

// 状态机
typedef enum
{
    FSM_STATE_IDLE = 0,    // 空闲状态
    FSM_STATE_WAIT_HAEDER, // 等待包头
    FSM_STATE_WAIT_LENGTH, // 等待长度
    FSM_STATE_WAIT_DATA,   // 等待数据
    FSM_STATE_ERROR,       // 错误状态
} fsm_state_t;

// 追踪器结构体
typedef struct
{
    uint16_t packet_id;        // 包ID
    struct timespec send_time; // 发送时间
    uint8_t ack_received;      // 是否收到ACK
    uint8_t retry_count;       // 重试次数
    uint8_t data[256];         // 数据
    uint32_t data_len;         // 数据长度
} ack_tracker_t;

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
void app_bt_reset_internal_state(void);

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

/**
 * @brief 蓝牙数据接收回调函数
 *
 * @param device    设备
 * @param data       接收到的数据
 * @param data_len   数据长度
 * @return int       返回0表示成功，返回-1表示失败
 */
int app_bt_add_tracker(Device *device, const char *data, int data_len);

/**
 * @brief 检查并重试
 *
 * @param device     设备
 * @return int       返回0表示成功，返回-1表示失败
 */
int app_bt_check_and_retry(Device *device);
#endif // !__APP_BT_H
