#include "ota_http.h"
#include "log/log.h"

int main(int argc, char *argv[])
{
    char *json = ota_http_getJson(OTA_URL_FILEINFO);
    log_debug("json: %s", json);

    int res = ota_http_download(OTA_URL_DOWNLOAD, OTA_LOCAL_FILE_PATH);
    if (res == -1)
    {
        log_error("download failed");
        return -1;
    }
    log_info("download success");
    return 0;
}
