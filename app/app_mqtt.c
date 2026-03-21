#include "app_mqtt.h"
#include "log/log.h"
#include "MQTTClient.h"
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

static pthread_mutex_t mqtt_mutex = PTHREAD_MUTEX_INITIALIZER;

static MQTTClient client;
static MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;

static int (*recv_callback)(char *) = NULL;

static unconfirmed_message_t unconfirmed_messages[UNCONFIRMED_MESSAGES_MAX];
static int unconfirmed_count = 0;

// 配置连接选项
static int reconnect_attempts = 0; // 重连次数
static const int MAX_RECONNECT_ATTEMPTS = 10;
static const int RECONNECT_DELAY_MS = 5000; // seconds

// 消息确认回调
static void delivered(void *context, MQTTClient_deliveryToken dt)
{
    log_debug("Message send success with token: %d", dt);

    pthread_mutex_lock(&mqtt_mutex);
    // 标记消息为已确认
    for (int i = 0; i < unconfirmed_count; i++)
    {
        if (unconfirmed_messages[i].token == dt)
        {
            unconfirmed_messages[i].confirmed = 1;

            // 释放资源
            if (unconfirmed_messages[i].topic != NULL)
            {
                free(unconfirmed_messages[i].topic);
                unconfirmed_messages[i].topic = NULL;
            }
            if (unconfirmed_messages[i].payload != NULL)
            {
                free(unconfirmed_messages[i].payload);
                unconfirmed_messages[i].payload = NULL;
            }

            // 移除已确认的消息
            if (i < unconfirmed_count - 1)
            {
                memmove(&unconfirmed_messages[i], &unconfirmed_messages[i + 1], sizeof(unconfirmed_message_t) * (unconfirmed_count - i - 1));
            }
            unconfirmed_count--;

            break;
        }
    }

    pthread_mutex_unlock(&mqtt_mutex);
}

// 添加未确认消息队列
static int add_unconfirmed_message(char *topic, char *payload, int payload_len, MQTTClient_deliveryToken token)
{

    if (unconfirmed_count >= UNCONFIRMED_MESSAGES_MAX)
    {
        log_error("Unconfirmed message queue is full");
        return -1;
    }

    char *topic_copy = strdup(topic);
    if (!topic_copy)
    {
        log_error("strdup topic failed");

        return -1;
    }

    char *payload_copy = strdup(payload);
    if (!payload_copy)
    {
        log_error("strdup payload failed");
        free(topic_copy);
        return -1;
    }

    unconfirmed_messages[unconfirmed_count].topic = topic_copy;
    unconfirmed_messages[unconfirmed_count].payload = payload_copy;
    unconfirmed_messages[unconfirmed_count].payload_len = payload_len;
    unconfirmed_messages[unconfirmed_count].token = token;
    unconfirmed_messages[unconfirmed_count].confirmed = 0;
    time(&unconfirmed_messages[unconfirmed_count].send_time);

    unconfirmed_count++;
    return 0;
}

// 重发未确认消息
static void resend_unconfirmed_messages(void)
{
    log_info("Resending %d unconfirmed messages", unconfirmed_count);

    int i = 0;
    while (1)
    {
        pthread_mutex_lock(&mqtt_mutex);
        // 找到下一个未确认的消息
        while (i < unconfirmed_count && unconfirmed_messages[i].confirmed)
        {
            i++;
        }
        if (i >= unconfirmed_count)
        {
            pthread_mutex_unlock(&mqtt_mutex);
            break;
        }

        // 复制消息信息（topic 和 payload 在重发的期间 可能被释放，先复制字符串）
        char *topic = strdup(unconfirmed_messages[i].topic);
        char *payload = strdup(unconfirmed_messages[i].payload);
        int payload_len = unconfirmed_messages[i].payload_len;
        pthread_mutex_unlock(&mqtt_mutex);

        if (!topic || !payload)
        {
            free(topic);
            free(payload);
            log_error("strdup failed during resend");
            i++;
            continue;
        }

        // 重发消息
        MQTTClient_message pubmsg = MQTTClient_message_initializer;
        pubmsg.payload = payload;
        pubmsg.payloadlen = payload_len;
        pubmsg.qos = QOS;
        pubmsg.retained = 0;
        MQTTClient_deliveryToken token;
        if (MQTTClient_publishMessage(client, topic, &pubmsg, &token) != MQTTCLIENT_SUCCESS)
        {
            log_error("Failed to resend message to topic %s", topic);
        }
        else
        {
            log_info("Resent message to topic %s with token %d", topic, token);
            // 更新原队列中的 token 和发送时间（需要加锁）
            pthread_mutex_lock(&mqtt_mutex);
            // 此时 i 可能已经发生变化，需要重新找到对应的消息
            for (int j = 0; j < unconfirmed_count; j++)
            {
                if (!unconfirmed_messages[j].confirmed &&
                    strcmp(unconfirmed_messages[j].topic, topic) == 0 &&
                    unconfirmed_messages[j].payload_len == payload_len &&
                    memcmp(unconfirmed_messages[j].payload, payload, payload_len) == 0)
                {
                    unconfirmed_messages[j].token = token;
                    time(&unconfirmed_messages[j].send_time);
                    break;
                }
            }
            pthread_mutex_unlock(&mqtt_mutex);
        }

        free(topic);
        free(payload);
        i++;
    }
}

