#include "ota_http.h"
#include "log/log.h"
#include <curl/curl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

// 接收请求响应数据的回调函数
size_t receive_callback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    // 将响应数据（ptr）拷贝到外部配置的容器中（userdata）
    char *response_data = (char *)ptr;
    char *json_buf = (char *)userdata;
    long len = size * nmemb;
    memcpy(json_buf, response_data, len);
    json_buf[len] = '\0';

    return len;
}

char *ota_http_getJson(char *url)
{
    // 创建CURL
    CURL *curl = curl_easy_init();
    // 设置请求url
    curl_easy_setopt(curl, CURLOPT_URL, url);
    // 设置返回数据接收函数
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receive_callback);
    // 设置向接收函数传递的容器参数
    char *json_buf = (char *)malloc(1024);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, json_buf);
    // 执行请求
    CURLcode res = curl_easy_perform(curl);
    // 检查请求是否成功
    if (res != CURLE_OK)
    {
        log_error("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return NULL;
    }

    // 释放CURL对象
    curl_easy_cleanup(curl);

    return json_buf;
}

int ota_http_download(char *url, char *filename)
{
    // 创建CURL
    CURL *curl = curl_easy_init();
    // 设置请求url
    curl_easy_setopt(curl, CURLOPT_URL, url);
    // 设置返回数据接收函数
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
    // 设置向接收函数传递的容器参数
    FILE *file = fopen(filename, "wb");
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    // 执行请求
    CURLcode res = curl_easy_perform(curl);
    // 检查请求是否成功
    if (res != CURLE_OK)
    {
        log_error("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        fclose(file);
        return -1;
    }

    // 释放CURL对象
    curl_easy_cleanup(curl);
    fclose(file);

    return 0;
}
