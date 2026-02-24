#include "servo_module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

// ─────────────────────────────────────────────
//  상수 정의
// ─────────────────────────────────────────────
#define PERIOD_NS       20000000    // 20ms (50Hz)
#define DUTY_MIN_NS     500000      // 0.5ms  →   0°
#define DUTY_MAX_NS     2500000     // 2.5ms  → 180°
#define DUTY_CENTER_NS  1500000     // 1.5ms  →  90°
#define EXPORT_DELAY_US 100000      // export 후 대기 100ms

// ─────────────────────────────────────────────
//  내부 헬퍼
// ─────────────────────────────────────────────

/**
 * @brief sysfs 파일에 문자열 쓰기
 * @return 0: 성공, -1: 실패
 */
static int write_sysfs(const char *path, const char *value)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "[servo] open failed: %s (%s)\n", path, strerror(errno));
        return -1;
    }

    ssize_t written = write(fd, value, strlen(value));
    close(fd);

    if (written < 0) {
        fprintf(stderr, "[servo] write failed: %s (%s)\n", path, strerror(errno));
        return -1;
    }
    return 0;
}

/**
 * @brief 각도 → duty cycle(ns) 변환
 * MG996R: 0° = 0.5ms, 180° = 2.5ms
 */
static int angle_to_duty_ns(float angle)
{
    return DUTY_MIN_NS + (int)((angle / 180.0f) * (DUTY_MAX_NS - DUTY_MIN_NS));
}

/**
 * @brief 실제 PWM sysfs 적용
 * @return SERVO_OK or SERVO_ERR_IO
 */
static ServoError apply_duty(int chip, int channel, int duty_ns)
{
    char path[128];
    char buf[32];

    snprintf(path, sizeof(path),
             "/sys/class/pwm/pwmchip%d/pwm%d/duty_cycle", chip, channel);
    snprintf(buf, sizeof(buf), "%d", duty_ns);

    if (write_sysfs(path, buf) < 0)
        return SERVO_ERR_IO;

    return SERVO_OK;
}

// ─────────────────────────────────────────────
//  ServoChannel 구현
// ─────────────────────────────────────────────

ServoError servo_channel_init(ServoChannel *ch,
                               int pwm_chip, int pwm_ch,
                               float min_angle, float max_angle)
{
    if (!ch) return SERVO_ERR_INIT;

    memset(ch, 0, sizeof(ServoChannel));
    ch->pwm_chip    = pwm_chip;
    ch->pwm_channel = pwm_ch;
    ch->min_angle   = min_angle;
    ch->max_angle   = max_angle;
    ch->current_angle = 90.0f;

    pthread_mutex_init(&ch->lock, NULL);

    char path[128];
    char buf[32];

    // 1. export
    snprintf(path, sizeof(path),
             "/sys/class/pwm/pwmchip%d/export", pwm_chip);
    snprintf(buf, sizeof(buf), "%d", pwm_ch);
    // 이미 export된 경우 EBUSY 무시
    int fd = open(path, O_WRONLY);
    if (fd >= 0) {
        write(fd, buf, strlen(buf));
        close(fd);
    }
    usleep(EXPORT_DELAY_US);

    // 2. period 설정
    snprintf(path, sizeof(path),
             "/sys/class/pwm/pwmchip%d/pwm%d/period", pwm_chip, pwm_ch);
    snprintf(buf, sizeof(buf), "%d", PERIOD_NS);
    if (write_sysfs(path, buf) < 0) return SERVO_ERR_INIT;

    // 3. 초기 duty (중앙 90°)
    if (apply_duty(pwm_chip, pwm_ch, DUTY_CENTER_NS) != SERVO_OK)
        return SERVO_ERR_INIT;

    // 4. enable
    snprintf(path, sizeof(path),
             "/sys/class/pwm/pwmchip%d/pwm%d/enable", pwm_chip, pwm_ch);
    if (write_sysfs(path, "1") < 0) return SERVO_ERR_INIT;

    ch->initialized = 1;
    printf("[servo] chip%d-ch%d initialized (range: %.0f°~%.0f°)\n",
           pwm_chip, pwm_ch, min_angle, max_angle);
    return SERVO_OK;
}

