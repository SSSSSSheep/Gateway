#ifndef __OTA_VERSION_H__
#define __OTA_VERSION_H__

#define VERSION_MAJOR 3 // 主版本号
#define VERSION_MINOR 0 // 次版本号
#define VERSION_PATCH 0 // 修订版本号
/**
 * @brief 检查是否有新版本
 *
 * @return int
 */
int ota_version_checkUpdate();

/**
 * @brief 检查是否有新版本，每天检查一次
 *
 * @return int
 */
int ota_version_checkUpdateDaily();

void ota_version_printVersion();

#endif // !__OTA_VERSION_H__
