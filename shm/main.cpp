#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <stdint.h>

#define SHM_KEY 1234
#define SEM_KEY 5678

static void fill_yuv_buffer(uint8_t *buffer, int width, int height, int frame_index) 
{
    int x, y, i = frame_index;

    /* Y */
    for (y = 0; y < height; y++)
        for (x = 0; x < width; x++)
            buffer[y * width + x] = x + y + i * 3;

    /* Cb and Cr */
    for (y = 0; y < height / 2; y++) {
        for (x = 0; x < width / 2; x++) {
            buffer[width * height + y * width / 2 + x] = 128 + y + i * 2;
            buffer[width * height * 5 / 4 + y * width / 2 + x] = 64 + x + i * 5;
        }
    }
}

int main() {
    int shmid, semid;
    uint8_t *yuv_buffer;
    struct sembuf sem_op;
    int width = 640;
    int height = 512;
    int buffer_size = width * height * 3 / 2;

    // 创建共享内存段
    shmid = shmget(SHM_KEY, buffer_size, IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }

    // 将共享内存附加到进程的地址空间
    yuv_buffer = (uint8_t *)shmat(shmid, NULL, 0);
    if (yuv_buffer == (uint8_t *)-1) {
        perror("shmat");
        exit(1);
    }

    // 创建信号量
    semid = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    if (semid < 0) {
        perror("semget");
        exit(1);
    }

    // 初始化信号量值为1
    if (semctl(semid, 0, SETVAL, 1) < 0) {
        perror("semctl");
        exit(1);
    }

    int frame_index = 0;
    while (1) {
        // 等待信号量
        sem_op.sem_num = 0;
        sem_op.sem_op = -1;
        sem_op.sem_flg = 0;
        if (semop(semid, &sem_op, 1) < 0) {
            perror("semop");
            exit(1);
        }

        // 填充YUV缓冲区数据
        fill_yuv_buffer(yuv_buffer, width, height, frame_index);
        frame_index++;

        // 释放信号量
        sem_op.sem_num = 0;
        sem_op.sem_op = 1;
        sem_op.sem_flg = 0;
        if (semop(semid, &sem_op, 1) < 0) {
            perror("semop");
            exit(1);
        }

        usleep(1000000 / 30); // 30 FPS
    }

    // 分离共享内存
    shmdt(yuv_buffer);

    return 0;
}