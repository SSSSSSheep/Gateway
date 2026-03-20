#define _GNU_SOURCE
#include "app_bt.h"
#include "app_serial.h"
#include "log/log.h"
#include <string.h>
#include <unistd.h>
#include <time.h>

static Device *g_bt_device = NULL;

// 获取当前时间（毫秒）
static uint64_t get_current_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// 蓝牙数据接收回调函数
static ack_tracker_t ack_trackers[MAX_PENDING_ACKS];
// 蓝牙数据接收回调函数的索引
static uint8_t next_tracker_index = 0;

// 接收缓冲区
static char read_buf[1024];
static int read_len = 0;

// FSM 当前状态
static fsm_state_t current_state = FSM_STATE_IDLE;

// FSM 解析位置
static int parse_pos = 0;

// FSM 临时存储的数据
static uint8_t temp_len = 0;
static uint16_t temp_id = 0;

/**
 * @brief 重置蓝牙模块的内部状态
 * 
 * 这个函数用于重置接收缓冲区、FSM状态和ACK跟踪器等内部状态。
 * 主要用于测试场景，或者在通信出现严重错误时重置状态。
 */
void app_bt_reset_internal_state(void)
{
    // 清空接收缓冲区
    read_len = 0;
    memset(read_buf, 0, sizeof(read_buf));

    // 重置FSM状态
    current_state = FSM_STATE_IDLE;
    parse_pos = 0;
    temp_len = 0;
    temp_id = 0;

    // 重置ACK跟踪器
    memset(ack_trackers, 0, sizeof(ack_trackers));
    next_tracker_index = 0;

    log_debug("Bluetooth internal state reset");
}

// 在处理ACK的函数中添加校验逻辑
// 前向声明
static void trigger_resync(Device *device);

int app_bt_add_tracker(Device *device, const char *data, int data_len)
{
    // 找到可用的跟踪器
    int tracker_index = -1;
    for (int i = 0; i < MAX_PENDING_ACKS; i++)
    {
        if (ack_trackers[i].ack_received || ack_trackers[i].packet_id == 0)
        {
            tracker_index = i;
            break;
        }
    }

    if (tracker_index == -1)
    {
        log_error("No available tracker slot");
        return -1;
    }

    // 读取包ID
    uint16_t packet_id;
    memcpy(&packet_id, data + 8, 2);

    // 初始化跟踪器
    ack_trackers[tracker_index].packet_id = packet_id;
    ack_trackers[tracker_index].retry_count = 0;
    ack_trackers[tracker_index].ack_received = 0;
    ack_trackers[tracker_index].data_len = data_len;
    memcpy(ack_trackers[tracker_index].data, data, data_len);
    clock_gettime(CLOCK_MONOTONIC, &ack_trackers[tracker_index].send_time);

    return 0;
}

static int check_and_retry(void)
{
    uint64_t current_time = get_current_time_ms();

    for (int i = 0; i < MAX_PENDING_ACKS; i++)
    {
        if (!ack_trackers[i].ack_received && ack_trackers[i].packet_id != 0)
        {
            uint64_t send_time_ms = (uint64_t)ack_trackers[i].send_time.tv_sec * 1000 +
                                    ack_trackers[i].send_time.tv_nsec / 1000000;
            uint64_t elapsed = current_time - send_time_ms;

            if (elapsed > RETRY_TIMEOUT_MS)
            {
                if (ack_trackers[i].retry_count < MAX_RETRY_COUNT)
                {
                    // 重试发送
                    ack_trackers[i].retry_count++;
                    clock_gettime(CLOCK_MONOTONIC, &ack_trackers[i].send_time);
                    // 重新发送数据
                    int written = write(g_bt_device->fd, ack_trackers[i].data, ack_trackers[i].data_len);
                    if (written != ack_trackers[i].data_len)
                    {
                        log_error("Failed to retry packet ID: %d", ack_trackers[i].packet_id);
                        return -1;
                    }
                    log_info("Retry packet ID: %d, retry count: %d",
                             ack_trackers[i].packet_id, ack_trackers[i].retry_count);
                }
                else
                {
                    // 超过最大重试次数，触发重同步
                    log_error("Max retry count reached for packet ID: %d",
                              ack_trackers[i].packet_id);
                    trigger_resync(g_bt_device);
                    return -1;
                }
            }
        }
    }
    return 0;
}

// 公共函数：检查超时并重试
int app_bt_check_and_retry(Device *device)
{
    // 更新全局设备指针
    if (device != NULL) {
        g_bt_device = device;
    }
    return check_and_retry();
}

