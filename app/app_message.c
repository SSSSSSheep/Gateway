#include "app_message.h"
#include "app_common.h"
#include "cJSON/cJSON.h"
#include "log/log.h"
#include <string.h>
#include <stdlib.h>

/*
    chars: 1 2 4 XX abcd
    json: {"conn_type": 1, "id":"5858","msg":"20030223"}
*/
char *app_message_chars2Json(char *chars, int chars_len)
{
    log_debug("chars2Json: len=%d", chars_len);
    // 打印前几个字节（作为字符，可能不可打印，用十六进制更稳妥）
    for (int i = 0; i < chars_len && i < 10; i++)
    {
        log_debug("chars[%d]=0x%02x", i, (unsigned char)chars[i]);
    }

    // 从chars中读取出conn_type, id_len, msg_len,id, msg
    int conn_type = chars[0];
    int id_len = chars[1];
    int msg_len = chars[2];

    // 检查chars_len是否正确
    if (chars_len != 3 + id_len + msg_len)
    {
        log_error("chars_len is error!");
        return NULL;
    }

    char id[id_len];
    memcpy(id, chars + 3, id_len);
    char msg[msg_len];
    memcpy(msg, chars + 3 + id_len, msg_len);

    // 生成16进制的id和msg字符串
    char *id_hexstr = app_common_chars2HexStr(id, id_len);
    char *msg_hexstr = app_common_chars2HexStr(msg, msg_len);

    // 生成json格式消息
    cJSON *root_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(root_obj, "conn_type", conn_type);
    cJSON_AddStringToObject(root_obj, "id", id_hexstr);
    cJSON_AddStringToObject(root_obj, "msg", msg_hexstr);
    char *json = cJSON_PrintUnformatted(root_obj);

    // 释放内存
    cJSON_Delete(root_obj);
    free(id_hexstr);
    free(msg_hexstr);

    // 返回json格式消息
    return json;
}

/*
    chars: 1 2 4 XX abcd
    json: {"conn_type": 1, "id":"5858","msg":"20030223"}
*/
int app_message_json2Chars(char *json, char *chars_buf, int buf_size)
{
    // 解析json格式消息 得到conn_type, id_hexstr, msg_hexstr
    cJSON *root_obj = cJSON_Parse(json);
    int conn_type = cJSON_GetObjectItem(root_obj, "conn_type")->valueint;
    char *id_hexstr = cJSON_GetObjectItem(root_obj, "id")->valuestring;
    char *msg_hexstr = cJSON_GetObjectItem(root_obj, "msg")->valuestring;

    // 将16进制的id_hexstr和msg_hexstr转换为chars类型的id和msg
    int id_len = -1;
    int msg_len = -1;
    char *id = app_common_hexStr2Chars(id_hexstr, &id_len);
    char *msg = app_common_hexStr2Chars(msg_hexstr, &msg_len);

    // 检查buf_size是否足够
    if (buf_size < 3 + id_len + msg_len)
    {
        log_error("buf_size is too short");
        cJSON_Delete(root_obj);
        free(id);
        free(msg);
        return -1;
    }

    // 向chars_buf中拼接字符数组消息
    chars_buf[0] = conn_type;
    chars_buf[1] = id_len;
    chars_buf[2] = msg_len;
    memcpy(chars_buf + 3, id, id_len);
    memcpy(chars_buf + 3 + id_len, msg, msg_len);

    // 释放内存
    cJSON_Delete(root_obj);
    free(id);
    free(msg);

    // 返回拼接的字符数组消息的长度
    return 3 + id_len + msg_len;
}
