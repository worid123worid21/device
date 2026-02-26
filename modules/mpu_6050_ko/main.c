#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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
#define ALPHA          0.96    /* Complementary Filter 계수 */
#define LOOP_US        2000L   /* 루프 주기: 2ms            */
#define PRINT_MS       500.0   /* 출력 주기: 500ms          */

/* =====================================================
 * Yaw drift 억제 임계값
 * 이 값(m°/s) 이하의 자이로 Z 값은 0으로 처리
 * → 정지 시 yaw가 천천히 돌아가는 현상 제거
 * ===================================================== */
#define GYRO_Z_DEADZONE  15    /* 단위: m°/s */

/* =====================================================
 * 드라이버 로드 확인
 * ===================================================== */
static int driver_loaded(void)
{
    struct stat st;
    return (stat(BASE_PATH "accel_x", &st) == 0);
}

/* =====================================================
 * sysfs 읽기
 * ===================================================== */
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

/* =====================================================
 * 루프 슬립
 * ===================================================== */
static void sleep_us(long us)
{
    struct timespec ts;
    ts.tv_sec  = us / 1000000L;
    ts.tv_nsec = (us % 1000000L) * 1000L;
    nanosleep(&ts, NULL);
}

/* =====================================================
 * delta time (초)
 * ===================================================== */
static double get_dt(void)
{
    static struct timespec last = {0, 0};
    struct timespec now;
    double dt;

    clock_gettime(CLOCK_MONOTONIC, &now);

    if (last.tv_sec == 0 && last.tv_nsec == 0)
        dt = LOOP_US / 1e6;
    else
        dt = (now.tv_sec  - last.tv_sec) +
             (now.tv_nsec - last.tv_nsec) / 1e9;

    last = now;
    return dt;
}

/* =====================================================
 * 각도 클램프 ±90°
 * ===================================================== */
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

    printf("MPU-6050 (Kernel Driver via sysfs)\n");
    printf("Pitch / Roll / Yaw  +  Accel  Monitor\n\n");

    /* 커널 캘리브레이션 재실행 */
    FILE *cal = fopen(BASE_PATH "calibrate", "w");
    if (cal) { fprintf(cal, "1"); fclose(cal); }
    printf("[INFO] Gyro calibration done.\n\n");

    double pitch = 0.0, roll = 0.0, yaw = 0.0;
    double print_timer = 0.0;
    int    first = 1;

    while (1) {
        double dt = get_dt();

        /* ── 원시 데이터 읽기 ──────────────────────── */
        int ax_mg = read_sysfs("accel_x");
        int ay_mg = read_sysfs("accel_y");
        int az_mg = read_sysfs("accel_z");
        int gx_m  = read_sysfs("gyro_x");
        int gy_m  = read_sysfs("gyro_y");
        int gz_m  = read_sysfs("gyro_z");

        /* ── 단위 변환 ─────────────────────────────── */
        double ax = ax_mg / 1000.0;   /* g */
        double ay = ay_mg / 1000.0;
        double az = az_mg / 1000.0;

        double gx = gx_m / 1000.0;   /* °/s */
        double gy = gy_m / 1000.0;
        double gz = gz_m / 1000.0;

        /* ── 가속도 기반 pitch / roll ──────────────── *
         *                                               *
         *  pitch: X축 기울기                            *
         *    → atan2(-ax, sqrt(ay²+az²))               *
         *                                               *
         *  roll : Y축 기울기 (개선된 공식)              *
         *    → atan2(ay, az >= 0 ? norm : -norm)        *
         *    기존 atan2(ay, az) 는 az 부호 변화(뒤집힘) *
         *    시 180° 점프 + pitch 변화에 roll이         *
         *    같이 튀는 문제 있음 → norm 기반으로 수정   *
         * ─────────────────────────────────────────── */
        double norm_yz = sqrt(ay * ay + az * az);

        double accel_pitch = atan2(-ax, norm_yz) * 180.0 / M_PI;

        /* roll: az 부호에 따라 분기 → pitch 변화에 독립적 */
        double accel_roll;
        if (az >= 0.0)
            accel_roll = atan2( ay,  norm_yz) * 180.0 / M_PI;
        else
            accel_roll = atan2(-ay, -norm_yz) * 180.0 / M_PI;

        /* ── 첫 루프: 가속도 기반으로 초기화 ─────── */
        if (first) {
            pitch = accel_pitch;
            roll  = accel_roll;
            yaw   = 0.0;
            first = 0;
            sleep_us(LOOP_US);
            continue;
        }

        /* ── Complementary Filter ─────────────────── *
         *  pitch: gyro_y 적분 + 가속도 보정           *
         *  roll : gyro_x 적분 + 가속도 보정           *
         * ─────────────────────────────────────────── */
        pitch = ALPHA * (pitch + gy * dt) + (1.0 - ALPHA) * accel_pitch;
        roll  = ALPHA * (roll  + gx * dt) + (1.0 - ALPHA) * accel_roll;

        /* ── Yaw: 자이로 적분 (deadzone으로 drift 억제) */
        if (abs(gz_m) > GYRO_Z_DEADZONE)
            yaw += gz * dt;

        /* ── 중력 크기 ──────────────────────────────── */
        double g_mag = sqrt(ax*ax + ay*ay + az*az);

        /* ── 출력 (500ms마다) ────────────────────────── */
        print_timer += dt * 1000.0;
        if (print_timer >= PRINT_MS) {
            print_timer = 0.0;

            printf("Pitch: %7.2f°  Roll: %7.2f°  Yaw: %8.2f°\n",
                   clamp90(pitch), clamp90(roll), yaw);
            printf("Accel (g):  X: %6.3f  Y: %6.3f  Z: %6.3f  |g|: %.3f\n\n",
                   ax, ay, az, g_mag);
        }

        sleep_us(LOOP_US);
    }

    return 0;
}