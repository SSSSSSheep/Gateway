#ifndef __APP_COMMON_H__
#define __APP_COMMON_H__

/**
 * @brief 获取当前时间戳 毫秒单位
 *
 * @return long 当前时间戳 毫秒单位
 */
long app_common_getCurrentTime(void);

/**
 * @brief 字符串转16进制字符串
 *
 * @param chars 字符数组
 * @param chars_len 字符数组长度
 * @return char* 16进制字符串
 */
char *app_common_chars2HexStr(char *chars, int chars_len);

/**
 * @brief 16进制字符串转字符数组
 *
 * @param hexStr 16进制字符串
 * @param hexStr_len 接收16进制字符串长度的指针
 * @return char* 字符数组
 */
char *app_common_hexStr2Chars(char *hexStr, int *hexStr_len);

#endif