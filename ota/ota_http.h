#ifndef __OTA_HTTP_H__
#define __OTA_HTTP_H__

// 版主信息地址
#define OTA_URL_FILEINFO "http://192.168.1.4:8000/fileinfo.json"
// 下载地址
#define OTA_URL_DOWNLOAD "http://192.168.1.4:8000/download/gateway"
// #define OTA_LOCAL_FILE_PATH "/home/admin123/gateway.update"
#define OTA_LOCAL_FILE_PATH "/root/gateway.update"
/**
 * @brief 请求指定url获取json数据
 *
 * @param url
 * @return char*
 */
char *ota_http_getJson(char *url);

/**
 * @brief 下载指定url文件到指定路径
 *
 * @param url
 * @param filename
 * @return int
 */
int ota_http_download(char *url, char *filename);

#endif // !__OTA_HTTP_H__
