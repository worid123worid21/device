#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <math.h>
#include <time.h>

#define MPU_ADDR 0x68

#define PWR_MGMT_1   0x6B
#define ACCEL_XOUT_H 0x3B
#define GYRO_XOUT_H  0x43

#define ACCEL_SCALE 16384.0
#define GYRO_SCALE  131.0

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

// -180 ~ 180
double angle180(double angle) {
    while(angle > 180.0) angle -= 360.0;
    while(angle < -180.0) angle += 360.0;
    return angle;
}

// -90 ~ 90
double clamp90(double angle) {
    if(angle > 90.0) return 90.0;
    if(angle < -90.0) return -90.0;
    return angle;
}

int main() {
    int fd;
    const char *device = "/dev/i2c-1";

    if ((fd = open(device, O_RDWR)) < 0) return 1;
    if (ioctl(fd, I2C_SLAVE, MPU_ADDR) < 0) return 1;

    // MPU-6050 Sleep 모드 해제
    if (write(fd, (uint8_t[]){PWR_MGMT_1, 0}, 2) != 2) return 1;

    double pitch = 0, roll = 0, yaw = 0;
    double alpha = 0.98;

    while (1) {
        double dt = get_delta_time();

        // 가속도 raw 읽기
        int16_t ax = read_word(fd, ACCEL_XOUT_H);
        int16_t ay = read_word(fd, ACCEL_XOUT_H + 2);
        int16_t az = read_word(fd, ACCEL_XOUT_H + 4);

        // 자이로 raw 읽기
        int16_t gx = read_word(fd, GYRO_XOUT_H);
        int16_t gy = read_word(fd, GYRO_XOUT_H + 2);
        int16_t gz = read_word(fd, GYRO_XOUT_H + 4);

        // 단위 변환
        double accel_x = ax / ACCEL_SCALE;
        double accel_y = ay / ACCEL_SCALE;
        double accel_z = az / ACCEL_SCALE;

        double gyro_x = gx / GYRO_SCALE;
        double gyro_y = gy / GYRO_SCALE;
        double gyro_z = gz / GYRO_SCALE;

        // 가속도로 기울기 계산
        double accel_pitch = atan2(-accel_x, sqrt(accel_y*accel_y + accel_z*accel_z)) * 180.0 / M_PI;
        double accel_roll  = atan2(accel_y, accel_z) * 180.0 / M_PI;

        // Complementary Filter 적용 (Pitch, Roll)
        pitch = alpha * (pitch + gyro_x * dt) + (1 - alpha) * accel_pitch;
        roll  = alpha * (roll  + gyro_y * dt) + (1 - alpha) * accel_roll;

        // Yaw 계산 (자이로 적분)
        yaw += gyro_z * dt;

        // 범위 조정
        double pitch90 = clamp90(pitch);       // -90 ~ 90
        double roll90  = clamp90(roll);        // -90 ~ 90
        double yaw180  = angle180(yaw);        // -180 ~ 180

        printf("Pitch(X): %.2f°, Roll(Y): %.2f°, Yaw(Z): %.2f°\n", pitch90, roll90, yaw180);

        usleep(500000); // 500ms
    }

    close(fd);
    return 0;
}