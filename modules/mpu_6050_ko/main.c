#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#define BASE_PATH "/sys/bus/i2c/devices/1-0068/"
//#define G_TO_MS2 9.80665 중력 가속도 사용시
#define G_TO_MS2 1

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* -------------------------------------------------- */
/* 드라이버 존재 확인 (accel_x 파일 기준) */
/* -------------------------------------------------- */
int driver_loaded()
{
    struct stat st;
    return (stat(BASE_PATH "accel_x", &st) == 0);
}

/* -------------------------------------------------- */
/* sysfs 값 읽기 */
/* -------------------------------------------------- */
int read_sysfs_value(const char *name)
{
    char path[128];
    char buf[32];
    FILE *fp;

    snprintf(path, sizeof(path), "%s%s", BASE_PATH, name);

    fp = fopen(path, "r");
    if (!fp)
        return 0;

    fgets(buf, sizeof(buf), fp);
    fclose(fp);

    return atoi(buf);
}

/* -------------------------------------------------- */
void sleep_us(long us)
{
    struct timespec ts;
    ts.tv_sec = us / 1000000L;
    ts.tv_nsec = (us % 1000000L) * 1000L;
    nanosleep(&ts, NULL);
}

/* -------------------------------------------------- */
double get_delta_time()
{
    static struct timespec last = {0,0};
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    double dt;
    if (last.tv_sec == 0 && last.tv_nsec == 0)
        dt = 0.01;
    else
        dt = (now.tv_sec - last.tv_sec) +
             (now.tv_nsec - last.tv_nsec)/1e9;

    last = now;
    return dt;
}

/* -------------------------------------------------- */
double clamp90(double angle)
{
    if(angle > 90.0) return 90.0;
    if(angle < -90.0) return -90.0;
    return angle;
}

/* -------------------------------------------------- */
int main()
{
    if (!driver_loaded()) {
        printf("MPU-6050 kernel module not loaded.\n");
        printf("Please run: sudo insmod mpu_6050.ko\n");
        return 1;
    }

    printf("MPU-6050 (Kernel Driver via sysfs) - Roll/Pitch/Yaw Demo\n\n");

    /* 초기 캘리브레이션 실행 */
    FILE *cal = fopen(BASE_PATH "calibrate", "w");
    if (cal) {
        fprintf(cal, "1");
        fclose(cal);
    }

    double pitch = 0, roll = 0, yaw = 0;
    double alpha = 0.96;
    int first_loop = 1;
    double print_timer = 0.0;

    while (1) {
        double dt = get_delta_time();

        /* sysfs 단위:
           accel = mg
           gyro  = m°/s
        */
        int ax = read_sysfs_value("accel_x");
        int ay = read_sysfs_value("accel_y");
        int az = read_sysfs_value("accel_z");

        int gx = read_sysfs_value("gyro_x");
        int gy = read_sysfs_value("gyro_y");
        int gz = read_sysfs_value("gyro_z");

        /* 단위 변환 */
        double accel_x = (ax / 1000.0) * G_TO_MS2;
        double accel_y = (ay / 1000.0) * G_TO_MS2;
        double accel_z = (az / 1000.0) * G_TO_MS2;

        double gyro_x = gx / 1000.0; // deg/s
        double gyro_y = gy / 1000.0;
        double gyro_z = gz / 1000.0;

        /* 가속도 기반 각도 계산 */
        double accel_pitch = atan2(-accel_x,
                                   sqrt(accel_y*accel_y + accel_z*accel_z))
                             * 180.0 / M_PI;
        double accel_roll  = atan2(accel_y, accel_z)
                             * 180.0 / M_PI;

        if(first_loop) {
            pitch = accel_pitch;
            roll  = accel_roll;
            yaw   = 0.0;
            first_loop = 0;
        }

        /* Complementary Filter 적용 */
        roll  = alpha * (roll + gyro_x * dt) + (1 - alpha) * accel_roll;
        pitch = alpha * (pitch + gyro_y * dt) + (1 - alpha) * accel_pitch;
        yaw   += gyro_z * dt; // yaw는 자이로 적분만

        /* 중력 크기 계산 */
        double g_mag = sqrt(accel_x*accel_x +
                            accel_y*accel_y +
                            accel_z*accel_z);

        print_timer += dt*1000;
        if(print_timer >= 500.0) { // 0.5초마다 출력
            print_timer = 0.0;

            printf("Pitch: %.2f°, Roll: %.2f°, Yaw: %.2f°\n",
                   clamp90(pitch), clamp90(roll), yaw);

            // m/s 단위 사용 시 활성화
            // printf("Accel (m/s²): X: %.2f Y: %.2f Z: %.2f\n",
            //        accel_x, accel_y, accel_z);

            printf("Accel (g): X: %.2f Y: %.2f Z: %.2f\n",
                   accel_x, accel_y, accel_z);

            printf("Gravity |g|: %.2f\n\n", g_mag);
        }

        sleep_us(2000); // 2ms
    }

    return 0;
}