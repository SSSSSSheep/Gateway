#ifndef __APP_POOL_H__
#define __APP_POOL_H__

#include <stdint.h>

// 任务类型枚举
typedef enum
{
    JOB_TYPE_INVALID = 0,  // 无效任务
    JOB_TYPE_BT_SCAN,      // 蓝牙扫描任务
    JOB_TYPE_BT_CONNECT,   // 蓝牙连接任务
    JOB_TYPE_MQTT_PUBLISH, // MQTT发布任务
    JOB_TYPE_SERIAL_SEND,  // 串口发送任务
    JOB_TYPE_MAX           // 用于边界检查
} job_type_t;

// 回压策略枚举
typedef enum
{
    BACKPRESSURE_DROP = 0,       // 丢弃策略：队列满时直接丢弃新任务
    BACKPRESSURE_MERGE,          // 合并策略：队列满时合并相同类型的任务
    BACKPRESSURE_DOWNSAMPLE,     // 降采样策略：队列满时按时间间隔降采样
    BACKPRESSURE_OUTBOX,         // 本地存储策略：队列满时写入本地文件
    BACKPRESSURE_MAX             // 用于边界检查
} backpressure_strategy_t;

// 任务数据结构（使用数据副本而非指针）
typedef struct
{
    job_type_t type;
    uint8_t data[256]; // 数据副本，根据实际需求调整大小
    uint32_t data_len; // 实际数据长度
} Task;

/**
 * @brief 任务处理函数类型定义
 *
 * @param data 任务数据
 * @param len 数据长度
 * @return int
 */
typedef int (*job_handler_t)(const uint8_t *data, uint32_t len);

/**
 * @brief 注册任务处理器
 *
 * @param type 任务类型
 * @param handler 处理函数
 * @return int 0成功，-1失败
 */
int app_pool_register_handler(job_type_t type, job_handler_t handler);

/**
 * @brief 初始化线程池
 *
 * @param size 线程池大小
 * @return int 0成功，-1失败
 */
int app_pool_init(int size);

/**
 * @brief 销毁线程池
 *
 */
void app_pool_destroy(void);

/**
 * @brief 添加任务到线程池
 *
 * @param type 任务类型
 * @param data 任务数据
 * @param len 数据长度
 * @return int 0成功，-1失败
 */
int app_pool_add_task(job_type_t type, const uint8_t *data, uint32_t len);

/**
 * @brief 设置回压策略
 *
 * @param type 任务类型
 * @param strategy 回压策略
 * @return int 0成功，-1失败
 */
int app_pool_set_backpressure_strategy(job_type_t type, backpressure_strategy_t strategy);

/**
 * @brief 重置回压统计信息
 *
 * @return int 0成功，-1失败
 */
int app_pool_reset_backpressure_stats(void);

/**
 * @brief 设置降采样间隔
 *
 * @param interval_ms 降采样间隔（毫秒）
 * @return int 0成功，-1失败
 */
int app_pool_set_downsample_interval(uint32_t interval_ms);

/**
 * @brief 报告回压统计信息
 *
 * @return int 0成功，-1失败
 */
int app_pool_report_backpressure_stats(void);

/**
 * @brief 尝试发送合并后的任务
 * @brief 这个函数应该在任务处理完成后调用，以尝试发送合并后的任务
 *
 * @param type 任务类型
 * @return int 0成功，-1失败
 */
int app_pool_try_send_merged_task(job_type_t type);

#endif // !__APP_POOL_H__