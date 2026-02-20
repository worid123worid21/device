#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <time.h>

#define MPU_ADDR 0x68

#define PWR_MGMT_1   0x6B
#define ACCEL_XOUT_H 0x3B
#define GYRO_XOUT_H  0x43

// 시간 측정
double get_delta_time() {
    static struct timespec last = {0,0};
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    double dt;
    if (last.tv_sec == 0 && last.tv_nsec == 0) dt = 0.01;
    else dt = (now.tv_sec - last.tv_sec) + (now.tv_nsec - last.tv_nsec)/1e9;

    last = now;
    return dt;
}

// I2C에서 16bit 값 읽기
int16_t read_word(int fd, uint8_t reg) {
    uint8_t buf[2];
    if (write(fd, &reg, 1) != 1) return 0;
    if (read(fd, buf, 2) != 2) return 0;
    return (buf[0] << 8) | buf[1];
}

int main() {
    int fd;
    const char *device = "/dev/i2c-1";

    if ((fd = open(device, O_RDWR)) < 0) return 1;
    if (ioctl(fd, I2C_SLAVE, MPU_ADDR) < 0) return 1;

    // MPU-6050 Sleep 모드 해제
    if (write(fd, (uint8_t[]){PWR_MGMT_1, 0}, 2) != 2) return 1;

    printf("AX\tAY\tAZ\tGX\tGY\tGZ\n");

    while (1) {
        // 원시값 읽기
        int16_t ax = read_word(fd, ACCEL_XOUT_H);
        int16_t ay = read_word(fd, ACCEL_XOUT_H + 2);
        int16_t az = read_word(fd, ACCEL_XOUT_H + 4);

        int16_t gx = read_word(fd, GYRO_XOUT_H);
        int16_t gy = read_word(fd, GYRO_XOUT_H + 2);
        int16_t gz = read_word(fd, GYRO_XOUT_H + 4);

        // 원시값 출력
        printf("%d\t%d\t%d\t%d\t%d\t%d\n", ax, ay, az, gx, gy, gz);

        usleep(1000000); // 1000ms
    }

    close(fd);
    return 0;
}