static int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    int res = 0;
    int (*cb)(char *json) = NULL;

    pthread_mutex_lock(&mqtt_mutex);
    cb = recv_callback;
    pthread_mutex_unlock(&mqtt_mutex);

    if (cb)
    {
        res = cb((char *)message->payload) == 0 ? 1 : 0;
    }

    // 释放资源
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);

    return res;
}

static void connlost(void *context, char *cause)
{
    log_error("MQTT connection lost, cause: %s", cause);

    // 尝试重新连接
    reconnect_attempts = 0;
    while (reconnect_attempts < MAX_RECONNECT_ATTEMPTS)
    {
        log_info("Attempting to reconnect... (attempt %d/%d)",
                 reconnect_attempts + 1, MAX_RECONNECT_ATTEMPTS);

        if (MQTTClient_connect(client, &conn_opts) == MQTTCLIENT_SUCCESS)
        {
            log_info("Reconnected successfully!");

            // 重新订阅主题
            if (MQTTClient_subscribe(client, TOPIC_R2G, QOS) != MQTTCLIENT_SUCCESS)
            {
                log_error("Failed to resubscribe to topic %s", TOPIC_R2G);
                reconnect_attempts++;
                continue;
            }

            // 重发未确认的消息
            resend_unconfirmed_messages();

            reconnect_attempts = 0;

            return;
        }

        reconnect_attempts++;
        usleep(RECONNECT_DELAY_MS * 1000);
    }

    log_error("Failed to reconnect after %d attempts", MAX_RECONNECT_ATTEMPTS);
}

int app_mqtt_init(void)
{
    // 配置连接选项
    conn_opts.keepAliveInterval = 20; // 设置心跳间隔
    conn_opts.cleansession = 0;       // 设置为0 保持会话
    conn_opts.connectTimeout = 5;     // 连接超时5秒
    conn_opts.reliable = 0;           // 禁用可靠模式（由我们自己的机制保证）
    conn_opts.retryInterval = 5;      // 重试间隔5秒

    // 1. 初始化客户端
    if (MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_DEFAULT, PERISITENCE_DIR) != MQTTCLIENT_SUCCESS)
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
    pthread_mutex_lock(&mqtt_mutex);
    for (int i = 0; i < unconfirmed_count; i++)
    {
        free(unconfirmed_messages[i].topic);
        free(unconfirmed_messages[i].payload);
    }

    unconfirmed_count = 0;
    pthread_mutex_unlock(&mqtt_mutex);

    MQTTClient_disconnect(client, TIMEOUT);
    MQTTClient_destroy(&client);
}

int app_mqtt_send(char *json)
{

    // 检查队列容量
    pthread_mutex_lock(&mqtt_mutex);
    if (unconfirmed_count >= UNCONFIRMED_MESSAGES_MAX)
    {
        pthread_mutex_unlock(&mqtt_mutex);
        log_error("Unconfirmed messages queue is full,cannot send message");
        return -1;
    }

    pthread_mutex_unlock(&mqtt_mutex);

    // 指定要发送的数据
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = json;
    pubmsg.payloadlen = strlen(json);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    // 获取deliveryToken用于跟踪消息
    MQTTClient_deliveryToken token;
    if (MQTTClient_publishMessage(client, TOPIC_G2R, &pubmsg, &token) != MQTTCLIENT_SUCCESS)
    {
        log_error("MQTTClient_publishMessage failed");
        return -1;
    }

    // 发布消息
    if (add_unconfirmed_message(TOPIC_G2R, json, strlen(json), token) != 0)
    {
        log_error("Failed to add message to unconfirmed queue");
    }
    log_debug("MQTT send success! payload: %s", json);
    return 0;
}

void app_mqtt_registerRecvCallback(int (*callback)(char *json))
{
    pthread_mutex_lock(&mqtt_mutex);
    recv_callback = callback;
    pthread_mutex_unlock(&mqtt_mutex);
}

// 添加定期检查未确认消息的函数
void app_mqtt_check_unconfirmed_messages(void)
{
    pthread_mutex_lock(&mqtt_mutex);
    time_t current_time;
    time(&current_time);

    for (int i = 0; i < unconfirmed_count; i++)
    {
        if (unconfirmed_messages[i].confirmed)
        {
            continue;
        }

        // 如果消息超过30s未确认，记录警告
        if (current_time - unconfirmed_messages[i].send_time > 30)
        {
            log_warn("Message to topic %s unconfirmed for %ld seconds", unconfirmed_messages[i].topic,
                     current_time - unconfirmed_messages[i].send_time);
        }
    }
    pthread_mutex_unlock(&mqtt_mutex);
}