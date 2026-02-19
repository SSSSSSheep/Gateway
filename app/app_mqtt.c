#include "app_mqtt.h"
#include "log/log.h"
#include "MQTTClient.h"
#include <string.h>

static MQTTClient_message pubmsg = MQTTClient_message_initializer;
static MQTTClient client;
static MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;

static int (*recv_callback)(char *) = NULL;

static void delivered(void *context, MQTTClient_deliveryToken dt)
{
    log_debug("Message send success with token: %d", dt);
}
static int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    int res = 0;
    if (recv_callback)
    {
        res = recv_callback((char *)message->payload) == 0 ? 1 : 0;
    }

    // 释放资源
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);

    return res;
}
static void connlost(void *context, char *cause)
{
    log_error("MQTT connection lost, cause: %s", cause);
}

int app_mqtt_init(void)
{
    // 1. 初始化客户端
    if (MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL) != MQTTCLIENT_SUCCESS)
    {
        log_error("MQTTClient_create failed");
        return -1;
    }
    // 2. 设置回调函数
    if (MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered) != MQTTCLIENT_SUCCESS)
    {
        log_error("MQTTClient_setCallbacks failed");
        MQTTClient_destroy(&client);
        return -1;
    }
    // 3. 连接服务器

    if (MQTTClient_connect(client, &conn_opts) != MQTTCLIENT_SUCCESS)
    {
        log_error("MQTTClient_connect failed");
        MQTTClient_destroy(&client);
        return -1;
    }
    // 4. 订阅主题
    if (MQTTClient_subscribe(client, TOPIC_R2G, QOS) != MQTTCLIENT_SUCCESS)
    {
        log_error("MQTTClient_subscribe failed");
        app_mqtt_close();
        return -1;
    }
    log_debug("MQTT init success!");
    return 0;
}

void app_mqtt_close(void)
{
    MQTTClient_disconnect(client, TIMEOUT);
    MQTTClient_destroy(&client);
}

int app_mqtt_send(char *json)
{
    // 指定要发送的数据
    pubmsg.payload = json;
    pubmsg.payloadlen = strlen(json);
    pubmsg.qos = QOS;

    // 发布消息
    if (MQTTClient_publishMessage(client, TOPIC_G2R, &pubmsg, NULL) != MQTTCLIENT_SUCCESS)
    {
        log_error("MQTTClient_publishMessage failed");
        return -1;
    }
    log_debug("MQTT send success! payload: %s", json);
    return 0;
}

void app_mqtt_registerRecvCallback(int (*callback)(char *json))
{
    recv_callback = callback;
}
