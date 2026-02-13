#include "app_common.h"
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

long app_common_getCurrentTime(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return tv.tv_sec *= 1000 + tv.tv_usec / 1000;
    // tv_sec:秒, tv_usec:微秒
}

/**
 * [abcd] => "61626364"
 */
char *app_common_chars2HexStr(char *chars, int chars_len)
{
    // 申请16进制字符串内存
    char *hexStr = (char *)malloc(chars_len * 2 + 1);
    // 遍历chars中的每个字符 将字符转换为对应的2位16进制字符保存到hexstr中
    for (int i = 0; i < chars_len; i++)
    {
        sprintf(hexStr + i * 2, "%02x", chars[i]);
    }
    // 向hexstr中添加结束符
    hexStr[chars_len * 2] = '\0';
    // 返回hexstr
    return hexStr;
}

/**
 * "61626364 => [abcd]"
 */
char *app_common_hexStr2Chars(char *hexStr, int *hexStr_len)
{
    // 得到hexStr的长度 并保存到hexStr_len_tmp中
    int hexStr_len_tmp = strlen(hexStr);
    // 得到chars的长度 并保存到chars_len中
    *hexStr_len = hexStr_len_tmp / 2;

    // 申请chars空间
    char *chars = (char *)malloc(*hexStr_len);
    // 遍历hexstr中的2个16进制字符（以2个为单位），得到1个对应的字符 保存到chars中
    for (int i = 0; i < hexStr_len_tmp; i += 2)
    {
        sscanf(hexStr + i, "%02X", (unsigned int *)(chars + i / 2));
    }
    // 返回chars
    return chars;
}
