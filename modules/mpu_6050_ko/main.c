#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>

#define BASE_PATH "/sys/bus/i2c/devices/1-0068/"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* =====================================================
 * 설정값
 * ===================================================== */
#define ALPHA       0.96     /* Complementary Filter 계수 (0~1, 클수록 자이로 비중↑) */
#define LOOP_US     2000L    /* 루프 주기: 2ms = 500Hz                                */
#define PRINT_MS    500.0    /* 출력 주기: 500ms                                      */

/* =====================================================
 * 유틸리티
 * ===================================================== */
static int driver_loaded(void)
{
    struct stat st;
    return (stat(BASE_PATH "accel_x", &st) == 0);
}

static int read_sysfs(const char *name)
{
    char path[128], buf[32];
    FILE *fp;
    snprintf(path, sizeof(path), "%s%s", BASE_PATH, name);
    fp = fopen(path, "r");
    if (!fp) return 0;
    if (!fgets(buf, sizeof(buf), fp)) { fclose(fp); return 0; }
    fclose(fp);
    return atoi(buf);
}

static void sleep_us(long us)
{
    struct timespec ts;
    ts.tv_sec  = us / 1000000L;
    ts.tv_nsec = (us % 1000000L) * 1000L;
    nanosleep(&ts, NULL);
}

static double get_dt(void)
{
    static struct timespec last = {0, 0};
    struct timespec now;
    double dt;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (last.tv_sec == 0 && last.tv_nsec == 0)
        dt = LOOP_US / 1e6;
    else
        dt = (now.tv_sec  - last.tv_sec)
           + (now.tv_nsec - last.tv_nsec) / 1e9;
    last = now;
    return dt;
}

static double clamp90(double v)
{
    if (v >  90.0) return  90.0;
    if (v < -90.0) return -90.0;
    return v;
}

/* =====================================================
 * main
 * ===================================================== */
int main(void)
{
    if (!driver_loaded()) {
        printf("[ERROR] MPU-6050 kernel module not loaded.\n");
        printf("        sudo insmod mpu_6050.ko\n");
        return 1;
    }

    printf("MPU-6050 Pitch / Roll Monitor  (Ctrl+C to exit)\n\n");

    /* 커널 캘리브레이션 재실행 */
    FILE *cal = fopen(BASE_PATH "calibrate", "w");
    if (cal) { fprintf(cal, "1"); fclose(cal); }
    printf("[INFO] Gyro calibration done.\n\n");

    double pitch = 0.0, roll = 0.0;
    double print_timer = 0.0;
    int    first = 1;

    while (1) {
        double dt = get_dt();

        /* ── 원시 데이터 읽기 ──────────────────────── */
        double ax = read_sysfs("accel_x") / 1000.0;  /* g */
        double ay = read_sysfs("accel_y") / 1000.0;
        double az = read_sysfs("accel_z") / 1000.0;
        double gx = read_sysfs("gyro_x")  / 1000.0;  /* °/s */
        double gy = read_sysfs("gyro_y")  / 1000.0;

        /* ── 가속도 기반 각도 계산 ─────────────────── *
         *                                               *
         *  핵심: pitch 와 roll 이 서로 독립적이려면     *
         *  각 축 계산에 상대방 축 성분이 섞이면 안 됨   *
         *                                               *
         *  pitch = atan2(-ax, sqrt(ay² + az²))          *
         *    → ax 만 pitch에 반응                       *
         *    → ay, az 는 분모에서 크기만 사용           *
         *                                               *
         *  roll  = atan2(ay, sqrt(ax² + az²))           *
         *    → ay 만 roll에 반응  ★ 기존과 다른 점      *
         *    → ax, az 는 분모에서 크기만 사용           *
         *    → 기존 atan2(ay, az) 공식은                *
         *       pitch 변화 시 az 가 바뀌면서            *
         *       roll 값도 같이 튀는 문제 있었음         *
         * ─────────────────────────────────────────── */
        double accel_pitch = atan2(-ax, sqrt(ay*ay + az*az)) * 180.0 / M_PI;
        double accel_roll  = atan2( ay, sqrt(ax*ax + az*az)) * 180.0 / M_PI;

        /* ── 첫 루프: 가속도 기반으로 초기화 ─────── */
        if (first) {
            pitch = accel_pitch;
            roll  = accel_roll;
            first = 0;
            sleep_us(LOOP_US);
            continue;
        }

        /* ── Complementary Filter ─────────────────── *
         *  자이로: 빠른 움직임 추적 (단기 정확)        *
         *  가속도: 중력 기준 보정   (장기 안정)        *
         *                                              *
         *  pitch += gyro_y * dt   (Y축 회전 = pitch)  *
         *  roll  += gyro_x * dt   (X축 회전 = roll)   *
         * ─────────────────────────────────────────── */
        pitch = ALPHA * (pitch + gy * dt) + (1.0 - ALPHA) * accel_pitch;
        roll  = ALPHA * (roll  + gx * dt) + (1.0 - ALPHA) * accel_roll;

        /* ── 출력 (500ms마다) ────────────────────────── */
        print_timer += dt * 1000.0;
        if (print_timer >= PRINT_MS) {
            print_timer = 0.0;
            printf("Pitch: %7.2f°  |  Roll: %7.2f°\n",
                   clamp90(pitch), clamp90(roll));
            printf("Accel (g)  X: %6.3f  Y: %6.3f  Z: %6.3f\n\n",
                   ax, ay, az);
        }

        sleep_us(LOOP_US);
    }

    return 0;
}