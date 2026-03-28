#include "ota_version.h"
#include "log/log.h"
#include "ota_http.h"
#include "cJSON/cJSON.h"
#include <openssl/sha.h>
#include <sys/reboot.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

/**
 * ��ȡ�ļ���SHA1��ϣֵ��40λ16�����ַ�����
 * ��ͬ�ļ�������ͬ�Ĺ�ϣֵ���������ж��ļ��Ƿ���ͬ
 * linux�������ɣ�sha1sum �ļ���
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
 * �汾������
 * 1. ��ȡԶ�̰汾��Ϣ��json
 * 2. ����json, �õ��汾��+�̼�hashֵ
 * 3. �Ƚϱ��ذ汾�ź�Զ�̰汾�ţ� ������ز�С��Զ�̣��򲻸��£�ֱ�ӽ���
 * 4. ������ذ汾��С��Զ�̰汾�ţ���ʼ�������ع̼�
 * 5. ���سɹ�����ʼУ��̼�hashֵ�����صĺ�json�������ģ�
 * 6. ���У��ʧ�ܣ� ɾ�����ص��ļ�����ʧ�ܽ���
 * 7. ���У��ɹ�������ϵͳ���������µĹ̼�
 */

int ota_version_checkUpdate()
{
    // ��ȡԶ�̰汾��Ϣ��json
    char *json = ota_http_getJson(OTA_URL_FILEINFO);
    if (!json)
    {
        log_debug("get json failed");
        return -1;
    }
    // ����json�õ��汾��+�̼�hashֵ
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
    // �Ƚϱ��ذ汾�ź�Զ�̰汾�ţ�������ذ汾�Ų�С��Զ�̣�����Ҫ���£�ֱ�ӽ���
    if (major < VERSION_MAJOR || (major == VERSION_MAJOR && minor < VERSION_MINOR) || (minor == VERSION_MINOR && patch <= VERSION_PATCH))
    {
        log_debug("current version is latset, no need to update");
        free(json);
        cJSON_Delete(root);
        return 0;
    }
    // ������ذ汾��С��Զ�̣������ع̼�
    int res = ota_http_download(OTA_URL_DOWNLOAD, OTA_LOCAL_FILE_PATH);
    if (res != 0)
    {
        log_debug("download file failed");
        free(json);
        cJSON_Delete(root);
        return -1;
    }
    // ���سɹ�����ʼУ��̼�hashֵ�����صĺ�json�������ģ�
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

    // ���У��ʧ�ܣ�ɾ�����ص��ļ�����ʧ�ܽ���
    if (strcmp(remote_sha, local_sha) != 0)
    {
        log_debug("sha1 check failed");
        unlink(OTA_LOCAL_FILE_PATH);
        free(local_sha);
        cJSON_Delete(root);
        free(json);
        return -1;
    }

    // ���У��ɹ�������ϵͳ�������¹̼�
    log_debug("sha1 check success, rebooting...");
    free(local_sha);
    cJSON_Delete(root);
    free(json);
    reboot(RB_AUTOBOOT); // ����ϵͳ =�� ��Ҫ��ǰ��root
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
