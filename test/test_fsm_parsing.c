
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>
#include <fcntl.h>
#include "app_bt.h"
#include "app_serial.h"
#include "log/log.h"

// 测试配置
#define TEST_PACKET_COUNT 5      // 测试数据包数量
#define TEST_PACKET_SIZE 64      // 测试数据包大小

// 全局变量
static Device test_device;
static int test_running = 1;
static uint16_t test_packet_id = 0;
static int packets_received = 0;

// 构造合法的数据包
void build_valid_packet(char *packet, int *packet_len, uint16_t packet_id)
{
    int msg_len = TEST_PACKET_SIZE - 2;

    // 构造数据包: 0xf1 0xdd len id[2] msg[len]
    packet[0] = 0xf1;  // 固定头部
    packet[1] = 0xdd;  // 固定头部
    packet[2] = msg_len; // 长度字段
    memcpy(packet + 3, &packet_id, 2); // ID
    memset(packet + 5, 'A' + (packet_id % 26), msg_len); // 消息内容

    *packet_len = 3 + msg_len;
}

// 构造非法长度的数据包
void build_invalid_length_packet(char *packet, int *packet_len, uint16_t packet_id)
{
    // 构造数据包: 0xf1 0xdd len id[2] msg[len]
    packet[0] = 0xf1;  // 固定头部
    packet[1] = 0xdd;  // 固定头部
    packet[2] = 255;   // 非法长度字段（超过250）
    memcpy(packet + 3, &packet_id, 2); // ID

    *packet_len = 5;
}

// 构造缺少固定头部的数据包
void build_missing_header_packet(char *packet, int *packet_len, uint16_t packet_id)
{
    int msg_len = TEST_PACKET_SIZE - 2;

    // 构造数据包: 0xf1 0x00 len id[2] msg[len]
    packet[0] = 0xf1;  // 不完整的固定头部
    packet[1] = 0x00;  // 错误的固定头部
    packet[2] = msg_len; // 长度字段
    memcpy(packet + 3, &packet_id, 2); // ID
    memset(packet + 5, 'A' + (packet_id % 26), msg_len); // 消息内容

    *packet_len = 3 + msg_len;
}

// 模拟发送数据包到接收缓冲区
int simulate_receive_packet(Device *device, const char *packet, int packet_len)
{
    // app_bt_postRead的第一个参数是从串口接收到的原始数据
    // 第二个参数是数据的长度
    // 函数会将这些数据添加到内部的read_buf缓冲区中
    // 然后使用FSM解析缓冲区中的数据
    // 如果解析出完整的数据包，会将解析后的数据存储在第一个参数中，并返回数据长度

    // 我们需要创建一个临时缓冲区来存储接收到的数据
    // 这个缓冲区既用于输入（存储接收到的原始数据），也用于输出（存储解析后的消息）
    char recv_buf[256];
    memcpy(recv_buf, packet, packet_len);

    // 调用app_bt_postRead处理接收到的数据
    // 注意：第二个参数既是输入数据的长度，也是输出缓冲区的大小
    // 在真实场景中，这是从串口读取的数据长度
    int processed = app_bt_postRead(recv_buf, packet_len);

    // 检查返回值
    if (processed > 0)
    {
        // 成功解析出一个完整的数据包
        packets_received++;
        // 解析后的数据格式：
        // recv_buf[0] = conn_type (1)
        // recv_buf[1] = id_len (2)
        // recv_buf[2] = msg_len
        // recv_buf[3:4] = id
        // recv_buf[5:] = msg
        uint16_t packet_id;
        memcpy(&packet_id, recv_buf + 3, 2);
        log_info("Received packet: id=%d, msg_len=%d, total_len=%d", 
                 packet_id, recv_buf[2], processed);
        return 0;
    }
    else if (processed < 0)
    {
        log_error("Failed to process packet, error=%d", processed);
        return -1;
    }
    else
    {
        // processed == 0 表示没有完整的数据包被解析
        // 可能的原因：
        // 1. 数据不完整，需要等待更多数据
        // 2. 触发了重同步（如非法长度）
        // 3. 数据被丢弃（如缺少固定头部）
        log_debug("No complete packet processed, processed=0");
        return 0;
    }
}

// 测试FSM解析 - 合法数据包
int test_fsm_valid_packets(Device *device)
{
    log_info("=== Test: FSM Parsing with Valid Packets ===");

    // 重置计数器
    packets_received = 0;

    // 重置蓝牙模块的内部状态
    app_bt_reset_internal_state();

    // 发送多个合法数据包
    for (int i = 0; i < TEST_PACKET_COUNT; i++)
    {
        char packet[256];
        int packet_len;

        // 构造合法数据包
        build_valid_packet(packet, &packet_len, test_packet_id);
        test_packet_id++;

        // 模拟接收数据包
        if (simulate_receive_packet(device, packet, packet_len) < 0)
        {
            log_error("Failed to receive packet %d", i);
            return -1;
        }
    }

    // 检查结果
    log_info("Total packets received: %d", packets_received);

    if (packets_received == TEST_PACKET_COUNT)
    {
        log_info("Test PASSED: All valid packets received correctly");
        return 0;
    }
    else
    {
        log_error("Test FAILED: Expected %d packets, received %d", TEST_PACKET_COUNT, packets_received);
        return -1;
    }
}

