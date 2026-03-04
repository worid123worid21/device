#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "motor.h"

/* ------------------------------------------------------------------ */
/* 내부 GPIO 유틸                                                        */
/* ------------------------------------------------------------------ */

static int gpio_export(int pin)
{
    char buf[64];
    int fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd < 0) { perror("gpio export open"); return -1; }
    snprintf(buf, sizeof(buf), "%d", pin);
    write(fd, buf, strlen(buf));
    close(fd);
    return 0;
}

static int gpio_unexport(int pin)
{
    char buf[64];
    int fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd < 0) { perror("gpio unexport open"); return -1; }
    snprintf(buf, sizeof(buf), "%d", pin);
    write(fd, buf, strlen(buf));
    close(fd);
    return 0;
}

static int gpio_set_direction(int pin, const char *dir) /* "out" or "in" */
{
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", pin);
    int fd = open(path, O_WRONLY);
    if (fd < 0) { perror("gpio direction open"); return -1; }
    write(fd, dir, strlen(dir));
    close(fd);
    return 0;
}

static int gpio_write(int pin, int value)
{
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);
    int fd = open(path, O_WRONLY);
    if (fd < 0) { perror("gpio value open"); return -1; }
    write(fd, value ? "1" : "0", 1);
    close(fd);
    return 0;
}

/* ------------------------------------------------------------------ */
/* 제어 핀 목록                                                          */
/* ------------------------------------------------------------------ */

static const int PINS[] = {
    MOTOR_PIN_FORWARD,
    MOTOR_PIN_BACKWARD,
    MOTOR_PIN_LEFT,
    MOTOR_PIN_RIGHT,
};
#define PIN_COUNT (int)(sizeof(PINS) / sizeof(PINS[0]))

/* ------------------------------------------------------------------ */
/* 공개 API                                                             */
/* ------------------------------------------------------------------ */

int motor_init(void)
{
    for (int i = 0; i < PIN_COUNT; i++) {
        if (gpio_export(PINS[i]) < 0)       return -1;
        usleep(100000); /* export 후 sysfs 안정화 대기 */
        if (gpio_set_direction(PINS[i], "out") < 0) return -1;
        gpio_write(PINS[i], 0);
    }
    printf("[motor] GPIO 초기화 완료\n");
    return 0;
}

void motor_cleanup(void)
{
    motor_stop();
    for (int i = 0; i < PIN_COUNT; i++)
        gpio_unexport(PINS[i]);
    printf("[motor] GPIO 해제 완료\n");
}

/* 모든 핀 LOW → 지정 핀만 HIGH */
void motor_set(MotorDir dir)
{
    /* 먼저 전체 OFF */
    for (int i = 0; i < PIN_COUNT; i++)
        gpio_write(PINS[i], 0);

    switch (dir) {
    case MOTOR_FORWARD:
        gpio_write(MOTOR_PIN_FORWARD,  1);
        printf("[motor] ↑ 전진\n");
        break;
    case MOTOR_BACKWARD:
        gpio_write(MOTOR_PIN_BACKWARD, 1);
        printf("[motor] ↓ 후진\n");
        break;
    case MOTOR_LEFT:
        gpio_write(MOTOR_PIN_LEFT,     1);
        printf("[motor] ← 좌회전\n");
        break;
    case MOTOR_RIGHT:
        gpio_write(MOTOR_PIN_RIGHT,    1);
        printf("[motor] → 우회전\n");
        break;
    case MOTOR_STOP:
    default:
        printf("[motor] ■ 정지\n");
        break;
    }
}

void motor_stop(void)
{
    motor_set(MOTOR_STOP);
}

