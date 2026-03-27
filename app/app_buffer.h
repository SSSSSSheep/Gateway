#ifndef __APP_BUFFER_H__
#define __APP_BUFFER_H__

#include <pthread.h>

// �ڲ����ӻ�����
typedef struct
{
    unsigned char *ptr; // ָ�򻺳�����ָ��
    int total_size;     // �������Ĵ�С
    int len;            // �����������ݳ���
} SubBuffer;

// ������
typedef struct
{
    SubBuffer *sub_buffers[2];  // �����ӻ�����
    int read_index;             // ��ǰ��ȡ���ӻ���������
    int write_index;            // ��ǰд����ӻ���������
    pthread_mutex_t read_lock;  // ����
    pthread_mutex_t write_lock; // д��
} Buffer;

/**
 * @brief ��ʼ��������
 *
 * @param size
 * @return Buffer*
 */
Buffer *app_buffer_init(int size); // ����������

/**
 * @brief ��ջ�����
 *
 * @param buffer
 */
void app_buffer_free(Buffer *buffer); // �ͷŻ�����

/**
 * @brief д�����ݵ�������
 *
 * @param buffer
 * @param data
 * @param len
 * @return int
 */
int app_buffer_write(Buffer *buffer, char *data, int data_len); // д������

/**
 * @brief �ӻ�������ȡ����
 *
 * @param buffer
 * @param data
 * @param len
 * @return int
 */
int app_buffer_read(Buffer *buffer, char *data_buf, int buf_size); // ��ȡ����

#endif // !__APP_BUFFER_H__
