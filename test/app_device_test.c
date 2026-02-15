#include "app_device.h"
#include "app_bt.h"
#include <unistd.h>
int main(int argc, char const *argv[])
{
    Device *device = app_device_init("/home/admin123/embe/Project/gateway/serial_file");

    app_bt_init(device);

    app_device_start();

    sleep(50);

    app_device_close();

    return 0;
}
