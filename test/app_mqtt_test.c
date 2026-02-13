#include "app_mqtt.h"
#include "log/log.h"
#include <unistd.h>

int app_mqtt_recv(char *json)
{
    log_debug("get ready to process the data : %s", json);
    return 0; // 返回0表示处理成功
}

int main(int argc, char *argv[])
{
    app_mqtt_init();

    app_mqtt_registerRecvCallback(app_mqtt_recv);

    app_mqtt_send("{\"conn_type\":1,\"id\":\"5858\",\"msg\":\"61626364\"}");

    // 阻塞一下
    sleep(50);

    app_mqtt_close();
    return 0;
}