ServoError servo_channel_set_angle(ServoChannel *ch, float angle)
{
    if (!ch || !ch->initialized) return SERVO_ERR_NOT_INIT;

    // 범위 클램핑
    if (angle < ch->min_angle) angle = ch->min_angle;
    if (angle > ch->max_angle) angle = ch->max_angle;

    int duty = angle_to_duty_ns(angle);

    pthread_mutex_lock(&ch->lock);
    ServoError ret = apply_duty(ch->pwm_chip, ch->pwm_channel, duty);
    if (ret == SERVO_OK)
        ch->current_angle = angle;
    pthread_mutex_unlock(&ch->lock);

    return ret;
}

ServoError servo_channel_get_angle(ServoChannel *ch, float *out)
{
    if (!ch || !ch->initialized || !out) return SERVO_ERR_NOT_INIT;

    pthread_mutex_lock(&ch->lock);
    *out = ch->current_angle;
    pthread_mutex_unlock(&ch->lock);

    return SERVO_OK;
}

void servo_channel_cleanup(ServoChannel *ch)
{
    if (!ch || !ch->initialized) return;

    char path[128];
    char buf[32];

    // disable
    snprintf(path, sizeof(path),
             "/sys/class/pwm/pwmchip%d/pwm%d/enable",
             ch->pwm_chip, ch->pwm_channel);
    write_sysfs(path, "0");

    // unexport
    snprintf(path, sizeof(path),
             "/sys/class/pwm/pwmchip%d/unexport", ch->pwm_chip);
    snprintf(buf, sizeof(buf), "%d", ch->pwm_channel);
    write_sysfs(path, buf);

    pthread_mutex_destroy(&ch->lock);
    ch->initialized = 0;

    printf("[servo] chip%d-ch%d released\n", ch->pwm_chip, ch->pwm_channel);
}

// ─────────────────────────────────────────────
//  PanTilt 통합 API 구현
// ─────────────────────────────────────────────

ServoError pantilt_init(PanTiltUnit *pt,
                         int chip,
                         int pan_channel,
                         int tilt_channel)
{
    if (!pt) return SERVO_ERR_INIT;

    ServoError err;

    // Pan: 70°~170° (수평 리밋)
    err = servo_channel_init(&pt->pan, chip, pan_channel, 70.0f, 170.0f);
    if (err != SERVO_OK) return err;

    // Tilt: 0°~180° (수직 전범위)
    err = servo_channel_init(&pt->tilt, chip, tilt_channel, 0.0f, 180.0f);
    if (err != SERVO_OK) {
        servo_channel_cleanup(&pt->pan);
        return err;
    }

    printf("[pantilt] Pan(ch%d) + Tilt(ch%d) ready\n",
           pan_channel, tilt_channel);
    return SERVO_OK;
}

ServoError pantilt_set(PanTiltUnit *pt, float pan_angle, float tilt_angle)
{
    if (!pt) return SERVO_ERR_NOT_INIT;

    ServoError e1 = servo_channel_set_angle(&pt->pan,  pan_angle);
    ServoError e2 = servo_channel_set_angle(&pt->tilt, tilt_angle);

    // 둘 다 성공 시 OK, 아니면 첫 번째 에러 반환
    return (e1 != SERVO_OK) ? e1 : e2;
}

ServoError pantilt_center(PanTiltUnit *pt)
{
    return pantilt_set(pt, 90.0f, 90.0f);
}

void pantilt_cleanup(PanTiltUnit *pt)
{
    if (!pt) return;
    servo_channel_cleanup(&pt->pan);
    servo_channel_cleanup(&pt->tilt);
    printf("[pantilt] cleanup done\n");
}

// ─────────────────────────────────────────────
//  유틸리티
// ─────────────────────────────────────────────

const char *servo_strerror(ServoError err)
{
    switch (err) {
        case SERVO_OK:           return "OK";
        case SERVO_ERR_INIT:     return "Initialization failed";
        case SERVO_ERR_ANGLE:    return "Angle out of range";
        case SERVO_ERR_IO:       return "sysfs I/O error";
        case SERVO_ERR_NOT_INIT: return "Channel not initialized";
        default:                 return "Unknown error";
    }
}