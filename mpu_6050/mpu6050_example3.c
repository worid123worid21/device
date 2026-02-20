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

double get_delta_time() {
    static struct timespec last = {0,0};
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    double dt;
    if (last.tv_sec == 0 && last.tv_nsec == 0) {
        dt = 0.01; // 첫 루프 초기값 10ms
    } else {
        dt = (now.tv_sec - last.tv_sec) + (now.tv_nsec - last.tv_nsec)/1e9;
    }

    last = now;
    return dt;
}

int16_t read_word(int fd, uint8_t reg) {
    uint8_t buf[2];
    if (write(fd, &reg, 1) != 1) return 0;
    if (read(fd, buf, 2) != 2) return 0;
    return (buf[0] << 8) | buf[1];
}

double clamp90(double angle) {
    if(angle > 90.0) return 90.0;
    if(angle < -90.0) return -90.0;
    return angle;
}

int main() {
    int fd;
    const char *device = "/dev/i2c-1";

    if ((fd = open(device, O_RDWR)) < 0) { perror("Open failed"); return 1; }
    if (ioctl(fd, I2C_SLAVE, MPU_ADDR) < 0) { perror("I2C_SLAVE failed"); return 1; }

    if (write(fd, (uint8_t[]){PWR_MGMT_1, 0}, 2) != 2) { perror("Wake failed"); return 1; }

    // ===== 자이로 바이어스 10초 =====
    printf("Calibrating gyro... Keep sensor still for 10 sec\n");
    int sample_rate_us = 10000; // 10ms
    int samples = 1000;         // 10초
    int64_t gx_sum=0, gy_sum=0, gz_sum=0;
    for(int i=0;i<samples;i++){
        gx_sum += read_word(fd, GYRO_XOUT_H);
        gy_sum += read_word(fd, GYRO_XOUT_H + 2);
        gz_sum += read_word(fd, GYRO_XOUT_H + 4);
        usleep(sample_rate_us);
    }
    double gx_bias = gx_sum / (double)samples;
    double gy_bias = gy_sum / (double)samples;
    double gz_bias = gz_sum / (double)samples;
    printf("Gyro bias: %.2f %.2f %.2f\n", gx_bias, gy_bias, gz_bias);

    double pitch = 0, roll = 0;
    double alpha = 0.96; // 실시간 반영률 높임
    int first_loop = 1;
    double print_timer = 0.0; // ms 단위 누적 시간

    while (1) {
        double dt = get_delta_time(); // 초 단위

        // 가속도 raw 읽기
        int16_t ax = read_word(fd, ACCEL_XOUT_H);
        int16_t ay = read_word(fd, ACCEL_XOUT_H + 2);
        int16_t az = read_word(fd, ACCEL_XOUT_H + 4);

        // 자이로 raw 읽기 + 바이어스 보정
        double gx = (double)read_word(fd, GYRO_XOUT_H) - gx_bias;
        double gy = (double)read_word(fd, GYRO_XOUT_H + 2) - gy_bias;

        // 단위 변환
        double accel_x = ax / ACCEL_SCALE;
        double accel_y = ay / ACCEL_SCALE;
        double accel_z = az / ACCEL_SCALE;
        double gyro_x = gx / GYRO_SCALE;
        double gyro_y = gy / GYRO_SCALE;

        // 가속도로 기울기 계산
        double accel_pitch = atan2(-accel_x, sqrt(accel_y*accel_y + accel_z*accel_z)) * 180.0 / M_PI;
        double accel_roll  = atan2(accel_y, accel_z) * 180.0 / M_PI;

        // 첫 루프에서 초기화
        if(first_loop){
            pitch = accel_pitch;
            roll  = accel_roll;
            first_loop = 0;
        }

        // Complementary Filter 적용
        pitch = alpha * (pitch + gyro_x * dt) + (1 - alpha) * accel_pitch;
        roll  = alpha * (roll  + gyro_y * dt) + (1 - alpha) * accel_roll;

        // 0.5초마다 출력
        print_timer += dt * 1000; // ms 단위 누적
        if(print_timer >= 500.0){ 
            print_timer = 0.0;
            printf("Pitch: %.2f°, Roll: %.2f° | Accel(g): X: %.2f Y: %.2f Z: %.2f\n",
                   clamp90(pitch), clamp90(roll),
                   accel_x, accel_y, accel_z);
        }

        usleep(2000); // 2ms → 500Hz 계산
    }

    close(fd);
    return 0;
}