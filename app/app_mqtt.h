#ifndef APP_MQTT_H__
#define APP_MQTT_H__

#include <MQTTClient.h>
#include <time.h>

#define ADDRESS "ws://192.168.1.4:8083"
// #define ADDRESS "tcp://192.168.1.4:1883"
#define CLIENTID "b253ba38-daf6-4b37-984f-5d8fdc6a1cfa"
#define TOPIC_R2G "remote_to_gateway" // 接收远程消息主题
#define TOPIC_G2R "gateway_to_remote" // 发送远程消息主题
#define PAYLOAD "Hello World!"

// 持久化目录
#define PERISITENCE_DIR "/tmp/mqtt_persistence"

// 最大未确认消息数
#define UNCONFIRMED_MESSAGES_MAX 50
#define QOS 1
#define TIMEOUT 10000L

typedef struct
{
    char *topic;                    // 主题
    char *payload;                  // 消息
    int payload_len;                // 消息长度
    MQTTClient_deliveryToken token; // 确认令牌
    int confirmed;                  // 是否已确认
    time_t send_time;               // 发送时间
} unconfirmed_message_t;

/**
 * @brief 初始化MQTT客户端
 *
 * @retval 0 成功
 */
int app_mqtt_init(void);

/**
 * @brief 关闭MQTT客户端
 *
 */
void app_mqtt_close(void);

/**
 * @brief 发送数据到MQTT服务器
 *
 * @param json
 * @return int
 */
int app_mqtt_send(char *json);

/**
 * @brief 注册MQTT消息接收回调函数
 *
 * @param callback 回调函数，参数为接收到的JSON字符串，返回值为处理结果
 */
void app_mqtt_registerRecvCallback(int (*callback)(char *json));

/**
 * @brief 检查未确认的消息
 * 如果存在未确认的消息，则尝试重新发送
 *
 */
void app_mqtt_check_unconfirmed_messages(void);
#endif // !APP_MQTT_H__
