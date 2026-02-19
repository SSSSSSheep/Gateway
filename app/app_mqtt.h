#ifndef APP_MQTT_H__
#define APP_MQTT_H__

#define ADDRESS "ws://192.168.1.4:8083"
#define CLIENTID "b253ba38-daf6-4b37-984f-5d8fdc6a1cfa"
#define TOPIC_R2G "remote_to_gateway" // 接收远程消息主题
#define TOPIC_G2R "gateway_to_remote" // 发送远程消息主题
#define PAYLOAD "Hello World!"
#define QOS 1
#define TIMEOUT 10000L

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
#endif // !APP_MQTT_H__
