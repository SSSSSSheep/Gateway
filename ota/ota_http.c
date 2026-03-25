#include "ota_http.h"
#include "log/log.h"
#include <curl/curl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

// 接收请求响应数据的回调函数
size_t receive_callback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    http_buffer_t *buf = (http_buffer_t *)userdata;
    size_t chunk = size * nmemb;

    // 扩展缓冲区
    if (buf->used + chunk + 1 > buf->size)
    {
        size_t new_size = buf->size + chunk + 1024;
        char *new_buf = realloc(buf->buf, new_size);
        if (!new_buf)
        {
            return 0;
        }
        buf->buf = new_buf;
        buf->size = new_size;
    }

    // 将响应数据（ptr）拷贝到外部配置的容器中（userdata）
    memcpy(buf->buf + buf->used, ptr, chunk);
    buf->used += chunk;
    buf->buf[buf->used] = '\0';
    return chunk;
}

char *ota_http_getJson(char *url)
{
    // 创建CURL
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        log_error("curl_easy_init() failed\n");
        return NULL;
    }

    http_buffer_t buffer = {0};
    buffer.buf = malloc(1024);
    if (!buffer.buf)
    {
        curl_easy_cleanup(curl);
        return NULL;
    }
    buffer.size = 1024;
    buffer.used = 0;

    // 设置请求url
    curl_easy_setopt(curl, CURLOPT_URL, url);
    // 设置返回数据接收函数
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receive_callback);
    // 设置向接收函数传递的容器参数
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    // 设置超时
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    // 执行请求
    CURLcode res = curl_easy_perform(curl);
    // 检查请求是否成功
    if (res != CURLE_OK)
    {
        log_error("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        free(buffer.buf);
        curl_easy_cleanup(curl);
        return NULL;
    }

    // 释放CURL对象
    curl_easy_cleanup(curl);

    return buffer.buf;
}

int ota_http_download(char *url, char *filename)
{
    // 创建CURL
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        log_error("curl_easy_init() failed\n");
        return -1;
    }

    FILE *file = fopen(filename, "wb");
    if (!file)
    {
        log_error("open file failed\n");
        curl_easy_cleanup(curl);
        return -1;
    }
    // 设置请求url
    curl_easy_setopt(curl, CURLOPT_URL, url);
    // 设置返回数据接收函数
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
    // 设置向接收函数传递的容器参数
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    // 设置超时
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
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
