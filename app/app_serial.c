#define _GNU_SOURCE
#include "app_serial.h"
#include "log/log.h"

int app_serial_setBaudRate(Device *device, BaudRate baudRate)
{
    // 读取串口属性
    struct termios attr;
    tcgetattr(device->fd, &attr);
    // 修改属性（波特率）
    // cfsetispeed(&attr, baudRate);
    // cfsetospeed(&attr, baudRate);
    cfsetspeed(&attr, baudRate);

    // 设置串口属性（当前不生效，都设置好后flush时 才生效）
    int res = tcsetattr(device->fd, TCSAFLUSH, &attr);
    if (res == -1)
    {
        log_error("tcsetattr BaudRate error");
        return -1;
    }
    log_debug("tcsetattr BaudRate success");
    return 0;
}

int app_serial_setParity(Device *device, Parity parity)
{
    // 读取串口属性
    struct termios attr;
    tcgetattr(device->fd, &attr);
    // 修改属性（校验位）
    attr.c_cflag &= ~(PARENB | PARODD); // 清除校验位
    attr.c_cflag |= parity;             // 设置校验位

    // 设置串口属性（当前不生效，都设置好后flush时 才生效）
    int res = tcsetattr(device->fd, TCSAFLUSH, &attr);
    if (res == -1)
    {
        log_error("tcsetattr Parity error");
        return -1;
    }
    log_debug("tcsetattr Parity success");
    return 0;
}

int app_serial_setStopBits(Device *device, StopBits stopBits)
{
    // 读取串口属性
    struct termios attr;
    tcgetattr(device->fd, &attr);
    // 修改属性（停止位）
    attr.c_cflag &= ~CSTOPB;  // 清除停止位
    attr.c_cflag |= stopBits; // 设置停止位

    // 设置串口属性（当前不生效，都设置好后flush时 才生效）
    int res = tcsetattr(device->fd, TCSAFLUSH, &attr);
    if (res == -1)
    {
        log_error("tcsetattr StopBits error");
        return -1;
    }
    log_debug("tcsetattr StopBits success");
    return 0;
}

int app_serial_setBlockMode(Device *device, int is_block)
{
    // 读取串口属性
    struct termios attr;
    tcgetattr(device->fd, &attr);
    // 修改属性（阻塞模式）
    if (is_block) // 阻塞
    {
        attr.c_cc[VMIN] = 1;  // 至少读一个字节才返回 没有读取到就阻塞
        attr.c_cc[VTIME] = 0; // 读取超时时间0秒 不超时
    }
    else // 非阻塞
    {
        attr.c_cc[VMIN] = 0;  // 最少读取的字符数是0
        attr.c_cc[VTIME] = 2; // 读取超时时间0.2秒 =》 单位是100ms
    }

    // 设置串口属性（当前不生效，都设置好后flush时 才生效）
    int res = tcsetattr(device->fd, TCSAFLUSH, &attr);
    if (res == -1)
    {
        log_error("tcsetattr BlockMode error");
        return -1;
    }
    log_debug("tcsetattr BlockMode success");
    return 0;
}

int app_serial_setRaw(Device *device)
{
    // 读取串口属性
    struct termios attr;
    tcgetattr(device->fd, &attr);
    // 修改属性（原始模式）
    cfmakeraw(&attr);

    // 设置串口属性（当前不生效，都设置好后flush时 才生效）
    int res = tcsetattr(device->fd, TCSAFLUSH, &attr);
    if (res == -1)
    {
        log_error("tcsetattr setRaw error");
        return -1;
    }
    log_debug("tcsetattr setRaw success");
    return 0;
}

int app_serial_init(Device *device)
{
    // 初始化串口的各个属性
    app_serial_setBaudRate(device, BR_9600);
    app_serial_setParity(device, Parity_NONE);
    app_serial_setStopBits(device, SB_1);
    app_serial_setBlockMode(device, 1);
    app_serial_setRaw(device);
    // 刷新生效
    int res = tcflush(device->fd, TCIOFLUSH);
    if (res == -1)
    {
        log_error("tcflush error");
        return -1;
    }
    log_debug("tcflush success");
    return 0;
}
