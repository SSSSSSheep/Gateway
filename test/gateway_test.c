#include "app_runner.h"
#include "ota_version.h"
#include "daemon_runner.h"
#include "log/log.h"
#include <string.h>
int main(int arg, char const *argv[])
{
    // ¼ì²é²ÎÊý
    if (arg == 1)
    {
        log_error("please input parameter");
        return -1;
    }

    if (strcmp(argv[1], "app") == 0)
    {
        app_runner_run();
    }
    else if (strcmp(argv[1], "ota") == 0)
    {
        ota_version_checkUpdateDaily();
    }
    else if (strcmp(argv[1], "daemon") == 0)
    {
        daemon_runner_run();
    }
    else
    {
        log_error("parameter error, please input app or ota or daemon");
        return -1;
    }
    return 0;
}