static int is_ack_timeout(const ack_tracker_t *tracker, uint32_t timeout_ms)
{
    uint64_t current_time = get_current_time_ms();
    uint64_t send_time_ms = (uint64_t)tracker->send_time.tv_sec * 1000 + tracker->send_time.tv_nsec / 1000000;
    uint64_t elapsed = current_time - send_time_ms;
    return elapsed > timeout_ms;
}

static void trigger_resync(Device *device)
{
    // 实现重同步逻辑，例如重新发送未确认的数据包
    log_error("trigger_resync");

    // 1. 清空接收缓冲区
    read_len = 0;
    memset(read_buf, 0, sizeof(read_buf));

    // 2. 重置所有ACK跟踪器
    memset(ack_trackers, 0, sizeof(ack_trackers));
    next_tracker_index = 0;

    // 3. 重置FSM状态
    current_state = FSM_STATE_IDLE;
    parse_pos = 0;

    // 4. 刷新串口缓冲区（如果device不为NULL）
    if (device != NULL)
    {
        app_serial_flush(device);
    }
    log_info("Resync completed");
}

int process_ack(uint16_t packet_id)
{
    // 查找对应的包ID
    for (int i = 0; i < MAX_PENDING_ACKS; i++)
    {
        if (ack_trackers[i].packet_id == packet_id && !ack_trackers[i].ack_received)
        {
            // 检查是否超时
            if (is_ack_timeout(&ack_trackers[i], ACK_TIMEOUT_MS))
            {
                log_error("ACK timeout for packet ID: %d", packet_id);
                // 触发重同步
                // 注意：这里没有device参数，需要根据实际情况调整
                return -1;
            }
            // 标记为已接受
            ack_trackers[i].ack_received = 1;
            return 0;
        }
    }

    // 未找到对应的包ID
    log_error("ACK for unknown packet ID: %d", packet_id);
    // 注意：这里没有device参数，需要根据实际情况调整
    return -1;
}

static int init_bluetooth(Device *device)
{
    // 初始化串口
    app_serial_init(device);

    // 设置串口为非阻塞模式
    app_serial_setBlockMode(device, 0);
    app_serial_flush(device);

    // 串口的波特率当前是9600
    // 如果当前蓝牙可用，说明当前的波特率为9600
    if (app_bt_status(device) == 0)
    {
        // 将蓝牙的波特率设置为115200
        app_bt_setBaudRate(device, BT_BR_115200);
        // 重启蓝牙设备
        app_bt_reset(device);
        // 等待蓝牙设备重启完成
        sleep(2);
    }
    // 将串口的波特率设置为115200
    app_serial_setBaudRate(device, BR_115200);
    app_serial_flush(device);

    // 判断蓝牙是否可用 如果不可用 返回-1
    if (app_bt_status(device) != 0)
    {
        log_error("bluetooth is not available");
        return -1;
    }
    // 设置组网ID: 组内相同 组间不同
    app_bt_setNetId(device, "1111");
    // 设置MAC地址: 组内不同 组间可以相同
    app_bt_setMaddr(device, "0001");
    // 将串口设置为阻塞模式
    app_serial_setBlockMode(device, 1);
    app_serial_flush(device);

    log_debug("bluetooth is available");
    return 0;
}

/**
 * 蓝牙模块初始化
 * 1. 给设备指定perWrite和postRead两个函数
 * 2. 蓝牙连接初始化配置
 */
int app_bt_init(Device *device)
{
    device->post_read = app_bt_postRead;
    device->pre_write = app_bt_preWrite;

    // 初始化蓝牙
    return init_bluetooth(device);
}

int app_bt_preWrite(char *data, int data_len)
{
    // 检查data数据受否合法
    if (data_len < 6)
    {
        log_error("data_len is too short(6)");
        return -1;
    }
    // 计算蓝牙数据的长度
    int blue_len = 8 + 2 + data[2] + 2;

    // 检查长度是否合法
    if (blue_len > MAXX_BLUE_DATA_LEN)
    {
        log_error("blue_len is too large: %d", blue_len);
    }

    // 创建蓝牙数据数组
    char blue_data[blue_len];

    // 根据data中的数据组装蓝牙数据
    // data: 1 2 3 XX abc
    // blue_data: AT+MESH XX abc \r\n
    // 拷贝AT+MESH
    memcpy(blue_data, "AT+MESH", 8);
    // 拷贝id
    memcpy(blue_data + 8, data + 3, 2);
    // 拷贝message
    memcpy(blue_data + 10, data + 5, data[2]);
    // 拷贝\r\n
    memcpy(blue_data + 10 + data[2], "\r\n", 2);

    // 清空data中的数据
    memset(data, 0, data_len);
    // 将蓝牙数据拷贝到data中
    memcpy(data, blue_data, blue_len);
    // 返回蓝牙数据的长度
    return blue_len;
}

