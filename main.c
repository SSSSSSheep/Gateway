#include "app_runner.h"
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
    else
    {
        log_error("parameter error");
        return -1;
    }
    return 0;
}
