#include "ota_version.h"
#include "log/log.h"
#include "ota_http.h"
#include "cJSON/cJSON.h"
#include <openssl/sha.h>
#include <sys/reboot.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

/**
 * 获取文件的SHA1哈希值（40位16进制字符串）
 * 相同文件返回相同的哈希值，可用于判断文件是否相同
 * linux命令生成：sha1sum 文件名
 */
static char *get_file_sha(char *filepath)
{
    FILE *file = fopen(filepath, "rb");
    if (!file)
    {
        perror("Failed to open file");
        return NULL;
    }

    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA_CTX sha1;
    SHA1_Init(&sha1);

    const int bufSize = 32768;
    unsigned char *buffer = (unsigned char *)malloc(bufSize);
    if (!buffer)
    {
        perror("Failed to allocate memory");
        fclose(file);
        return NULL;
    }

    int bytesRead;
    while ((bytesRead = fread(buffer, 1, bufSize, file)) > 0)
    {
        SHA1_Update(&sha1, buffer, bytesRead);
    }

    SHA1_Final(hash, &sha1);
    fclose(file);
    free(buffer);

    char *outputBuffer = (char *)malloc(SHA_DIGEST_LENGTH * 2 + 1);
    if (!outputBuffer)
    {
        perror("Failed to allocate memory");
        return NULL;
    }

    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
    {
        sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
    }

    return outputBuffer;
}

/**
 * 版本检查更新
 * 1. 获取远程版本信息的json
 * 2. 解析json, 得到版本号+固件hash值
 * 3. 比较本地版本号和远程版本号， 如果本地不小于远程，则不更新，直接结束
 * 4. 如果本地版本号小于远程版本号，则开始请求下载固件
 * 5. 下载成功，则开始校验固件hash值（下载的和json解析出的）
 * 6. 如果校验失败， 删除下载的文件，则失败结束
 * 7. 如果校验成功，重启系统，运行最新的固件
 */

int ota_version_checkUpdate()
{
    // 获取远程版本信息的json
    char *json = ota_http_getJson(OTA_URL_FILEINFO);
    if (!json)
    {
        log_debug("get json failed");
        return -1;
    }
    // 解析json得到版本号+固件hash值
    cJSON *root = cJSON_Parse(json);
    if (!root)
    {
        log_debug("parse json failed");
        free(json);
        return -1;
    }

    cJSON *major_item = cJSON_GetObjectItem(root, "major");
    cJSON *minor_item = cJSON_GetObjectItem(root, "minor");
    cJSON *patch_item = cJSON_GetObjectItem(root, "patch");
    cJSON *sha1_item = cJSON_GetObjectItem(root, "sha1");

    if (!major_item || !cJSON_IsNumber(major_item) || !minor_item || !cJSON_IsNumber(minor_item) || !patch_item || !cJSON_IsNumber(patch_item) || !sha1_item || !cJSON_IsString(sha1_item))
    {
        log_debug("Invalid version fields in JSON");
        free(json);
        cJSON_Delete(root);
        return -1;
    }
    if (!sha1_item || !cJSON_IsString(sha1_item))
    {
        log_debug("Invalid sha1 field in JSON");
        free(json);
        cJSON_Delete(root);
        return -1;
    }

    int major = major_item->valueint;
    int minor = minor_item->valueint;
    int patch = patch_item->valueint;
    log_debug("online version: %d.%d.%d", major, minor, patch);
    log_debug("current version: %d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
    // 比较本地版本号和远程版本号，如果本地版本号不小于远程，则不需要更新，直接结束
    if (major < VERSION_MAJOR || (major == VERSION_MAJOR && minor < VERSION_MINOR) || (minor == VERSION_MINOR && patch <= VERSION_PATCH))
    {
        log_debug("current version is latset, no need to update");
        free(json);
        cJSON_Delete(root);
        return 0;
    }
    // 如果本地版本号小于远程，则下载固件
    int res = ota_http_download(OTA_URL_DOWNLOAD, OTA_LOCAL_FILE_PATH);
    if (res != 0)
    {
        log_debug("download file failed");
        free(json);
        cJSON_Delete(root);
        return -1;
    }
    // 下载成功，则开始校验固件hash值（下载的和json解析出的）
    char *remote_sha = cJSON_GetObjectItem(root, "sha1")->valuestring;
    char *local_sha = get_file_sha(OTA_LOCAL_FILE_PATH);
    if (!local_sha)
    {
        log_debug("get file sha failed");
        unlink(OTA_LOCAL_FILE_PATH);
        free(json);
        cJSON_Delete(root);
        return -1;
    }

    // 如果校验失败，删除下载的文件，则失败结束
    if (strcmp(remote_sha, local_sha) != 0)
    {
        log_debug("sha1 check failed");
        unlink(OTA_LOCAL_FILE_PATH);
        free(local_sha);
        cJSON_Delete(root);
        free(json);
        return -1;
    }

    // 如果校验成功，重启系统，运行新固件
    log_debug("sha1 check success, rebooting...");
    free(local_sha);
    cJSON_Delete(root);
    free(json);
    reboot(RB_AUTOBOOT); // 重启系统 =》 需要当前是root
    return 0;
}

int ota_version_checkUpdateDaily()
{
    while (1)
    {
        int ret = ota_version_checkUpdate();
        if (ret != 0)
        {
            log_debug("Update check failed,retry after 1 hours");
            sleep(1 * 60 * 60);
        }
        else
        {
            log_debug("Update check success, sleep 24 hours");
            sleep(24 * 60 * 60);
        }
    }
    return 0;
}

void ota_version_printVersion()
{
    log_debug("current version: %d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
}