static char fixed_header[2] = {0xf1, 0xdd}; // 固定头部

// 删除缓存中指定长度数据
static void remove_data(int len)
{
    memmove(read_buf, read_buf + len, read_len - len);
    read_len -= len;
}

int app_bt_postRead(char *data, int data_len)
{
    // data_len 参数的含义：
    // - 输入时：表示输入数据的长度
    // - 输出时：表示输出缓冲区的大小

    log_debug("app_bt_postRead called: data_len=%d, read_len=%d, current_state=%d", 
              data_len, read_len, current_state);

    // 检查缓冲区是否超过上限
    // 注意：这里需要检查的是输入数据的长度，而不是输出缓冲区的大小
    // 由于 data_len 既表示输入长度又表示输出缓冲区大小，我们需要保存输入长度

    // 为了正确处理，我们需要在函数开始时保存输入数据的长度
    // 但由于函数签名限制，我们无法区分输入和输出
    // 解决方案：假设 data_len 是输入数据的长度，用于检查缓冲区溢出
    // 然后在输出时，假设 data_len 也是输出缓冲区的大小

    if (read_len + data_len > MAX_RECV_BUFFER_SIZE)
    {
        log_error("Receive buffer overflow, read_len=%d, data_len=%d, max=%d", 
                  read_len, data_len, MAX_RECV_BUFFER_SIZE);
        // 清空读缓冲区和重置状态
        read_len = 0;
        memset(read_buf, 0, sizeof(read_buf));
        memset(ack_trackers, 0, sizeof(ack_trackers));
        next_tracker_index = 0;
        current_state = FSM_STATE_IDLE;
        parse_pos = 0;
        return 0;
    }

    // 将当前数据添加到缓存中
    memcpy(read_buf + read_len, data, data_len);
    read_len += data_len;

    log_debug("Added data to buffer: new read_len=%d", read_len);

    // 打印缓冲区的前16个字节，用于调试
    if (read_len > 0) {
        char hex_str[64];
        int hex_len = read_len < 16 ? read_len : 16;
        for (int i = 0; i < hex_len; i++) {
            sprintf(hex_str + i*3, "%02x ", (unsigned char)read_buf[i]);
        }
        log_debug("Buffer content (first %d bytes): %s", hex_len, hex_str);
    }

    // 使用 FSM 解析数据
    int processed = 0;
    log_debug("Starting FSM parsing: processed=%d, read_len=%d, state=%d", 
              processed, read_len, current_state);
    while (processed < read_len)
    {
        switch (current_state)
        {
        case FSM_STATE_IDLE:
            // 等待固定头部 0xf1 0xdd
            unsigned char current_byte = (unsigned char)read_buf[processed];
            unsigned char next_byte = (processed + 1 < read_len) ? (unsigned char)read_buf[processed + 1] : 0;
            log_debug("FSM IDLE: processed=%d, read_len=%d, byte=0x%02x, next_byte=0x%02x", 
                      processed, read_len, current_byte, next_byte);

            // 检查条件
            int cond1 = (current_byte == 0xf1);
            int cond2 = (processed + 1 < read_len);
            int cond3 = (next_byte == 0xdd);
            log_debug("FSM IDLE conditions: cond1=%d (0x%02x==0xf1), cond2=%d (%d<%d), cond3=%d (0x%02x==0xdd)", 
                      cond1, current_byte, cond2, processed + 1, read_len, cond3, next_byte);

            if (cond1 && cond2 && cond3)
            {
                log_debug("Found header at position %d", processed);
                current_state = FSM_STATE_WAIT_LENGTH;
                parse_pos = processed + 2;
                processed += 2;
            }
            else
            {
                processed++;
            }
            break;

        case FSM_STATE_WAIT_LENGTH:
            // 读取长度字段
            if (parse_pos < read_len)
            {
                temp_len = read_buf[parse_pos];
                log_debug("Read length: %d at position %d", temp_len, parse_pos);
                // 检查长度是否合法
                if (temp_len > 250)
                { // 假设最大长度为250
                    log_error("Invalid length: %d, triggering resync", temp_len);
                    trigger_resync(g_bt_device);
                    return 0;
                }
                current_state = FSM_STATE_WAIT_DATA;
                parse_pos++;
            }
            else
            {
                log_debug("Waiting for more data, parse_pos=%d, read_len=%d", parse_pos, read_len);
                processed = read_len; // 等待更多数据
            }
            break;

        case FSM_STATE_WAIT_DATA:
            // 检查是否有足够的数据
            if (parse_pos + temp_len <= read_len)
            {
                log_debug("Have enough data: parse_pos=%d, temp_len=%d, read_len=%d", 
                          parse_pos, temp_len, read_len);
                // 读取ID
                if (temp_len >= 2)
                {
                    memcpy(&temp_id, read_buf + parse_pos, 2);
                    log_debug("Read packet ID: %d", temp_id);
                }

                // 检查输出缓冲区是否足够
                int output_len = temp_len + 3; // conn_type(1) + id_len(1) + msg_len(1) + id(2) + msg(temp_len-2)
                if (data_len < output_len)
                {
                    log_error("Output buffer too small: need %d, have %d", output_len, data_len);
                    trigger_resync(g_bt_device);
                    return 0;
                }

                // 构造输出数据
                memset(data, 0, data_len);
                data[0] = 1;                                              // conn_type;
                data[1] = 2;                                              // id_len;
                data[2] = temp_len - 2;                                   // msg_len;
                memcpy(data + 3, read_buf + parse_pos, 2);                // id;
                memcpy(data + 5, read_buf + parse_pos + 2, temp_len - 2); // msg;

                // 删除已处理的数据
                int total_len = parse_pos + temp_len;
                memmove(read_buf, read_buf + total_len, read_len - total_len);
                read_len -= total_len;

                // 重置FSM状态
                current_state = FSM_STATE_IDLE;
                parse_pos = 0;

                log_debug("Returning parsed packet, length=%d", output_len);
                // 返回消息的长度
                return temp_len + 3;
            }
            else
            {
                log_debug("Waiting for more data: parse_pos=%d, temp_len=%d, read_len=%d", 
                          parse_pos, temp_len, read_len);
                processed = read_len; // 等待更多数据
            }
            break;

        case FSM_STATE_ERROR:
            log_error("FSM error state, triggering resync");
            trigger_resync(g_bt_device);
            return 0;
        }
    }

    return 0;
}

