#include "cJSON/cJSON.h"
#include "log/log.h"
#include <stdlib.h>

int main(int argc, char *argv[])
{
    // 1. 生成json字符串
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", "sheep");
    cJSON_AddNumberToObject(root, "age", 18);
    char *json = cJSON_PrintUnformatted(root);
    log_debug("生成json字符串: %s", json);
    // 2. 解析json字符串
    cJSON *root2 = cJSON_Parse(json);
    if (root2 == NULL)
    {
        log_error("解析json字符串失败");
        return -1;
    }
    char *name = cJSON_GetObjectItem(root2, "name")->valuestring;
    int age = cJSON_GetObjectItem(root2, "age")->valueint;
    log_debug("解析json字符串成功: name=%s, age=%d", name, age);

    // 3. 释放内存
    cJSON_Delete(root);
    cJSON_Delete(root2);
    free(json);
    return 0;
}