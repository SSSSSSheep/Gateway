
#include "../app/app_mqtt.h"
#include "log/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

// 全局变量
static volatile int running = 1;

// 信号处理函数
void signal_handler(int signum)
{
    log_info("Received signal %d, stopping...", signum);
    running = 0;
}

// 接收回调函数
int recv_callback(char *json)
{
    log_info("Received message: %s", json);
    return 0;
}

int main(int argc, char *argv[])
{
    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    log_info("Starting MQTT receive test");

    // 初始化MQTT
    if (app_mqtt_init() != 0)
    {
        log_error("Failed to initialize MQTT");
        return -1;
    }

    // 注册接收回调
    app_mqtt_registerRecvCallback(recv_callback);

    log_info("Waiting for messages on topic: remote_to_gateway");
    log_info("Press Ctrl+C to stop");

    // 主循环
    while (running)
    {
        sleep(1);
    }

    // 清理
    app_mqtt_close();

    log_info("MQTT receive test completed");
    return 0;
}