// 判断是否收到ACK指令
int wait_ack(int fd)
{
    // 等待一定时间
    usleep(50 * 1000);
    // 读取数据
    char data_buf[4];
    read(fd, data_buf, 4);
    // 判断是否是OK\r\n
    if (memcmp(data_buf, "OK\r\n", 4) != 0)
    {
        log_error("wait_ack failed");
        return -1;
    }
    log_debug("wait_ack success");
    return 0;
}

int app_bt_status(Device *device)
{
    // 向蓝牙串口文件中写入"AT\r\n"的指令数据
    write(device->fd, "AT\r\n", 4);
    // 通过读取"OK\r\n"数据，判断蓝牙是否可用
    return wait_ack(device->fd);
}

int app_bt_rename(Device *device, char *name)
{
    // 拼接指令
    char cmd[20];
    sprintf(cmd, "AT+NAME%s\r\n", name);
    // 写入指令
    write(device->fd, cmd, strlen(cmd));
    // 等待ACK
    return wait_ack(device->fd);
}

int app_bt_setBaudRate(Device *device, BT_BaudRate baudRate)
{
    // 拼接指令
    char cmd[20];
    sprintf(cmd, "AT+BAUD%c\r\n", baudRate);
    // 写入指令
    write(device->fd, cmd, strlen(cmd));
    // 等待ACK
    return wait_ack(device->fd);
}

int app_bt_reset(Device *device)
{
    // 拼接指令
    char *cmd = "AT+RESET\r\n";
    // 写入指令
    write(device->fd, cmd, strlen(cmd));
    // 等待ACK
    return wait_ack(device->fd);
}

int app_bt_setNetId(Device *device, char *netid)
{
    // 拼接指令
    char cmd[20];
    sprintf(cmd, "AT+NETID%s\r\n", netid);
    // 写入指令
    write(device->fd, cmd, strlen(cmd));
    // 等待ACK
    return wait_ack(device->fd);
}

int app_bt_setMaddr(Device *device, char *maddr)
{
    // 拼接指令
    char cmd[20];
    sprintf(cmd, "AT+MADDR%s\r\n", maddr);
    // 写入指令
    write(device->fd, cmd, strlen(cmd));
    // 等待ACK
    return wait_ack(device->fd);
}
