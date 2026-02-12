#include "log/log.h"

int main(int argc, char *argv[])
{
    // 设置日志输出级别
    log_set_level(LOG_DEBUG);

    // 做不同级别的日志输出
    log_trace("This is a trace message");
    log_debug("This is a debug message");
    log_info("This is a info message");
    log_warn("This is a warn message");
    log_error("This is a error message");
    log_fatal("This is a fatal message");

    return 0;
}