// 测试FSM解析 - 非法长度数据包
int test_fsm_invalid_length(Device *device)
{
    log_info("=== Test: FSM Parsing with Invalid Length ===");

    // 重置计数器
    packets_received = 0;

    // 重置蓝牙模块的内部状态
    app_bt_reset_internal_state();

    // 发送非法长度数据包
    char packet[256];
    int packet_len;

    // 构造非法长度数据包
    build_invalid_length_packet(packet, &packet_len, test_packet_id);
    test_packet_id++;

    // 模拟接收数据包
    if (simulate_receive_packet(device, packet, packet_len) < 0)
    {
        log_error("Failed to receive invalid length packet");
        return -1;
    }

    // 检查结果
    log_info("Total packets received: %d", packets_received);

    // 非法长度的数据包不应该被正确接收
    if (packets_received == 0)
    {
        log_info("Test PASSED: Invalid length packet was discarded");
        return 0;
    }
    else
    {
        log_error("Test FAILED: Invalid length packet was incorrectly processed");
        return -1;
    }
}

// 测试FSM解析 - 缺少固定头部的数据包
int test_fsm_missing_header(Device *device)
{
    log_info("=== Test: FSM Parsing with Missing Header ===");

    // 重置计数器
    packets_received = 0;

    // 重置蓝牙模块的内部状态
    app_bt_reset_internal_state();

    // 发送缺少固定头部的数据包
    char packet[256];
    int packet_len;

    // 构造缺少固定头部的数据包
    build_missing_header_packet(packet, &packet_len, test_packet_id);
    test_packet_id++;

    // 模拟接收数据包
    if (simulate_receive_packet(device, packet, packet_len) < 0)
    {
        log_error("Failed to receive missing header packet");
        return -1;
    }

    // 检查结果
    log_info("Total packets received: %d", packets_received);

    // 缺少固定头部的数据包应该被丢弃，但不一定触发重同步
    // 因为FSM会逐个字节查找固定头部
    if (packets_received == 0)
    {
        log_info("Test PASSED: Missing header packet was discarded");
        return 0;
    }
    else
    {
        log_error("Test FAILED: Missing header packet was incorrectly processed");
        return -1;
    }
}

// 测试FSM解析 - 混合数据包
int test_fsm_mixed_packets(Device *device)
{
    log_info("=== Test: FSM Parsing with Mixed Packets ===");

    // 重置计数器
    packets_received = 0;

    // 重置蓝牙模块的内部状态
    app_bt_reset_internal_state();

    // 发送混合数据包
    for (int i = 0; i < TEST_PACKET_COUNT; i++)
    {
        char packet[256];
        int packet_len;

        if (i % 2 == 0)
        {
            // 发送合法数据包
            build_valid_packet(packet, &packet_len, test_packet_id);
        }
        else
        {
            // 发送非法长度数据包
            build_invalid_length_packet(packet, &packet_len, test_packet_id);
        }
        test_packet_id++;

        // 模拟接收数据包
        if (simulate_receive_packet(device, packet, packet_len) < 0)
        {
            log_error("Failed to receive packet %d", i);
            return -1;
        }
    }

    // 检查结果
    log_info("Total packets received: %d", packets_received);

    // 合法数据包应该被正确接收，非法数据包应该被丢弃
    // 发送 TEST_PACKET_COUNT 个包，偶数索引是合法包，奇数索引是非法包
    // 所以合法包的数量是 (TEST_PACKET_COUNT + 1) / 2
    int expected_packets = (TEST_PACKET_COUNT + 1) / 2;
    if (packets_received == expected_packets)
    {
        log_info("Test PASSED: Mixed packets processed correctly");
        return 0;
    }
    else
    {
        log_error("Test FAILED: Expected %d packets, got %d packets",
                 expected_packets, packets_received);
        return -1;
    }
}

int main(int argc, char *argv[])
{
    // 设置日志级别为 DEBUG，以便查看详细的调试信息
    log_set_level(LOG_DEBUG);

    log_info("Starting FSM Parsing Test");

    // 初始化测试设备
    memset(&test_device, 0, sizeof(test_device));

    // 运行测试
    int test_result = 0;
    test_result |= test_fsm_valid_packets(&test_device);
    test_result |= test_fsm_invalid_length(&test_device);
    test_result |= test_fsm_missing_header(&test_device);
    test_result |= test_fsm_mixed_packets(&test_device);

    // 返回测试结果
    if (test_result == 0)
    {
        log_info("All tests PASSED");
        return 0;
    }
    else
    {
        log_error("Some tests FAILED");
        return -1;
    }
}
