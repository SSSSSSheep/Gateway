#ifndef __APP_MESSAGE_H__
#define __APP_MESSAGE_H__

/**
 * @brief 字符数组消息转json消息
 *
 * @param chars 字符数组消息
 * @param chars_len 字符数组长度
 * @return char* json消息
 */
char *app_message_chars2Json(char *chars, int chars_len);

/**
 * @brief json消息转字符数组消息
 *
 * @param json json消息
 * @param chars_buf 字符数组消息缓冲区
 * @param buf_size 缓冲区大小
 * @return int 实际写入字符数组长度 -1失败
 */
int app_message_json2Chars(char *json, char *chars_buf, int buf_size);

#endif  // !__APP_MESSAGE_H__
